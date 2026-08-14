#include "recon_contract_market_data_bonds_internal.hpp"

#include "recon_contract_internal.hpp"

#include <cmath>

namespace tdx::recon_contract_detail {

void validate_convertible_bond_pricing(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-convertible-bonds-native-v1"),
                  "tdx-market-convertible-bonds-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view", string_is(member(document, "view"), "pricing"),
                  "pricing", value_or_null(member(document, "view")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    const auto* summary = member(document, "summary");
    const auto* active = summary ? member(*summary, "active_bonds") : nullptr;
    const auto* complete = summary ? member(*summary, "complete_valuations") : nullptr;
    add_assertion(result, "pricing_coverage",
                  active && active->is_number() && active->as_number() >= 300 &&
                      complete && complete->is_number() && complete->as_number() >= 1,
                  "at least 300 active terms and one selected complete valuation",
                  summary ? *summary : Json(nullptr));
    const auto* rows = member(document, "pricing");
    const Json* row = rows && rows->is_array() && rows->size() == 1
        ? &rows->as_array().front() : nullptr;
    const auto* bond = row ? member(*row, "bond") : nullptr;
    const auto* underlying = row ? member(*row, "underlying") : nullptr;
    add_assertion(result, "security_pair",
                  bond && string_is(member(*bond, "code"), "110076") &&
                      underlying && string_is(member(*underlying, "code"), "600521"),
                  "SH110076 -> SH600521", row ? *row : Json(nullptr));
    const auto* quote = row ? member(*row, "quote") : nullptr;
    const auto* valuation = row ? member(*row, "valuation") : nullptr;
    const auto* clean = quote ? member(*quote, "bond_last_price") : nullptr;
    const auto* full = valuation ? member(*valuation, "full_price") : nullptr;
    const auto* conversion = valuation ? member(*valuation, "conversion_value") : nullptr;
    const auto* premium = valuation ? member(*valuation, "conversion_premium_pct") : nullptr;
    const auto* ytm = valuation ? member(*valuation, "maturity_yield_pct") : nullptr;
    const auto* double_low = valuation ? member(*valuation, "double_low_score") : nullptr;
    add_assertion(result, "valuation_fields",
                  clean && clean->is_number() && full && full->is_number() &&
                      full->as_number() > clean->as_number() &&
                      conversion && conversion->is_number() && conversion->as_number() > 0 &&
                      premium && premium->is_number() && ytm && ytm->is_number() &&
                      double_low && double_low->is_number() &&
                      std::abs(double_low->as_number() - clean->as_number() -
                               premium->as_number()) < 0.000001 &&
                      string_is(member(*valuation, "accrued_interest_source"),
                                "coupon-schedule-derived-actual-365"),
                  "clean/full/conversion/premium/YTM/double-low with auditable accrued interest",
                  valuation ? *valuation : Json(nullptr));
    const auto* raw = row ? member(*row, "raw") : nullptr;
    add_assertion(result, "raw_row_retained", raw && raw->is_object(),
                  "object", value_or_null(raw));
    const auto* sources = member(document, "sources");
    const Json* source = sources && sources->is_array() && sources->size() == 1
        ? &sources->as_array().front() : nullptr;
    add_assertion(result, "pricing_source",
                  source && string_is(member(*source, "resource"),
                                      "list/gxjty_zq_kzzsy101_1.jsn"),
                  "list/gxjty_zq_kzzsy101_1.jsn", value_or_null(source));
}

}  // namespace tdx::recon_contract_detail
