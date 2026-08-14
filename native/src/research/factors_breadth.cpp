#include "factors_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <vector>

namespace tdx {

using namespace factor_detail;
Json build_factor_breadth_document(const Json& catalog_records,
                                   const Json& dashboard_records) {
    if (!catalog_records.is_array() || !dashboard_records.is_array())
        throw Error("factor breadth requires catalog and dashboard arrays");
    struct Aggregate {
        Json factor;
        std::uint64_t members{};
        std::uint64_t safety_count{};
        std::uint64_t change_count{};
        std::uint64_t ten_day_count{};
        std::uint64_t positive_change{};
        double safety_sum{};
        double change_sum{};
        double ten_day_sum{};
    };
    std::map<std::string, Aggregate> aggregates;
    for (const auto& factor : catalog_records.as_array()) {
        const auto name = text(factor, "name");
        if (!name.empty()) aggregates.emplace(name, Aggregate{factor});
    }
    std::map<std::pair<std::string, std::string>, std::uint64_t> pairs;
    std::map<int, std::uint64_t> signal_distribution;
    std::map<std::string, std::uint64_t> unresolved;
    std::uint64_t safety_missing = 0, safety_low = 0, safety_medium = 0, safety_high = 0;
    for (const auto& record : dashboard_records.as_array()) {
        if (const auto signal_count = number(record, "signal_count"))
            ++signal_distribution[static_cast<int>(std::llround(*signal_count))];
        const auto safety = number(record, "safety_score");
        if (!safety) ++safety_missing;
        else if (*safety < 60.0) ++safety_low;
        else if (*safety < 80.0) ++safety_medium;
        else ++safety_high;
        const auto change = number(record, "change_pct");
        const auto ten_day = number(record, "ten_day_change_pct");
        std::set<std::string> selected;
        const auto* names = field(record, "selected_factors");
        if (names && names->is_array())
            for (const auto& value : names->as_array())
                if (value.is_string() && !trim(value.as_string()).empty())
                    selected.insert(trim(value.as_string()));
        std::vector<std::string> resolved;
        for (const auto& name : selected) {
            auto found = aggregates.find(name);
            if (found == aggregates.end()) {
                ++unresolved[name];
                continue;
            }
            auto& aggregate = found->second;
            ++aggregate.members;
            if (safety) { aggregate.safety_sum += *safety; ++aggregate.safety_count; }
            if (change) {
                aggregate.change_sum += *change;
                ++aggregate.change_count;
                if (*change > 0) ++aggregate.positive_change;
            }
            if (ten_day) { aggregate.ten_day_sum += *ten_day; ++aggregate.ten_day_count; }
            resolved.push_back(name);
        }
        for (std::size_t left = 0; left < resolved.size(); ++left)
            for (std::size_t right = left + 1; right < resolved.size(); ++right)
                ++pairs[{resolved[left], resolved[right]}];
    }

    std::vector<std::pair<std::string, Aggregate>> sorted(
        aggregates.begin(), aggregates.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& left, const auto& right) {
        if (left.second.members != right.second.members)
            return left.second.members > right.second.members;
        return left.first < right.first;
    });
    Json records = Json::array();
    const auto universe = static_cast<double>(dashboard_records.size());
    for (const auto& [name, aggregate] : sorted) {
        Json item = Json::object();
        item["factor"] = aggregate.factor;
        item["member_count"] = aggregate.members;
        item["coverage_pct"] = universe > 0
            ? Json(static_cast<double>(aggregate.members) * 100.0 / universe) : Json(nullptr);
        item["average_safety_score"] = aggregate.safety_count
            ? Json(aggregate.safety_sum / static_cast<double>(aggregate.safety_count)) : Json(nullptr);
        item["average_change_pct"] = aggregate.change_count
            ? Json(aggregate.change_sum / static_cast<double>(aggregate.change_count)) : Json(nullptr);
        item["positive_change_pct"] = aggregate.change_count
            ? Json(static_cast<double>(aggregate.positive_change) * 100.0 /
                   static_cast<double>(aggregate.change_count)) : Json(nullptr);
        item["average_ten_day_change_pct"] = aggregate.ten_day_count
            ? Json(aggregate.ten_day_sum / static_cast<double>(aggregate.ten_day_count)) : Json(nullptr);
        records.push_back(std::move(item));
    }

