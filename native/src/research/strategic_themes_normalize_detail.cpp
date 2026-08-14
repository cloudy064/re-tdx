#include "strategic_themes_internal.hpp"

#include <set>

namespace tdx {

Json normalize_strategic_theme_details(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace detail::strategic_themes;
    if (!rows.is_array()) throw Error("strategic theme detail rows must be an array");
    Json result = Json::array();
    std::set<std::pair<int, std::string>> seen;
    for (const auto& raw : rows.as_array()) {
        const auto code = text_value(raw, "$ZQDM");
        if (!digits(code, 6)) continue;
        int market = -1;
        try { market = market_id(text_value(raw, "$SC")); }
        catch (...) { continue; }
        if (!seen.insert({market, code}).second)
            throw Error("duplicate security in strategic theme detail: " + code);
        Json row = Json::object();
        row["security"] = security_document(market, code, securities);
        row["logic"] = text_value(raw, "tzlj");
        row["description"] = text_value(raw, "xxsm");
        Json references = Json::object();
        references["3d"] = text_value(raw, "fqprice_d3");
        references["5d"] = text_value(raw, "fqprice_d5");
        references["20d"] = text_value(raw, "fqprice_d20");
        references["60d"] = text_value(raw, "fqprice_d60");
        references["3m"] = text_value(raw, "price1");
        row["reference_prices"] = std::move(references);
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

}  // namespace tdx
