#include "leverage_internal.hpp"

#include <algorithm>
#include <utility>

namespace tdx {
using namespace leverage_detail;

Json normalize_margin_market_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("margin market rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json value = Json::object();
        value["date"] = value_text(row, "date");
        value["financing_balance_yuan"] = number_json(value_number(row, "rzye1"), 1e8);
        value["financing_balance_float_market_pct"] = number_json(value_number(row, "rzye4"));
        value["financing_net_change_yuan"] = number_json(value_number(row, "rzzj"), 1e8);
        value["sh_financing_balance_yuan"] = number_json(value_number(row, "rzye2"), 1e8);
        value["sz_financing_balance_yuan"] = number_json(value_number(row, "rzye3"), 1e8);
        value["short_balance_yuan"] = number_json(value_number(row, "rqye1"), 1e8);
        value["short_balance_float_market_pct"] = number_json(value_number(row, "rqye4"));
        value["sh_short_balance_yuan"] = number_json(value_number(row, "rqye2"), 1e8);
        value["sz_short_balance_yuan"] = number_json(value_number(row, "rqye3"), 1e8);
        result.push_back(std::move(value));
    }
    sort_date(result, true);
    return result;
}

Json normalize_margin_transfer_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("margin transfer rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json value = Json::object();
        value["date"] = value_text(row, "$ZQDM");
        // func_rzt101 exposes amount fields in yuan and quantity fields in shares.
        // Preserve an absent upstream value as null; do not infer a net value.
        value["transfer_financing_lent_yuan"] = number_json(value_number(row, "zrz1"));
        value["transfer_financing_repaid_yuan"] = number_json(value_number(row, "zrz2"));
        value["transfer_financing_net_change_yuan"] = number_json(value_number(row, "zrz3"));
        value["transfer_financing_balance_yuan"] = number_json(value_number(row, "zrz7"));
        value["securities_lending_lent_shares"] = number_json(value_number(row, "zrq1"));
        value["securities_lending_repaid_shares"] = number_json(value_number(row, "zrq2"));
        value["securities_lending_net_change_shares"] = number_json(value_number(row, "zrq3"));
        value["securities_lending_balance_shares"] = number_json(value_number(row, "zrq4"));
        value["securities_lending_balance_yuan"] = number_json(value_number(row, "zrq5"));
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    sort_date(result, true);
    return result;
}

Json normalize_margin_security_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("margin security rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = value_text(row, "$ZQDM");
        std::optional<int> id;
        try { if (!code.empty()) id = market_id(value_text(row, "$SC"), true); }
        catch (...) {}
        Json value = Json::object();
        if (id && digits(code, 6)) value["security"] = security_json(*id, code, securities);
        value["date"] = value_text(row, "date");
        value["change_pct"] = number_json(value_number(row, "ZAF"));
        value["float_market_cap_yuan"] = number_json(value_number(row, "J_LTSZ"), 1e8);
        value["float_shares"] = number_json(value_number(row, "J_LTGB"), 1e8);
        value["financing_net_buy_yuan"] = number_json(value_number(row, "drjme"));
        value["financing_balance_yuan"] = number_json(value_number(row, "rzye"));
        value["financing_balance_float_market_pct"] = number_json(value_number(row, "rzzltsz"));
        value["short_net_sell_shares"] = number_json(value_number(row, "drjml"));
        value["short_balance_shares"] = number_json(value_number(row, "rqyl"));
        value["short_balance_float_shares_pct"] = number_json(value_number(row, "zqzltgf"));
        value["financing_short_difference_yuan"] = number_json(value_number(row, "rzrqce"));
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    sort_date(result, true);
    return result;
}

Json normalize_margin_classification_rows(const Json& rows,
                                          const std::string& classification) {
    if (!rows.is_array()) throw Error("margin classification rows must be an array");
    if (classification != "industry" && classification != "concept" &&
        classification != "style")
        throw Error("margin classification must be industry, concept, or style");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = value_text(row, "$ZQDM");
        const auto name = value_text(row, "hy");
        if (code.empty() || name.empty()) continue;
        Json value = Json::object();
        value["classification"] = classification;
        value["classification_code"] = code;
        value["classification_name"] = name;
        value["date"] = value_text(row, "date");
        value["financing_buy_yuan"] = number_json(value_number(row, "hyrzmr"));
        value["financing_sell_yuan"] = number_json(value_number(row, "hyrzmc"));
        value["financing_net_buy_yuan"] = number_json(value_number(row, "hyrzjm"));
        value["financing_balance_yuan"] = number_json(value_number(row, "hyrzye"));
        value["short_buy_yuan"] = number_json(value_number(row, "hyrqmr"));
        value["short_sell_yuan"] = number_json(value_number(row, "hyrqmc"));
        value["short_net_sell_yuan"] = number_json(value_number(row, "hyrqjm"));
        value["short_balance_yuan"] = number_json(value_number(row, "hyrqye"));
        value["financing_short_difference_yuan"] = number_json(value_number(row, "hylrce"));
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return value_number(left, "financing_net_buy_yuan").value_or(-1e300) >
                   value_number(right, "financing_net_buy_yuan").value_or(-1e300);
        });
    return result;
}

Json normalize_margin_classification_history_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("margin classification history rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json value = Json::object();
        value["date"] = value_text(row, "date");
        // The chart CFG labels hyrzye/hylrce as 亿元 and hyrqye as 千万元.
        value["financing_balance_yuan"] = number_json(value_number(row, "hyrzye"), 1e8);
        value["short_balance_yuan"] = number_json(value_number(row, "hyrqye"), 1e7);
        value["financing_short_difference_yuan"] = number_json(value_number(row, "hylrce"), 1e8);
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    sort_date(result, true);
    return result;
}


} // namespace tdx