    struct PairResult { std::string left, right; std::uint64_t count{}; };
    std::vector<PairResult> pair_results;
    for (const auto& [pair, count] : pairs)
        pair_results.push_back({pair.first, pair.second, count});
    std::sort(pair_results.begin(), pair_results.end(), [](const auto& left, const auto& right) {
        if (left.count != right.count) return left.count > right.count;
        if (left.left != right.left) return left.left < right.left;
        return left.right < right.right;
    });
    Json cooccurrence = Json::array();
    for (const auto& pair : pair_results) {
        const auto& left = aggregates.at(pair.left);
        const auto& right = aggregates.at(pair.right);
        const auto union_count = left.members + right.members - pair.count;
        Json item = Json::object();
        item["factor_a_id"] = text(left.factor, "factor_id");
        item["factor_a_name"] = pair.left;
        item["factor_b_id"] = text(right.factor, "factor_id");
        item["factor_b_name"] = pair.right;
        item["member_count"] = pair.count;
        item["jaccard_pct"] = union_count
            ? Json(static_cast<double>(pair.count) * 100.0 /
                   static_cast<double>(union_count)) : Json(nullptr);
        cooccurrence.push_back(std::move(item));
    }
    Json distribution = Json::array();
    for (const auto& [signal_count, securities] : signal_distribution) {
        Json item = Json::object();
        item["signal_count"] = signal_count;
        item["security_count"] = securities;
        distribution.push_back(std::move(item));
    }
    Json unresolved_values = Json::array();
    for (const auto& [name, count] : unresolved) {
        Json item = Json::object();
        item["name"] = name;
        item["occurrences"] = count;
        unresolved_values.push_back(std::move(item));
    }
    Json safety = Json::object();
    safety["missing"] = safety_missing;
    safety["below_60"] = safety_low;
    safety["60_to_79"] = safety_medium;
    safety["80_to_100"] = safety_high;
    Json counts = Json::object();
    counts["catalog_factors"] = static_cast<std::uint64_t>(catalog_records.size());
    counts["dashboard_securities"] = static_cast<std::uint64_t>(dashboard_records.size());
    counts["factor_pairs"] = static_cast<std::uint64_t>(cooccurrence.size());
    counts["unresolved_factor_names"] = static_cast<std::uint64_t>(unresolved.size());
    Json result = Json::object();
    result["schema"] = "tdx-factor-breadth-derived-v1";
    result["counts"] = std::move(counts);
    result["records"] = std::move(records);
    result["cooccurrence"] = std::move(cooccurrence);
    result["signal_count_distribution"] = std::move(distribution);
    result["safety_distribution"] = std::move(safety);
    result["unresolved_factor_names"] = std::move(unresolved_values);
    return result;
}

Json reconcile_factor_breadth_document(Json breadth,
                                       const Json& dashboard_records,
                                       const Json& standard_matrix_records) {
    if (!breadth.is_object() || !dashboard_records.is_array() ||
        !standard_matrix_records.is_array())
        throw Error("factor breadth reconciliation requires breadth, dashboard and matrix data");
    auto* breadth_records = &breadth["records"];
    if (!breadth_records->is_array()) throw Error("factor breadth has no records array");
    std::map<std::string, std::string> name_to_id;
    for (const auto& record : breadth_records->as_array()) {
        const auto* factor = field(record, "factor");
        if (!factor || !factor->is_object()) continue;
        name_to_id.emplace(text(*factor, "name"), text(*factor, "factor_id"));
    }
    std::map<std::string, std::set<std::string>> dashboard_sets;
    for (const auto& record : dashboard_records.as_array()) {
        const auto* security = field(record, "security");
        const auto* selected = field(record, "selected_factors");
        if (!security || !security->is_object() || !selected || !selected->is_array()) continue;
        const auto security_id = text(*security, "security_id");
        for (const auto& name : selected->as_array()) {
            if (!name.is_string()) continue;
            const auto found = name_to_id.find(name.as_string());
            if (found != name_to_id.end()) dashboard_sets[found->second].insert(security_id);
        }
    }
    std::map<std::string, std::set<std::string>> direct_sets;
    std::set<std::string> unavailable;
    for (const auto& matrix : standard_matrix_records.as_array()) {
        const auto* factor = field(matrix, "factor");
        if (!factor || !factor->is_object()) continue;
        const auto id = text(*factor, "factor_id");
        if (text(matrix, "status") != "live") {
            unavailable.insert(id);
            continue;
        }
        const auto* members = field(matrix, "members");
        if (!members || !members->is_array()) continue;
        for (const auto& member : members->as_array()) {
            const auto* security = field(member, "security");
            if (security && security->is_object())
                direct_sets[id].insert(text(*security, "security_id"));
        }
    }
    std::uint64_t consistent = 0, divergent = 0, unavailable_count = 0;
    for (auto& record : breadth_records->as_array()) {
        const auto* factor = field(record, "factor");
        const auto id = factor && factor->is_object() ? text(*factor, "factor_id") : "";
        const auto& dashboard_set = dashboard_sets[id];
        if (unavailable.count(id)) {
            record["direct_membership_status"] = "unavailable";
            record["direct_member_count"] = Json(nullptr);
            record["membership_consistent"] = Json(nullptr);
            ++unavailable_count;
            continue;
        }
        const auto& direct_set = direct_sets[id];
        std::uint64_t intersection = 0;
        for (const auto& security : dashboard_set)
            if (direct_set.count(security)) ++intersection;
        const auto dashboard_only = dashboard_set.size() - intersection;
        const auto direct_only = direct_set.size() - intersection;
        const auto union_count = dashboard_set.size() + direct_set.size() - intersection;
        const bool same = !dashboard_only && !direct_only;
        record["dashboard_member_count"] = static_cast<std::uint64_t>(dashboard_set.size());
        record["direct_membership_status"] = "live";
        record["direct_member_count"] = static_cast<std::uint64_t>(direct_set.size());
        record["membership_intersection_count"] = intersection;
        record["dashboard_only_count"] = static_cast<std::uint64_t>(dashboard_only);
        record["direct_only_count"] = static_cast<std::uint64_t>(direct_only);
        record["membership_jaccard_pct"] = union_count
            ? Json(static_cast<double>(intersection) * 100.0 /
                   static_cast<double>(union_count)) : Json(100.0);
        record["membership_consistent"] = same;
        if (same) ++consistent; else ++divergent;
    }
    auto counts = breadth.at("counts");
    counts["membership_consistent_factors"] = consistent;
    counts["membership_divergent_factors"] = divergent;
    counts["membership_unavailable_factors"] = unavailable_count;
    breadth["counts"] = std::move(counts);
    breadth["membership_reconciliation"] = true;
    breadth["membership_reconciliation_note"] =
        "200646 dashboard labels and complete 200636 factor-member lists are reported as independent upstream views; differences are retained.";
    return breadth;
}

}  // namespace tdx