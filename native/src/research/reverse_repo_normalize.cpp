#include "reverse_repo_internal.hpp"

#include <map>
#include <set>

namespace tdx {
Json normalize_reverse_repo_rows(const Json& schedule_rows, const Json& quote_rows,
    int principal_yuan,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace detail::reverse_repo;
    if (!schedule_rows.is_array()) throw Error("reverse-repo schedule rows must be an array");
    if (!quote_rows.is_array()) throw Error("reverse-repo quote rows must be an array");
    if (principal_yuan < 1000 || principal_yuan > 1000000000)
        throw Error("principal_yuan must be in 1000..1000000000");
    std::map<std::pair<int, std::string>, const Json*> quotes;
    for (const auto& quote : quote_rows.as_array()) {
        const auto market = json_number(quote, "market_id"); const auto code = json_text(quote, "code");
        if (market && digits(code, 6)) quotes[{static_cast<int>(*market), code}] = &quote;
    }
    Json result = Json::array(); std::set<std::pair<int, std::string>> seen; std::uint64_t rank = 0;
    for (const auto& raw : schedule_rows.as_array()) {
        ++rank; const auto code = text_value(raw, "$ZQDM"); if (!digits(code, 6)) continue;
        int id = -1; try { id = market_id(text_value(raw, "$SC")); } catch (...) { continue; }
        if (!seen.insert({id, code}).second) throw Error("duplicate reverse-repo security: " + market_prefix(id) + code);
        const auto term = number_value(raw, "TS"), days = number_value(raw, "SJTS"), fee100k = number_value(raw, "SXF");
        if (!term || !days || !fee100k || *term <= 0 || *days <= 0) continue;
        Json security = security_document(id, code, securities);
        const auto found = quotes.find({id, code}); const Json* live = found == quotes.end() ? nullptr : found->second;
        if (live && !json_text(*live, "name").empty()) { security["name"] = json_text(*live, "name"); security["name_resolved"] = true; }
        auto rate = live ? json_number(*live, "last_price") : std::nullopt; std::string rate_source = "unavailable";
        if (rate && *rate > 0) rate_source = "last-price";
        else { const auto previous = live ? json_number(*live, "pre_close_price") : std::nullopt;
            rate = previous && *previous > 0 ? previous : std::nullopt; if (rate) rate_source = "pre-close"; }
        const double fee = *fee100k * principal_yuan / 100000.0;
        std::optional<double> gross, net, net_rate;
        if (rate) { gross = principal_yuan * (*rate / 100.0) * *days / 365.0; net = *gross - fee;
            net_rate = *net / principal_yuan * 365.0 / *days * 100.0; }
        Json row = Json::object(); row["repo_id"] = market_prefix(id) + code; row["source_rank"] = rank;
        row["security"] = std::move(security); row["term_days"] = static_cast<int>(*term);
        row["interest_days"] = static_cast<int>(*days); row["bonus_interest_days"] = static_cast<int>(*days - *term);
        row["fee_yuan_per_100k"] = *fee100k; row["settlement_date"] = iso_date(text_value(raw, "SCZJJS"));
        row["funds_available_date"] = iso_date(text_value(raw, "ZJKY")); row["funds_withdrawable_date"] = iso_date(text_value(raw, "ZJKQ"));
        row["principal_yuan"] = principal_yuan; row["annualized_rate_pct"] = rate ? Json(*rate) : Json(nullptr);
        row["gross_interest_yuan"] = gross ? Json(*gross) : Json(nullptr); row["fee_yuan"] = fee;
        row["net_interest_yuan"] = net ? Json(*net) : Json(nullptr); row["net_annualized_rate_pct"] = net_rate ? Json(*net_rate) : Json(nullptr);
        row["quote_available"] = live && rate; row["quote_price_source"] = rate_source;
        for (const auto& [source, target] : std::map<std::string, std::string>{{"pre_close_price","previous_rate_pct"},{"open_price","open_rate_pct"},{"high_price","high_rate_pct"},{"low_price","low_rate_pct"},{"amount","turnover_amount_yuan"}})
            row[target] = live && json_number(*live, source) ? Json(*json_number(*live, source)) : Json(nullptr);
        row["rate_change_pct"] = live && value_ptr(*live, "change_pct") ? *value_ptr(*live, "change_pct") : Json(nullptr);
        row["quote_time_raw"] = live && value_ptr(*live, "time_raw") ? *value_ptr(*live, "time_raw") : Json(nullptr);
        row["raw"] = raw; result.push_back(std::move(row));
    }
    return result;
}
}  // namespace tdx
