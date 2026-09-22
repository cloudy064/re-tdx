/* tdx_convertible.h - the convertible-bond overview document.
 *
 * The reference builds a convertible-bond view by JOINING six documents - overview,
 * progress, coupons, sellback, redemption and revision - keyed on the bond's
 * market and code, plus up to two exchangeable-bond projections, and then attaches
 * three trigger documents.
 *
 * THIS MODULE IS ONLY THE OVERVIEW.  That is a deliberate slice rather than an
 * unfinished join: the overview is where the field knowledge lives (twenty-odd
 * columns of terms, ratings, dates and ratios), it is a single resource with no
 * join semantics, and it can be verified end to end on its own.  The join, the
 * trigger documents, the coupon-rate array and the projection precedence are NOT
 * ported here, and nothing in this file pretends otherwise.
 *
 * The one piece of join semantics that does show up - a projection filling fields
 * the overview leaves empty - is therefore absent: every field below is read from
 * the overview resource alone, and a field the resource does not carry is reported
 * as absent rather than fetched from somewhere else.
 *
 * The resource: bi/list/kzz_kzzsy201_1.jsn, 314 rows of 56 columns when captured.
 * The reference's catalog names six core documents plus projections:
 *
 *   list/kzz_kzzsy201_1.jsn      overview        <- this module
 *   list/func_kzz_tkjd201.jsn    progress
 *   list/func_kzz_lltk201.jsn    coupon rates
 *   list/func_kzz_hstk201.jsn    sellback terms
 *   list/func_kzz_shtk201.jsn    redemption terms
 *   list/func_kzz_xztk201.jsn    revision terms
 *   list/kjhz_kjhzsy201_1.jsn    exchangeable bonds
 *   list/func_kzz103_1.jsn       exchangeable projection
 *   list/dfkzz201_1.jsn          pending issues
 *   list/func_kzz102_1.jsn       pending projection
 *   list/gxjty_zq_dfkzz102_1.jsn pending projection
 *   list/func_kkzss101_1.jsn     subscriptions
 *   list/gxjty_zq_xkzz102_1.jsn  new-bond projection
 *   list/gxjty_zq_kzzsy101_1.jsn pricing
 *
 * The instrument type comes from the code, not from a column: a bond whose code
 * starts with 132 is an exchangeable bond and anything else is a convertible one.
 * That is the reference's rule and it is reproduced rather than guessed at.
 *
 * Text fields are zero-copy views into the JSN document, as in tdx_bonds.h, so the
 * document must outlive the row. */
#ifndef TDX_CONVERTIBLE_H
#define TDX_CONVERTIBLE_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bonds.h"
#include "tdx_error.h"
#include "tdx_jsn.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The overview resource this module reads. */
#define TDX_CONVERTIBLE_OVERVIEW_RESOURCE "list/kzz_kzzsy201_1.jsn"

typedef enum tdx_convertible_kind {
    TDX_CONVERTIBLE_BOND = 0,
    TDX_CONVERTIBLE_EXCHANGEABLE
} tdx_convertible_kind;

typedef struct tdx_convertible_row {
    /* Identity, built here. */
    int bond_market_id;
    char bond_market[16];
    char bond_security_id[24];
    tdx_bond_text bond_code;
    tdx_bond_text bond_name;

    tdx_convertible_kind kind;

    /* The underlying equity, when the row names one. */
    int has_underlying;
    int underlying_market_id;
    char underlying_market[16];
    char underlying_security_id[24];
    tdx_bond_text underlying_code;
    /* The ZGDM column.  The reference binds it to the underlying's NAME, but
     * measured across all 314 live rows it is never a name: 217 are six-digit
     * codes and 97 are empty.  It is kept under what it actually holds. */
    tdx_bond_text underlying_reference_code;

    /* Overview terms. */
    tdx_bond_text risk_notice;
    tdx_bond_text listing_date;
    tdx_bond_text issue_date;
    tdx_bond_text conversion_start_date;
    tdx_bond_text conversion_end_date;
    tdx_bond_text maturity_date;
    tdx_bond_text bond_rating;
    tdx_bond_text issuer_rating;
    tdx_bond_text current_state;

    /* Numbers.  has_* separates "absent" from zero. */
    int has_face_value;
    double face_value;
    int has_issue_price;
    double issue_price;
    int has_return_since_listing_pct;
    double return_since_listing_pct;
    int has_return_5d_pct;
    double return_5d_pct;
    int has_return_10d_pct;
    double return_10d_pct;
    int has_issue_size_100m_yuan;
    double issue_size_100m_yuan;
    int has_remaining_balance_100m_yuan;
    double remaining_balance_100m_yuan;
    int has_remaining_ratio_pct;
    double remaining_ratio_pct;
    int has_conversion_price;
    double conversion_price;
    int has_remaining_years;
    double remaining_years;
    int has_maturity_redemption_price;
    double maturity_redemption_price;
    int has_unpaid_coupon_sum;
    double unpaid_coupon_sum;
    int has_sellback_trigger_ratio_pct;
    double sellback_trigger_ratio_pct;
    int has_redemption_trigger_ratio_pct;
    double redemption_trigger_ratio_pct;

    /* The reference's own completeness test: face value, conversion price and a
     * maturity date are the three terms without which the row is not usable. */
    int core_terms_complete;
} tdx_convertible_row;

/* The instrument kind the reference derives from a bond code. */
tdx_convertible_kind tdx_convertible_kind_of(const char *code, size_t length);

/* Maps one overview row.  Fails when the row has no usable identity, as the
 * reference does. */
int tdx_convertible_normalize(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                              size_t row_in_group, tdx_convertible_row *out, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_CONVERTIBLE_H */
