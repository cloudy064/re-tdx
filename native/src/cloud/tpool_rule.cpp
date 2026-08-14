#include "tpool_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace tdx::tpool_detail {
namespace {

std::string output_name(const Json& calculation, std::string requested, int index) {
    requested = upper_ascii(trim(std::move(requested)));
    const auto* output_rows = optional(calculation, "outputs");
    if (!output_rows || !output_rows->is_array()) return "";
    const auto& outputs = output_rows->as_array();
    if (!requested.empty()) {
        for (const auto& item : outputs)
            if (item.is_string() && upper_ascii(item.as_string()) == requested)
                return item.as_string();
    }
    if (index >= 0 && static_cast<std::size_t>(index) < outputs.size() &&
        outputs[static_cast<std::size_t>(index)].is_string())
        return outputs[static_cast<std::size_t>(index)].as_string();
    return "";
}

const Json* point_value(const Json& point, const std::string& name) {
    const auto* values = optional(point, "values");
    return values && values->is_object() ? optional(*values, name) : nullptr;
}

RuleResult evaluate_anchor(const Json& rule,
                           const Json& calculation,
                           std::size_t anchor_index,
                           int offset_from_latest) {
    RuleResult result;
    result.selected_offset = offset_from_latest;
    const int operation = json_integer_text(rule, "noperate", -1);
    const auto& points = calculation.at("points").as_array();
    const int first_index = json_integer_text(rule, "nfirst", 0);
    const int second_index = json_integer_text(rule, "nsecond", -1);
    result.left_name = output_name(calculation, json_text(rule, "cfirst"), first_index);
    if (result.left_name.empty()) {
        result.error = "first output cannot be resolved";
        return result;
    }
    const auto* current_left = point_value(points[anchor_index], result.left_name);
    if (!current_left || current_left->is_null() || !current_left->is_number()) {
        result.error = "first output is unavailable at historical offset " +
                       std::to_string(offset_from_latest);
        return result;
    }
    result.left = current_left->as_number();
    if (ranking_operation(operation)) {
        result.right_name = "rank-threshold";
        result.right = json_float_text(rule, "fsecond", 0.0);
        result.ranking_pending = true;
        return result;
    }

    const bool scalar = second_index < 0;
    if (!scalar) {
        result.right_name = output_name(
            calculation, json_text(rule, "csecond"), second_index);
        if (result.right_name.empty()) {
            result.error = "second output cannot be resolved";
            return result;
        }
        const auto* current_right = point_value(points[anchor_index], result.right_name);
        if (!current_right || current_right->is_null() || !current_right->is_number()) {
            result.error = "second output is unavailable at historical offset " +
                           std::to_string(offset_from_latest);
            return result;
        }
        result.right = current_right->as_number();
    } else {
        result.right_name = "constant";
        result.right = json_float_text(rule, "fsecond", 0.0);
    }

    constexpr double epsilon = 0.00001;
    if (operation == 0) {
        result.matched = std::abs(result.left - result.right) < epsilon;
    } else if (operation == 1) {
        result.matched = result.left > result.right;
    } else if (operation == 2) {
        result.matched = result.left < result.right;
    } else if (operation == 3 || operation == 4) {
        if (anchor_index < 1) {
            result.error = "cross operation requires a preceding valid bar";
            return result;
        }
        const auto* previous_left = point_value(points[anchor_index - 1], result.left_name);
        const auto* previous_right = scalar
            ? nullptr : point_value(points[anchor_index - 1], result.right_name);
        if (!previous_left || previous_left->is_null() || !previous_left->is_number() ||
            (!scalar && (!previous_right || previous_right->is_null() ||
                         !previous_right->is_number()))) {
            result.error = "cross operation preceding values are unavailable";
            return result;
        }
        const double left_before = previous_left->as_number();
        const double right_before = scalar ? result.right : previous_right->as_number();
        result.matched = operation == 3
            ? left_before <= right_before + epsilon && result.left > result.right + epsilon
            : left_before >= right_before - epsilon && result.left < result.right - epsilon;
    } else if (operation == 8 || operation == 9) {
        if (anchor_index < 2) {
            result.error = "turning-point operation requires two preceding valid bars";
            return result;
        }
        const auto* before = point_value(points[anchor_index - 2], result.left_name);
        const auto* pivot = point_value(points[anchor_index - 1], result.left_name);
        if (!before || !pivot || before->is_null() || pivot->is_null() ||
            !before->is_number() || !pivot->is_number()) {
            result.error = "turning-point preceding values are unavailable";
            return result;
        }
        result.matched = operation == 8
            ? pivot->as_number() < before->as_number() - epsilon &&
                  pivot->as_number() < result.left - epsilon
            : pivot->as_number() > before->as_number() + epsilon &&
                  pivot->as_number() > result.left + epsilon;
        result.right_name = operation == 8
            ? "previous-local-minimum" : "previous-local-maximum";
        result.right = pivot->as_number();
    } else {
        result.error = "unsupported TPool operation";
        return result;
    }
    result.evaluated = true;
    return result;
}

}  // namespace

