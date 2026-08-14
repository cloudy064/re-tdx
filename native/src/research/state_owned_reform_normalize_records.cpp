#include "state_owned_reform_internal.hpp"

#include "tdx/common.hpp"

namespace tdx {

Json normalize_state_owned_details(
    const Json& rows, const Json& quote_rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace state_owned_detail;
    if (!rows.is_array()) throw Error("state-owned detail rows must be an array");
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        const auto code = text_value(raw, "$ZQDM");
        if (!digits(code, 6)) continue;
        const auto market = inferred_market(text_value(raw, "$SC"), code);
        const auto* quote = find_quote(quote_rows, market, code);
        const auto ref3 = number_value(raw, "fqprice_d3");
        const auto ref5 = number_value(raw, "fqprice_d5");
        const auto ref20 = number_value(raw, "fqprice_d20");
        const auto ref60 = number_value(raw, "fqprice_d60");
        const auto ref3m = number_value(raw, "JSYSP");
        const auto refytd = number_value(raw, "NZJSP");
        Json row = Json::object();
        row["security"] = security_document(market, code, securities);
        row["actual_controller"] = text_value(raw, "skr");
        row["controlling_stake_pct"] = number_json(number_value(raw, "kgbl"));
        row["leading_count"] = number_json(number_value(raw, "lzcs"));
        row["logic"] = text_value(raw, "LJSM");
        Json references = Json::object();
        references["3d"] = number_json(ref3);
        references["5d"] = number_json(ref5);
        references["20d"] = number_json(ref20);
        references["60d"] = number_json(ref60);
        references["3m"] = number_json(ref3m);
        references["ytd"] = number_json(refytd);
        row["reference_close_prices"] = std::move(references);
        row["last_price"] = quote_metric(quote, "last_price");
        row["change_pct"] = quote_metric(quote, "change_pct");
        row["amount_yuan"] = quote_metric(quote, "amount");
        Json returns = Json::object();
        returns["3d"] = return_metric(quote, ref3);
        returns["5d"] = return_metric(quote, ref5);
        returns["20d"] = return_metric(quote, ref20);
        returns["60d"] = return_metric(quote, ref60);
        returns["3m"] = return_metric(quote, ref3m);
        returns["ytd"] = return_metric(quote, refytd);
        row["returns_pct"] = std::move(returns);
        row["quote_available"] = quote != nullptr;
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

Json normalize_state_owned_restructuring(
    const Json& rows, const Json& quote_rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace state_owned_detail;
    if (!rows.is_array())
        throw Error("state-owned restructuring rows must be an array");
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        const auto code = text_value(raw, "$ZQDM");
        if (!digits(code, 6)) continue;
        const auto market = inferred_market(text_value(raw, "$SC"), code);
        const auto* quote = find_quote(quote_rows, market, code);
        const auto date = iso_date(text_value(raw, "DATE"));
        Json row = Json::object();
        row["security"] = security_document(market, code, securities);
        row["major_shareholder_holding_pct"] =
            number_json(number_value(raw, "DGDCGBL"));
        row["net_profit_10k_yuan"] = number_json(number_value(raw, "JLR"));
        row["capital_operation"] = text_value(raw, "ZBYZ");
        row["explanation"] = text_value(raw, "XXSM");
        row["actual_controller"] = text_value(raw, "SKR");
        row["controlling_stake_pct"] = number_json(number_value(raw, "KGBL"));
        row["as_of_date"] = date.empty() ? Json(nullptr) : Json(date);
        row["last_price"] = quote_metric(quote, "last_price");
        row["change_pct"] = quote_metric(quote, "change_pct");
        row["amount_yuan"] = quote_metric(quote, "amount");
        row["quote_available"] = quote != nullptr;
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

}  // namespace tdx
