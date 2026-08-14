#include "tpool_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace tdx::tpool_detail {
namespace {

struct RankedCandidate {
    std::size_t input_index{};
    std::string security_id;
    double value{};
    int rank_descending{};
    int rank_from_bottom{};
    bool matched{};
};

std::vector<RankedCandidate> rank_candidates(const Json& candidates,
                                             int operation,
                                             double threshold) {
    if (!ranking_operation(operation))
        throw Error("ranking operation must be 5, 6, or 7");
    if (!std::isfinite(threshold) || threshold < -2000000.0 ||
        threshold > 2000000.0)
        throw Error("ranking threshold is outside the safe range");
    const int target = static_cast<int>(threshold);
    if (target == 0) throw Error("ranking threshold must not truncate to zero");
    if (!candidates.is_array()) throw Error("ranking candidates must be an array");

    std::vector<RankedCandidate> rows;
    rows.reserve(candidates.size());
    for (const auto& candidate : candidates.as_array()) {
        const auto* index = optional(candidate, "input_index");
        const auto* value = optional(candidate, "value");
        if (!index || !index->is_number() || !value || !value->is_number() ||
            !std::isfinite(value->as_number()))
            throw Error("ranking candidate requires finite input_index and value");
        const auto raw_index = index->as_number();
        if (raw_index < 0.0 || raw_index > 1000000.0 ||
            std::floor(raw_index) != raw_index)
            throw Error("ranking candidate input_index is invalid");
        rows.push_back(RankedCandidate{static_cast<std::size_t>(raw_index),
            json_text(candidate, "security_id"), value->as_number()});
    }

    std::sort(rows.begin(), rows.end(), [](const RankedCandidate& left,
                                           const RankedCandidate& right) {
        if (left.value != right.value) return left.value > right.value;
        return left.input_index > right.input_index;
    });
    const int population = static_cast<int>(rows.size());
    for (int index = 0; index < population; ++index) {
        auto& row = rows[static_cast<std::size_t>(index)];
        row.rank_descending = index + 1;
        row.rank_from_bottom = population - index;
        if (operation == 5)
            row.matched = target > 0 ? row.rank_descending == target
                                     : row.rank_from_bottom == -target;
        else if (operation == 6)
            row.matched = target > 0 ? row.rank_descending <= target
                                     : row.rank_from_bottom <= -target;
        else
            row.matched = target > 0 ? row.rank_from_bottom <= target
                                     : row.rank_descending >= -target;
    }
    return rows;
}

Json ranking_rows_document(const std::vector<RankedCandidate>& ranked,
                           std::size_t& selected_count) {
    Json rows = Json::array();
    selected_count = 0;
    for (const auto& item : ranked) {
        Json row = Json::object();
        row["input_index"] = static_cast<std::uint64_t>(item.input_index);
        row["security_id"] = item.security_id;
        row["value"] = item.value;
        row["rank_descending"] = item.rank_descending;
        row["rank_from_bottom"] = item.rank_from_bottom;
        row["matched"] = item.matched;
        if (item.matched) ++selected_count;
        rows.push_back(std::move(row));
    }
    return rows;
}

int observation_offset(const Json& observation,
                       int window_begin_offset,
                       int window_end_offset) {
    const auto* value = optional(observation, "offset_from_latest");
    if (!value || !value->is_number() || !std::isfinite(value->as_number()) ||
        std::floor(value->as_number()) != value->as_number())
        throw Error("ranking observation requires an integer offset_from_latest");
    if (value->as_number() < 0.0 ||
        value->as_number() > kMaximumTpoolRuleHistoryBars)
        throw Error("ranking observation offset is outside the safe range");
    const auto offset = static_cast<int>(value->as_number());
    if (offset < window_end_offset || offset > window_begin_offset)
        throw Error("ranking observation is outside the requested historical window");
    return offset;
}

}  // namespace