RuleHistoryPlan tpool_rule_history_plan(int operation,
                                        int begin_offset,
                                        int end_offset,
                                        int calculation_bars) {
    RuleHistoryPlan result;
    result.begin_offset = begin_offset;
    result.end_offset = end_offset;
    result.calculation_bars = std::max(0, calculation_bars);
    const int operation_bars = operation == 3 || operation == 4
        ? 2 : (operation == 8 || operation == 9 ? 3 : 1);
    if (begin_offset < 0 || end_offset < 0 || begin_offset < end_offset) {
        result.blocking_reason =
            "historical window requires nbeginday >= nendday >= 0";
        return result;
    }
    const auto minimum = static_cast<long long>(begin_offset) + operation_bars;
    if (minimum > kMaximumTpoolRuleHistoryBars) {
        result.blocking_reason = "historical window exceeds the 16000-bar safety limit";
        return result;
    }
    result.minimum_bars = static_cast<int>(minimum);
    if (calculation_bars > kMaximumTpoolRuleHistoryBars) {
        result.blocking_reason = "nperiodnum exceeds the 16000-bar safety limit";
        return result;
    }
    if (result.calculation_bars > 0 &&
        result.calculation_bars < result.minimum_bars) {
        result.blocking_reason =
            "nperiodnum is too small for the requested operation and historical window";
        return result;
    }
    result.fetch_bars = result.calculation_bars > 0
        ? result.calculation_bars : result.minimum_bars;
    result.supported = true;
    return result;
}

RuleHistoryPlan annotate_tpool_rule_history(Json& rule) {
    const int operation = json_integer_text(rule, "noperate", -1);
    const int begin = json_integer_text(rule, "nbeginday", 0);
    const int end = json_integer_text(rule, "nendday", 0);
    const int calculation_bars = json_integer_text(rule, "nperiodnum", 0);
    const auto plan = tpool_rule_history_plan(
        operation, begin, end, calculation_bars);
    rule["history_window_begin_offset"] = begin;
    rule["history_window_end_offset"] = end;
    rule["history_window_mode"] = begin == 0 && end == 0
        ? "latest"
        : (ranking_operation(operation)
            ? "per-offset-cross-security-ranking-union"
            : "any-match-inclusive");
    rule["history_window_supported"] = plan.supported;
    rule["history_minimum_bars"] = plan.minimum_bars;
    rule["history_fetch_bars"] = plan.fetch_bars;
    rule["calculation_lookback_bars"] = plan.calculation_bars > 0
        ? Json(plan.calculation_bars) : Json(nullptr);
    rule["history_blocking_reason"] = plan.blocking_reason;
    rule["history_mapping_source"] =
        "TPool.dll sub_1001B9E0 offsets +698/+700/+705; "
        "sub_10019A40/sub_10019B80 offset-keyed candidate groups";
    return plan;
}

