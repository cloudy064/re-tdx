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

void validate_market_fund_statistics_contract(
    const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-fund-statistics-native-v1"),
                  "tdx-market-fund-statistics-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view_mode",
                  string_is(member(document, "view"), "all") &&
                      string_is(member(document, "mode"), "catalog") &&
                      string_is(member(document, "availability"), "live"),
                  "all/catalog/live", value_or_null(member(document, "mode")));
    const std::vector<std::pair<std::string, double>> expected{
        {"new_funds", 200}, {"fund_dividends", 350},
        {"equity_fund_performance", 2700}, {"fund_market_size", 20},
        {"fund_market_size_chart", 20}, {"etf_market_size", 20},
        {"etf_subscription_chart", 20}, {"etf_weekly", 50},
        {"listed_funds", 120}};
    bool population = true;
    double total = 0;
    for (const auto& [name, minimum] : expected) {
        const auto* value = member_path(document, {"summary", name});
        population = population && value && value->is_number() &&
            value->as_number() >= minimum;
        if (value && value->is_number()) total += value->as_number();
    }
    add_assertion(result, "nine_family_population", population,
                  "nine rolling fund families retain substantial populations",
                  population);
    const auto* matched = member(document, "match_count");
    const bool reconciliation = matched && matched->is_number() &&
        total == matched->as_number();
    add_assertion(result, "summary_reconciliation", reconciliation,
                  "nine family counts equal match_count", reconciliation);

    bool otc_market = false, fund_units = false, etf_units = false;
    bool weekly_units = false, listing_units = false, raw_audit = true;
    const auto* records = member(document, "records");
    if (records && records->is_array()) {
        for (const auto& row : records->as_array()) {
            const auto* raw = member(row, "raw");
            raw_audit = raw_audit && raw && raw->is_object() &&
                member(row, "source_resource") &&
                member(row, "source_resource")->is_string();
            if (!raw || !raw->is_object()) continue;
            if (string_is(member(row, "kind"), "new-funds")) {
                const auto* security = member(row, "security");
                otc_market = otc_market || (security && security->is_object() &&
                    numeric_value(member(*security, "market_id")) ==
                        std::optional<double>(33) &&
                    string_is(member(*security, "market"), "fund"));
            } else if (string_is(member(row, "kind"), "fund-market-size")) {
                const auto source = numeric_value(member(*raw, "qbzc"));
                const auto normalized = numeric_value(member(row, "all_fund_nav_yuan"));
                fund_units = fund_units || (source && normalized &&
                    std::abs(*normalized - *source * 1e8) < 0.1);
            } else if (string_is(member(row, "kind"), "etf-market-size")) {
                const auto source = numeric_value(member(*raw, "hjss"));
                const auto normalized = numeric_value(
                    member(row, "total_net_subscription_units"));
                etf_units = etf_units || (source && normalized &&
                    std::abs(*normalized - *source * 1e8) < 0.1);
            } else if (string_is(member(row, "kind"), "etf-weekly")) {
                const auto source = numeric_value(member(*raw, "BZCJE"));
                const auto normalized = numeric_value(member(row, "turnover_yuan"));
                weekly_units = weekly_units || (source && normalized &&
                    std::abs(*normalized - *source * 1e8) < 0.1);
            } else if (string_is(member(row, "kind"), "listed-funds")) {
                const auto source = numeric_value(member(*raw, "mjfe"));
                const auto normalized = numeric_value(member(row, "raised_units"));
                listing_units = listing_units || (source && normalized &&
                    std::abs(*normalized - *source * 1e8) < 0.1);
            }
        }
    } else raw_audit = false;
    add_assertion(result, "otc_market_identity", otc_market,
                  "source market 33 remains fund/FUND rather than A-share", otc_market);
    add_assertion(result, "hundred_million_units",
                  fund_units && etf_units && weekly_units && listing_units,
                  "亿 fields convert to yuan or units with 1e8 scale",
                  fund_units && etf_units && weekly_units && listing_units);
    add_assertion(result, "raw_audit", raw_audit,
                  "all normalized rows retain source resource and raw fields", raw_audit);

    const std::vector<std::string> required_sources{
        "list/func_jjtj101_1.jsn", "list/func_jjtj102_1.jsn",
        "list/func_jjtj103_1.jsn", "list/func_jjtj104_1.jsn",
        "list/func_jjtj104_2.jsn", "list/func_jjtj105_1.jsn",
        "list/func_jjtj105_2.jsn", "list/func_jjtj108_1.jsn",
        "list/func_jjtj109_1.jsn"};
    const auto* sources = member(document, "sources");
    bool exact_sources = sources && sources->is_array() &&
        sources->size() == required_sources.size();
    bool local_first = exact_sources;
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
