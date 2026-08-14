#include "state_owned_reform_internal.hpp"

#include <algorithm>

namespace tdx::state_owned_detail {

void sort_groups(Json& rows, GroupSortKind sort, bool descending) {
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [sort, descending](const Json& a, const Json& b) {
            if (sort == GroupSortKind::count) {
                const auto x = a.at("member_count").as_number();
                const auto y = b.at("member_count").as_number();
                if (x != y) return descending ? x > y : x < y;
            } else {
                const auto* key = sort == GroupSortKind::name
                    ? "name" : "group_id";
                const auto& x = a.at(key).as_string();
                const auto& y = b.at(key).as_string();
                if (x != y) return descending ? x > y : x < y;
            }
            return a.at("group_id").as_string() <
                   b.at("group_id").as_string();
        });
}

void sort_restructuring(Json& rows, ReformSortKind sort, bool descending) {
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [sort, descending](const Json& a, const Json& b) {
            if (sort == ReformSortKind::code) {
                const auto& x = a.at("security").at("security_id").as_string();
                const auto& y = b.at("security").at("security_id").as_string();
                if (x != y) return descending ? x > y : x < y;
            } else if (sort == ReformSortKind::date) {
                const auto& x = a.at("as_of_date");
                const auto& y = b.at("as_of_date");
                const auto xs = x.is_string() ? x.as_string() : std::string{};
                const auto ys = y.is_string() ? y.as_string() : std::string{};
                if (xs != ys) return descending ? xs > ys : xs < ys;
            } else {
                const auto* key = sort == ReformSortKind::profit
                    ? "net_profit_10k_yuan" : "controlling_stake_pct";
                const auto x = json_number(a, key);
                const auto y = json_number(b, key);
                if (x.has_value() != y.has_value()) return x.has_value();
                if (x && y && *x != *y)
                    return descending ? *x > *y : *x < *y;
            }
            return a.at("security").at("security_id").as_string() <
                   b.at("security").at("security_id").as_string();
        });
}

}  // namespace tdx::state_owned_detail
