#include "tdx/convertible_bonds.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{2, "920001"}] =
            tdx::Security{2, "BJ", "北京", "920001", "北证样本"};
        auto source = tdx::Json::object();
        source["rows"] = tdx::Json::array();
        source["rows"].push_back(tdx::Json::parse(
            R"({"$SC":"2","$ZQDM":"920001","zzlx":"可转债","mzgm":"10.68","fadj":"申购计划","date0":"20260806","byhq":"2.669933251668708","zgj":"20.2800","gdpsl":"0.02669900","sgrq":"20260806","fxrq":"20260806","zql":"","zqr":"20260810","sgdm":"371565","sgmc":"样本发债","fxjg":"100.000"})"));
        source["rows"].push_back(tdx::Json::parse(
            R"({"$SC":"0","$ZQDM":"000001","zzlx":"可转债","mzgm":"20","fadj":"证监会核准批复","date0":"20260728","byhq":"1.5","zgj":"12.3","gdpsl":"","sgrq":"","fxrq":"","zql":"","zqr":"","sgdm":"","sgmc":"","fxjg":""})"));
        auto rows = tdx::normalize_pending_convertible_bond_document(source, securities);
        const auto& first = rows.as_array()[0];
        require(first.at("underlying").at("security_id").as_string() == "BJ920001" &&
                    first.at("underlying").at("name").as_string() == "北证样本" &&
                    std::abs(first.at("planned_issue_size_100m_yuan").as_number() - 10.68) < 0.000001 &&
                    std::abs(first.at("shareholder_placement_ratio").as_number() - 0.026699) < 0.000001 &&
                    first.at("lottery_rate").is_null() && first.at("raw").is_object(),
                "normalizer must preserve identity, upstream units, blanks and raw row");

        tdx::sort_pending_convertible_bond_rows(rows, "progress-date", "desc");
        require(rows.as_array()[0].at("progress_date").as_string() == "20260806",
                "default client date order should be descending");
        tdx::sort_pending_convertible_bond_rows(rows, "issue-size", "desc");
        require(rows.as_array()[0].at("underlying").at("security_id").as_string() == "SZ000001",
                "issue-size sort should use normalized numeric values");
        tdx::sort_pending_convertible_bond_rows(rows, "subscription-date", "asc");
        require(rows.as_array()[0].at("underlying").at("security_id").as_string() == "BJ920001",
                "missing subscription dates should stay after actual dates in either order");

        bool rejected = false;
        try { tdx::sort_pending_convertible_bond_rows(rows, "unknown", "desc"); }
        catch (const std::exception&) { rejected = true; }
        require(rejected, "unknown pending sort must be rejected");

        auto subscription_source = tdx::Json::object();
        subscription_source["rows"] = tdx::Json::array();
        subscription_source["rows"].push_back(tdx::Json::parse(
            R"({"$ZQDM":"123281","$SC":"0","ZQJC":"嘉戎转债","sgrq":"20260806","sgdm":"371565","sgsx":"100.0000","zgr":"20270212","$ZQDM1":"301565","$SC1":"0","zxj":"19.620","zgj":"20.280","zxsp":"100.000","fxzs":"10.6800000000","zqr":"20260810","zql":"0.00103202","ssrq":""})"));
        const auto subscriptions =
            tdx::normalize_convertible_bond_subscription_document(
                subscription_source, securities);
        require(subscriptions.size() == 1,
                "subscription normalizer must retain a valid bond");
        const auto& subscription = subscriptions.as_array().front();
        const double expected_value = 19.62 * 100.0 / 20.28;
        const double expected_premium = (100.0 - expected_value) * 100.0 /
            expected_value;
        require(subscription.at("bond").at("security_id").as_string() ==
                    "SZ123281" &&
                subscription.at("underlying").at("security_id").as_string() ==
                    "SZ301565" &&
                std::abs(subscription.at("conversion_value_yuan").as_number() -
                         expected_value) < 0.000001 &&
                std::abs(subscription.at("conversion_premium_pct").as_number() -
                         expected_premium) < 0.000001 &&
                std::abs(subscription.at("lottery_rate_pct").as_number() -
                         0.00103202) < 0.000000001 &&
                !subscription.at("listed").as_bool() &&
                subscription.at("raw").is_object(),
                "subscription units, client formulas and raw row must be preserved");
        auto sorted_subscriptions = subscriptions;
        tdx::sort_convertible_bond_subscription_rows(
            sorted_subscriptions, "premium", "desc");
        bool subscription_sort_rejected = false;
        try {
            tdx::sort_convertible_bond_subscription_rows(
                sorted_subscriptions, "unknown", "desc");
        } catch (const std::exception&) { subscription_sort_rejected = true; }
        require(subscription_sort_rejected,
                "unknown subscription sort must be rejected");

        auto new_bond_source = tdx::Json::object();
        new_bond_source["rows"] = tdx::Json::array();
        new_bond_source["rows"].push_back(tdx::Json::parse(
            R"({"sgdm":"371565","sgmc":"嘉戎发债","sgrq":"2022-08-17 周三","fxjg":"100","zzlx":"可转债","mzgm":"25.00","byhq":"2.670","zgj":"20.2800","zg":"是","zgqsr":"20270212","zgjzr":"20320805","gdpsl":"0.026699","zqr":"20260810","zql":"0.00103202","ssdy":"福建厦门","$ZQDM":"301565","$SC":"0","fadj":"股东大会通过","date0":"20260602","fxrq":"20220817"})"));
        const auto new_bond_rows =
            tdx::normalize_new_convertible_bond_projection_document(
                new_bond_source, securities);
        require(new_bond_rows.size() == 1 &&
                    new_bond_rows.as_array().front().at("subscription_date").as_string() ==
                        "20220817" &&
                    new_bond_rows.as_array().front().at("underlying").at(
                        "security_id").as_string() == "SZ301565" &&
                    new_bond_rows.as_array().front().at("raw").is_object(),
                "new-bond projection must normalize the client date and preserve raw fields");
        const auto projection_reconciliation =
            tdx::reconcile_new_convertible_bond_projection(
                subscriptions, new_bond_rows);
        const auto& projection_summary =
            projection_reconciliation.at("summary");
        const auto& projection_match = projection_reconciliation.at("rows")
            .as_array().front().at("subscription_match");
        require(projection_summary.at("exact_subscription_code_matches").as_number() == 1 &&
                    projection_summary.at("issue_size_mismatch_count").as_number() == 1 &&
                    projection_summary.at("subscription_date_mismatch_count").as_number() == 1 &&
                    projection_summary.at("hybrid_or_stale_count").as_number() == 1 &&
                    projection_match.at("match_method").as_string() ==
                        "subscription-code" &&
                    projection_match.at("bond").at("security_id").as_string() ==
                        "SZ123281",
                "new-bond projection reconciliation must expose stale hybrid rows without remapping them");

        auto documents = tdx::Json::array();
        for (int i = 0; i < 8; ++i) {
            auto document = tdx::Json::object();
            document["rows"] = tdx::Json::array();
            documents.push_back(std::move(document));
        }
        documents.as_array()[1]["rows"].push_back(tdx::Json::parse(
            R"({"$SC":"1","$ZQDM":"132024","ZGJD":"100","DQJD":"6.57"})"));
        documents.as_array()[6]["rows"].push_back(tdx::Json::parse(
            R"({"$SC":"1","$ZQDM":"132024","ZQJC":"26江铜EB","MZ":"100","SSRQ":"20260420","FXJG":"100","ZF1":"36.599","ZF2":"-0.4365","ZF3":"1.8681","ZGJ":"52.4","ZGQSR":"20270412","ZGJZR":"20310409","$ZQDM1":"600362","$SC1":"1","QXRQ":"20260409","DQRQ":"20310409","SYNX":"4.69315","ZQPJ":"","ZTPJ":"AAA","DQLB":"正常存续"})"));
        documents.as_array()[7]["rows"].push_back(tdx::Json::parse(
            R"({"$SC":"1","$ZQDM":"132024","$ZQDM1":"600362","$SC1":"1","ZGJ":"52.4","MZ":"100","DQRQ":"20310409","SYNX":"4.671","DQSHJ":"105","QSCFBL":"120","LLZH":"0.040"})"));
        const auto listed = tdx::normalize_convertible_bond_documents(documents, {});
        require(listed.size() == 1, "exchangeable supplement should join the core catalog");
        const auto& exchangeable = listed.as_array()[0];
        require(exchangeable.at("instrument_type").as_string() == "exchangeable-bond" &&
                    exchangeable.at("exchangeable_supplemented").as_bool() &&
                    exchangeable.at("exchangeable_projection_verified").as_bool() &&
                    exchangeable.at("bond").at("name").as_string() == "26江铜EB" &&
                    exchangeable.at("underlying").at("security_id").as_string() == "SH600362" &&
                    std::abs(exchangeable.at("overview").at("conversion_price").as_number() - 52.4) < 0.000001 &&
                    exchangeable.at("overview").at("maturity_date").as_string() == "20310409" &&
                    std::abs(exchangeable.at("overview").at(
                        "maturity_redemption_price").as_number() - 105.0) < 0.000001 &&
                    std::abs(exchangeable.at("overview").at(
                        "unpaid_coupon_sum").as_number() - 0.04) < 0.000001 &&
                    exchangeable.at("overview").at("core_terms_complete").as_bool() &&
                    exchangeable.at("overview").at("source_resource").as_string() ==
                        "list/kjhz_kjhzsy201_1.jsn",
                "exchangeable supplement must restore identity, underlying and core terms");

        auto pricing_source = tdx::Json::array();
        pricing_source.push_back(tdx::Json::parse(
            R"({"$SC":"1","$ZQDM":"110076","ZQJC":"华海转债","MZ":"100","ZF1":"14.98","ZF2":"-4.54","ZF3":"-7.00","ZGJ":"16.500","ZGQSR":"20210506","ZGJZR":"20261101","$ZQDM1":"600521","$SC1":"1","XXCFBL":"80","HSCFBL":"70","QSCFBL":"130","SSRQ":"20201125","QXRQ":"20201102","DQRQ":"20261102","SYNX":"0.241","ZQPJ":"AA","ZTPJ":"AA","ZSQX1":"0.2","ZSSYL1":"1.5164","ZSQX2":"0.25","ZSSYL2":"1.5258","SYNXSYL":"0.015241","ZQLX":"可转债","LLLX":"递进利率","LLLXBZ":"3","FXRQXL":"20211102,20221102,20231102,20241102,20251102,20261102","FXLLXL":"0.003,0.005,0.01,0.015,0.018,0.1","SYFXCS":"1","SYFXRQXL":"20261102","SYFXLLXL":"0.1","SGFXRQ":"20251102","FXPL1":"12","XGFXRQ":"20261102","YEZB":"99.99"})"));
        auto quotes = tdx::Json::array();
        quotes.push_back(tdx::Json::parse(
            R"({"market_id":1,"code":"110076","last_price":114.98,"change_pct":-1.2911644518,"amount":104257544})"));
        quotes.push_back(tdx::Json::parse(
            R"({"market_id":1,"code":"600521","last_price":16.46,"change_pct":-1.2004801921,"amount":494392096})"));
        const auto pricing = tdx::normalize_convertible_bond_pricing_rows(
            pricing_source, quotes, {}, "20260807");
        require(pricing.size() == 1, "pricing normalizer should retain the bond");
        const auto& priced = pricing.as_array().front();
        require(priced.at("active").as_bool() &&
                    priced.at("bond").at("security_id").as_string() == "SH110076" &&
                    priced.at("underlying").at("security_id").as_string() == "SH600521" &&
                    std::abs(priced.at("valuation").at("accrued_interest").as_number() -
                             7.61643835616438) < 0.000001 &&
                    std::abs(priced.at("valuation").at("full_price").as_number() -
                             122.596438356164) < 0.000001 &&
                    std::abs(priced.at("valuation").at("conversion_value").as_number() -
                             99.7575757575758) < 0.000001 &&
                    std::abs(priced.at("valuation").at("conversion_premium_pct").as_number() -
                             22.8943640872851) < 0.000001 &&
                    std::abs(priced.at("valuation").at("maturity_yield_pct").as_number() +
                             36.546033180448) < 0.000001 &&
                    priced.at("valuation").at("cash_flow_count").as_number() == 1 &&
                    priced.at("raw").is_object(),
                "pricing formulas must reproduce full-price premium and remaining cash-flow YTM");
        auto sortable = pricing;
        tdx::sort_convertible_bond_pricing_rows(sortable, "double-low", "asc");
        bool pricing_sort_rejected = false;
        try { tdx::sort_convertible_bond_pricing_rows(sortable, "unknown", "asc"); }
        catch (const std::exception&) { pricing_sort_rejected = true; }
        require(pricing_sort_rejected, "unknown pricing sort must be rejected");

        std::cout << "convertible-bonds tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "convertible-bonds test failed: " << error.what() << '\n';
        return 1;
    }
}
