#include "funds_internal.hpp"

namespace tdx {
Json intraday_funds_cache_document(const IntradayFundsService::FetchResult& master,
    const IntradayFundsService::FetchResult* detail, int ttl_seconds) {
    Json cache = Json::object(); cache["ttl_seconds"] = ttl_seconds;
    cache["hit"] = !master.refreshed || (detail && !detail->refreshed);
    cache["stale"] = master.stale || (detail && detail->stale);
    cache["master_refreshed"] = master.refreshed; cache["master_age_seconds"] = master.age_seconds;
    cache["master_stale"] = master.stale; cache["master_upstream_error"] = master.upstream_error.empty() ? Json(nullptr) : Json(master.upstream_error);
    if (detail) { cache["detail_refreshed"] = detail->refreshed; cache["detail_age_seconds"] = detail->age_seconds;
        cache["detail_stale"] = detail->stale; cache["detail_upstream_error"] = detail->upstream_error.empty() ? Json(nullptr) : Json(detail->upstream_error); }
    return cache;
}
Json normalize_intraday_fund_record(const Json& record,
    const std::map<std::pair<int, std::string>, Security>& securities,
    bool detail, const std::map<std::string, std::string>& industry_names) {
    using namespace detail::funds;
    const auto market_text = trim(scalar_text(find_value(record, "market")));
    const auto code = trim(scalar_text(find_value(record, "code")));
    if (code.empty()) throw Error("fund-flow row has no code");
    const int market_id = market_id_from_text(market_text);
    const auto known = securities.find({market_id, code}); const auto industry = industry_names.find(code);
    const std::string name = known != securities.end() ? known->second.name : industry != industry_names.end() ? industry->second : "";
    Json value = Json::object(); value["market"] = market_text; value["market_id"] = market_id;
    value["code"] = code; value["security_id"] = market_prefix(market_id) + code;
    value["name"] = name; value["name_resolved"] = !name.empty();
    Json quote = Json::object(); quote["last"] = scalar_copy(record, "xj"); quote["change_pct"] = scalar_copy(record, "zdf");
    quote["change"] = scalar_copy(record, "zd"); quote["previous_5day_minute_volume"] = scalar_copy(record, "q5rjl"); value["quote"] = std::move(quote);
    Json normalized_periods = Json::object(); const auto baseline = number_value(record, "q5rjl");
    for (const auto& spec : periods()) {
        const auto suffix = std::to_string(spec.suffix); Json period = Json::object(); period["name"] = spec.name;
        period["net_main_inflow"] = scalar_copy(record, "jlr_" + suffix); period["turnover"] = scalar_copy(record, "cje_" + suffix);
        period["net_main_share_pct"] = scalar_copy(record, "zlzb_" + suffix);
        if (spec.minutes) { period["change_pct"] = scalar_copy(record, "zf_" + suffix); period["volume"] = scalar_copy(record, "cjl_" + suffix);
            const auto volume = number_value(record, "cjl_" + suffix); period["relative_volume"] = volume && baseline && *baseline != 0.0
                ? Json(*volume * (detail ? 100.0 : 1.0) / spec.minutes / *baseline) : Json(nullptr); }
        normalized_periods[spec.key] = std::move(period);
    }
    value["periods"] = std::move(normalized_periods); return value;
}
}  // namespace tdx
