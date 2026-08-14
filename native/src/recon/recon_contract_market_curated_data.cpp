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

void validate_market_curated_data_contract(
    const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-curated-data-native-v1"),
                  "tdx-market-curated-data-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view_mode",
                  string_is(member(document, "view"), "all") &&
                      string_is(member(document, "mode"), "catalog") &&
                      string_is(member(document, "availability"), "live"),
                  "all/catalog/live", value_or_null(member(document, "mode")));
    const auto* media = member_path(document, {"summary", "media_entertainment"});
    const auto* low = member_path(document, {"summary", "low_valuation_smallcap"});
    const auto* dividends = member_path(document, {"summary", "dividend_fundraising"});
    const auto* buybacks = member_path(document, {"summary", "buyback_statistics"});
    const auto* high_dividend = member_path(document, {"summary", "high_dividend"});
    const auto* hk = member_path(document, {"summary", "hk_performance"});
    const auto* lending = member_path(document, {"summary", "high_refinancing_lending"});
    const auto* soe = member_path(document, {"summary", "below_book_soe"});
    const auto* matched = member(document, "match_count");
    const bool population = media && media->is_number() && media->as_number() >= 70 &&
        low && low->is_number() && low->as_number() >= 45 &&
        dividends && dividends->is_number() && dividends->as_number() >= 490 &&
        buybacks && buybacks->is_number() && buybacks->as_number() >= 12 &&
        high_dividend && high_dividend->is_number() && high_dividend->as_number() >= 3 &&
        hk && hk->is_number() && hk->as_number() >= 650 &&
        lending && lending->is_number() && lending->as_number() >= 290 &&
        soe && soe->is_number() && soe->as_number() >= 330 &&
        matched && matched->is_number();
    add_assertion(result, "eight_family_population", population,
                  "70 media, 45 low valuation, 490 dividend/fundraising, 12 buyback, "
                  "3 high dividend, 650 HK, 290 lending and 330 SOE rows", population);
    const bool reconciliation = population &&
        media->as_number() + low->as_number() + dividends->as_number() +
            buybacks->as_number() + high_dividend->as_number() + hk->as_number() +
            lending->as_number() + soe->as_number() == matched->as_number();
    add_assertion(result, "summary_reconciliation", reconciliation,
                  "eight family counts equal match_count", reconciliation);

    bool dividend_formula = false, buyback_formula = false;
    bool peg_formula = false, lending_units = false;
    bool hk_market_49 = false, soe_controller = false, media_projection = false;
    const auto* records = member(document, "records");
    if (records && records->is_array()) {
        for (const auto& row : records->as_array()) {
            const auto* raw = member(row, "raw");
            const bool auditable = raw && raw->is_object() &&
                member(row, "source_resource") &&
                member(row, "source_resource")->is_string();
            if (string_is(member(row, "kind"), "dividend-fundraising")) {
                const auto raw_dividend = auditable
                    ? numeric_value(member(*raw, "ljfh")) : std::nullopt;
                const auto raw_fundraising = auditable
                    ? numeric_value(member(*raw, "ljmj")) : std::nullopt;
                const auto ratio = numeric_value(member(row, "dividend_fundraising_ratio"));
                const auto yuan = numeric_value(member(row, "cumulative_dividend_yuan"));
                dividend_formula = dividend_formula || (raw_dividend && raw_fundraising &&
                    ratio && yuan && std::abs(*raw_fundraising) > 0.000001 &&
                    std::abs(*ratio - *raw_dividend / *raw_fundraising) < 0.000001 &&
                    std::abs(*yuan - *raw_dividend * 1e8) < 1.0);
            } else if (string_is(member(row, "kind"), "buyback-statistics")) {
                const auto raw_amount = auditable
                    ? numeric_value(member(*raw, "hgsz")) : std::nullopt;
                const auto raw_cap = auditable
                    ? numeric_value(member(*raw, "J_ZSZ")) : std::nullopt;
                const auto amount = numeric_value(member(row, "planned_buyback_yuan"));
                const auto ratio = numeric_value(member(row, "market_cap_ratio_pct"));
                buyback_formula = buyback_formula || (raw_amount && raw_cap && amount &&
                    ratio && std::abs(*raw_cap) > 0.000001 &&
                    std::abs(*amount - *raw_amount * 1e8) < 1.0 &&
                    std::abs(*ratio - *raw_amount * 1e10 / *raw_cap) < 0.000001);
            } else if (string_is(member(row, "kind"), "low-valuation-smallcap")) {
                const auto pe = auditable ? numeric_value(member(*raw, "PE")) : std::nullopt;
                const auto growth = auditable
                    ? numeric_value(member(*raw, "YCEPS")) : std::nullopt;
                const auto peg = numeric_value(member(row, "estimated_peg"));
                peg_formula = peg_formula || (pe && growth && peg &&
                    std::abs(*growth) > 0.000001 &&
                    std::abs(*peg - *pe / *growth) < 0.000001);
            } else if (string_is(member(row, "kind"), "high-refinancing-lending")) {
                const auto raw_balance = auditable
                    ? numeric_value(member(*raw, "zxye")) : std::nullopt;
                const auto balance = numeric_value(
                    member(row, "refinancing_lending_balance_yuan"));
                lending_units = lending_units || (raw_balance && balance &&
                    std::abs(*balance - *raw_balance * 1e4) < 0.01);
            } else if (string_is(member(row, "kind"), "hk-performance")) {
                const auto* security = member(row, "security");
                hk_market_49 = hk_market_49 || (security && security->is_object() &&
                    numeric_value(member(*security, "market_id")) ==
                        std::optional<double>(49));
            } else if (string_is(member(row, "kind"), "below-book-soe")) {
                const auto pb = numeric_value(member(row, "price_to_book_ratio"));
                const auto* controller = member(row, "controlling_shareholder");
                soe_controller = soe_controller || (pb && *pb < 1.0 && controller &&
                    controller->is_string() && !controller->as_string().empty());
            } else if (string_is(member(row, "kind"), "media-entertainment")) {
                media_projection = media_projection || (member(row, "title") &&
                    member(row, "title")->is_string() && member(row, "background_excerpt") &&
                    member(row, "background_excerpt")->is_string());
            }
        }
    }
    add_assertion(result, "client_formulas",
                  dividend_formula && buyback_formula && peg_formula,
                  "dividend/fundraising, buyback ratios and PE/YCEPS PEG", 
                  dividend_formula && buyback_formula && peg_formula);
    add_assertion(result, "unit_boundaries", lending_units,
                  "refinancing lending 万元 converted to yuan", lending_units);
    add_assertion(result, "hk_market_49", hk_market_49,
                  "Hong Kong fund/product market 49 retained", hk_market_49);
    add_assertion(result, "screen_semantics", soe_controller && media_projection,
                  "below-book SOE controller and media title/background retained",
                  soe_controller && media_projection);
    add_assertion(result, "historical_lending_boundary",
                  numeric_value(member_path(document,
                      {"summary", "valid_refinancing_lending_rows"})) ==
                          std::optional<double>(3) &&
                      string_is(member_path(document,
                          {"summary", "refinancing_lending_latest_date"}), "20240930"),
                  "3 nonblank rows; latest source date 20240930",
                  value_or_null(member_path(document,
                      {"summary", "refinancing_lending_latest_date"})));
    const std::vector<std::string> required_sources{
        "list/func_cmyl101_1.jsn", "list/func_dgzxz101.jsn",
        "list/func_fhmz101_1.jsn", "list/func_gfhgtj101_1.jsn",
        "list/func_gfhl101_1.jsn", "list/func_ggthq101_1.jsn",
        "list/func_ggzrt101_1.jsn", "list/func_gqpjg101_1.jsn"};
    bool exact_sources = true, local_first = true;
    const auto* sources = member(document, "sources");
    exact_sources = sources && sources->is_array() &&
        sources->size() == required_sources.size();
    for (const auto& source : required_sources)
        exact_sources = exact_sources && source_exists(document, source);
    if (sources && sources->is_array())
        for (const auto& source : sources->as_array()) {
            const auto* endpoint = member(source, "endpoint");
            local_first = local_first && endpoint && endpoint->is_string() &&
                endpoint->as_string().rfind("local-jsn:", 0) == 0;
        }
    else local_first = false;
    add_assertion(result, "exact_sources", exact_sources,
                  static_cast<std::uint64_t>(required_sources.size()), exact_sources);
    add_assertion(result, "local_first_boundary", local_first,
                  "normal API query reads the mirrored JSN files", local_first);
}

}  // namespace tdx::recon_contract_detail
