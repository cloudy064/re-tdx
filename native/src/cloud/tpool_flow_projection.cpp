#include "tpool_internal.hpp"

#include "tdx/common.hpp"

#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace tdx {

using namespace tpool_detail;

Json project_tpool_flow_document(const Json& evaluation) {
    const auto& inspection = evaluation.at("inspection");
    if (!json_bool(inspection.at("flow_graph"), "references_valid"))
        throw Error("TPool flow state requires unique cells and valid flow references");
    const auto& graph = inspection.at("flow_graph");
    Json result = Json::object();
    result["schema_version"] = 1;
    result["schema"] = "tdx-tpool-read-only-flow-projection-v1";
    result["read_only"] = true;
    result["tdx_state_mutated"] = false;
    result["runtime_semantics_verified"] = false;
    result["semantics"] =
        "topological projection: seed stocks enter their XML cell, every rule in that cell must match, then enabled edges receive candidates";
    result["excluded_runtime_features"] =
        "start/cycle timers, cyclic workers, clear/emptyps behavior, host callbacks and XML pool writeback";
    const bool valid = json_bool(graph, "references_valid") && json_bool(graph, "acyclic");
    result["evaluated"] = valid;
    Json transitions = Json::array();
    Json cell_rows = Json::array();
    std::size_t planned_action_count = 0;
    if (!valid) {
        result["message"] = "projection requires unique cells, valid edge references and an acyclic graph";
        result["cells"] = std::move(cell_rows);
        result["transitions"] = std::move(transitions);
        return result;
    }

    std::set<std::string, std::less<>> nodes;
    std::map<std::string, int, std::less<>> indegree;
    std::map<std::string, std::vector<std::string>, std::less<>> adjacency;
    for (const auto& cell : inspection.at("cells").as_array()) {
        const auto id = json_text(cell, "id");
        if (!id.empty()) nodes.insert(id);
    }
    for (const auto& node : nodes) { indegree[node] = 0; adjacency[node] = {}; }
    for (const auto& flow : inspection.at("flows").as_array()) {
        if (!json_bool(flow, "transfer_enabled")) continue;
        const auto start = json_text(flow, "startid");
        const auto end = json_text(flow, "endid");
        if (!nodes.count(start) || !nodes.count(end)) continue;
        adjacency[start].push_back(end);
        ++indegree[end];
    }
    std::vector<std::string> queue;
    for (const auto& node : nodes) if (indegree[node] == 0) queue.push_back(node);
    for (std::size_t cursor = 0; cursor < queue.size(); ++cursor)
        for (const auto& next : adjacency[queue[cursor]])
            if (--indegree[next] == 0) queue.push_back(next);

    std::map<std::string, std::set<std::string, std::less<>>, std::less<>> memberships;
    std::map<std::string, const Json*, std::less<>> securities;
    for (const auto& security : evaluation.at("securities").as_array()) {
        const auto id = json_text(security, "security_id");
        const auto cell = json_text(security, "cell_id");
        if (id.empty()) continue;
        securities.emplace(id, &security);
        if (!cell.empty()) memberships[cell].insert(id);
    }
    std::map<std::string, std::vector<std::size_t>, std::less<>> cell_rules;
    const auto& functions = inspection.at("functions").as_array();
    for (std::size_t index = 0; index < functions.size(); ++index)
        cell_rules[json_text(functions[index], "cell_id")].push_back(index);

    for (const auto& cell : queue) {
        const auto candidates = memberships[cell];
        std::set<std::string, std::less<>> passed;
        for (const auto& id : candidates) {
            const auto found = securities.find(id);
            if (found == securities.end()) continue;
            const auto& rules = found->second->at("rules").as_array();
            bool all_matched = true;
            for (const auto index : cell_rules[cell]) {
                if (index >= rules.size() || json_text(rules[index], "status") != "evaluated" ||
                    !json_bool(rules[index], "matched")) {
                    all_matched = false;
                    break;
                }
            }
            if (all_matched) passed.insert(id);
        }
        Json cell_row = Json::object();
        cell_row["cell_id"] = cell;
        cell_row["input_count"] = static_cast<std::uint64_t>(candidates.size());
        cell_row["rule_count"] = static_cast<std::uint64_t>(cell_rules[cell].size());
        cell_row["passed_count"] = static_cast<std::uint64_t>(passed.size());
        Json passed_ids = Json::array();
        for (const auto& id : passed) passed_ids.push_back(id);
        cell_row["passed_securities"] = std::move(passed_ids);
        cell_rows.push_back(std::move(cell_row));
        for (const auto& end : adjacency[cell]) {
            memberships[end].insert(passed.begin(), passed.end());
            Json transition = Json::object();
            transition["start_cell_id"] = cell;
            transition["end_cell_id"] = end;
            transition["candidate_count"] = static_cast<std::uint64_t>(candidates.size());
            transition["matched_count"] = static_cast<std::uint64_t>(passed.size());
            Json ids = Json::array();
            for (const auto& id : passed) ids.push_back(id);
            transition["matched_securities"] = std::move(ids);
            auto planned_actions = tpool_action_plans_document(
                inspection, end, passed, "projected-cell-entry");
            planned_action_count += planned_actions.size();
            transition["planned_actions"] = std::move(planned_actions);
            transitions.push_back(std::move(transition));
        }
    }
    result["message"] = "projection completed without invoking TPool.dll or changing XML/state";
    result["action_execution_mode"] = "planned-only";
    result["planned_action_count"] =
        static_cast<std::uint64_t>(planned_action_count);
    result["host_actions_executed"] = false;
    result["cells"] = std::move(cell_rows);
    result["transitions"] = std::move(transitions);
    return result;
}

}  // namespace tdx
