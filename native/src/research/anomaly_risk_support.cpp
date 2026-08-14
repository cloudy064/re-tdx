#include "anomaly_risk_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/cloud_resilience.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::detail::anomaly_risk {

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
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
    output << local_timestamp_text(local);
    return output.str();
}

const Json* field(const Json& row, std::string_view key) {
    if (!row.is_object()) return nullptr;
    const auto exact = row.as_object().find(key);
    if (exact != row.as_object().end()) return &exact->second;
    const auto wanted = lower_ascii(std::string(key));
    for (const auto& [name, value] : row.as_object())
        if (lower_ascii(name) == wanted) return &value;
    return nullptr;
}

std::string text(const Json& row, std::string_view key) {
    const auto* value = field(row, key);
    if (!value || value->is_null()) return {};
    if (value->is_string()) return trim(value->as_string());
    if (value->is_number()) {
        std::ostringstream output;
        output << std::setprecision(15) << value->as_number();
        return output.str();
    }
    return {};
}

std::optional<double> number(const Json& row, std::string_view key) {
    const auto* value = field(row, key);
    if (!value || value->is_null()) return std::nullopt;
    if (value->is_number()) return value->as_number();
    if (!value->is_string()) return std::nullopt;
    auto raw = trim(value->as_string());
    raw.erase(std::remove(raw.begin(), raw.end(), ','), raw.end());
    if (!raw.empty() && raw.back() == '%') raw.pop_back();
    if (raw.empty() || raw == "-" || raw == "--") return std::nullopt;
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(raw, &used);
        if (used == raw.size() && std::isfinite(parsed)) return parsed;
    } catch (...) {}
    return std::nullopt;
}

Json number_json(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

bool digits(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(),
        [](unsigned char ch) { return ch >= '0' && ch <= '9'; });
}

Json date_json(const std::string& raw) {
    const auto value = trim(raw);
    if (value.empty() || value == "0" || value == "00000000") return Json(nullptr);
    if (value.size() == 8 && digits(value))
        return value.substr(0, 4) + "-" + value.substr(4, 2) + "-" +
               value.substr(6, 2);
    return value;
}

int row_market_id(const Json& row, std::string_view key) {
    const auto raw = text(row, key);
    if (raw == "0") return 0;
    if (raw == "1") return 1;
    if (raw == "2" || raw == "44") return 2;
    throw Error("anomaly-risk row has invalid market in " +
                std::string(key) + ": " + raw);
}

std::string market_name(int market) {
    return market == 0 ? "sz" : market == 1 ? "sh" : "bj";
}

std::string market_prefix(int market) {
    return market == 0 ? "SZ" : market == 1 ? "SH" : "BJ";
}

std::string resolve_name(const BlockData& blocks, int market,
                         const std::string& code, std::string fallback) {
    fallback = trim(std::move(fallback));
    if (!fallback.empty()) return fallback;
    const auto found = blocks.securities.find({market, code});
    return found == blocks.securities.end() ? std::string{} : found->second.name;
}

Json identity_document(const BlockData& blocks, int market,
                       const std::string& code, const std::string& fallback_name,
                       const std::string& entity_type) {
    Json result = Json::object();
    const auto name = resolve_name(blocks, market, code, fallback_name);
    result["market"] = market_name(market);
    result["market_id"] = market;
    result["code"] = code;
    result["security_id"] = market_prefix(market) + code;
    result["name"] = name.empty() ? Json(nullptr) : Json(name);
    result["name_resolved"] = !name.empty();
    result["entity_type"] = entity_type;
    return result;
}

int bounded(const std::string& raw, std::string_view name,
            int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(raw, &used);
        if (used != raw.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) +
                    ".." + std::to_string(maximum));
    }
}

bool transient_error(const std::string& message) {
    return tdx::detail::is_transient_cloud_error(message);
}

}  // namespace tdx::detail::anomaly_risk
