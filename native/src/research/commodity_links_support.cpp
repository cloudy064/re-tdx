#include "commodity_links_internal.hpp"
#include "tdx/time.hpp"

namespace tdx::commodity_links_detail {
namespace fs = std::filesystem;

const Json* value_ptr(const Json& value, std::string_view key) {
    if (!value.is_object()) return nullptr;
    const auto found = value.as_object().find(key);
    return found == value.as_object().end() ? nullptr : &found->second;
}

std::string text_value(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found ? trim(jsn_scalar_text(*found)) : std::string{};
}

std::string text_value_any(const Json& value,
                           std::initializer_list<std::string_view> keys) {
    for (const auto key : keys) {
        const auto result = text_value(value, key);
        if (!result.empty()) return result;
    }
    return {};
}

std::optional<double> number_value(const Json& value, std::string_view key) {
    const auto text = text_value(value, key);
    if (text.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto result = std::stod(text, &used);
        return used == text.size() && std::isfinite(result)
            ? std::optional<double>(result) : std::nullopt;
    } catch (...) { return std::nullopt; }
}

std::optional<double> number_value_any(
    const Json& value, std::initializer_list<std::string_view> keys) {
    for (const auto key : keys) {
        const auto result = number_value(value, key);
        if (result) return result;
    }
    return std::nullopt;
}

Json number_json(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

bool digits(const std::string& value, std::size_t size) {
    return value.size() == size &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

std::string iso_date(const std::string& value) {
    if (!digits(value, 8)) return value;
    return value.substr(0, 4) + "-" + value.substr(4, 2) + "-" + value.substr(6, 2);
}

int market_id(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "0" || value == "sz") return 0;
    if (value == "1" || value == "sh") return 1;
    if (value == "2" || value == "44" || value == "bj") return 2;
    throw Error("market must be sz/sh/bj or 0/1/2");
}

std::string market_name(int id) {
    return id == 0 ? "sz" : id == 1 ? "sh" : "bj";
}

std::string market_prefix(int id) {
    return id == 0 ? "SZ" : id == 1 ? "SH" : "BJ";
}

Json security_document(
    int id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const auto found = securities.find({id, code});
    Json result = Json::object();
    result["market"] = market_name(id);
    result["market_id"] = id;
    result["code"] = code;
    result["security_id"] = market_prefix(id) + code;
    result["name"] = found == securities.end() ? "" : found->second.name;
    result["name_resolved"] = found != securities.end() && !found->second.name.empty();
    return result;
}

Json parse_security_set(
    const std::string& text,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    Json result = Json::array();
    std::set<std::pair<int, std::string>> seen;
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const auto end = text.find(',', begin);
        const auto item = trim(text.substr(begin, end == std::string::npos
            ? std::string::npos : end - begin));
        const auto bar = item.find('|');
        if (bar != std::string::npos) {
            const auto market = trim(item.substr(0, bar));
            const auto code = trim(item.substr(bar + 1));
            try {
                const auto id = market_id(market);
                if (digits(code, 6) && seen.insert({id, code}).second)
                    result.push_back(security_document(id, code, securities));
            } catch (...) {}
        }
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return result;
}

bool security_matches(const Json& security, int selected_market,
                      const std::string& code) {
    return security.is_object() &&
        static_cast<int>(security.at("market_id").as_number()) == selected_market &&
        security.at("code").as_string() == code;
}

bool security_set_contains(const Json& set, int selected_market,
                           const std::string& code) {
    if (!set.is_array()) return false;
    for (const auto& security : set.as_array())
        if (security_matches(security, selected_market, code)) return true;
    return false;
}

std::optional<double> pct_change(const std::optional<double>& latest,
                                 const std::optional<double>& base,
                                 bool absolute_denominator) {
    if (!latest || !base) return std::nullopt;
    const auto denominator = absolute_denominator ? std::abs(*base) : *base;
    if (denominator == 0.0) return std::nullopt;
    return (*latest - *base) * 100.0 / denominator;
}

bool json_contains(const Json& value, const std::string& needle) {
    if (value.is_string())
        return lower_ascii(value.as_string()).find(needle) != std::string::npos;
    if (value.is_number() || value.is_bool())
        return lower_ascii(jsn_scalar_text(value)).find(needle) != std::string::npos;
    if (value.is_array()) {
        for (const auto& child : value.as_array())
            if (json_contains(child, needle)) return true;
    } else if (value.is_object()) {
        for (const auto& [key, child] : value.as_object())
            if (lower_ascii(key).find(needle) != std::string::npos ||
                json_contains(child, needle)) return true;
    }
    return false;
}

std::string json_text(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found && found->is_string() ? found->as_string() : std::string{};
}

std::optional<double> json_number(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found && found->is_number()
        ? std::optional<double>(found->as_number()) : std::nullopt;
}

std::string now_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now)) throw Error("cannot read local time");
#else
    if (!localtime_r(&now, &local)) throw Error("cannot read local time");
#endif
    std::ostringstream output;
    output << local_timestamp_text(local);
    return output.str();
}

int bounded(const std::string& text, const std::string& name,
            int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const auto value = std::stoi(text, &used);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(name + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}


const Json* find_by_id(const Json& rows, std::string_view key,
                       const std::string& id) {
    if (!rows.is_array()) return nullptr;
    for (const auto& row : rows.as_array())
        if (json_text(row, key) == id) return &row;
    return nullptr;
}

Json find_all_by_id(const Json& rows, std::string_view key,
                    const std::string& id) {
    Json result = Json::array();
    if (!rows.is_array()) return result;
    for (const auto& row : rows.as_array())
        if (json_text(row, key) == id) result.push_back(row);
    return result;
}

void sort_rows(Json& rows, const std::string& view,
               const std::string& sort, const std::string& order) {
    if (!rows.is_array()) return;
    const bool descending = order == "desc";
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            std::string text_key;
            if (sort == "name") text_key = "name";
            else if (sort == "quote-date") text_key = "quote_date";
            else if (sort == "latest-driver-date") text_key = "latest_driver_date";
            else if (sort == "trigger-date") text_key = "trigger_date";
            if (!text_key.empty()) {
                const auto a = json_text(left, text_key), b = json_text(right, text_key);
                if (a != b) return descending ? a > b : a < b;
            } else {
                std::string key = sort == "price" ? "latest_price" :
                    sort == "day-change" ? "day_change_pct" :
                    sort == "5d" ? "change_5d_pct" :
                    sort == "10d" ? "change_10d_pct" :
                    sort == "30d" ? "change_30d_pct" :
                    sort == "60d" ? "change_60d_pct" :
                    sort == "stocks" ? "stock_count" : "source_rank";
                const auto a = json_number(left, key), b = json_number(right, key);
                if (a.has_value() != b.has_value()) return a.has_value();
                if (a && b && *a != *b) return descending ? *a > *b : *a < *b;
            }
            const auto a = json_text(left, view == "commodities" ? "commodity_id" : "theme_id");
            const auto b = json_text(right, view == "commodities" ? "commodity_id" : "theme_id");
            return a < b;
        });
}

}  // namespace tdx::commodity_links_detail
