#include "equity_valuation_internal.hpp"

#include "tdx/cloud_resilience.hpp"

#include <iomanip>
#include <sstream>

namespace tdx::detail::equity_valuation {

Json security_identity(const std::string& raw_market, const std::string& code,
                       std::string name, const BlockData& blocks) {
    const int market = security_market(raw_market);
    if (code.size() != 6 || !ascii_digits(code))
        throw Error("valuation row contains an invalid security code");
    const auto known = blocks.securities.find({market, code});
    if (name.empty() && known != blocks.securities.end()) name = known->second.name;
    Json result = Json::object();
    result["market"] = market_name(market);
    result["market_id"] = market;
    result["code"] = code;
    result["security_id"] = market_prefix(market) + code;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    return result;
}

Json industry_identity(const std::string& code, const std::string& raw_market,
                       std::string name, const BlockData& blocks) {
    if (code.size() != 6 || !ascii_digits(code))
        throw Error("valuation row contains an invalid industry code");
    const int market = integer_value(raw_market, "industry market", 0, 255);
    const Block* selected = nullptr;
    for (const auto& block : blocks.blocks) {
        if (block.block_code != code) continue;
        if (!selected || block.family == "research-industry") selected = &block;
        if (block.family == "research-industry") break;
    }
    if (name.empty() && selected) name = selected->name;
    Json result = Json::object();
    result["code"] = code;
    result["market_id"] = market;
    result["industry_id"] = "M" + std::to_string(market) + code;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    result["block_id"] = selected ? Json(selected->block_id) : Json(nullptr);
    result["family"] = selected ? Json(selected->family) : Json(nullptr);
    return result;
}

Json judgment(const Json& row, std::string_view key, bool allow_reasonable) {
    const auto raw = text(row, key);
    Json result = Json::object();
    result["code"] = Json(nullptr);
    result["label"] = Json(nullptr);
    if (raw.empty() || raw == "--") return result;
    int code = 0;
    try {
        std::size_t used = 0;
        code = std::stoi(raw, &used);
        if (used != raw.size()) return result;
    } catch (...) { return result; }
    result["code"] = code;
    if (code == 1) result["label"] = "overvalued";
    else if (code == 2) result["label"] = "undervalued";
    else if (code == 3 && allow_reasonable) result["label"] = "reasonable";
    return result;
}

Json forecast(const Json& row, const std::string& prefix) {
    Json result = Json::object();
    result["eps_consensus"] = number_json(number(row, prefix + "_forecast"));
    result["implied_price"] = number_json(number(row, "price_" + prefix + "_forecast"));
    result["price_judgment"] = judgment(row, "gjpd_" + prefix, false);
    result["implied_pe"] = number_json(number(row, "pe_" + prefix + "_forecast"));
    result["valuation_judgment"] = judgment(row, "gzpd_" + prefix);
    result["horizon"] = prefix == "T" ? "current-fiscal-year" : "next-fiscal-year";
    return result;
}

std::string cache_key(const EquityValuationQuery& query) {
    std::ostringstream output;
    output << query.view << '|' << query.market << '|' << query.code << '|'
           << query.industry_market << '|' << query.industry_code << '|'
           << query.start_date << '|' << query.end_date << '|' << query.pe_type << '|'
           << std::setprecision(15) << query.required_return_rate_pct << '|' << query.limit;
    return output.str();
}

bool transient_error(const std::string& message) {
    return detail::is_transient_cloud_error(message);
}

Json source_document(const Json& upstream, const std::string& request_id) {
    Json result = Json::object();
    result["transport"] = "PBRPC reqformat=22";
    result["request_id"] = request_id;
    result["source_file"] = upstream.at("source_file");
    result["module"] = upstream.at("module");
    result["rpc_id"] = upstream.at("rpc_id");
    result["rounds"] = upstream.at("rounds");
    result["raw_size"] = upstream.at("raw_size");
    return result;
}

Json methodology(const ViewSpec& view) {
    Json result = Json::object();
    if (view.pe) {
        result["model"] = "historical-pe-range-and-consensus-forecast";
        result["pe_type_source"] = "client-selectable annual or TTM";
        result["history_window_default_months"] = 36;
        result["forecast_note"] =
            "Implied price and PE are upstream model outputs; required return is a percentage.";
    } else {
        result["model"] = "industry-pb-roe-regression-band";
        result["valuation_note"] =
            "Above the regression equilibrium band is overvalued, below it is undervalued.";
    }
    result["valuation_codes"] = Json::parse(
        "{\"1\":\"overvalued\",\"2\":\"undervalued\",\"3\":\"reasonable\"}");
    result["advice"] = false;
    return result;
}

}  // namespace tdx::detail::equity_valuation
