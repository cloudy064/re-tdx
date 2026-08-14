#include "formula_scan_internal.hpp"

namespace tdx {
using namespace formula_scan_detail;

Json diff_formula_scan_results(const Json& previous, const Json& current) {
    const auto collect = [](const Json& document) {
        std::map<std::string, Json, std::less<>> rows;
        const Json* matches = optional(document, "active_matches");
        if (!matches) matches = optional(document, "matches");
        if (!matches) return rows;
        if (!matches->is_array())
            throw Error("formula scan matches must be an array");
        for (const auto& row : matches->as_array()) {
            if (!row.is_object())
                throw Error("formula scan match must be an object");
            const auto* security_id = optional(row, "security_id");
            if (!security_id || !security_id->is_string() ||
                trim(security_id->as_string()).empty())
                throw Error("formula scan match requires security_id");
            rows[lower_ascii(trim(security_id->as_string()))] = row;
        }
        return rows;
    };
    const auto before = collect(previous);
    const auto after = collect(current);
    Json entered = Json::array(), exited = Json::array(), updated = Json::array();
    Json active = Json::array(), entered_ids = Json::array(), exited_ids = Json::array();
    Json updated_ids = Json::array(), active_ids = Json::array();
    for (const auto& [security_id, row] : after) {
        active.push_back(row);
        active_ids.push_back(security_id);
        const auto found = before.find(security_id);
        if (found == before.end()) {
            entered.push_back(row);
            entered_ids.push_back(security_id);
        } else if (found->second.dump(-1) != row.dump(-1)) {
            Json change = Json::object();
            change["security_id"] = security_id;
            change["previous"] = found->second;
            change["current"] = row;
            updated.push_back(std::move(change));
            updated_ids.push_back(security_id);
        }
    }
    for (const auto& [security_id, row] : before) {
        if (after.count(security_id)) continue;
        exited.push_back(row);
        exited_ids.push_back(security_id);
    }
    Json result = Json::object();
    result["schema_version"] = 1;
    result["schema"] = "tdx-formula-scan-diff-v1";
    result["previous_count"] = static_cast<std::uint64_t>(before.size());
    result["active_count"] = static_cast<std::uint64_t>(after.size());
    result["entered_count"] = static_cast<std::uint64_t>(entered.size());
    result["exited_count"] = static_cast<std::uint64_t>(exited.size());
    result["updated_count"] = static_cast<std::uint64_t>(updated.size());
    result["changed"] = entered.size() || exited.size() || updated.size();
    result["entered_ids"] = std::move(entered_ids);
    result["exited_ids"] = std::move(exited_ids);
    result["updated_ids"] = std::move(updated_ids);
    result["active_ids"] = std::move(active_ids);
    result["entered"] = std::move(entered);
    result["exited"] = std::move(exited);
    result["updated"] = std::move(updated);
    result["active"] = std::move(active);
    return result;
}

}  // namespace tdx
