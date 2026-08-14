#include "convertible_bonds_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/market.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {

using namespace convertible_bond_detail;

Json normalize_convertible_bond_documents(
    const Json& documents,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!documents.is_array() ||
        documents.size() < master_resources.size() ||
        documents.size() > master_resources.size() + 2)
        throw Error("convertible-bond normalization requires six core documents and up to two exchangeable-bond projections");
    std::map<std::string, const Json*> overview, progress, coupons, sellback,
        redemption, revision, exchangeable_projection;
    auto index = [](const Json& document, std::map<std::string, const Json*>& target) {
        for (const auto& row : document.at("rows").as_array()) {
            const auto key = key_for(row);
            if (!key.empty()) target[key] = &row;
        }
    };
    index(documents.as_array()[0], overview);
    index(documents.as_array()[1], progress);
    index(documents.as_array()[2], coupons);
    index(documents.as_array()[3], sellback);
    index(documents.as_array()[4], redemption);
    index(documents.as_array()[5], revision);
    std::set<std::string> supplemented_exchangeable;
    if (documents.size() > master_resources.size()) {
        for (const auto& row : documents.as_array()[6].at("rows").as_array()) {
            const auto key = key_for(row);
            if (key.empty()) continue;
            overview[key] = &row;
            supplemented_exchangeable.insert(key);
        }
    }
    if (documents.size() > master_resources.size() + 1)
        index(documents.as_array()[7], exchangeable_projection);
    std::set<std::string> keys;
    for (const auto* table : {&overview, &progress, &coupons, &sellback,
                              &redemption, &revision, &exchangeable_projection})
        for (const auto& [key, row] : *table) { (void)row; keys.insert(key); }
    Json rows = Json::array();
    for (const auto& key : keys) {
        const auto colon = key.find(':');
        const int bond_market = std::stoi(key.substr(0, colon));
        const auto bond_code = key.substr(colon + 1);
        const Json empty = Json::object();
        const auto pick = [&](const std::map<std::string, const Json*>& table) -> const Json& {
            const auto found = table.find(key);
            return found == table.end() ? empty : *found->second;
        };
        const auto& base = pick(overview);
        const auto& step = pick(progress);
        const auto& rate = pick(coupons);
        const auto& sell = pick(sellback);
        const auto& redeem = pick(redemption);
        const auto& revise = pick(revision);
        const auto& projected = pick(exchangeable_projection);
        const auto text_with_projection = [&](std::string_view name) {
            const auto primary = value_text(base, name);
            return primary.empty() ? value_text(projected, name) : primary;
        };
        const auto number_with_projection = [&](std::string_view name) {
            const auto primary = value_number(base, name);
            return primary ? Json(*primary) : number(projected, name);
        };
        int stock_market = -1;
        auto stock_code = text_with_projection("$ZQDM1");
        if (stock_code.empty()) stock_code = value_text(rate, "$ZQDM1");
        try {
            auto raw = text_with_projection("$SC1");
            if (raw.empty()) raw = value_text(rate, "$SC1");
            stock_market = market_id(raw);
        } catch (...) {}
        Json item = Json::object();
        item["bond"] = security_document(bond_market, bond_code,
            value_text(base, "ZQJC"), securities);
        const bool exchangeable = bond_code.rfind("132", 0) == 0;
        const bool projection_verified = exchangeable_projection.count(key) != 0;
        item["instrument_type"] = exchangeable ? "exchangeable-bond" : "convertible-bond";
        item["exchangeable_supplemented"] = supplemented_exchangeable.count(key) != 0;
        item["exchangeable_projection_verified"] = projection_verified;
        item["underlying"] = stock_market >= 0 && valid_code(stock_code)
            ? security_document(stock_market, stock_code, value_text(base, "ZGDM"), securities)
            : Json(nullptr);
        Json overview_doc = Json::object();
        overview_doc["issuer"] = "";
        overview_doc["risk_notice"] = text_with_projection("FXTS");
        overview_doc["face_value"] = number_with_projection("MZ");
        overview_doc["listing_date"] = value_text(base, "SSRQ");
        overview_doc["issue_date"] = value_text(base, "QXRQ");
        overview_doc["issue_price"] = number(base, "FXJG");
        overview_doc["return_since_listing_pct"] = number(base, "ZF1");
        overview_doc["return_5d_pct"] = number(base, "ZF2");
        overview_doc["return_10d_pct"] = number(base, "ZF3");
        overview_doc["issue_size_100m_yuan"] = number(base, "FXZE");
        overview_doc["remaining_balance_100m_yuan"] = number(base, "ZQYE");
        overview_doc["remaining_ratio_pct"] = number(base, "YEZB");
        overview_doc["conversion_price"] = number_with_projection("ZGJ");
        overview_doc["conversion_start_date"] = text_with_projection("ZGQSR");
        overview_doc["conversion_end_date"] = text_with_projection("ZGJZR");
        overview_doc["maturity_date"] = text_with_projection("DQRQ");
        overview_doc["remaining_years"] = number_with_projection("SYNX");
        overview_doc["maturity_redemption_price"] = number_with_projection("DQSHJ");
        overview_doc["unpaid_coupon_sum"] = number_with_projection("LLZH");
        overview_doc["sellback_trigger_ratio_pct"] = number_with_projection("HSCFBL");
        overview_doc["redemption_trigger_ratio_pct"] = number_with_projection("QSCFBL");
        overview_doc["bond_rating"] = value_text(base, "ZQPJ");
        overview_doc["issuer_rating"] = value_text(base, "ZTPJ");
        overview_doc["current_state"] = value_text(base, "DQLB");
        overview_doc["source_resource"] = supplemented_exchangeable.count(key)
            ? exchangeable_resource : projection_verified
                ? exchangeable_projection_resource : master_resources.front();
        overview_doc["projection_resource"] = projection_verified
            ? exchangeable_projection_resource : "";
        overview_doc["core_terms_complete"] =
            !overview_doc.at("face_value").is_null() &&
            !overview_doc.at("conversion_price").is_null() &&
            !overview_doc.at("maturity_date").as_string().empty();
        item["overview"] = std::move(overview_doc);
        Json progress_doc = Json::object();
        progress_doc["issue_size_100m_yuan"] = number(step, "FXZL");
        progress_doc["remaining_balance_100m_yuan"] = number(step, "ZQYE");
        progress_doc["conversion_progress_pct"] = number(step, "ZGJD");
        progress_doc["redeemed_amount_100m_yuan"] = number(step, "YSHME");
        progress_doc["sellback_amount_100m_yuan"] = number(step, "YHSME");
        progress_doc["maturity_progress_pct"] = number(step, "DQJD");
        item["progress"] = std::move(progress_doc);
        Json coupon_doc = Json::object();
        Json rates = Json::array();
        for (int year = 1; year <= 6; ++year)
            rates.push_back(number(rate, "PMLL_" + std::to_string(year)));
        coupon_doc["term_years"] = number(rate, "FXQX");
        coupon_doc["rates_pct"] = std::move(rates);
        coupon_doc["compensation_rate_pct"] = number(rate, "BCLL");
        coupon_doc["payment_dates"] = text_array(value_text(base, "FXRQXL"));
        coupon_doc["payment_rates"] = numeric_array(value_text(base, "FXLLXL"));
        item["coupons"] = std::move(coupon_doc);
        item["sellback"] = trigger_document(sell, "HSQSRQ", "HSJG", "ZGJG",
            "CFJD", "CFQK", "YCFCS", "YHSRQ", "HSCFSYTS");
        item["redemption"] = trigger_document(redeem, "SHQSRQ", "SHJG", "ZGJG",
            "CFJD", "CFQK", "YCFCS", "YSHRQ", "SHCFSYTS");
        item["revision"] = trigger_document(revise, "XZQSRQ", "CFJG", "ZGJG",
            "CFJD", "CFQK", "ZGJTZCS", "YXZRQ", "XZCFSYTS");
        rows.push_back(std::move(item));
    }
    return rows;
}

}  // namespace tdx
