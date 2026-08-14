#include "market_internal.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::market_detail {

fs::path from_utf8(const std::string &value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

int bounded_integer(const std::string &value, std::string_view option, int minimum,
                    int maximum) {
    try {
        std::size_t used = 0;
        const int parsed = std::stoi(value, &used);
        if (used != value.size() || parsed < minimum || parsed > maximum)
            throw std::invalid_argument("range");
        return parsed;
    } catch (...) {
        throw Error(std::string(option) + " must be in " + std::to_string(minimum) +
                    ".." + std::to_string(maximum));
    }
}

int positive_integer(const std::string &value, std::string_view option, int maximum) {
    return bounded_integer(value, option, 1, maximum);
}

std::string now_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%dT%H:%M:%S");
    return output.str();
}

CommonOptions parse_common_options(Args &args, std::string default_output) {
    const auto values = args.take_options("--security");
    if (values.empty())
        throw Error("at least one --security is required");
    CommonOptions result;
    std::set<std::pair<int, std::string>> seen;
    for (const auto &value : values) {
        auto code = parse_security(value);
        if (seen.insert(code.key()).second)
            result.codes.push_back(std::move(code));
    }
    result.timeout_ms = positive_integer(args.take_option("--timeout-ms", "10000"),
                                         "--timeout-ms", 600000);
    result.batch_size = positive_integer(args.take_option("--batch-size", "80"),
                                         "--batch-size", 1000);
    const auto root_text = args.take_option("--root");
    result.root = find_tdx_root(root_text.empty() ? fs::path{} : from_utf8(root_text));
    const auto hosts = args.take_options("--host");
    if (hosts.empty()) {
        auto selected = load_public_quote_endpoints(result.root);
        result.endpoints = std::move(selected.endpoints);
        result.endpoint_source = std::move(selected.source);
        result.available_endpoint_count = selected.available_endpoint_count;
        result.primary_configured = selected.primary_configured;
    } else {
        for (const auto &host : hosts)
            result.endpoints.push_back(parse_endpoint(host));
        result.endpoint_source = "explicit-host";
        result.available_endpoint_count = result.endpoints.size();
    }
    result.output = from_utf8(args.take_option("--output", std::move(default_output)));
    result.compact = args.take_flag("--compact");
    return result;
}

MarketL1SessionOptions session_options(const CommonOptions &selected) {
    MarketL1SessionOptions result;
    result.timeout_ms = selected.timeout_ms;
    result.batch_size = selected.batch_size;
    for (const auto &endpoint : selected.endpoints)
        result.endpoints.push_back(endpoint.address());
    result.endpoint_source = selected.endpoint_source;
    result.available_endpoint_count = selected.available_endpoint_count;
    result.primary_configured = selected.primary_configured;
    return result;
}

std::vector<std::string> display_codes(const CommonOptions &selected) {
    std::vector<std::string> result;
    result.reserve(selected.codes.size());
    for (const auto &code : selected.codes)
        result.push_back(code.display());
    return result;
}

} // namespace tdx::market_detail
