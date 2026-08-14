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

void validate_market_etf_share_ranking_contract(
    const Json& document, Json& result) {
    add_assertion(result, "schema_view",
                  string_is(member(document, "schema"),
                            "tdx-market-exchange-funds-native-v1") &&
                      string_is(member(document, "view"), "etf-share-ranking") &&
                      string_is(member(document, "mode"), "catalog"),
                  "exchange-funds-v1/etf-share-ranking/catalog", true);
    const auto* matched = member(document, "match_count");
    const auto* count = member_path(document, {"summary", "etf_share_ranking"});
    const auto* nonzero = member_path(
        document, {"summary", "etf_share_ranking_nonzero_net_inflow"});
    const bool population = matched && matched->is_number() &&
        matched->as_number() >= 450 && count && count->is_number() &&
        count->as_number() == matched->as_number() && nonzero && nonzero->is_number();
    add_assertion(result, "population", population,
                  ">=450 rows and summary reconciliation", population);
    add_assertion(result, "offline_quote_boundary",
                  member(document, "quote_source") &&
                      member(document, "quote_source")->is_null(),
                  "include_quotes=0", value_or_null(member(document, "quote_source")));
    bool typed = false;
    const auto* records = member(document, "records");
    if (records && records->is_array()) {
        for (const auto& row : records->as_array()) {
            const auto* raw = member(row, "raw");
            const auto* unit_10k = member(row, "subscription_unit_10k_shares");
            const auto* unit = member(row, "subscription_unit_shares");
            const auto* shares = member(row, "latest_shares");
            const auto* reference = member(row, "reference_instrument");
            if (!raw || !raw->is_object() || !unit_10k || !unit_10k->is_number() ||
                !unit || !unit->is_number() || !shares || !shares->is_number() ||
                !reference || !reference->is_object()) continue;
            typed = std::abs(unit->as_number() - unit_10k->as_number() * 10000.0) < 0.001 &&
                member(row, "net_inflow_yuan") &&
                member(row, "prior_week_scale_yuan") &&
                member(row, "prior_month_scale_yuan") &&
                string_is(member(row, "source_resource"),
                          "list/func_tlfeyxetf101_1.jsn");
            if (typed) break;
        }
    }
    add_assertion(result, "typed_units_and_reference", typed,
                  "raw 10k-share unit converted once; flow, scale baselines and index retained",
                  typed);
    add_assertion(result, "exact_source",
                  source_exists(document, "list/func_tlfeyxetf101_1.jsn"),
                  "list/func_tlfeyxetf101_1.jsn",
                  source_exists(document, "list/func_tlfeyxetf101_1.jsn"));
}

}  // namespace tdx::recon_contract_detail
