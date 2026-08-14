#include "tdx/formula_engine.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace tdx {
namespace {

constexpr std::size_t maximum_replay_observations = 64;
constexpr std::size_t maximum_replay_events = 10000;

const Json* member(const Json& value, std::string_view key) {
    if (!value.is_object()) return nullptr;
    const auto found = value.as_object().find(key);
    return found == value.as_object().end() ? nullptr : &found->second;
}

bool output_value_equal(const Json& left, const Json& right) {
    if (left.is_null() || right.is_null()) return left.is_null() && right.is_null();
    return left.is_number() && right.is_number() &&
           left.as_number() == right.as_number();
}

Json point_stamp(const Json& point) {
    Json result = Json::object();
    result["date"] = point.at("date");
    result["time"] = point.at("time");
    return result;
}

std::vector<std::string> parameter_names(
    const std::map<std::string, double>& parameters) {
    std::vector<std::string> result;
    result.reserve(parameters.size());
    for (const auto& [name, value] : parameters) {
        (void)value;
        result.push_back(name);
    }
    return result;
}

} // namespace

Json replay_formula_future_document(const FormulaFutureReplayRequest& request) {
    if (request.max_observations < 1 ||
        request.max_observations > maximum_replay_observations)
        throw Error("future replay max_observations must be in 1..64");
    if (request.max_events < 1 || request.max_events > maximum_replay_events)
        throw Error("future replay max_events must be in 1..10000");
    if (!request.kline_document.is_object())
        throw Error("future replay K-line document must be an object");
    const auto* bars = member(request.kline_document, "bars");
    if (!bars || !bars->is_array() || bars->as_array().empty())
        throw Error("future replay requires at least one K-line bar");

    const auto analysis = analyze_formula_source(
        request.source, parameter_names(request.parameters));
    if (!analysis.at("has_future_function").as_bool())
        throw Error("future replay requires a formula containing a future function");
    if (!analysis.at("read_only_future_executable").as_bool())
        throw Error("future replay formula is not executable in read-only mode");

    const std::size_t bar_count = bars->size();
    const std::size_t baseline_count = bar_count > request.max_observations
        ? bar_count - request.max_observations : 1;
    const Json* context = request.context_provided ? &request.context : nullptr;

    auto evaluate_prefix = [&](std::size_t count) {
        Json prefix = request.kline_document;
        prefix["bars"].as_array().resize(count);
        return evaluate_formula_source_document(
            std::move(prefix), request.source, request.parameters,
            request.formula_code, context);
    };

    auto previous = evaluate_prefix(baseline_count);
    Json observations = Json::array();
    Json events = Json::array();
    std::set<std::string> changed_targets;
    std::uint64_t event_count = 0;

    for (std::size_t count = baseline_count + 1; count <= bar_count; ++count) {
        auto current = evaluate_prefix(count);
        const auto& previous_points = previous.at("points").as_array();
        const auto& current_points = current.at("points").as_array();
        const auto& output_names = current.at("outputs").as_array();
        std::uint64_t changed_point_count = 0;
        std::uint64_t stored_event_count = 0;
        for (std::size_t index = 0; index < previous_points.size(); ++index) {
            bool point_changed = false;
            const auto& before_values = previous_points[index].at("values");
            const auto& after_values = current_points[index].at("values");
            for (const auto& output_name : output_names) {
                const auto& name = output_name.as_string();
                const auto& before = before_values.at(name);
                const auto& after = after_values.at(name);
                if (output_value_equal(before, after)) continue;
                point_changed = true;
                ++event_count;
                changed_targets.insert(
                    previous_points[index].at("date").as_string() + "|" +
                    previous_points[index].at("time").as_string());
                if (events.size() >= request.max_events) continue;
                Json event = Json::object();
                event["observed_at"] = point_stamp(current_points.back());
                event["observed_bar_count"] = static_cast<std::uint64_t>(count);
                event["target"] = point_stamp(previous_points[index]);
                event["target_index"] = static_cast<std::uint64_t>(index);
                event["age_bars"] = static_cast<std::uint64_t>(count - 1 - index);
                event["output"] = name;
                event["previous_value"] = before;
                event["current_value"] = after;
                events.push_back(std::move(event));
                ++stored_event_count;
            }
            if (point_changed) ++changed_point_count;
        }
        Json observation = Json::object();
        observation["observed_at"] = point_stamp(current_points.back());
        observation["bar_count"] = static_cast<std::uint64_t>(count);
        observation["changed_point_count"] = changed_point_count;
        observation["stored_event_count"] = stored_event_count;
        observations.push_back(std::move(observation));
        previous = std::move(current);
    }

    Json result = Json::object();
    result["schema"] = "tdx-formula-future-replay-v1";
    result["formula"] = request.formula_code;
    result["future_functions"] = analysis.at("future_functions");
    result["source_bar_count"] = static_cast<std::uint64_t>(bar_count);
    result["baseline_bar_count"] = static_cast<std::uint64_t>(baseline_count);
    result["observation_count"] = static_cast<std::uint64_t>(observations.size());
    result["evaluation_count"] = static_cast<std::uint64_t>(observations.size() + 1);
    result["changed_target_count"] = static_cast<std::uint64_t>(changed_targets.size());
    result["event_count"] = event_count;
    result["stored_event_count"] = static_cast<std::uint64_t>(events.size());
    result["events_truncated"] = event_count > events.size();
    result["max_observations"] = static_cast<std::uint64_t>(request.max_observations);
    result["max_events"] = static_cast<std::uint64_t>(request.max_events);
    result["observations"] = std::move(observations);
    result["events"] = std::move(events);
    result["mode"] = "successive-prefix-read-only";
    result["newly_appended_points_counted_as_repaint"] = false;
    result["scan_allowed"] = false;
    result["backtest_allowed"] = false;
    result["account_accessed"] = false;
    result["orders_submitted"] = false;
    result["sdk_called"] = false;
    result["subscription_sent"] = false;
    result["network_requests"] = 0;
    result["entitlement_bypass"] = false;
    return result;
}

} // namespace tdx
