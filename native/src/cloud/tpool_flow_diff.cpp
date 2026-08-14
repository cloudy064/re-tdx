#include "tpool_internal.hpp"

#include "tdx/common.hpp"

#include <cstdint>
#include <map>
#include <set>
#include <string>

namespace tdx {

using namespace tpool_detail;

Json diff_tpool_alerts_document(const Json& previous, const Json& current) {
    const auto collect = [](const Json& document) {
        std::set<std::string, std::less<>> keys;
        const auto* securities = optional(document, "securities");
        if (securities && securities->is_array()) {
            for (const auto& security : securities->as_array()) {
                const auto security_id = json_text(security, "security_id");
                const auto cell_id = json_text(security, "cell_id");
                const auto* rules = optional(security, "rules");
                if (!rules || !rules->is_array()) continue;
                for (std::size_t index = 0; index < rules->size(); ++index) {
                    const auto& rule = rules->as_array()[index];
                    if (json_text(rule, "cell_id") != cell_id ||
                        json_text(rule, "status") != "evaluated" ||
                        !json_bool(rule, "matched")) continue;
                    keys.insert("rule:" + security_id + ":" + std::to_string(index));
                }
            }
        }
        const auto* projection = optional(document, "flow_projection");
        const auto* transitions = projection ? optional(*projection, "transitions") : nullptr;
        if (transitions && transitions->is_array()) {
            for (const auto& transition : transitions->as_array()) {
                const auto prefix = "flow:" + json_text(transition, "start_cell_id") + ":" +
                                    json_text(transition, "end_cell_id") + ":";
                const auto* ids = optional(transition, "matched_securities");
                if (!ids || !ids->is_array()) continue;
                for (const auto& id : ids->as_array())
                    if (id.is_string()) keys.insert(prefix + id.as_string());
            }
        }
        const auto* runtime = optional(document, "flow_runtime");
        const auto* memberships = runtime ? optional(*runtime, "memberships") : nullptr;
        if (memberships && memberships->is_array()) {
            for (const auto& membership : memberships->as_array()) {
                const auto prefix = "flow-state:" + json_text(membership, "cell_id") + ":";
                const auto* ids = optional(membership, "securities");
                if (!ids || !ids->is_array()) continue;
                for (const auto& id : ids->as_array())
                    if (id.is_string()) keys.insert(prefix + id.as_string());
            }
        }
        return keys;
    };
    const auto before = collect(previous);
    const auto after = collect(current);
    Json entered = Json::array();
    Json exited = Json::array();
    Json active = Json::array();
    for (const auto& key : after) {
        active.push_back(key);
        if (!before.count(key)) entered.push_back(key);
    }
    for (const auto& key : before) if (!after.count(key)) exited.push_back(key);
    Json result = Json::object();
    result["schema_version"] = 1;
    result["schema"] = "tdx-tpool-alert-diff-v1";
    result["previous_count"] = static_cast<std::uint64_t>(before.size());
    result["active_count"] = static_cast<std::uint64_t>(after.size());
    result["entered_count"] = static_cast<std::uint64_t>(entered.size());
    result["exited_count"] = static_cast<std::uint64_t>(exited.size());
    result["entered"] = std::move(entered);
    result["exited"] = std::move(exited);
    result["active"] = std::move(active);
    return result;
}

}  // namespace tdx

