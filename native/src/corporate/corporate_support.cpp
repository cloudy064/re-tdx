#include "corporate_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/minute.hpp"
#include "tdx/security_identity.hpp"
#include "tdx/session_audit.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::corporate_detail {
std::string now_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now)) throw Error("cannot read local time");
#else
    if (!localtime_r(&now, &local)) throw Error("cannot read local time");
#endif
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%dT%H:%M:%S");
    return output.str();
}

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

SecurityCode parse_security(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    int market_id = -1;
    std::string code;
    const auto colon = value.find(':');
    if (colon != std::string::npos) {
        const auto prefix = value.substr(0, colon);
        code = value.substr(colon + 1);
        if (prefix == "sz" || prefix == "0") market_id = 0;
        else if (prefix == "sh" || prefix == "1") market_id = 1;
        else if (prefix == "bj" || prefix == "2") market_id = 2;
        else throw Error("market must be sz/sh/bj or 0/1/2");
    } else if (value.size() == 8 &&
               (value.rfind("sz", 0) == 0 || value.rfind("sh", 0) == 0 ||
                value.rfind("bj", 0) == 0)) {
        market_id = value.rfind("sz", 0) == 0 ? 0 : value.rfind("sh", 0) == 0 ? 1 : 2;
        code = value.substr(2);
    } else {
        code = value;
        if (!code.empty() && (code[0] == '6' || code[0] == '9')) market_id = 1;
        else if (code.rfind("92", 0) == 0 || (!code.empty() && code[0] == '8')) market_id = 2;
        else market_id = 0;
    }
    if (market_id < 0 || market_id > 2 || code.size() != 6 ||
        !std::all_of(code.begin(), code.end(), [](char ch) { return ch >= '0' && ch <= '9'; }))
        throw Error("invalid security: " + value);
    return SecurityCode{market_id, code};
}

SecurityCode parse_kline_security(std::string value) {
    value = trim(std::move(value));
    const auto colon = value.find(':');
    if (colon == std::string::npos) {
        if (is_tdx_block_index_code(value)) return SecurityCode{1, value};
        return parse_security(value);
    }
    const auto prefix = lower_ascii(trim(value.substr(0, colon)));
    if (prefix == "sz" || prefix == "sh" || prefix == "bj" ||
        prefix == "0" || prefix == "1" || prefix == "2")
        return parse_security(value);

    static const std::map<std::string, int> aliases{
        {"qz", 28}, {"qd", 29}, {"qs", 30}, {"cz", 47}, {"qg", 66}
    };
    int market_id = -1;
    if (const auto found = aliases.find(prefix); found != aliases.end()) {
        market_id = found->second;
    } else {
        try {
            std::size_t used = 0;
            const auto parsed = std::stoul(prefix, &used);
            if (used == prefix.size() && parsed >= 3 && parsed <= 255)
                market_id = static_cast<int>(parsed);
        } catch (...) {}
    }
    auto code = trim(value.substr(colon + 1));
    std::transform(code.begin(), code.end(), code.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    if (market_id < 3 || !valid_expansion_code(code))
        throw Error("invalid expansion security; use MARKET:CODE, for example 47:IFL9");
    return SecurityCode{market_id, std::move(code)};
}

std::vector<SecurityCode> parse_securities(const std::vector<std::string>& values) {
    if (values.empty()) throw Error("at least one --security is required");
    std::vector<SecurityCode> result;
    std::set<std::pair<int, std::string>> seen;
    for (const auto& value : values) {
        auto security = parse_security(value);
        if (seen.insert(security.key()).second) result.push_back(std::move(security));
    }
    return result;
}


Bytes finance_request(const std::vector<SecurityCode>& securities) {
    if (securities.size() > 0xFFFF) throw Error("finance batch exceeds uint16");
    Bytes result{static_cast<std::uint8_t>(securities.size()),
                 static_cast<std::uint8_t>(securities.size() >> 8)};
    for (const auto& item : securities) {
        result.push_back(static_cast<std::uint8_t>(item.market_id));
        result.insert(result.end(), item.code.begin(), item.code.end());
    }
    return result;
}

Bytes capital_request(const SecurityCode& security) {
    return finance_request({security});
}

Bytes limits_request(int start_index) {
    if (start_index < 0 || start_index > 0xFFFF)
        throw Error("--start-index must be in 0..65535");
    Bytes result(14, 0);
    result[0] = static_cast<std::uint8_t>(start_index);
    result[1] = static_cast<std::uint8_t>(start_index >> 8);
    return result;
}


std::vector<Endpoint> effective_endpoints(const std::vector<Endpoint>& values) {
    if (!values.empty()) return values;
    return {parse_endpoint("110.41.147.114:7709")};
}

Json quote_transport_metadata(int connection_attempts, int transient_retries,
                              int endpoints_attempted,
                              const std::vector<Endpoint>& candidates) {
    Json transport = Json::object();
    transport["connection_attempts"] = connection_attempts;
    transport["transient_retries"] = transient_retries;
    transport["endpoints_attempted"] = endpoints_attempted;
    transport["max_attempts_per_endpoint"] = 3;
    transport["recovered_after_retry"] = transient_retries > 0;
    transport["endpoint_failover"] = endpoints_attempted > 1;
    transport["candidate_endpoint_count"] =
        static_cast<std::uint64_t>(candidates.size());
    const auto configured = !candidates.empty() &&
        candidates.front().name != "command line" &&
        candidates.front().name != "compiled public fallback";
    transport["endpoint_source"] = configured
        ? "connect.cfg:hqhost-primary-first"
        : (candidates.empty() || candidates.front().name == "compiled public fallback"
            ? "compiled-default" : "explicit-host");
    return transport;
}

int bounded_integer(const std::string& text, std::string_view name, int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

std::vector<Endpoint> command_endpoints(Args& args, const fs::path& root) {
    std::vector<Endpoint> result;
    for (const auto& value : args.take_options("--host")) result.push_back(parse_endpoint(value));
    return result.empty() ? load_public_quote_endpoints(root).endpoints : result;
}

void write_document(const fs::path& output, const Json& document, bool compact) {
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
}

}  // namespace tdx::corporate_detail
