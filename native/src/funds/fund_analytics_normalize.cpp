#include "fund_analytics_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>

namespace tdx {

Json normalize_fund_analytics_rows(const Json& rows, const std::string& view) {
    if (!rows.is_array()) throw Error("fund analytics rows must be an array");
    const auto* definition = fund_analytics_detail::view_definition(view);
    if (!definition)
        throw Error("unknown fund analytics normalization view: " + view);
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        if (!row.is_object()) continue;
        auto item = definition->normalize(row);
        if (item) result.push_back(std::move(*item));
    }
    using fund_analytics_detail::SortPolicy;
    switch (definition->sort) {
    case SortPolicy::date_ascending:
        std::sort(result.as_array().begin(), result.as_array().end(),
                  [](const Json& left, const Json& right) {
                      return left.at("date").as_string() <
                             right.at("date").as_string();
                  });
        break;
    case SortPolicy::month_ascending:
        std::sort(result.as_array().begin(), result.as_array().end(),
                  [](const Json& left, const Json& right) {
                      return left.at("month").as_string() <
                             right.at("month").as_string();
                  });
        break;
    case SortPolicy::report_date_descending:
        std::sort(result.as_array().begin(), result.as_array().end(),
                  [](const Json& left, const Json& right) {
                      return left.at("report_date").as_string() >
                             right.at("report_date").as_string();
                  });
        break;
    case SortPolicy::none:
        break;
    }
    return result;
}

}  // namespace tdx
