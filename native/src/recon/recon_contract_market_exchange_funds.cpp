#include "recon_contract_market_corporate_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace tdx::recon_contract_detail {

void validate_market_exchange_funds_contract(
    const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-exchange-funds-native-v1"),
                  "tdx-market-exchange-funds-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view",
                  string_is(member(document, "view"), "all") &&
                      string_is(member(document, "mode"), "catalog"),
                  "all/catalog", value_or_null(member(document, "view")));
    add_assertion(result, "offline_quote_boundary",
                  member(document, "quote_source") &&
                      member(document, "quote_source")->is_null() &&
                      member(document, "quote_errors") &&
                      member(document, "quote_errors")->is_array() &&
                      member(document, "quote_errors")->as_array().empty(),
                  "include_quotes=0 leaves public L1 detached", true);
    const auto* matched = member(document, "match_count");
    const auto* etf = member_path(document, {"summary", "etf_performance"});
    const auto* etf_share = member_path(document, {"summary", "etf_share_ranking"});
    const auto* arbitrage = member_path(document, {"summary", "cash_arbitrage"});
    const auto* cash_yield = member_path(document, {"summary", "cash_yield"});
    const auto* etf_scale = member_path(document, {"summary", "etf_scale_flow"});
    const auto* commodity = member_path(document, {"summary", "commodity_etf"});
    const auto* lof = member_path(document, {"summary", "lof"});
    const auto* closed = member_path(document, {"summary", "closed_fund"});
    const auto* cash_calendar = member_path(
        document, {"summary", "cash_management_calendar"});
    const auto* reits = member_path(document, {"summary", "reits_issued"});
    const auto* pipeline = member_path(document, {"summary", "reits_pipeline"});
    const auto* configured_empty = member_path(
        document, {"summary", "configured_empty_sources"});
    const bool population = matched && matched->is_number() &&
        etf && etf->is_number() && etf->as_number() >= 1600 &&
        arbitrage && arbitrage->is_number() && arbitrage->as_number() >= 27 &&
        cash_yield && cash_yield->is_number() && cash_yield->as_number() >= 13 &&
        reits && reits->is_number() && reits->as_number() >= 90 &&
        pipeline && pipeline->is_number() && pipeline->as_number() == 0 &&
        configured_empty && configured_empty->is_number() &&
        configured_empty->as_number() >= 1;
    const bool expanded_population = !etf_scale ||
        (etf_scale->is_number() && etf_scale->as_number() >= 2800 &&
         (!etf_share || (etf_share->is_number() && etf_share->as_number() >= 450)) &&
         commodity && commodity->is_number() && commodity->as_number() >= 10 &&
         lof && lof->is_number() && lof->as_number() >= 400 &&
         closed && closed->is_number() && closed->as_number() >= 30 &&
         cash_calendar && cash_calendar->is_number() &&
             cash_calendar->as_number() >= 18);
    add_assertion(result, "population", population,
                  "ETF performance/scale, commodity ETF, LOF, closed/cash funds and REIT populations",
                  population);
    const double expanded_total = etf_scale
        ? etf_scale->as_number() + commodity->as_number() + lof->as_number() +
            closed->as_number() + cash_calendar->as_number() +
            (etf_share ? etf_share->as_number() : 0.0) : 0.0;
    const bool reconciliation = population && expanded_population &&
        etf->as_number() + arbitrage->as_number() + cash_yield->as_number() +
            expanded_total + reits->as_number() + pipeline->as_number() ==
                matched->as_number();
    add_assertion(result, "summary_reconciliation", reconciliation,
                  "all family counts equal match_count", reconciliation);

    const auto* records = member(document, "records");
    bool etf_formula = false;
    bool arbitrage_formula = false;
    bool source_market_34_sz = false;
    bool source_market_34_sh = false;
    bool reit_units = false;
    if (records && records->is_array()) {
        for (const auto& row : records->as_array()) {
            const auto* security = member(row, "security");
            const bool auditable = security && security->is_object() &&
                member(row, "raw") && member(row, "raw")->is_object() &&
                member(row, "source_resource") &&
                member(row, "source_resource")->is_string();
            if (string_is(member(row, "kind"), "etf-performance")) {
                const auto* close = member(row, "close_price");
                const auto* reference = member(row, "reference_close_5d");
                const auto* change = member(row, "change_5d_pct");
                if (auditable && close && close->is_number() && reference &&
                    reference->is_number() && change && change->is_number() &&
                    std::abs(reference->as_number()) > 0.000001) {
                    const auto expected = (close->as_number() - reference->as_number()) *
                        100.0 / reference->as_number();
                    etf_formula = etf_formula ||
                        std::abs(change->as_number() - expected) < 0.000001;
                }
            } else if (string_is(member(row, "kind"), "cash-arbitrage")) {
                const auto* yield = member(row, "seven_day_annualized_pct");
                const auto* days = member(row, "buy_redeem_interest_days");
                const auto* nav = member(row, "theoretical_nav");
                if (auditable && yield && yield->is_number() && days &&
                    days->is_number() && nav && nav->is_number()) {
                    const auto expected = 100.0 + days->as_number() *
                        yield->as_number() / 365.0;
                    arbitrage_formula = arbitrage_formula ||
                        std::abs(nav->as_number() - expected) < 0.000001;
                }
            } else if (string_is(member(row, "kind"), "cash-yield") &&
                       member(row, "source_market_id") &&
                       member(row, "source_market_id")->is_number() &&
                       member(row, "source_market_id")->as_number() == 34 &&
                       security && security->is_object()) {
                const auto code = member(*security, "code");
                if (code && code->is_string() && code->as_string().rfind("159", 0) == 0)
                    source_market_34_sz = source_market_34_sz ||
                        string_is(member(*security, "market"), "sz");
                if (code && code->is_string() && code->as_string().rfind("519", 0) == 0)
                    source_market_34_sh = source_market_34_sh ||
                        string_is(member(*security, "market"), "sh");
            } else if (string_is(member(row, "kind"), "reit-issued")) {
                const auto* total = member(row, "offering_total_units");
                const auto* price = member(row, "subscription_price");
                reit_units = reit_units || (auditable && total && total->is_number() &&
                    total->as_number() >= 1000000 && price && price->is_number() &&
                    member(row, "dates") && member(row, "dates")->is_object());
            }
        }
    }
    add_assertion(result, "etf_formula", etf_formula,
                  "(close-reference)*100/reference", etf_formula);
    add_assertion(result, "cash_arbitrage_formula", arbitrage_formula,
                  "100 + interest_days * seven_day_yield / 365", arbitrage_formula);
    add_assertion(result, "source_market_34_routing",
                  source_market_34_sz && source_market_34_sh,
                  "159-prefix to sz and 519-prefix to sh while retaining source 34",
                  source_market_34_sz && source_market_34_sh);
    add_assertion(result, "reit_units", reit_units,
                  "offering units, subscription price, dates and raw retained",
                  reit_units);
    const std::vector<std::string> expanded_sources{
        "list/func_etfhq101.jsn", "list/func_tlfeyxetf101_1.jsn",
        "list/gxjty_etfjj101.jsn",
        "list/gxjty_etfjj102.jsn", "list/gxjty_etfjj103.jsn",
        "list/gxjty_etfjj104.jsn", "list/gxjty_etfjj105.jsn",
        "list/gxjty_etfjj106.jsn", "list/gxjty_etfjj107.jsn",
        "list/gxjty_lofjj101.jsn", "list/gxjty_lofjj102.jsn",
        "list/gxjty_lofjj103.jsn", "list/gxjty_lofjj104.jsn",
        "list/gxjty_lofjj105.jsn", "list/gxjty_lofjj106.jsn",
        "list/gxjty_lofjj107.jsn", "list/gxjty_fbjj101.jsn",
        "list/gxjty_fbjj102.jsn", "list/gxjty_xjgl101.jsn",
        "list/gxjty_xjgl104.jsn", "list/func_reits101_1.jsn",
        "list/func_reits102_1.jsn"};
    const std::vector<std::string> legacy_sources{
        "list/func_etfhq101.jsn", "list/gxjty_etfjj103.jsn",
        "list/gxjty_etfjj104.jsn", "list/func_reits101_1.jsn",
        "list/func_reits102_1.jsn"};
    const auto& required_sources = etf_scale ? expanded_sources : legacy_sources;
    bool all_sources = true;
    for (const auto& source : required_sources)
        all_sources = all_sources && source_exists(document, source);
    add_assertion(result, "exact_sources", all_sources,
                  static_cast<std::uint64_t>(required_sources.size()), all_sources);
    bool pipeline_blank = false;
    const auto* sources = member(document, "sources");
    if (sources && sources->is_array())
        for (const auto& source : sources->as_array())
            if (string_is(member(source, "resource"),
                          "list/func_reits102_1.jsn"))
                pipeline_blank = member(source, "row_count") &&
                    member(source, "row_count")->is_number() &&
                    member(source, "row_count")->as_number() >= 1 &&
                    member(source, "normalized_row_count") &&
                    member(source, "normalized_row_count")->is_number() &&
                    member(source, "normalized_row_count")->as_number() == 0 &&
                    member(source, "blank_row_count") &&
                    member(source, "blank_row_count")->is_number() &&
                    member(source, "blank_row_count")->as_number() >= 1;
    add_assertion(result, "pipeline_blank_relation", pipeline_blank,
                  "configured one-row blank table becomes zero normalized rows",
                  pipeline_blank);
}

}  // namespace tdx::recon_contract_detail
