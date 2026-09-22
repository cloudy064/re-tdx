/* tdx_convertible_join.h - the convertible-bond six-document join.
 *
 * The reference builds its convertible-bond view by joining six documents keyed on
 * the bond's market and code, then attaching three trigger documents:
 *
 *   overview   list/kzz_kzzsy201_1.jsn   -> tdx_convertible.h
 *   progress   list/func_kzz_tkjd201.jsn -> issue/remaining/conversion progress
 *   coupons    list/func_kzz_lltk201.jsn -> term, six annual rates, compensation
 *   sellback   list/func_kzz_hstk201.jsn -> the sellback trigger document
 *   redemption list/func_kzz_shtk201.jsn -> the redemption trigger document
 *   revision   list/func_kzz_xztk201.jsn -> the revision trigger document
 *
 * A bond appears in the view if it appears in ANY of them, which the reference
 * implements by unioning the keys and then picking each field from the document that
 * carries it.  That union matters: a bond whose overview row is missing still shows
 * up through its progress row, with the overview fields reported absent rather than
 * the whole bond disappearing.
 *
 * The lookup is a linear scan per bond.  314 bonds against 320 rows is about a
 * hundred thousand comparisons per document, which costs nothing next to the
 * transfers that produced the rows, and it keeps the join to the one rule the
 * reference states - same market, same code - with no hashing to get subtly wrong.
 *
 * The exchangeable-bond projections the reference adds on top are NOT part of this
 * join: they are two further documents with their own precedence rules, and they are
 * not ported. */
#ifndef TDX_CONVERTIBLE_JOIN_H
#define TDX_CONVERTIBLE_JOIN_H

#include <stddef.h>

#include "tdx_bonds.h"
#include "tdx_convertible.h"
#include "tdx_error.h"
#include "tdx_jsn.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The union of the six documents names a few hundred bonds today; this is the cap
 * the decoder and the CLI are sized for, and a union larger than it is refused
 * rather than truncated. */
#define TDX_CONVERTIBLE_KEYS_MAX 8192

#define TDX_CONVERTIBLE_PROGRESS_RESOURCE "list/func_kzz_tkjd201.jsn"
#define TDX_CONVERTIBLE_COUPONS_RESOURCE "list/func_kzz_lltk201.jsn"
#define TDX_CONVERTIBLE_SELLBACK_RESOURCE "list/func_kzz_hstk201.jsn"
#define TDX_CONVERTIBLE_REDEMPTION_RESOURCE "list/func_kzz_shtk201.jsn"
#define TDX_CONVERTIBLE_REVISION_RESOURCE "list/func_kzz_xztk201.jsn"

/* The six documents.  Any of them may be NULL, in which case its fields are
 * reported absent and the join simply carries less. */
typedef struct tdx_convertible_documents {
    const tdx_jsn_document *overview;
    const tdx_jsn_document *progress;
    const tdx_jsn_document *coupons;
    const tdx_jsn_document *sellback;
    const tdx_jsn_document *redemption;
    const tdx_jsn_document *revision;
} tdx_convertible_documents;

/* Which of the six carried the bond.  A joined row that came only from a non-overview
 * document has no overview fields, and a caller needs to know that. */
typedef struct tdx_convertible_join_flags {
    int from_overview;
    int from_progress;
    int from_coupons;
    int from_sellback;
    int from_redemption;
    int from_revision;
} tdx_convertible_join_flags;

/* The trigger document the reference attaches three times: for sellback, redemption
 * and revision, with different column names. */
typedef struct tdx_convertible_trigger {
    tdx_bond_text condition;      /* CFSJTJ */
    int has_price_ratio_pct;
    double price_ratio_pct;       /* CFJGBL */
    tdx_bond_text start_date;     /* HSQSRQ / SHQSRQ / XZQSRQ */
    int has_trigger_price;
    double trigger_price;         /* HSJG / SHJG / CFJG */
    int has_conversion_price;
    double conversion_price;      /* ZGJG */
    tdx_bond_text current_days;   /* CFJD */
    int has_current_ratio_pct;
    double current_ratio_pct;     /* CFJDBL */
    tdx_bond_text status;         /* CFQK */
    int has_history_count;
    double history_count;         /* YCFCS / ZGJTZCS */
    tdx_bond_text history_dates;  /* YHSRQ / YSHRQ / YXZRQ, a comma list */
    int has_available_days;
    double available_days;        /* HSCFSYTS / SHCFSYTS / XZCFSYTS */
} tdx_convertible_trigger;

/* The progress and coupon groups, which the join adds to tdx_convertible_row. */
typedef struct tdx_convertible_extra {
    int has_issue_size_100m_yuan;
    double issue_size_100m_yuan;              /* FXZL */
    int has_remaining_balance_100m_yuan;
    double remaining_balance_100m_yuan;       /* ZQYE */
    int has_conversion_progress_pct;
    double conversion_progress_pct;           /* ZGJD */
    int has_redeemed_amount_100m_yuan;
    double redeemed_amount_100m_yuan;         /* YSHME */
    int has_sellback_amount_100m_yuan;
    double sellback_amount_100m_yuan;         /* YHSME */
    int has_maturity_progress_pct;
    double maturity_progress_pct;             /* DQJD */

    int has_term_years;
    double term_years;                        /* FXQX */
    int has_rate[6];
    double rates_pct[6];                      /* PMLL_1 .. PMLL_6 */
    int has_compensation_rate_pct;
    double compensation_rate_pct;             /* BCLL */

    /* The two comma lists the overview also carries; taken from the overview when
     * it has them and left absent otherwise, exactly as the reference does. */
    tdx_bond_text payment_dates;              /* FXRQXL */
    tdx_bond_text payment_rates;              /* FXLLXL */

    tdx_convertible_trigger sellback;
    tdx_convertible_trigger redemption;
    tdx_convertible_trigger revision;
} tdx_convertible_extra;

/* Joins one bond.  identity is the bond to look up; the overview row may be absent
 * and the bond will still be found through the other documents. */
int tdx_convertible_join(const tdx_convertible_documents *documents, const tdx_code *identity,
                         tdx_convertible_row *out, tdx_convertible_extra *extra,
                         tdx_convertible_join_flags *flags, tdx_error *err);

/* The union of the keys of every document that is present, in ascending
 * (market, code) order.  Fills out with up to capacity entries and reports how many
 * the union actually holds, which is what a caller walks. */
int tdx_convertible_keys(const tdx_convertible_documents *documents, tdx_code *out,
                         size_t capacity, size_t *out_count, size_t *union_count,
                         tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_CONVERTIBLE_JOIN_H */
