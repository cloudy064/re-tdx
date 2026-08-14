#include "funds_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/cloud_workflow.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;
namespace tdx::detail::funds {

std::string now_text() {
    const auto now = std::time(nullptr); std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now)) throw Error("cannot read local time");
#else
    if (!localtime_r(&now, &local)) throw Error("cannot read local time");
#endif
    std::ostringstream output; output << local_timestamp_text(local); return output.str();
}
fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}
const Json* find_value(const Json& object, const std::string& name) {
    if (!object.is_object()) throw Error("fund-flow record must be an object");
    const auto found = object.as_object().find(name);
    return found == object.as_object().end() ? nullptr : &found->second;
}
std::string scalar_text(const Json* value) {
    if (!value || value->is_null()) return {};
    if (value->is_string()) return value->as_string();
    if (value->is_number() || value->is_bool()) return value->dump(-1);
    throw Error("fund-flow field must be scalar");
}
Json scalar_copy(const Json& object, const std::string& name) {
    const auto* value = find_value(object, name); return value ? *value : Json("");
}
std::optional<double> number_value(const Json& object, const std::string& name) {
    const auto* value = find_value(object, name);
    if (!value || value->is_null()) return std::nullopt;
    if (value->is_number()) return value->as_number();
    if (!value->is_string()) return std::nullopt;
    try { std::size_t used = 0; const double parsed = std::stod(value->as_string(), &used);
        if (used == value->as_string().size() && std::isfinite(parsed)) return parsed;
    } catch (...) {} return std::nullopt;
}
int market_id_from_text(const std::string& value) {
    if (value == "0" || lower_ascii(value) == "sz") return 0;
    if (value == "1" || lower_ascii(value) == "sh") return 1;
    if (value == "2" || lower_ascii(value) == "bj") return 2;
    throw Error("market must be sz/sh/bj or 0/1/2");
}
std::string market_name(int id) { return id == 0 ? "sz" : id == 1 ? "sh" : id == 2 ? "bj" : "m" + std::to_string(id); }
std::string market_prefix(int id) { return id == 0 ? "SZ" : id == 1 ? "SH" : id == 2 ? "BJ" : "M" + std::to_string(id); }
int bounded_integer(const std::string& text, const std::string& name,
                    int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(name + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}
const Json& response_from_document(const Json& document) {
    const auto found = document.as_object().find("response");
    if (found == document.as_object().end() || !found->second.is_object()) throw Error("PBRPC document has no business response");
    return found->second;
}
Json response_rows(const Json& document) { return cloud_result_rows(response_from_document(document)); }
std::vector<std::pair<std::string, std::string>> industry_keys(const Json& rows) {
    if (!rows.is_array()) throw Error("fund-flow rows must be an array");
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto& row : rows.as_array()) { const auto market = scalar_text(find_value(row, "market")); const auto code = scalar_text(find_value(row, "code")); if (code.rfind("881", 0) == 0) result.emplace_back(market, code); }
    return result;
}
const Json* find_row(const Json& rows, const std::string& market, const std::string& code) {
    for (const auto& row : rows.as_array()) if (scalar_text(find_value(row, "market")) == market && scalar_text(find_value(row, "code")) == code) return &row;
    return nullptr;
}
bool transient_funds_error(const std::string& message) {
    for (const auto* token : {"PBRPC HTTP status 429", "PBRPC HTTP status 502", "PBRPC HTTP status 503", "PBRPC HTTP status 504", "PBRPC business ErrorCode 4", "RpcID -1", "WinHttpSendRequest failed", "WinHttpReceiveResponse failed"})
        if (message.find(token) != std::string::npos) return true;
    return false;
}
Json normalized_rows(const Json& rows, const std::map<std::pair<int, std::string>, Security>& securities, bool detail, const std::map<std::string, std::string>& names) {
    Json result = Json::array(); for (const auto& row : rows.as_array()) result.push_back(normalize_intraday_fund_record(row, securities, detail, names)); return result;
}
Json security_reference(int id, const std::string& code, const std::map<std::pair<int, std::string>, Security>& securities) {
    Json value = Json::object(); value["market_id"] = id; value["market"] = market_name(id); value["code"] = code; value["security_id"] = market_prefix(id) + code;
    const auto found = securities.find({id, code}); value["name"] = found == securities.end() ? "" : found->second.name; value["name_resolved"] = found != securities.end(); return value;
}
}  // namespace tdx::detail::funds
