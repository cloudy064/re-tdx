#include "tpool_internal.hpp"

#include <algorithm>

namespace tdx::tpool_detail {

std::string flow_source_signature(const Json& inspection) {
    const auto source = json_text(inspection, "source");
    const auto digest = json_text(inspection, "sha256");
    if (!digest.empty()) return source + "#sha256=" + digest;
    return source + "#cells=" + std::to_string(inspection.at("cell_count").as_number()) +
           ";flows=" + std::to_string(inspection.at("flow_count").as_number()) +
           ";functions=" + std::to_string(inspection.at("function_count").as_number());
}

FlowScheduleDecision first_flow_schedule(const Json& flow,
                                         const TpoolFlowClock& clock,
                                         std::int64_t pool_tick) {
    const int start_type = static_cast<int>(json_number(flow, "start_type_id", -1));
    const int cycle_type = static_cast<int>(json_number(flow, "cycle_type_id", -1));
    const auto start_seconds = static_cast<std::int64_t>(
        std::max(0.0, json_number(flow, "start_offset_seconds", 0.0)));
    const auto window_seconds = static_cast<std::int64_t>(
        std::max(0.0, json_number(flow, "cycle_window_seconds", 0.0)));
    if (start_type < 0 || start_type > 7 || cycle_type < 0 || cycle_type > 2)
        return {FlowScheduleDecision::Kind::invalid, "unsupported start/cycle type"};
    if (start_type == 0)
        return {FlowScheduleDecision::Kind::ready, "immediate"};
    if (start_type == 1) {
        if (pool_tick >= start_seconds)
            return {FlowScheduleDecision::Kind::ready, "pool tick reached start threshold"};
        return {FlowScheduleDecision::Kind::waiting, "pool tick has not reached start threshold"};
    }

    const int current_seconds = hhmmss_seconds(clock.local_hhmmss);
    if (current_seconds < 0 || clock.local_weekday < 1 || clock.local_weekday > 7)
        return {FlowScheduleDecision::Kind::invalid, "invalid local clock"};
    std::int64_t boundary = 0;
    if (start_type == 2) boundary = 9 * 3600 + 30 * 60 - start_seconds;
    else if (start_type == 3) boundary = 9 * 3600 + 30 * 60 + start_seconds;
    else if (start_type == 4) boundary = 15 * 3600 - start_seconds;
    else if (start_type == 5) boundary = 15 * 3600 + start_seconds;
    else {
        const int scheduled = hhmmss_seconds(json_integer_text(flow, "starttimehms", 0));
        if (scheduled < 0)
            return {FlowScheduleDecision::Kind::invalid, "invalid scheduled clock"};
        boundary = scheduled;
        if (start_type == 6 && (clock.local_weekday == 1 || clock.local_weekday == 7))
            return {FlowScheduleDecision::Kind::waiting, "scheduled weekday excludes weekend"};
    }
    if (current_seconds < boundary)
        return {FlowScheduleDecision::Kind::waiting, "local clock is before start boundary"};
    if (cycle_type == 1 && current_seconds - boundary > window_seconds)
        return {FlowScheduleDecision::Kind::expired, "first activation missed its cycle window"};
    return {FlowScheduleDecision::Kind::ready, "local clock reached start boundary"};
}

FlowRuntimeState read_flow_runtime(const Json& row) {
    FlowRuntimeState result;
    result.run_count = static_cast<std::uint64_t>(
        std::max<std::int64_t>(0, json_integer_number(row, "run_count", 0)));
    result.first_run_tick = json_integer_number(row, "first_run_tick", -1);
    result.last_run_tick = json_integer_number(row, "last_run_tick", -1);
    result.completed = json_bool(row, "completed");
    return result;
}

}  // namespace tdx::tpool_detail

