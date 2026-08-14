#include "equity_valuation_internal.hpp"

#include <algorithm>
#include <set>

namespace tdx {

Json normalize_equity_valuation_rows(
    const std::string& raw_view, const Json& rows, const BlockData& blocks) {
    using namespace detail::equity_valuation;
    if (!rows.is_array()) throw Error("equity valuation rows must be an array");
    const auto& view = view_spec(raw_view);
    Json result = Json::array();
    std::set<std::string> identities;
    for (const auto& row : rows.as_array()) {
        if (!row.is_object()) continue;
        std::string identity;
        auto item = view.normalize_row(row, blocks, identity);
        if (!identities.insert(identity).second)
            throw Error("equity valuation response contains a duplicate identity: " + identity);
        result.push_back(std::move(item));
    }

    const auto key = view.sort == SortKind::industry ? "industry_id" : "security_id";
    std::sort(result.as_array().begin(), result.as_array().end(),
        [&](const Json& left, const Json& right) {
            if (view.sort == SortKind::history)
                return left.at("date").as_string() < right.at("date").as_string();
            const auto& l = view.sort == SortKind::industry
                ? left.at("industry") : left.at("security");
            const auto& r = view.sort == SortKind::industry
                ? right.at("industry") : right.at("security");
            return l.at(key).as_string() < r.at(key).as_string();
        });
    return result;
}

}  // namespace tdx
