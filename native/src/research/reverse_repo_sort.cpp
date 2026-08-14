#include "reverse_repo_internal.hpp"

#include <algorithm>

namespace tdx::detail::reverse_repo {
void sort_rows(Json& rows, const SortSpec& sort, const std::string& order) {
    if (!rows.is_array()) throw Error("reverse-repo sort requires an array");
    if (order != "asc" && order != "desc") throw Error("order must be asc or desc");
    const bool descending = order == "desc";
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            if (sort.text) {
                const auto a = json_text(left, sort.field);
                const auto b = json_text(right, sort.field);
                if (a != b) return descending ? a > b : a < b;
            } else {
                const auto a = json_number(left, sort.field);
                const auto b = json_number(right, sort.field);
                if (a.has_value() != b.has_value()) return a.has_value();
                if (a && b && *a != *b) return descending ? *a > *b : *a < *b;
            }
            return left.at("repo_id").as_string() < right.at("repo_id").as_string();
        });
}
}  // namespace tdx::detail::reverse_repo

namespace tdx {
void sort_reverse_repo_rows(Json& rows, const std::string& sort_value,
                            const std::string& order_value) {
    using namespace detail::reverse_repo;
    sort_rows(rows, sort_spec(sort_value), lower_ascii(trim(order_value)));
}
}  // namespace tdx
