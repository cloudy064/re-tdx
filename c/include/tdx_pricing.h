/* tdx_pricing.h - the convertible-bond pricing view.
 *
 * The row mapping, joined against LIVE QUOTES and valued with tdx_bond_math.  Three
 * groups come out of one row plus two quotes:
 *
 *   terms      what the bond is: face, conversion terms, coupon schedule, the three
 *              trigger ratios AND the trigger PRICES derived from them
 *              (conversion price * ratio / 100), and the two-point yield curve
 *   quote      the bond's and the underlying's price, change and amount, with a
 *              source for each price
 *   valuation  accrued interest, full price, conversion value, conversion premium,
 *              yield to maturity, pure bond value, pure bond premium, double-low score
 *
 * The quote side is an adapter, not a protocol: this layer takes a `tdx_quote_view`
 * that the caller fills from whatever its quote source is, so the same mapping works
 * against an 0x054C snapshot.  A price is "usable" when it is positive - last price
 * first, previous close as the fallback - and the view records WHICH one was used,
 * because a valuation built on a previous close is not the same claim as one built on
 * a live price.
 *
 * The arithmetic itself is in tdx_bond_math; this module only assembles its inputs and
 * decides what is present.  Every derived field is absent when its inputs are, and the
 * `availability` word says how far the row got: complete, bond-only, or terms-only.
 *
 * Text fields are zero-copy views into the JSN document, as elsewhere. */
#ifndef TDX_PRICING_H
#define TDX_PRICING_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bond_math.h"
#include "tdx_bonds.h"
#include "tdx_error.h"
#include "tdx_jsn.h"
#include "tdx_quote.h"
#include "tdx_snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_PRICING_RESOURCE "list/gxjty_zq_kzzsy101_1.jsn"
#define TDX_PRICING_ROWS_MAX 4096

typedef enum tdx_price_source {
    TDX_PRICE_UNAVAILABLE = 0,
    TDX_PRICE_LAST,
    TDX_PRICE_PRE_CLOSE
} tdx_price_source;

/* One side of the join: what a quote source says about a security.  The caller fills
 * it; the mapping never looks at a protocol. */
typedef struct tdx_quote_view {
    int available;
    int has_last_price;
    double last_price;
    int has_pre_close_price;
    double pre_close_price;
    int has_amount;
    double amount;
    int has_change_pct;
    double change_pct;
} tdx_quote_view;

/* The price the view contributes, and where it came from.  Returns 0 when neither a
 * positive last price nor a positive previous close is available. */
int tdx_pricing_quote_price(const tdx_quote_view *view, double *out,
                            tdx_price_source *source);

/* Fills a view from an 0x054C snapshot, deriving the change from the two prices the
 * snapshot carries because it does not carry a change itself. */
void tdx_pricing_quote_from_snapshot(const tdx_snapshot *snapshot, tdx_quote_view *out);

typedef struct tdx_pricing_row {
    /* Identity. */
    int bond_market_id;
    char bond_market[16];
    char bond_security_id[24];
    tdx_bond_text bond_code;
    tdx_bond_text bond_name;

    int has_underlying;
    int stock_market_id;
    char stock_market[16];
    char stock_security_id[24];
    tdx_bond_text stock_code;
    tdx_bond_text stock_name; /* ZGSM */

    /* "exchangeable-bond" when the type text contains the character for exchange. */
    int exchangeable;
    /* Non-empty maturity date that is not in the past. */
    int active;

    /* terms */
    int has_face_value;
    double face_value;
    int has_return_since_listing_pct;
    double return_since_listing_pct;
    int has_return_5d_pct;
    double return_5d_pct;
    int has_return_10d_pct;
    double return_10d_pct;
    int has_conversion_price;
    double conversion_price;
    tdx_bond_text conversion_start_date;
    tdx_bond_text conversion_end_date;
    tdx_bond_text listing_date;
    tdx_bond_text interest_start_date;
    tdx_bond_text maturity_date;
    int has_remaining_years;
    double remaining_years;
    tdx_bond_text bond_rating;
    tdx_bond_text issuer_rating;
    tdx_bond_text bond_type;
    tdx_bond_text rate_type;
    tdx_bond_text rate_type_code;
    tdx_bond_text payment_dates;
    tdx_bond_text payment_rates;
    int has_remaining_payment_count;
    double remaining_payment_count;
    tdx_bond_text remaining_payment_dates;
    tdx_bond_text remaining_payment_rates;
    tdx_bond_text previous_payment_date;
    tdx_bond_text next_payment_date;
    int has_payment_frequency_months;
    double payment_frequency_months;
    int has_remaining_balance_pct;
    double remaining_balance_pct;
    int has_revision_trigger_ratio_pct;
    double revision_trigger_ratio_pct;
    int has_sellback_trigger_ratio_pct;
    double sellback_trigger_ratio_pct;
    int has_redemption_trigger_ratio_pct;
    double redemption_trigger_ratio_pct;
    /* Derived: conversion price * ratio / 100. */
    int has_revision_trigger_price;
    double revision_trigger_price;
    int has_sellback_trigger_price;
    double sellback_trigger_price;
    int has_redemption_trigger_price;
    double redemption_trigger_price;
    int has_curve_short_years;
    double curve_short_years;
    int has_curve_short_yield_pct;
    double curve_short_yield_pct;
    int has_curve_long_years;
    double curve_long_years;
    int has_curve_long_yield_pct;
    double curve_long_yield_pct;
    /* SYNXSYL as a fraction; reported as a percentage. */
    int has_curve_yield;
    double curve_yield;

    /* quote */
    tdx_quote_view bond_quote;
    tdx_quote_view stock_quote;
    /* A bond price above ten times its face value cannot be a bond price, and the
     * valuation built on it is meaningless.  The numbers are still reported; this says
     * not to trust them.  Measured cause: the wire price for the 132xxx exchangeable
     * bonds is 100 times par, because the reference's divisor table has no "13" rule
     * other than "1318". */
    int price_plausible;

    /* valuation */
    tdx_bond_text as_of_date;
    int has_accrued_interest;
    double accrued_interest;
    int has_full_price;
    double full_price;
    int has_conversion_value;
    double conversion_value;
    int has_conversion_premium_pct;
    double conversion_premium_pct;
    int has_maturity_yield_pct;
    double maturity_yield_pct;
    int has_pure_bond_value;
    double pure_bond_value;
    int has_pure_bond_premium_pct;
    double pure_bond_premium_pct;
    int has_double_low_score;
    double double_low_score;
    size_t cash_flow_count;
    /* "complete", "bond-only" or "terms-only". */
    const char *availability;
} tdx_pricing_row;

/* The distinct securities a pricing document needs quotes for: the bonds and, where
 * present, their underlyings.  A caller fetches quotes for exactly this list, which is
 * why it is collected in one pass rather than by walking the rows twice. */
int tdx_pricing_collect_codes(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                              tdx_code *out, size_t capacity, size_t *out_count,
                              size_t *skipped_count, tdx_error *err);

/* Maps one document against a quote lookup.  quotes are searched by market and code;
 * a NULL list means every price is unavailable, which is a legitimate way to ask for
 * the terms alone. */
int tdx_pricing_normalize(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                          const tdx_snapshot *quotes, size_t quote_count, const char *as_of_date,
                          size_t as_of_length, tdx_pricing_row *out, size_t capacity,
                          size_t *out_count, size_t *skipped_count, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_PRICING_H */