Json rank_tpool_observation_groups_document(
    const Json& observations,
    int operation,
    double threshold,
    int window_begin_offset,
    int window_end_offset) {
    if (window_begin_offset < 0 || window_end_offset < 0 ||
        window_begin_offset < window_end_offset ||
        window_begin_offset > kMaximumTpoolRuleHistoryBars)
        throw Error("historical ranking window requires begin >= end >= 0");
    if (!observations.is_array())
        throw Error("ranking observations must be an array");

    // Validate operator and threshold even when every security lacks a usable point.
    (void)rank_candidates(Json::array(), operation, threshold);

    std::map<int, Json> candidates_by_offset;
    std::set<std::pair<int, std::size_t>> seen;
    for (const auto& observation : observations.as_array()) {
        const int offset = observation_offset(
            observation, window_begin_offset, window_end_offset);
        const auto* index = optional(observation, "input_index");
        if (!index || !index->is_number() || !std::isfinite(index->as_number()) ||
            index->as_number() < 0.0 || index->as_number() > 1000000.0 ||
            std::floor(index->as_number()) != index->as_number())
            throw Error("ranking observation requires a valid input_index");
        const auto input_index = static_cast<std::size_t>(index->as_number());
        if (!seen.emplace(offset, input_index).second)
            throw Error("ranking observation duplicates a security at one historical offset");

        Json candidate = Json::object();
        candidate["input_index"] = static_cast<std::uint64_t>(input_index);
        candidate["security_id"] = json_text(observation, "security_id");
        const auto* value = optional(observation, "value");
        candidate["value"] = value ? *value : Json(nullptr);
        auto found = candidates_by_offset.find(offset);
        if (found == candidates_by_offset.end())
            found = candidates_by_offset.emplace(offset, Json::array()).first;
        found->second.push_back(std::move(candidate));
    }

    Json groups = Json::array();
    std::set<std::size_t> selected_securities;
    std::size_t selected_observations = 0;
    for (auto& [offset, candidates] : candidates_by_offset) {
        const auto ranked = rank_candidates(candidates, operation, threshold);
        std::size_t group_selected = 0;
        auto ranking_rows = ranking_rows_document(ranked, group_selected);
        for (const auto& row : ranking_rows.as_array())
            if (row.at("matched").as_bool())
                selected_securities.insert(static_cast<std::size_t>(
                    row.at("input_index").as_number()));
        selected_observations += group_selected;

        Json group = Json::object();
        group["offset_from_latest"] = offset;
        group["dll_map_key"] = window_begin_offset == window_end_offset
            ? offset : offset - window_end_offset;
        group["population"] = static_cast<std::uint64_t>(ranked.size());
        group["selected_count"] = static_cast<std::uint64_t>(group_selected);
        group["rankings"] = std::move(ranking_rows);
        groups.push_back(std::move(group));
    }

    Json result = Json::object();
    result["schema_version"] = 1;
    result["schema"] = "tdx-tpool-historical-ranking-v1";
    result["operation"] = operation;
    result["operator"] = operator_name(operation);
    result["threshold"] = threshold;
    result["threshold_integer"] = static_cast<int>(threshold);
    result["history_window_begin_offset"] = window_begin_offset;
    result["history_window_end_offset"] = window_end_offset;
    result["grouping"] =
        "rank securities independently at each historical offset; union matched securities";
    result["group_order"] = "offset-from-latest ascending (newest requested bar first)";
    result["ranking_order"] =
        "value-descending; equal values use later input first";
    result["group_count"] = static_cast<std::uint64_t>(groups.size());
    result["observation_count"] = static_cast<std::uint64_t>(observations.size());
    result["selected_observation_count"] =
        static_cast<std::uint64_t>(selected_observations);
    result["selected_security_count"] =
        static_cast<std::uint64_t>(selected_securities.size());
    result["groups"] = std::move(groups);
    return result;
}

