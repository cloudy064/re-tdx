#include "tpool_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace tdx {

using namespace tpool_detail;

Json advance_tpool_flow_state_document(const Json& evaluation,
                                       const Json& previous_state,
                                       const TpoolFlowClock& clock) {
    if (!evaluation.is_object() || !optional(evaluation, "inspection") ||
        !optional(evaluation, "securities"))
        throw Error("TPool flow state requires an evaluation with inspection and securities");
    if (clock.epoch_seconds < 0 || clock.local_date < 19000101 ||
        hhmmss_seconds(clock.local_hhmmss) < 0 ||
        clock.local_weekday < 1 || clock.local_weekday > 7)
        throw Error("TPool flow clock is invalid");
    const auto& inspection = evaluation.at("inspection");
    const auto& source = inspection.at("source");
    const auto signature = flow_source_signature(inspection);

    const bool previous_empty = !previous_state.is_object() || previous_state.size() == 0;
    if (!previous_empty && json_text(previous_state, "schema") != "tdx-tpool-flow-state-v1")
        throw Error("unsupported TPool flow state schema");
    const bool source_changed = !previous_empty &&
        json_text(previous_state, "source_signature") != signature;
    const bool resume = !previous_empty && !source_changed;
    const auto previous_epoch = resume
        ? json_integer_number(previous_state, "observed_epoch_seconds", clock.epoch_seconds)
        : clock.epoch_seconds;
    const auto previous_tick = resume
        ? std::max<std::int64_t>(0, json_integer_number(previous_state, "pool_tick_seconds", 0))
        : 0;
    const auto elapsed = std::max<std::int64_t>(0, clock.epoch_seconds - previous_epoch);
    const auto pool_tick = previous_tick + elapsed;
    const auto iteration = resume
        ? std::max<std::int64_t>(0, json_integer_number(previous_state, "iteration", 0)) + 1
        : 1;

    std::set<std::string, std::less<>> nodes;
    for (const auto& cell : inspection.at("cells").as_array()) {
        const auto id = json_text(cell, "id");
        if (!id.empty()) nodes.insert(id);
    }
    std::map<std::string, std::set<std::string, std::less<>>, std::less<>> memberships;
    for (const auto& node : nodes) memberships[node] = {};
    if (resume) {
        const auto* rows = optional(previous_state, "memberships");
        if (rows && rows->is_array()) {
            for (const auto& row : rows->as_array()) {
                const auto cell = json_text(row, "cell_id");
                if (!nodes.count(cell)) continue;
                const auto* ids = optional(row, "securities");
                if (!ids || !ids->is_array()) continue;
                for (const auto& id : ids->as_array())
                    if (id.is_string() && !id.as_string().empty())
                        memberships[cell].insert(id.as_string());
            }
        }
    } else {
        for (const auto& security : evaluation.at("securities").as_array()) {
            const auto id = json_text(security, "security_id");
            const auto cell = json_text(security, "cell_id");
            if (!id.empty() && nodes.count(cell)) memberships[cell].insert(id);
        }
    }

    std::map<int, FlowRuntimeState> previous_flows;
    if (resume) {
        const auto* rows = optional(previous_state, "flows");
        if (rows && rows->is_array()) {
            for (const auto& row : rows->as_array()) {
                const int index = static_cast<int>(json_integer_number(row, "flow_index", -1));
                if (index >= 0) previous_flows[index] = read_flow_runtime(row);
            }
        }
    }
    std::map<std::string, const Json*, std::less<>> securities;
    for (const auto& security : evaluation.at("securities").as_array()) {
        const auto id = json_text(security, "security_id");
        if (!id.empty()) securities[id] = &security;
    }
    std::map<std::string, std::vector<std::size_t>, std::less<>> cell_rules;
    const auto& functions = inspection.at("functions").as_array();
    for (std::size_t index = 0; index < functions.size(); ++index)
        cell_rules[json_text(functions[index], "cell_id")].push_back(index);

    Json events = Json::array();
    if (source_changed) {
        Json event = Json::object();
        event["type"] = "state-reset";
        event["iteration"] = iteration;
        event["pool_tick_seconds"] = pool_tick;
        event["reason"] = "pool source signature changed";
        event["previous_source_signature"] = json_text(previous_state, "source_signature");
        event["source_signature"] = signature;
        events.push_back(std::move(event));
    }
    if (resume && clock.epoch_seconds < previous_epoch) {
        Json event = Json::object();
        event["type"] = "clock-rewind";
        event["iteration"] = iteration;
        event["pool_tick_seconds"] = pool_tick;
        event["previous_epoch_seconds"] = previous_epoch;
        event["observed_epoch_seconds"] = clock.epoch_seconds;
        event["applied_elapsed_seconds"] = 0;
        events.push_back(std::move(event));
    }

    Json flow_rows = Json::array();
    std::size_t run_event_count = 0;
    std::size_t planned_action_count = 0;
    const auto& flows = inspection.at("flows").as_array();
    for (std::size_t index = 0; index < flows.size(); ++index) {
        const auto& flow = flows[index];
        const auto start = json_text(flow, "startid");
        const auto end = json_text(flow, "endid");
        const bool enabled = json_bool(flow, "transfer_enabled");
        const bool references_valid = nodes.count(start) && nodes.count(end);
        const int cycle_type = static_cast<int>(json_number(flow, "cycle_type_id", -1));
        const auto interval = std::max<std::int64_t>(
            1, json_integer_number(flow, "interval_seconds", 1));
        const auto window = static_cast<std::int64_t>(
            std::max(0.0, json_number(flow, "cycle_window_seconds", 0.0)));
        auto runtime = previous_flows.count(static_cast<int>(index))
            ? previous_flows.at(static_cast<int>(index)) : FlowRuntimeState{};
        bool due = false;
        bool newly_expired = false;
        std::string status;
        std::string schedule_reason;
        if (!enabled) {
            status = "disabled";
            schedule_reason = "tran=0";
        } else if (!references_valid) {
            status = "invalid-reference";
            schedule_reason = "startid or endid does not resolve to a cell";
        } else if (runtime.completed) {
            status = "completed";
            schedule_reason = "flow already completed";
        } else if (runtime.run_count == 0) {
            const auto decision = first_flow_schedule(flow, clock, pool_tick);
            schedule_reason = decision.reason;
            if (decision.kind == FlowScheduleDecision::Kind::ready) due = true;
            else if (decision.kind == FlowScheduleDecision::Kind::expired) {
                runtime.completed = true;
                newly_expired = true;
                status = "completed";
            } else if (decision.kind == FlowScheduleDecision::Kind::invalid)
                status = "invalid-schedule";
            else status = "waiting";
        } else if (cycle_type == 2) {
            runtime.completed = true;
            status = "completed";
            schedule_reason = "one-shot flow already ran";
        } else if (cycle_type == 1 &&
                   pool_tick - runtime.first_run_tick > window) {
            runtime.completed = true;
            newly_expired = true;
            status = "completed";
            schedule_reason = "cycle window elapsed";
        } else if (pool_tick - runtime.last_run_tick >= interval) {
            due = true;
            schedule_reason = "repeat interval elapsed";
        } else {
            status = "waiting";
            schedule_reason = "repeat interval has not elapsed";
        }

        if (newly_expired) {
            Json event = Json::object();
            event["type"] = "flow-completed";
            event["iteration"] = iteration;
            event["pool_tick_seconds"] = pool_tick;
            event["flow_index"] = static_cast<std::uint64_t>(index);
            event["start_cell_id"] = start;
            event["end_cell_id"] = end;
            event["reason"] = schedule_reason;
            events.push_back(std::move(event));
        }
        if (due) {
            const auto candidates = memberships[start];
            std::set<std::string, std::less<>> matched;
            std::set<std::string, std::less<>> unavailable;
            for (const auto& id : candidates) {
                const auto security = securities.find(id);
                if (security == securities.end()) {
                    unavailable.insert(id);
                    continue;
                }
                const auto* rules = optional(*security->second, "rules");
                bool passed = rules && rules->is_array();
                for (const auto rule_index : cell_rules[start]) {
                    if (!passed || rule_index >= rules->size()) {
                        passed = false;
                        break;
                    }
                    const auto& rule = rules->as_array()[rule_index];
                    if (json_text(rule, "status") != "evaluated" || !json_bool(rule, "matched")) {
                        passed = false;
                        break;
                    }
                }
                if (passed) matched.insert(id);
            }
            std::set<std::string, std::less<>> entered;
            for (const auto& id : matched)
                if (memberships[end].insert(id).second) entered.insert(id);
            ++runtime.run_count;
            if (runtime.first_run_tick < 0) runtime.first_run_tick = pool_tick;
            runtime.last_run_tick = pool_tick;
            if (cycle_type == 2) runtime.completed = true;
            status = runtime.completed ? "completed" : "active";

            Json event = Json::object();
            event["type"] = "flow-run";
            event["iteration"] = iteration;
            event["pool_tick_seconds"] = pool_tick;
            event["flow_index"] = static_cast<std::uint64_t>(index);
            event["start_cell_id"] = start;
            event["end_cell_id"] = end;
            event["run_count"] = runtime.run_count;
            event["candidate_count"] = static_cast<std::uint64_t>(candidates.size());
            event["matched_count"] = static_cast<std::uint64_t>(matched.size());
            event["entered_count"] = static_cast<std::uint64_t>(entered.size());
            event["unavailable_count"] = static_cast<std::uint64_t>(unavailable.size());
            event["matched_securities"] = string_set_json(matched);
            event["entered_securities"] = string_set_json(entered);
            event["unavailable_securities"] = string_set_json(unavailable);
            auto planned_actions = tpool_action_plans_document(
                inspection, end, entered, "stateful-cell-entry");
            planned_action_count += planned_actions.size();
            event["planned_actions"] = std::move(planned_actions);
            event["empty_previous_set_requested"] = json_bool(flow, "empty_previous_set");
            event["empty_previous_set_handling"] =
                "rule match sets are rebuilt from the current evaluation on every native advance";
            event["reason"] = schedule_reason;
            events.push_back(std::move(event));
            ++run_event_count;
        }

        Json row = Json::object();
        row["flow_index"] = static_cast<std::uint64_t>(index);
        row["start_cell_id"] = start;
        row["end_cell_id"] = end;
        row["transfer_enabled"] = enabled;
        row["references_valid"] = references_valid;
        row["start_type_id"] = json_integer_number(flow, "start_type_id", -1);
        row["start_type_name"] = json_text(flow, "start_type_name");
        row["cycle_type_id"] = cycle_type;
        row["cycle_type_name"] = json_text(flow, "cycle_type_name");
        row["interval_seconds"] = interval;
        row["cycle_window_seconds"] = window;
        row["empty_previous_set"] = json_bool(flow, "empty_previous_set");
        row["status"] = status;
        row["schedule_reason"] = schedule_reason;
        row["run_count"] = runtime.run_count;
        row["first_run_tick"] = runtime.first_run_tick;
        row["last_run_tick"] = runtime.last_run_tick;
        row["completed"] = runtime.completed;
        flow_rows.push_back(std::move(row));
    }

    Json membership_rows = Json::array();
    std::size_t membership_count = 0;
    for (const auto& [cell, ids] : memberships) {
        Json row = Json::object();
        row["cell_id"] = cell;
        row["count"] = static_cast<std::uint64_t>(ids.size());
        row["securities"] = string_set_json(ids);
        membership_count += ids.size();
        membership_rows.push_back(std::move(row));
    }

    Json history = Json::array();
    if (resume) {
        const auto* old_history = optional(previous_state, "event_history");
        if (old_history && old_history->is_array())
            for (const auto& event : old_history->as_array()) history.push_back(event);
    }
    for (const auto& event : events.as_array()) history.push_back(event);
    constexpr std::size_t history_limit = 512;
    if (history.size() > history_limit) {
        Json bounded = Json::array();
        const auto begin = history.size() - history_limit;
        for (std::size_t index = begin; index < history.size(); ++index)
            bounded.push_back(history.as_array()[index]);
        history = std::move(bounded);
    }

    Json result = Json::object();
    result["schema_version"] = 1;
    result["schema"] = "tdx-tpool-flow-state-v1";
    result["source"] = source;
    result["source_signature"] = signature;
    result["read_only"] = true;
    result["tdx_state_mutated"] = false;
    result["original_worker_started"] = false;
    result["scheduler_semantics_verified"] = true;
    result["original_worker_equivalent"] = false;
    result["fidelity"] =
        "verified one-second scheduler, XML flow order, membership propagation, bounded state and recovered psatt action planning; rule match sets are freshly rebuilt, while host callbacks, action execution and XML writeback are omitted";
    result["iteration"] = iteration;
    result["observed_epoch_seconds"] = clock.epoch_seconds;
    result["observed_local_date"] = clock.local_date;
    result["observed_local_hhmmss"] = clock.local_hhmmss;
    result["observed_local_weekday"] = clock.local_weekday;
    result["pool_tick_seconds"] = pool_tick;
    result["clock_elapsed_seconds"] = elapsed;
    result["state_reset"] = source_changed;
    result["flow_count"] = static_cast<std::uint64_t>(flows.size());
    result["flow_run_event_count"] = static_cast<std::uint64_t>(run_event_count);
    result["action_execution_mode"] = "planned-only";
    result["planned_action_count"] =
        static_cast<std::uint64_t>(planned_action_count);
    result["host_actions_executed"] = false;
    result["membership_count"] = static_cast<std::uint64_t>(membership_count);
    result["memberships"] = std::move(membership_rows);
    result["flows"] = std::move(flow_rows);
    result["events"] = std::move(events);
    result["event_history_limit"] = static_cast<std::uint64_t>(history_limit);
    result["event_history"] = std::move(history);
    return result;
}

}  // namespace tdx
