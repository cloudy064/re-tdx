#include "tdx/announcement_signals.hpp"
#include "tdx/announcement_signals_internal.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace tdx {

using detail::announcement_signals::json_text;
using detail::announcement_signals::json_number;

void sort_announcement_signal_rows(Json& rows, const std::string& sort_value,
                                   const std::string& order_value) {
    if (!rows.is_array()) throw Error("announcement signal sort requires an array");
    const auto sort = lower_ascii(trim(sort_value));
    const auto order = lower_ascii(trim(order_value));
    const std::set<std::string> sorts{"date", "recent-3d", "recent-10d",
        "pre-3d", "post-3d", "source-rank", "code"};
    if (!sorts.count(sort))
        throw Error("sort must be date, recent-3d, recent-10d, pre-3d, post-3d, source-rank, or code");
    if (order != "asc" && order != "desc") throw Error("order must be asc or desc");
    const bool descending = order == "desc";
    const std::map<std::string, std::string> number_keys{
        {"recent-3d", "recent_3d_return_pct"}, {"recent-10d", "recent_10d_return_pct"},
        {"pre-3d", "pre_3d_return_pct"}, {"post-3d", "post_3d_return_pct"},
        {"source-rank", "source_rank"}};
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            if (sort == "date") {
                const auto a = json_text(left, "date"), b = json_text(right, "date");
                if (a != b) return descending ? a > b : a < b;
            } else if (sort == "code") {
                const auto a = left.at("security").at("security_id").as_string();
                const auto b = right.at("security").at("security_id").as_string();
                if (a != b) return descending ? a > b : a < b;
            } else {
                const auto& key = number_keys.at(sort);
                const auto a = json_number(left, key), b = json_number(right, key);
                if (a.has_value() != b.has_value()) return a.has_value();
                if (a && b && *a != *b) return descending ? *a > *b : *a < *b;
            }
            return left.at("signal_id").as_string() < right.at("signal_id").as_string();
        });
}

namespace detail {
namespace announcement_signals {

Json summarize(const Json& rows) {
    std::set<std::string> securities, types;
    std::uint64_t bullish = 0, bearish = 0, unknown = 0, pdf = 0;
    std::uint64_t selected = 0, risks = 0, history = 0;
    std::string first_date, latest_date;
    for (const auto& row : rows.as_array()) {
        const auto& security = row.at("security");
        securities.insert(security.at("security_id").as_string());
        const auto direction = row.at("direction").as_string();
        if (direction == "bullish") ++bullish;
        else if (direction == "bearish") ++bearish;
        else ++unknown;
        const auto type = row.at("announcement_type").as_string();
        if (!type.empty()) types.insert(type);
        if (!row.at("pdf_url").is_null()) ++pdf;
        const auto kind = row.at("record_kind").as_string();
        if (kind == "selected") ++selected;
        else if (kind == "risk") ++risks;
        else if (kind == "history") ++history;
        const auto date = row.at("date").as_string();
        if (first_date.empty() || date < first_date) first_date = date;
        latest_date = std::max(latest_date, date);
    }
    Json result = Json::object();
    result["rows"] = static_cast<std::uint64_t>(rows.size());
    result["unique_securities"] = static_cast<std::uint64_t>(securities.size());
    result["announcement_types"] = static_cast<std::uint64_t>(types.size());
    result["bullish"] = bullish;
    result["bearish"] = bearish;
    result["unknown_direction"] = unknown;
    result["pdf_links"] = pdf;
    result["selected_rows"] = selected;
    result["risk_rows"] = risks;
    result["history_rows"] = history;
    result["first_date"] = first_date.empty() ? Json(nullptr) : Json(first_date);
    result["latest_date"] = latest_date.empty() ? Json(nullptr) : Json(latest_date);
    return result;
}

}  // namespace announcement_signals
}  // namespace detail
}  // namespace tdx
