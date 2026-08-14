#include "special_situations_internal.hpp"

namespace tdx {
using namespace special_situations_detail;

Json normalize_special_situation_rows(
    const std::string& resource, const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("special-situation rows must be an array");
    const auto* definition = find_resource_definition(resource);
    if (!definition)
        throw Error("unknown special-situation resource: " + resource);
    const auto resource_kind = definition->kind;
    Json result = Json::array();
    for (std::size_t source_row_index = 0;
         source_row_index < rows.as_array().size(); ++source_row_index) {
        const auto& raw = rows.as_array()[source_row_index];
        const auto primary = security_from_raw(raw, "$SC", "$ZQDM", securities);
        if (primary.is_null()) continue;
        Json item = Json::object();
        item["primary_security"] = primary;
        item["source_resource"] = resource;
        item["raw"] = raw;
        item["kind"] = std::string(definition->event_kind);
        item["kind_label"] = std::string(definition->label);
        if (resource_kind == ResourceKind::merger) {
            const auto related = security_from_raw(
                raw, "$SC1", "$ZQDM1", securities);
            if (related.is_null()) continue;
            const auto status = text_value(raw, "jd");
            item["related_security"] = related;
            item["status"] = status;
            item["active"] = status.find("失败") == std::string::npos &&
                status.find("终止") == std::string::npos;
            item["absorber_exchange_price"] = number(raw, "hgj1");
            item["absorbed_cash_option_price"] = number(raw, "xjxzq");
            item["absorbed_exchange_price"] = number(raw, "hgj2");
            item["currency"] = "CNY";
            item["announcement_url"] = "";
            item["description"] = text_value(raw, "xq");
            item["date"] = "";
            item["event_id"] = resource + ":" +
                primary.at("security_id").as_string() + ":" +
                related.at("security_id").as_string();
        } else if (resource_kind == ResourceKind::b_to_h) {
            const auto related = security_from_raw(
                raw, "$SC1", "$ZQDM1", securities);
            if (related.is_null()) continue;
            const auto status = text_value(raw, "jd");
            item["related_security"] = related;
            item["status"] = status;
            item["active"] = status.find("失败") == std::string::npos &&
                status.find("终止") == std::string::npos;
            item["cash_option_price"] = number(raw, "xjxzq");
            item["currency"] = text_value(raw, "bz");
            item["announcement_url"] = text_value(raw, "yagg");
            item["description"] = text_value(raw, "xq");
            item["date"] = "";
            item["event_id"] = resource + ":" +
                primary.at("security_id").as_string() + ":" +
                related.at("security_id").as_string();
        } else if (resource_kind == ResourceKind::market_cap_risk) {
            const auto type = text_value(raw, "rxlx");
            const auto twenty = number_value(raw, "zaf1");
            const auto yearly = number_value(raw, "zaf2");
            item["related_security"] = Json(nullptr);
            item["status"] = type;
            item["active"] = true;
            item["date"] = text_value(raw, "date1");
            item["sample_index"] = text_value(raw, "ybzs");
            item["sample_indexes"] = string_array(text_value(raw, "ybzs"));
            item["trigger_type"] = type;
            item["twenty_day_change_pct"] = number(raw, "zaf1");
            item["one_year_high_price"] = number(raw, "price4");
            item["high_to_current_change_pct"] = number(raw, "zaf2");
            item["twenty_day_triggered"] = type == "20日超跌" ||
                type == "同时触发" || (twenty && *twenty <= -20.0);
            item["one_year_triggered"] = type == "1年超跌" ||
                type == "同时触发" || (yearly && *yearly <= -50.0);
            item["breach_count"] = (item.at("twenty_day_triggered").as_bool() ? 1 : 0) +
                (item.at("one_year_triggered").as_bool() ? 1 : 0);
            item["currency"] = "CNY";
            item["announcement_url"] = "";
            item["description"] = "";
            item["event_id"] = resource + ":" +
                primary.at("security_id").as_string() + ":" +
                text_value(raw, "ybzs") + ":" + text_value(raw, "date1");
        } else if (resource_kind == ResourceKind::major_restructuring_plan ||
                   resource_kind == ResourceKind::major_restructuring_review ||
                   resource_kind == ResourceKind::major_restructuring_completed ||
                   resource_kind == ResourceKind::ordinary_merger_plan) {
            const auto status = text_value(raw, "xmjd");
            item["related_security"] = Json(nullptr);
            item["status"] = status;
            item["active"] = status.find("失败") == std::string::npos &&
                status.find("终止") == std::string::npos &&
                status.find("停止") == std::string::npos &&
                status.find("中止") == std::string::npos;
            item["date"] = text_value(raw, "date");
            item["industry"] = text_value(raw, "hy");
            item["acquiring_party"] = text_value(raw, "bdhdf");
            item["disposing_party"] = text_value(raw, "bdcrf");
            item["transaction_type"] = text_value(raw, "bdlx");
            item["transaction_amount_yuan"] = number(raw, "sjje");
            const auto amount = number_value(raw, "sjje");
            item["transaction_amount_100m_yuan"] = amount
                ? Json(*amount / 100000000.0) : Json(nullptr);
            item["currency"] = "CNY";
            item["announcement_url"] = "";
            item["description"] = text_value(raw, "bdjj");
            item["event_id"] = resource + ":" +
                primary.at("security_id").as_string() + ":" +
                text_value(raw, "date") + ":" + text_value(raw, "bdlx") +
                ":" + text_value(raw, "bdhdf");
        } else if (resource_kind == ResourceKind::neeq_transfer_plan) {
            const auto status = text_value(raw, "mqjd");
            item["related_security"] = Json(nullptr);
            item["status"] = status;
            item["active"] = status.find("终止") == std::string::npos &&
                status.find("撤回") == std::string::npos;
            item["date"] = text_value(raw, "bgq");
            item["report_period"] = text_value(raw, "bgq");
            item["target_board"] = text_value(raw, "ssbk");
            item["net_assets_yuan"] = number(raw, "JZC");
            item["net_profit_yuan"] = number(raw, "jlr1");
            item["prior_net_profit_yuan"] = number(raw, "jlr2");
            item["revenue_yuan"] = number(raw, "yysr1");
            item["adviser"] = text_value(raw, "fdjg");
            item["currency"] = "CNY";
            item["announcement_url"] = "";
            item["description"] = text_value(raw, "xq");
            item["event_id"] = resource + ":" +
                primary.at("security_id").as_string() + ":" +
                text_value(raw, "bgq");
        } else if (resource_kind == ResourceKind::neeq_regulation) {
            item["related_security"] = Json(nullptr);
            item["status"] = text_value(raw, "JGCS");
            item["active"] = true;
            item["date"] = text_value(raw, "GGRQ");
            item["regulation_reason"] = text_value(raw, "JGYY");
            item["regulation_measure"] = text_value(raw, "JGCS");
            item["industry"] = text_value(raw, "hy");
            item["currency"] = "CNY";
            item["announcement_url"] = "";
            item["description"] = text_value(raw, "fxts");
            item["event_id"] = resource + ":" +
                primary.at("security_id").as_string() + ":" +
                text_value(raw, "GGRQ") + ":" + text_value(raw, "JGCS");
        } else {
            item["related_security"] = Json(nullptr);
            item["status"] = "已上市";
            item["active"] = false;
            item["date"] = text_value(raw, "N003");
            item["acceptance_date"] = text_value(raw, "N001");
            item["registration_date"] = text_value(raw, "N002");
            item["listing_date"] = text_value(raw, "N003");
            item["listing_venue_before"] = text_value(raw, "N004");
            item["listing_venue_after"] = text_value(raw, "N005");
            item["currency"] = "CNY";
            item["announcement_url"] = "";
            item["description"] = text_value(raw, "N006");
            item["event_id"] = resource + ":" +
                primary.at("security_id").as_string() + ":" +
                text_value(raw, "N003");
        }
        item["event_id"] = item.at("event_id").as_string() + ":" +
            std::to_string(source_row_index);
        item["source_row_index"] = static_cast<std::uint64_t>(source_row_index);
        item["primary_quote"] = Json(nullptr);
        item["related_quote"] = Json(nullptr);
        result.push_back(std::move(item));
    }
    return result;
}

}  // namespace tdx
