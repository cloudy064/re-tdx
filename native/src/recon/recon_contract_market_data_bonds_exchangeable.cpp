#include "recon_contract_market_data_bonds_internal.hpp"

#include "recon_contract_internal.hpp"

#include <cmath>
#include <cstdint>

namespace tdx::recon_contract_detail {

void validate_exchangeable_bond_supplement(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-convertible-bonds-native-v1"),
                  "tdx-market-convertible-bonds-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view", string_is(member(document, "view"), "listed"),
                  "listed", value_or_null(member(document, "view")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    const auto* summary = member(document, "summary");
    const auto* exchangeable = summary ? member(*summary, "exchangeable_bonds") : nullptr;
    const auto* supplemented = summary ? member(*summary, "exchangeable_bonds_supplemented") : nullptr;
    const auto* bonds = summary ? member(*summary, "bonds") : nullptr;
    const auto* complete = summary ? member(*summary, "core_terms_complete") : nullptr;
    const auto* missing = summary ? member(*summary, "core_terms_missing") : nullptr;
    add_assertion(result, "exchangeable_coverage",
                  exchangeable && exchangeable->is_number() && exchangeable->as_number() >= 2 &&
                      supplemented && supplemented->is_number() && supplemented->as_number() >= 2,
                  "at least two exchangeable bonds, both supplemented",
                  summary ? *summary : Json(nullptr));
    add_assertion(result, "catalog_core_terms",
                  bonds && bonds->is_number() && bonds->as_number() >= 300 &&
                      complete && complete->is_number() &&
                      missing && missing->is_number() && missing->as_number() <= 5 &&
                      std::abs(complete->as_number() + missing->as_number() -
                               bonds->as_number()) < 0.5 &&
                      complete->as_number() / bonds->as_number() >= 0.98,
                  "300+ catalog, exact complete/missing reconciliation and at least 98% complete",
                  summary ? *summary : Json(nullptr));
    const auto* bond_rows = member(document, "bonds");
    const Json* row = bond_rows && bond_rows->is_array() && bond_rows->size() == 1
        ? &bond_rows->as_array().front() : nullptr;
    const auto* bond = row ? member(*row, "bond") : nullptr;
    const auto* underlying = row ? member(*row, "underlying") : nullptr;
    const auto* overview = row ? member(*row, "overview") : nullptr;
    add_assertion(result, "exchangeable_identity",
                  row && string_is(member(*row, "instrument_type"), "exchangeable-bond") &&
                      bool_is(member(*row, "exchangeable_supplemented"), true) &&
                      bool_is(member(*row, "exchangeable_projection_verified"), true) &&
                      bond && string_is(member(*bond, "code"), "132024") &&
                      underlying && string_is(member(*underlying, "code"), "600362"),
                  "SH132024 exchangeable bond -> SH600362",
                  row ? *row : Json(nullptr));
    const auto* face = overview ? member(*overview, "face_value") : nullptr;
    const auto* conversion = overview ? member(*overview, "conversion_price") : nullptr;
    const auto* maturity_redemption = overview
        ? member(*overview, "maturity_redemption_price") : nullptr;
    const auto* unpaid_coupon_sum = overview
        ? member(*overview, "unpaid_coupon_sum") : nullptr;
    add_assertion(result, "restored_core_terms",
                  overview && bool_is(member(*overview, "core_terms_complete"), true) &&
                      face && face->is_number() && std::abs(face->as_number() - 100.0) < 0.000001 &&
                      conversion && conversion->is_number() &&
                          std::abs(conversion->as_number() - 52.4) < 0.000001 &&
                      string_is(member(*overview, "maturity_date"), "20310409") &&
                      string_is(member(*overview, "issuer_rating"), "AAA") &&
                      string_is(member(*overview, "source_resource"),
                                "list/kjhz_kjhzsy201_1.jsn") &&
                      string_is(member(*overview, "projection_resource"),
                                "list/func_kzz103_1.jsn") &&
                      maturity_redemption && maturity_redemption->is_number() &&
                          std::abs(maturity_redemption->as_number() - 105.0) < 0.000001 &&
                      unpaid_coupon_sum && unpaid_coupon_sum->is_number() &&
                          std::abs(unpaid_coupon_sum->as_number() - 0.04) < 0.000001,
                  "core supplement plus DQSHJ=105 and LLZH=0.04 projection",
                  overview ? *overview : Json(nullptr));
    const auto* errors = member(document, "master_errors");
    add_assertion(result, "supplement_errors",
                  errors && errors->is_array() && errors->size() == 0,
                  "empty array", value_or_null(errors));
    const auto* sources = member(document, "sources");
    bool supplement_source = false, projection_source = false;
    if (sources && sources->is_array()) {
        for (const auto& source : sources->as_array())
            if (string_is(member(source, "resource"), "list/kjhz_kjhzsy201_1.jsn"))
                supplement_source = true;
            else if (string_is(member(source, "resource"), "list/func_kzz103_1.jsn"))
                projection_source = true;
    }
    add_assertion(result, "exchangeable_source",
                  sources && sources->is_array() && sources->size() == 8 &&
                      supplement_source && projection_source,
                  "eight sources including supplement and second projection",
                  sources ? Json(static_cast<std::uint64_t>(sources->size())) : Json(nullptr));
    const auto* reconciliation = member(document, "exchangeable_projection_reconciliation");
    const bool projection_exact = reconciliation && reconciliation->is_object() &&
        bool_is(member(*reconciliation, "exact_security_set"), true) &&
        numeric_value(member(*reconciliation, "common_securities")).value_or(0) >= 2;
    add_assertion(result, "exchangeable_projection_reconciliation", projection_exact,
                  "two exchangeable bonds exactly covered", projection_exact);
}

}  // namespace tdx::recon_contract_detail
