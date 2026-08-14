#include "state_owned_reform_internal.hpp"

#include "tdx/common.hpp"

#include <set>
#include <sstream>

namespace tdx {

Json normalize_state_owned_groups(
    const Json& rows, const StateOwnedDimension& dimension,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace state_owned_detail;
    if (!rows.is_array()) throw Error("state-owned group rows must be an array");
    Json result = Json::array();
    std::set<std::string> seen;
    for (const auto& raw : rows.as_array()) {
        const auto id = text_value(raw, "$ZQDM");
        if (id.empty() || !seen.insert(id).second)
            throw Error("missing or duplicate state-owned group id");
        Json members = Json::array();
        std::set<std::pair<int, std::string>> member_seen;
        std::stringstream input(text_value(raw, "$S_ZQDM"));
        std::string item;
        while (std::getline(input, item, ',')) {
            item = trim(item);
            const auto separator = item.find('|');
            if (separator == std::string::npos) continue;
            const auto code = trim(item.substr(separator + 1));
            if (!digits(code, 6)) continue;
            int market = -1;
            try {
                market = market_id(item.substr(0, separator));
            } catch (...) {
                continue;
            }
            if (member_seen.insert({market, code}).second)
                members.push_back(security_document(market, code, securities));
        }
        const auto declared = number_value(raw, "CFGS");
        Json row = Json::object();
        row["group_id"] = id;
        row["dimension"] = dimension.name;
        row["dimension_label"] = dimension.label;
        row["unit_id"] = dimension.unit_id;
        row["name"] = text_value(raw, dimension.name_field);
        row["declared_member_count"] = declared
            ? Json(static_cast<std::uint64_t>(*declared)) : Json(nullptr);
        row["member_count"] = static_cast<std::uint64_t>(members.size());
        row["count_matches"] = !declared ||
            static_cast<std::uint64_t>(*declared) == members.size();
        row["detail_resource"] = "gqgg/" + id + ".jsn";
        row["members"] = std::move(members);
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

}  // namespace tdx
