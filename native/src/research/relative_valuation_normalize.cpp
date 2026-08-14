#include "relative_valuation_internal.hpp"

#include <algorithm>
#include <set>

namespace tdx {

Json normalize_relative_valuation_master_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace detail::relative_valuation;
    if (!rows.is_array()) throw Error("relative valuation master rows must be an array");
    Json result = Json::array();
    std::set<std::pair<int, std::string>> identities;
    for (const auto& row : rows.as_array()) {
        const auto code = text(row, "code");
        if (code.size() != 6 || !digits(code))
            throw Error("relative valuation master row has invalid code");
        const int market = market_id(text(row, "market"));
        if (!identities.emplace(market, code).second)
            throw Error("relative valuation master contains a duplicate market/code");
        const auto quantile = number(row, "statQuantiles");
        Json value = Json::object();
        value["security"] = security_document(market, code, securities);
        value["current_ratio"] = number_json(number(row, "statRatios"));
        value["historical_quantile_pct"] = number_json(quantile);
        value["historical_maximum_ratio"] = number_json(number(row, "maxRatio"));
        value["historical_minimum_ratio"] = number_json(number(row, "minRatio"));
        value["relative_position"] = relative_position(quantile);
        result.push_back(std::move(value));
    }
    std::sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return left.at("security").at("security_id").as_string() <
                   right.at("security").at("security_id").as_string();
        });
    return result;
}

Json normalize_relative_valuation_history_rows(const Json& rows) {
    using namespace detail::relative_valuation;
    if (!rows.is_array()) throw Error("relative valuation history rows must be an array");
    Json result = Json::array();
    std::set<std::string> dates;
    for (const auto& row : rows.as_array()) {
        const auto date = compact_date(text(row, "date"), "upstream date");
        if (!dates.insert(date).second)
            throw Error("relative valuation history contains duplicate date: " + date);
        Json value = Json::object();
        value["date"] = display_date(date);
        value["ratio"] = number_json(number(row, "ratio"));
        result.push_back(std::move(value));
    }
    std::sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return left.at("date").as_string() < right.at("date").as_string();
        });
    return result;
}

}  // namespace tdx
