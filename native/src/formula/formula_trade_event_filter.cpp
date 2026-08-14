#include "formula_trade_event_filter_internal.hpp"

#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::formula_trade_event_filter_detail {
namespace {

using formula_language_detail::NodeKind;

enum class Position { flat, long_position, short_position };
enum class Action {
    buy,
    sell,
    sell_short,
    buy_short,
    buy_short_buy,
    sell_sell_short,
};

const char* position_name(Position position) {
    switch (position) {
    case Position::flat: return "flat";
    case Position::long_position: return "long";
    case Position::short_position: return "short";
    }
    return "flat";
}

std::optional<Action> action_for(std::string_view function) {
    if (function == "BUY") return Action::buy;
    if (function == "SELL") return Action::sell;
    if (function == "SELLSHORT") return Action::sell_short;
    if (function == "BUYSHORT") return Action::buy_short;
    if (function == "BUYSHORT_BUY") return Action::buy_short_buy;
    if (function == "SELL_SELLSHORT") return Action::sell_sell_short;
    return std::nullopt;
}

bool is_composite(Action action) {
    return action == Action::buy_short_buy ||
           action == Action::sell_sell_short;
}

struct Transition {
    bool accepted{};
    Position before{Position::flat};
    Position after{Position::flat};
    const char* effective_action{"filtered-out"};
    const char* rejection_reason{"none"};
};

Transition apply_action(Position& position, Action action) {
    Transition transition;
    transition.before = position;
    transition.after = position;
    switch (action) {
    case Action::buy:
        if (position == Position::flat) {
            transition.accepted = true;
            transition.after = Position::long_position;
            transition.effective_action = "open-long";
        } else {
            transition.rejection_reason = position == Position::long_position
                ? "already-long" : "requires-flat";
        }
        break;
    case Action::sell:
        if (position == Position::long_position) {
            transition.accepted = true;
            transition.after = Position::flat;
            transition.effective_action = "close-long";
        } else {
            transition.rejection_reason = "requires-long";
        }
        break;
    case Action::sell_short:
        if (position == Position::flat) {
            transition.accepted = true;
            transition.after = Position::short_position;
            transition.effective_action = "open-short";
        } else {
            transition.rejection_reason = position == Position::short_position
                ? "already-short" : "requires-flat";
        }
        break;
    case Action::buy_short:
        if (position == Position::short_position) {
            transition.accepted = true;
            transition.after = Position::flat;
            transition.effective_action = "close-short";
        } else {
            transition.rejection_reason = "requires-short";
        }
        break;
    case Action::buy_short_buy:
        if (position == Position::flat ||
            position == Position::short_position) {
            transition.accepted = true;
            transition.after = Position::long_position;
            transition.effective_action = position == Position::flat
                ? "open-long-half-of-composite"
                : "close-short-and-open-long";
        } else {
            transition.rejection_reason = "already-long";
        }
        break;
    case Action::sell_sell_short:
        if (position == Position::flat ||
            position == Position::long_position) {
            transition.accepted = true;
            transition.after = Position::short_position;
            transition.effective_action = position == Position::flat
                ? "open-short-half-of-composite"
                : "close-long-and-open-short";
        } else {
            transition.rejection_reason = "already-short";
        }
        break;
    }
    if (transition.accepted) position = transition.after;
    return transition;
}

Json string_array(std::initializer_list<const char*> values) {
    Json result = Json::array();
    for (const auto* value : values) result.push_back(value);
    return result;
}

Json marker_statement_indices(
    const formula_language_detail::Program& program) {
    Json indices = Json::array();
    for (std::size_t index = 0; index < program.statements.size(); ++index) {
        const auto& expression = *program.statements[index].expression;
        if (expression.kind == NodeKind::symbol &&
            expression.text == "AUTOFILTER")
            indices.push_back(static_cast<std::uint64_t>(index));
    }
    return indices;
}

}  // namespace

