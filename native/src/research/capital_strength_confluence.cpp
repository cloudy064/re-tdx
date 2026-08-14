#include "capital_strength_internal.hpp"

#include <algorithm>
#include <limits>
#include <map>

namespace tdx {

Json compose_capital_strength_confluence(const Json& period_rows) {
    using namespace detail::capital_strength;
    if (!period_rows.is_array())
        throw Error("capital-strength confluence input must be an array");
    struct Aggregate {
        Json security{Json::object()};
        Json observations{Json::array()};
        double ddx_sum{};
        double ddx_min{std::numeric_limits<double>::infinity()};
        double ddx_max{-std::numeric_limits<double>::infinity()};
        std::uint64_t ddx_count{};
        std::uint64_t best_rank{std::numeric_limits<std::uint64_t>::max()};
        std::string latest_date;
    };
    std::map<std::string, Aggregate> aggregates;
    std::uint64_t source_rows = 0;
    for (const auto& group : period_rows.as_array()) {
        if (!group.is_object())
            throw Error("capital-strength period group must be an object");
        const auto* rows = value_ptr(group, "rows");
        if (!rows || !rows->is_array())
            throw Error("capital-strength period group rows missing");
        for (const auto& row : rows->as_array()) {
            ++source_rows;
            const auto& security = row.at("security");
            const auto key = security.at("security_id").as_string();
            auto& aggregate = aggregates[key];
            if (aggregate.observations.size() == 0) aggregate.security = security;
            aggregate.observations.push_back(row);
            const auto ddx = json_number(row, "ddx_float_share_pct");
            if (ddx) {
                aggregate.ddx_sum += *ddx;
                aggregate.ddx_min = std::min(aggregate.ddx_min, *ddx);
                aggregate.ddx_max = std::max(aggregate.ddx_max, *ddx);
                ++aggregate.ddx_count;
            }
            const auto rank = json_number(row, "source_rank");
            if (rank) aggregate.best_rank = std::min(
                aggregate.best_rank, static_cast<std::uint64_t>(*rank));
            aggregate.latest_date = std::max(
                aggregate.latest_date, json_text(row, "statistics_date"));
        }
    }

    Json records = Json::array();
    std::array<std::uint64_t, 6> distribution{};
    std::uint64_t all_five = 0, at_least_two = 0, names = 0;
    for (auto& [key, aggregate] : aggregates) {
        (void)key;
        const auto count = aggregate.observations.size();
        Json row = Json::object();
        row["security"] = aggregate.security;
        row["period_count"] = static_cast<std::uint64_t>(count);
        row["all_five_periods"] = count == all_period_specs().size();
        row["periods"] = Json::array();
        for (const auto& observation : aggregate.observations.as_array())
            row["periods"].push_back(observation.at("period"));
        row["average_ddx_float_share_pct"] = aggregate.ddx_count
            ? Json(aggregate.ddx_sum / static_cast<double>(aggregate.ddx_count))
            : Json(nullptr);
        row["minimum_ddx_float_share_pct"] = aggregate.ddx_count
            ? Json(aggregate.ddx_min) : Json(nullptr);
        row["maximum_ddx_float_share_pct"] = aggregate.ddx_count
            ? Json(aggregate.ddx_max) : Json(nullptr);
        row["best_source_rank"] = aggregate.best_rank ==
            std::numeric_limits<std::uint64_t>::max()
                ? Json(nullptr) : Json(aggregate.best_rank);
        row["latest_statistics_date"] = aggregate.latest_date;
        row["observations"] = std::move(aggregate.observations);
        if (count < distribution.size()) ++distribution[count];
        if (count == all_period_specs().size()) ++all_five;
        if (count >= 2) ++at_least_two;
        if (aggregate.security.at("name_resolved").as_bool()) ++names;
        records.push_back(std::move(row));
    }
    Json summary = Json::object();
    summary["source_rows"] = source_rows;
    summary["unique_securities"] = static_cast<std::uint64_t>(aggregates.size());
    summary["names_resolved"] = names;
    summary["at_least_two_periods"] = at_least_two;
    summary["all_five_periods"] = all_five;
    summary["period_count_distribution"] = Json::object();
    for (std::size_t count = 1; count < distribution.size(); ++count)
        summary["period_count_distribution"][std::to_string(count)] = distribution[count];
    Json result = Json::object();
    result["records"] = std::move(records);
    result["summary"] = std::move(summary);
    return result;
}

}  // namespace tdx
