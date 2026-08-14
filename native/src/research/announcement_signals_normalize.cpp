#include "tdx/announcement_signals.hpp"
#include "tdx/announcement_signals_internal.hpp"

namespace tdx {

using detail::announcement_signals::value_ptr;
using detail::announcement_signals::text_value;
using detail::announcement_signals::text_value_ci;
using detail::announcement_signals::number_value;
using detail::announcement_signals::number_json;
using detail::announcement_signals::digits;
using detail::announcement_signals::iso_date;
using detail::announcement_signals::market_id;
using detail::announcement_signals::security_document;
using detail::announcement_signals::split_title_url;
using detail::announcement_signals::normalized_direction;

Json normalize_announcement_signal_rows(
    const Json& rows, const std::string& source_family_value,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("announcement signal rows must be an array");
    const auto source_family = lower_ascii(trim(source_family_value));
    if (source_family != "selected" && source_family != "risk")
        throw Error("source_family must be selected or risk");
    Json result = Json::array();
    std::uint64_t source_rank = 0;
    for (const auto& raw : rows.as_array()) {
        ++source_rank;
        const auto code = text_value(raw, "$ZQDM");
        const auto raw_date = text_value_ci(raw, "date");
        if (!digits(code, 6) || !digits(raw_date, 8)) continue;
        int id = -1;
        try { id = market_id(text_value(raw, "$SC")); }
        catch (...) { continue; }
        const auto [title, pdf_url] = split_title_url(text_value_ci(raw, "title"));
        const auto direction_raw = text_value_ci(raw, "dk");
        Json row = Json::object();
        row["signal_id"] = source_family + ":" + std::to_string(id) + code +
            ":" + raw_date + ":" + std::to_string(source_rank);
        row["record_kind"] = source_family;
        row["source_rank"] = source_rank;
        row["security"] = security_document(id, code, securities);
        row["date"] = iso_date(raw_date);
        row["title"] = title;
        row["pdf_url"] = pdf_url.empty() ? Json(nullptr) : Json(pdf_url);
        row["direction"] = normalized_direction(direction_raw);
        row["direction_raw"] = direction_raw;
        row["announcement_type"] = text_value_ci(raw, "gglx");
        row["recent_3d_return_pct"] = number_json(number_value(raw, "zf1"));
        row["recent_10d_return_pct"] = number_json(number_value(raw, "zf2"));
        row["pre_3d_return_pct"] = Json(nullptr);
        row["post_3d_return_pct"] = Json(nullptr);
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

Json normalize_announcement_history_rows(
    const Json& rows, int id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("announcement history rows must be an array");
    if (id < 0 || id > 2) throw Error("announcement history market id must be 0, 1, or 2");
    if (!digits(code, 6)) throw Error("announcement history code must contain six digits");
    Json result = Json::array();
    std::uint64_t source_rank = 0;
    for (const auto& raw : rows.as_array()) {
        ++source_rank;
        const auto raw_date = text_value_ci(raw, "date");
        if (!digits(raw_date, 8)) continue;
        const auto [title, pdf_url] = split_title_url(text_value_ci(raw, "title"));
        const auto direction_raw = text_value_ci(raw, "dk");
        Json row = Json::object();
        row["signal_id"] = "history:" + std::to_string(id) + code +
            ":" + raw_date + ":" + std::to_string(source_rank);
        row["record_kind"] = "history";
        row["source_rank"] = source_rank;
        row["security"] = security_document(id, code, securities);
        row["date"] = iso_date(raw_date);
        row["title"] = title;
        row["pdf_url"] = pdf_url.empty() ? Json(nullptr) : Json(pdf_url);
        row["direction"] = normalized_direction(direction_raw);
        row["direction_raw"] = direction_raw;
        row["announcement_type"] = text_value_ci(raw, "gglx");
        row["recent_3d_return_pct"] = Json(nullptr);
        row["recent_10d_return_pct"] = Json(nullptr);
        row["pre_3d_return_pct"] = number_json(number_value(raw, "zf1"));
        row["post_3d_return_pct"] = number_json(number_value(raw, "zf2"));
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

}  // namespace tdx