RuleResult evaluate_tpool_rule(const Json& rule, const Json& calculation) {
    RuleResult result;
    const int operation = json_integer_text(rule, "noperate", -1);
    const auto plan = tpool_rule_history_plan(
        operation,
        json_integer_text(rule, "nbeginday", 0),
        json_integer_text(rule, "nendday", 0),
        json_integer_text(rule, "nperiodnum", 0));
    result.window_begin_offset = plan.begin_offset;
    result.window_end_offset = plan.end_offset;
    if (!plan.supported) {
        result.error = plan.blocking_reason;
        return result;
    }
    const auto* points_node = optional(calculation, "points");
    if (!points_node || !points_node->is_array() || points_node->size() == 0) {
        result.error = "indicator returned no points";
        return result;
    }
    const auto& points = points_node->as_array();
    result.requested_anchor_count = static_cast<std::size_t>(
        plan.begin_offset - plan.end_offset + 1);
    RuleResult latest_ranking_candidate;
    std::vector<RuleRankingObservation> ranking_observations;
    std::string last_error;
    for (int offset = plan.begin_offset; offset >= plan.end_offset; --offset) {
        if (static_cast<std::size_t>(offset) >= points.size()) {
            last_error = "historical offset exceeds available indicator points";
            continue;
        }
        const auto anchor_index = points.size() - 1 - static_cast<std::size_t>(offset);
        auto candidate = evaluate_anchor(rule, calculation, anchor_index, offset);
        if (candidate.ranking_pending) {
            ranking_observations.push_back({offset, candidate.left});
            latest_ranking_candidate = std::move(candidate);
            continue;
        }
        if (!candidate.evaluated) {
            last_error = candidate.error;
            continue;
        }
        ++result.evaluated_anchor_count;
        const auto requested_count = result.requested_anchor_count;
        const auto evaluated_count = result.evaluated_anchor_count;
        result = std::move(candidate);
        result.window_begin_offset = plan.begin_offset;
        result.window_end_offset = plan.end_offset;
        result.requested_anchor_count = requested_count;
        result.evaluated_anchor_count = evaluated_count;
        if (result.matched) {
            result.matched_offset = offset;
            return result;
        }
    }
    if (ranking_operation(operation)) {
        if (ranking_observations.empty()) {
            result.error = "historical ranking window has no evaluable indicator point";
            if (!last_error.empty()) result.error += ": " + last_error;
            return result;
        }
        result = std::move(latest_ranking_candidate);
        result.window_begin_offset = plan.begin_offset;
        result.window_end_offset = plan.end_offset;
        result.requested_anchor_count = static_cast<std::size_t>(
            plan.begin_offset - plan.end_offset + 1);
        result.evaluated_anchor_count = ranking_observations.size();
        result.ranking_observations = std::move(ranking_observations);
        return result;
    }
    if (result.evaluated_anchor_count > 0) {
        result.evaluated = true;
        result.matched = false;
        result.error.clear();
        return result;
    }
    result.error = "historical window has no evaluable indicator point";
    if (!last_error.empty()) result.error += ": " + last_error;
    return result;
}

}  // namespace tdx::tpool_detail

namespace tdx {

Json evaluate_tpool_rule_document(const Json& rule, const Json& calculation) {
    const auto outcome = tpool_detail::evaluate_tpool_rule(rule, calculation);
    Json result = Json::object();
    result["schema_version"] = 1;
    result["schema"] = "tdx-tpool-rule-evaluation-v1";
    result["status"] = outcome.ranking_pending
        ? "ranking-pending" : (outcome.evaluated ? "evaluated" : "error");
    result["matched"] = outcome.evaluated ? Json(outcome.matched) : Json(nullptr);
    result["ranking_pending"] = outcome.ranking_pending;
    result["left_output"] = outcome.left_name;
    result["left_value"] = outcome.evaluated || outcome.ranking_pending
        ? Json(outcome.left) : Json(nullptr);
    result["right_output"] = outcome.right_name;
    result["right_value"] = outcome.evaluated || outcome.ranking_pending
        ? Json(outcome.right) : Json(nullptr);
    result["history_window_begin_offset"] = outcome.window_begin_offset;
    result["history_window_end_offset"] = outcome.window_end_offset;
    result["requested_anchor_count"] =
        static_cast<std::uint64_t>(outcome.requested_anchor_count);
    result["evaluated_anchor_count"] =
        static_cast<std::uint64_t>(outcome.evaluated_anchor_count);
    Json ranking_observations = Json::array();
    for (const auto& observation : outcome.ranking_observations) {
        Json row = Json::object();
        row["offset_from_latest"] = observation.offset_from_latest;
        row["value"] = observation.value;
        ranking_observations.push_back(std::move(row));
    }
    result["ranking_observations"] = std::move(ranking_observations);
    result["selected_offset_from_latest"] = outcome.selected_offset >= 0
        ? Json(outcome.selected_offset) : Json(nullptr);
    result["matched_offset_from_latest"] = outcome.matched_offset >= 0
        ? Json(outcome.matched_offset) : Json(nullptr);
    result["message"] = outcome.error;
    return result;
}

}  // namespace tdx
