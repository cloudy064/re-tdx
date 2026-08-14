#include "shareholder_signals_internal.hpp"

namespace tdx {

Json normalize_notable_investor_directory_rows(const Json& rows) {
    using namespace detail::shareholder_signals;
    if (!rows.is_array()) throw Error("notable-investor directory rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto investor_id = text_value(row, "$ZQDM");
        const auto name = text_value(row, "name");
        if (investor_id.empty() || name.empty()) continue;
        Json item = Json::object();
        item["kind"] = "investor-directory";
        item["kind_label"] = "知名自然人持股目录";
        item["investor_id"] = investor_id;
        item["investor_name"] = name;
        item["detail_url"] = text_value(row, "gdjc");
        add_number(item, row, "holding_company_count", "bqjs");
        add_number(item, row, "holding_value_yuan", "bqje");
        add_number(item, row, "holding_shares", "bqcg");
        item["as_of_date"] = text_value(row, "bgq");
        item["signal_value"] = item.at("holding_company_count");
        item["record_id"] = "investor-directory:" + investor_id;
        item["source_resource"] =
            resource_spec(ResourceKind::investor_directory).resource;
        item["raw"] = row;
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_notable_investor_holding_rows(
    const std::string& investor_id, const std::string& investor_name,
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace detail::shareholder_signals;
    if (!rows.is_array()) throw Error("notable-investor holding rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        auto security = security_document(
            integer_value(row, "$SC"), text_value(row, "$ZQDM"), securities);
        if (security.is_null()) continue;
        Json item = Json::object();
        item["kind"] = "investor-holding";
        item["kind_label"] = "知名自然人持股明细";
        item["investor_id"] = investor_id;
        item["investor_name"] = investor_name;
        item["security"] = std::move(security);
        add_number(item, row, "current_rank", "bqpm");
        add_number(item, row, "prior_rank", "sqpm");
        add_number(item, row, "holding_shares", "bqcg");
        add_number(item, row, "prior_holding_shares", "sqcg");
        item["holding_change_shares"] = difference(row, "bqcg", "sqcg");
        add_number(item, row, "holding_value_yuan", "bqje");
        add_number(item, row, "prior_holding_value_yuan", "sqje");
        item["holding_value_change_yuan"] = difference(row, "bqje", "sqje");
        add_number(item, row, "holding_pct", "bqbl");
        add_number(item, row, "prior_holding_pct", "sqbl");
        item["holding_pct_change"] = difference(row, "bqbl", "sqbl");
        item["latest_date"] = text_value(row, "zxrq");
        item["source_type"] = text_value(row, "ly");
        item["signal_value"] = item.at("holding_value_yuan");
        item["record_id"] = "investor-holding:" + investor_id + ":" +
            item.at("security").at("security_id").as_string() + ":" +
            item.at("latest_date").as_string();
        item["source_resource"] = "nscg/" + investor_id + ".jsn";
        item["raw"] = row;
        result.push_back(std::move(item));
    }
    return result;
}

}  // namespace tdx
