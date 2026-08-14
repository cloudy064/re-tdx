#include "hk_events_internal.hpp"

#include <cmath>

namespace tdx {

Json normalize_hk_event_rows(const std::string& resource, const Json& rows) {
    if (!rows.is_array()) throw Error("HK event rows must be an array");
    const auto& definition = detail::find_hk_event_resource(resource);
    const auto kind = definition.kind;
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        Json item = Json::object();
        item["kind"] = std::string(kind);
        item["kind_label"] = std::string(definition.label);
        item["source_resource"] = resource;
        item["raw"] = raw;
        if (kind == "listing-application") {
            const auto company = detail::json_text(raw, "mc");
            const auto filing_date = detail::json_text(raw, "rq");
            const auto status_date = detail::json_text(raw, "rq1");
            if (company.empty() || (filing_date.empty() && status_date.empty())) continue;
            item["date"] = status_date.empty() ? filing_date : status_date;
            item["title"] = company;
            item["security"] = Json(nullptr);
            item["company"] = company;
            item["filing_date"] = filing_date;
            item["sequence"] = detail::json_text(raw, "jc");
            item["board"] = detail::json_text(raw, "sc");
            item["listing_type"] = detail::json_text(raw, "lx");
            item["status"] = detail::json_text(raw, "zt");
            item["status_date"] = status_date;
            item["sponsors"] = detail::json_text(raw, "jr");
            item["last_year_revenue_thousand_currency_units"] = detail::json_number(raw, "sr");
            item["last_year_revenue_currency_units"] = detail::json_scaled_number(raw, "sr", 1000.0);
            item["last_year_profit_thousand_currency_units"] = detail::json_number(raw, "lr");
            item["last_year_profit_currency_units"] = detail::json_scaled_number(raw, "lr", 1000.0);
            item["currency"] = detail::json_text(raw, "bz");
            item["controlling_shareholders"] = detail::json_text(raw, "gd");
            item["business"] = detail::json_text(raw, "zy");
            item["event_id"] = resource + ":" + filing_date + ":" +
                status_date + ":" + company;
            result.push_back(std::move(item));
            continue;
        }

        auto security = detail::hk_security_document(raw);
        if (security.is_null()) continue;
        const auto id = static_cast<int>(security.at("market_id").as_number());
        const auto code = security.at("code").as_string();
        item["security"] = std::move(security);
        if (kind == "dividend") {
            const auto announcement_date = detail::json_text(raw, "date1");
            const auto ex_date = detail::json_text(raw, "date4");
            const auto plan = detail::json_text(raw, "fags");
            item["date"] = announcement_date;
            item["title"] = plan;
            item["plan"] = plan;
            Json dates = Json::object();
            dates["announcement"] = announcement_date;
            dates["fiscal_year_end"] = detail::json_text(raw, "date2");
            dates["payment"] = detail::json_text(raw, "date3");
            dates["ex_dividend"] = ex_date;
            dates["register_start"] = detail::json_text(raw, "date5");
            dates["register_end"] = detail::json_text(raw, "date6");
            item["dates"] = std::move(dates);
            item["event_id"] = resource + ":" + std::to_string(id) + ":" +
                code + ":" + announcement_date + ":" + ex_date;
        } else if (kind == "holding-disclosure") {
            const auto date = detail::json_text(raw, "date");
            const auto investor = detail::json_text(raw, "tzz");
            const auto reason = detail::json_text(raw, "bm");
            const auto position = detail::json_text(raw, "hdc");
            item["date"] = date;
            item["title"] = investor;
            item["investor"] = investor;
            item["changed_shares_10k"] = detail::json_number(raw, "bdgs");
            item["changed_shares"] = detail::json_scaled_number(raw, "bdgs", 10000.0);
            item["holding_after_shares_10k"] = detail::json_number(raw, "bdhgs");
            item["holding_after_shares"] = detail::json_scaled_number(raw, "bdhgs", 10000.0);
            item["holding_after_pct"] = detail::json_number(raw, "bdhcgl");
            item["disclosure_reason"] = reason;
            item["position"] = position;
            item["event_id"] = resource + ":" + std::to_string(id) + ":" +
                code + ":" + date + ":" + investor + ":" + position;
        } else {
            const auto date = detail::json_text(raw, "date");
            const auto session = detail::json_text(raw, "sssj");
            const auto amount = detail::json_number_value(raw, "gkje");
            const auto turnover = detail::json_number_value(raw, "cjje");
            item["date"] = date;
            item["title"] = "沽空统计";
            item["short_shares_10k"] = detail::json_number(raw, "gksl");
            item["short_shares"] = detail::json_scaled_number(raw, "gksl", 10000.0);
            item["short_amount_10k_currency_units"] = detail::json_number(raw, "gkje");
            item["short_amount_currency_units"] = detail::json_scaled_number(raw, "gkje", 10000.0);
            item["turnover_10k_currency_units"] = detail::json_number(raw, "cjje");
            item["turnover_currency_units"] = detail::json_scaled_number(raw, "cjje", 10000.0);
            item["short_turnover_pct"] = amount && turnover && std::abs(*turnover) > 0.000001
                ? Json(*amount * 100.0 / *turnover) : Json(nullptr);
            item["currency"] = "HKD";
            item["session"] = session;
            item["event_id"] = resource + ":" + std::to_string(id) + ":" +
                code + ":" + date + ":" + session;
        }
        result.push_back(std::move(item));
    }
    return result;
}

}  // namespace tdx