void resolve_cross_security_rankings(Json& security_rows,
                                     const Json::Array& functions,
                                     std::size_t selected_security_count,
                                     std::size_t available_security_count) {
    for (std::size_t rule_index = 0; rule_index < functions.size(); ++rule_index) {
        const auto& function = functions[rule_index];
        const int set = json_integer_text(function, "nset", 0);
        const int operation = json_integer_text(function, "noperate", -1);
        if (!tpool_ranking_operation(set, operation)) continue;

        const int canonical_operation =
            tpool_canonical_ranking_operation(set, operation);
        const double threshold = json_float_text(function, "fsecond", 0.0);
        const bool formula_rule =
            tpool_rule_source_kind(set) == RuleSourceKind::formula;
        const int window_begin = formula_rule
            ? json_integer_text(function, "nbeginday", 0) : 0;
        const int window_end = formula_rule
            ? json_integer_text(function, "nendday", 0) : 0;

        Json observations = Json::array();
        std::size_t pending_count = 0;
        for (std::size_t security_index = 0;
             security_index < security_rows.size(); ++security_index) {
            const auto& security = security_rows.as_array()[security_index];
            const auto& rule = security.at("rules").as_array()[rule_index];
            if (json_text(rule, "status") != "ranking-pending") continue;
            ++pending_count;
            const auto* rows = optional(rule, "ranking_observations");
            if (rows && rows->is_array() && rows->size() > 0) {
                for (const auto& source : rows->as_array()) {
                    Json observation = source;
                    observation["input_index"] =
                        static_cast<std::uint64_t>(security_index);
                    observation["security_id"] = json_text(security, "security_id");
                    observations.push_back(std::move(observation));
                }
                continue;
            }
            const auto* value = optional(rule, "left_value");
            if (value && value->is_number()) {
                Json observation = Json::object();
                observation["input_index"] =
                    static_cast<std::uint64_t>(security_index);
                observation["security_id"] = json_text(security, "security_id");
                observation["offset_from_latest"] = 0;
                observation["value"] = *value;
                observations.push_back(std::move(observation));
            }
        }
        if (pending_count == 0) continue;

        try {
            const auto ranking = rank_tpool_observation_groups_document(
                observations, canonical_operation, threshold,
                window_begin, window_end);
            const auto& groups = ranking.at("groups").as_array();
            if (groups.empty())
                throw Error("cross-security ranking has no evaluable observations");

            const auto expected_group_count = static_cast<std::size_t>(
                window_begin - window_end + 1);
            bool population_complete =
                selected_security_count == available_security_count &&
                groups.size() == expected_group_count;
            for (const auto& group : groups)
                population_complete = population_complete &&
                    static_cast<std::size_t>(group.at("population").as_number()) ==
                        selected_security_count;

            for (std::size_t security_index = 0;
                 security_index < security_rows.size(); ++security_index) {
                auto& rule = security_rows.as_array()[security_index]
                                 .as_object().at("rules").as_array()[rule_index];
                if (json_text(rule, "status") != "ranking-pending") continue;
                rule["status"] = "evaluated";
                rule["matched"] = false;
                rule["rank_descending"] = nullptr;
                rule["rank_from_bottom"] = nullptr;
                rule["ranking_population"] = nullptr;
                rule["ranking_canonical_operation"] = canonical_operation;
                rule["ranking_population_complete"] = population_complete;
                rule["ranking_order"] = ranking.at("ranking_order");
                rule["ranking_grouping"] = ranking.at("grouping");
                rule["ranking_group_count"] = ranking.at("group_count");
                rule["ranking_observation_count"] = ranking.at("observation_count");
                rule["ranking_results"] = Json::array();
                rule["matched_offsets_from_latest"] = Json::array();
                rule["message"] = "";
            }

            const bool single_group = groups.size() == 1;
            for (const auto& group : groups) {
                const int offset = static_cast<int>(
                    group.at("offset_from_latest").as_number());
                for (const auto& ranked : group.at("rankings").as_array()) {
                    const auto security_index = static_cast<std::size_t>(
                        ranked.at("input_index").as_number());
                    auto& rule = security_rows.as_array()[security_index]
                                     .as_object().at("rules").as_array()[rule_index];
                    Json result = ranked;
                    result["offset_from_latest"] = offset;
                    result["dll_map_key"] = group.at("dll_map_key");
                    result["population"] = group.at("population");
                    rule["ranking_results"].push_back(std::move(result));
                    if (single_group) {
                        rule["rank_descending"] = ranked.at("rank_descending");
                        rule["rank_from_bottom"] = ranked.at("rank_from_bottom");
                        rule["ranking_population"] = group.at("population");
                    }
                    if (!ranked.at("matched").as_bool()) continue;
                    rule["matched"] = true;
                    rule["matched_offsets_from_latest"].push_back(offset);
                    if (!optional(rule, "matched_offset_from_latest") ||
                        rule.at("matched_offset_from_latest").is_null())
                        rule["matched_offset_from_latest"] = offset;
                }
            }
        } catch (const std::exception& error) {
            for (std::size_t security_index = 0;
                 security_index < security_rows.size(); ++security_index) {
                auto& rule = security_rows.as_array()[security_index]
                                 .as_object().at("rules").as_array()[rule_index];
                if (json_text(rule, "status") != "ranking-pending") continue;
                rule["status"] = "error";
                rule["matched"] = nullptr;
                rule["message"] = error.what();
            }
        }
    }
}

}  // namespace tdx::tpool_detail

namespace tdx {

Json rank_tpool_candidates_document(const Json& candidates,
                                    int operation,
                                    double threshold) {
    const auto ranked = tpool_detail::rank_candidates(
        candidates, operation, threshold);
    std::size_t selected = 0;
    auto rows = tpool_detail::ranking_rows_document(ranked, selected);
    Json result = Json::object();
    result["schema_version"] = 1;
    result["operation"] = operation;
    result["operator"] = tpool_detail::operator_name(operation);
    result["threshold"] = threshold;
    result["threshold_integer"] = static_cast<int>(threshold);
    result["order"] = "value-descending; equal values use later input first";
    result["population"] = static_cast<std::uint64_t>(ranked.size());
    result["selected_count"] = static_cast<std::uint64_t>(selected);
    result["rankings"] = std::move(rows);
    return result;
}

}  // namespace tdx
