#include "shareholder_signals_internal.hpp"

namespace tdx::detail::shareholder_signals {
namespace {

Json base_signal_row(
    const Json& row, const ResourceSpec& spec,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    auto security = security_document(
        integer_value(row, "$SC"), text_value(row, "$ZQDM"), securities);
    if (security.is_null()) return Json(nullptr);
    Json item = Json::object();
    item["kind"] = spec.id;
    item["kind_label"] = spec.label;
    item["security"] = std::move(security);
    item["source_resource"] = spec.resource;
    item["raw"] = row;
    return item;
}

}  // namespace

Json normalize_notable_signal_row(
    const Json& row,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const auto& spec = resource_spec(ResourceKind::notable_investors);
    auto item = base_signal_row(row, spec, securities);
    if (item.is_null()) return item;
    item["report_period"] = text_value(row, "N001");
    add_number(item, row, "notable_investor_count", "N002");
    add_number(item, row, "holding_shares", "N003");
    add_number(item, row, "total_shares", "N006");
    add_number(item, row, "report_close", "N007");
    add_number(item, row, "pe_ttm", "N008");
    item["holding_pct"] = ratio_pct(row, "N003", "N006");
    const auto shares = number_value(row, "N003");
    const auto close = number_value(row, "N007");
    item["holding_value_yuan"] = shares && close
        ? Json(*shares * *close) : Json(nullptr);
    item["signal_value"] = item.at("notable_investor_count");
    return item;
}

Json normalize_institution_signal_row(
    const Json& row,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const auto& spec = resource_spec(ResourceKind::institution_accumulation);
    auto item = base_signal_row(row, spec, securities);
    if (item.is_null()) return item;
    item["report_period"] = text_value(row, "zxbgq");
    add_number(item, row, "institution_holding_shares", "zxcg");
    add_number(item, row, "prior_institution_holding_shares", "sqcg");
    item["institution_holding_change_shares"] = difference(row, "zxcg", "sqcg");
    add_number(item, row, "institution_holding_growth_ratio", "cgzz");
    item["institution_holding_growth_pct"] = scaled(row, "cgzz", 100.0);
    add_number(item, row, "shareholder_count", "zxgdrs");
    add_number(item, row, "prior_shareholder_count", "sqgdrs");
    item["shareholder_count_change"] = difference(row, "zxgdrs", "sqgdrs");
    add_number(item, row, "shareholder_count_change_ratio", "rsjs");
    item["shareholder_count_change_pct"] = scaled(row, "rsjs", 100.0);
    add_number(item, row, "top10_float_holding_shares", "sdlt1");
    add_number(item, row, "top10_float_holding_pct", "sdlt2");
    add_number(item, row, "top10_holding_shares", "sdgd1");
    add_number(item, row, "top10_holding_pct", "sdgd2");
    item["signal_value"] = item.at("institution_holding_growth_pct");
    return item;
}

Json normalize_small_cap_signal_row(
    const Json& row,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const auto& spec = resource_spec(ResourceKind::small_cap_institution);
    auto item = base_signal_row(row, spec, securities);
    if (item.is_null()) return item;
    item["report_period"] = text_value(row, "zxbgq");
    add_number(item, row, "report_float_market_cap_yuan", "bgqmsz");
    add_number(item, row, "institution_holding_shares", "zxcg");
    add_number(item, row, "institution_holding_change_shares", "cgbd");
    item["institution_holding_growth_pct"] = ratio_pct(row, "cgbd", "zxcg");
    add_number(item, row, "institution_holding_value_yuan", "ccsz");
    add_number(item, row, "institution_float_holding_pct", "ccbl");
    add_number(item, row, "institution_holding_value_change_yuan", "zcsz");
    item["institution_holding_value_change_pct"] = ratio_pct(row, "zcsz", "ccsz");
    add_number(item, row, "institution_count_source", "jgsl");
    add_number(item, row, "institution_count_change_source", "jgbhl");
    item["institution_count_change_pct"] = ratio_pct(row, "jgbhl", "jgsl");
    item["signal_value"] = item.at("institution_holding_growth_pct");
    return item;
}

Json normalize_research_signal_row(
    const Json& row,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const auto& spec = resource_spec(ResourceKind::research_growth);
    auto item = base_signal_row(row, spec, securities);
    if (item.is_null()) return item;
    item["report_period"] = text_value(row, "date1");
    item["latest_research_date"] = text_value(row, "date");
    add_number(item, row, "net_profit_yuan", "zxjlr");
    add_number(item, row, "prior_net_profit_yuan", "qntqjlr");
    add_number(item, row, "net_profit_growth_pct", "jlrzf");
    add_number(item, row, "research_count_1m", "yydycs");
    add_number(item, row, "research_count_3m", "sydycs");
    add_number(item, row, "research_count_6m", "bndycs");
    add_number(item, row, "institutions_last", "ycjgsl");
    add_number(item, row, "institutions_1m", "yyjgsl");
    add_number(item, row, "institutions_3m", "syjgsl");
    add_number(item, row, "institutions_6m", "bnjgsl");
    add_number(item, row, "return_1m_pct", "yyzaf");
    add_number(item, row, "return_3m_pct", "syzaf");
    add_number(item, row, "return_6m_pct", "bnzaf");
    item["signal_value"] = item.at("research_count_6m");
    return item;
}

}  // namespace tdx::detail::shareholder_signals

namespace tdx {

Json normalize_shareholder_signal_rows(
    const std::string& resource, const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace detail::shareholder_signals;
    if (!rows.is_array()) throw Error("shareholder-signals rows must be an array");
    const auto& spec = resource_spec(resource);
    if (!spec.normalize_row)
        throw Error("unknown shareholder-signals resource: " + resource);
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        auto item = spec.normalize_row(row, securities);
        if (item.is_null()) continue;
        item["record_id"] = std::string(spec.id) + ":" +
            item.at("security").at("security_id").as_string() + ":" +
            item.at("report_period").as_string();
        result.push_back(std::move(item));
    }
    return result;
}

}  // namespace tdx