Json apply_autofilter_projection(
    const formula_language_detail::Program& program,
    Json& trade_event_primitives, std::size_t bar_count) {
    Json metadata = Json::object();
    metadata["schema"] = "tdx-formula-autofilter-projection-v1";
    metadata["marker_match"] =
        "top-level-expression-exact-symbol-AUTOFILTER";
    metadata["projection"] = "offline-read-only-paired-signal-filter";
    metadata["native_host_equivalent"] = false;
    metadata["evidence_scope"] =
        "documented AUTOFILTER pairing rules projected over recovered "
        "top-level trade-event IR; the host postprocessor is not invoked";
    metadata["same_bar_native_dedup_verified"] = false;
    metadata["position_model"] = "single-flat-long-short";
    metadata["initial_position_assumption"] = "flat";
    metadata["traversal_order"] = "bar-outer-source-statement-inner";
    metadata["supported_functions"] = string_array({
        "BUY", "SELL", "SELLSHORT", "BUYSHORT",
        "BUYSHORT_BUY", "SELL_SELLSHORT"});
    metadata["complete_native_action_set"] = false;
    metadata["out_of_scope_functions"] =
        string_array({"CLOSEALLD", "CLOSEALLK"});
    metadata["composite_action_semantics"] =
        "atomic-position-transition;flat-uses-opening-half";
    metadata["historical_input"] =
        "raw-offline-per-bar-signal-candidates";
    metadata["decision_trace_scope"] =
        "one-entry-per-raw-candidate-in-bar-statement-order";
    metadata["latest_host_action_guard"] =
        "latest condition approximately 1 (epsilon 1e-5)";
    metadata["execution_side_effects"] = false;
    metadata["order_submission"] = false;
    metadata["account_access"] = false;
    metadata["network_access"] = false;

    auto marker_indices = marker_statement_indices(program);
    const bool enabled = marker_indices.size() != 0;
    metadata["enabled"] = enabled;
    metadata["marker_count"] =
        static_cast<std::uint64_t>(marker_indices.size());
    metadata["marker_statement_indices"] = std::move(marker_indices);
    if (!enabled) {
        metadata["reason"] = "exact-top-level-marker-absent";
        metadata["accepted_candidate_count"] = 0;
        metadata["filtered_out_candidate_count"] = 0;
        metadata["decision_count"] = 0;
        metadata["position_change_count"] = 0;
        metadata["decision_trace"] = Json::array();
        metadata["rejection_reason_counts"] = Json::object();
        metadata["final_position"] = "flat";
        return metadata;
    }

    auto& primitives = trade_event_primitives.as_array();
    std::vector<std::size_t> raw_candidate_cursors(primitives.size(), 0);
    std::vector<Json> filtered_candidates(primitives.size(), Json::array());
    std::vector<Json> filtered_latest_actions(
        primitives.size(), Json(nullptr));
    std::vector<std::uint64_t> rejected_counts(primitives.size(), 0);
    Position position = Position::flat;
    std::uint64_t accepted_count = 0;
    std::uint64_t rejected_count = 0;
    std::uint64_t position_change_count = 0;
    Json decision_trace = Json::array();
    std::map<std::string, std::uint64_t> rejection_reason_counts;

    for (std::size_t bar_index = 0; bar_index < bar_count; ++bar_index) {
        for (std::size_t primitive_index = 0;
             primitive_index < primitives.size(); ++primitive_index) {
            const auto& primitive = primitives[primitive_index];
            const auto& raw_candidates =
                primitive.at("historical_signal_candidates").as_array();
            auto& cursor = raw_candidate_cursors[primitive_index];
            while (cursor < raw_candidates.size() &&
                   static_cast<std::size_t>(
                       raw_candidates[cursor].at("index").as_number()) <
                       bar_index)
                ++cursor;
            if (cursor >= raw_candidates.size() ||
                static_cast<std::size_t>(
                    raw_candidates[cursor].at("index").as_number()) !=
                    bar_index)
                continue;

            const auto action = action_for(
                primitive.at("function").as_string());
            if (!action) {
                ++cursor;
                continue;
            }
            const auto transition = apply_action(position, *action);
            Json decision = raw_candidates[cursor];
            decision["decision_ordinal"] =
                static_cast<std::uint64_t>(decision_trace.size());
            decision["statement_index"] =
                static_cast<std::uint64_t>(
                    primitive.at("statement_index").as_number());
            decision["function"] = primitive.at("function");
            decision["projection"] = "offline-autofilter-decision";
            decision["accepted"] = transition.accepted;
            decision["position_before"] = position_name(transition.before);
            decision["position_after"] = position_name(transition.after);
            decision["position_changed"] =
                transition.before != transition.after;
            decision["effective_action"] = transition.effective_action;
            decision["rejection_reason"] = transition.accepted
                ? Json(nullptr) : Json(transition.rejection_reason);
            decision["composite_action"] = is_composite(*action);
            decision["native_host_action"] = false;
            decision["execution_side_effects"] = false;
            decision_trace.push_back(std::move(decision));
            if (transition.accepted) {
                Json candidate = raw_candidates[cursor];
                candidate["projection"] =
                    "offline-per-bar-autofiltered";
                candidate["autofilter_position_before"] =
                    position_name(transition.before);
                candidate["autofilter_position_after"] =
                    position_name(transition.after);
                candidate["autofilter_effective_action"] =
                    transition.effective_action;
                candidate["autofilter_atomic_action"] =
                    is_composite(*action);
                candidate["native_host_action"] = false;
                candidate["execution_side_effects"] = false;
                filtered_candidates[primitive_index].push_back(
                    std::move(candidate));
                ++accepted_count;
                if (transition.before != transition.after)
                    ++position_change_count;

                if (bar_index + 1 == bar_count &&
                    !primitive.at("latest_host_action").is_null()) {
                    Json latest = primitive.at("latest_host_action");
                    latest["projection"] =
                        "autofiltered-read-only-latest-host-action";
                    latest["autofilter_position_before"] =
                        position_name(transition.before);
                    latest["autofilter_position_after"] =
                        position_name(transition.after);
                    latest["autofilter_effective_action"] =
                        transition.effective_action;
                    latest["autofilter_atomic_action"] =
                        is_composite(*action);
                    latest["execution_side_effects"] = false;
                    latest["order_submission"] = false;
                    filtered_latest_actions[primitive_index] =
                        std::move(latest);
                }
            } else {
                ++rejected_count;
                ++rejected_counts[primitive_index];
                ++rejection_reason_counts[transition.rejection_reason];
            }
            ++cursor;
        }
    }

    for (std::size_t index = 0; index < primitives.size(); ++index) {
        primitives[index]["autofilter_applied"] = true;
        primitives[index]["filtered_historical_signal_candidates"] =
            std::move(filtered_candidates[index]);
        primitives[index]["filtered_latest_host_action"] =
            std::move(filtered_latest_actions[index]);
        primitives[index]["autofilter_filtered_out_candidate_count"] =
            rejected_counts[index];
    }
    metadata["accepted_candidate_count"] = accepted_count;
    metadata["filtered_out_candidate_count"] = rejected_count;
    metadata["decision_count"] = accepted_count + rejected_count;
    metadata["position_change_count"] = position_change_count;
    metadata["decision_trace"] = std::move(decision_trace);
    Json rejection_counts = Json::object();
    for (const auto& [reason, count] : rejection_reason_counts)
        rejection_counts[reason] = count;
    metadata["rejection_reason_counts"] = std::move(rejection_counts);
    metadata["final_position"] = position_name(position);
    return metadata;
}

}  // namespace tdx::formula_trade_event_filter_detail
