#include "repurchases_internal.hpp"

#include <algorithm>

namespace tdx {

using namespace detail::repurchases;

Json normalize_hk_repurchase_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities,
    bool with_security) {
    if (!rows.is_array()) throw Error("Hong Kong repurchase rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        if (with_security) {
            const auto code = text_value(row, "$ZQDM");
            if (!digits(code, 5)) continue;
            const int id = market_id(text_value(row, "$SC"));
            item["security"] = security_document(id, code, securities);
        }
        item["date"] = text_value(row, "jyrq");
        item["average_price"] = number_or_null(number_value(row, "hgjj"));
        item["currency"] = text_value(row, "jybz");
        item["close"] = number_or_null(number_value(row, "spj"));
        item["premium_pct"] = number_or_null(number_value(row, "hgyj"));
        item["shares"] = number_or_null(number_value(row, "hgsl"));
        item["amount"] = number_or_null(number_value(row, "hgje"));
        item["high"] = number_or_null(number_value(row, "zgj"));
        item["low"] = number_or_null(number_value(row, "zdj"));
        item["method"] = text_value(row, "hgfs");
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "date") > text_value(right, "date");
        });
    return result;
}

}  // namespace tdx
