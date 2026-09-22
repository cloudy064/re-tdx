/* tdx_newbond.h - the new-bond projection and its reconciliation with subscriptions.
 *
 * A second, independent list of upcoming convertible bonds, matched against the
 * subscription list so that the two can be compared rather than one trusted:
 *
 *   primary       list/func_kkzss101_1.jsn     the subscription events
 *   projection    list/gxjty_zq_xkzz102_1.jsn  the projection, 13 rows when captured
 *
 * The match prefers the SUBSCRIPTION CODE and falls back to the UNDERLYING security,
 * which is the reference's rule and the reason both keys are carried on every row.
 * Whether a row matched by code or by falling back is reported, because a fallback
 * match is weaker evidence than a code match and a caller needs to know which it got.
 *
 * Two normalisation details are load-bearing, and getting either wrong turns a clean
 * reconciliation into a table of false alarms:
 *
 *   * the projection's dates are FORMATTED - "2026-06-26" followed by the weekday in
 *     Chinese - while the subscription's are compact.  The projection's are therefore
 *     compacted before comparison, which is what the reference does.  Comparing the raw
 *     texts reports a mismatch on EVERY row: measured, 13 of 13, which is a defect in
 *     the comparison and not news about the bonds.
 *   * the projection's SIZES are in 100m yuan and so are the subscription's, but the
 *     subscription's size column is a different one; the delta is reported in 100m yuan
 *     and a difference counts only beyond 0.01, which is 1m yuan.
 *
 * Measured live: 13 of 13 projection rows match by subscription code, none by fallback
 * and none unmatched; no date differs once compacted; and TWO rows disagree about the
 * issue size, by +9.80 and -4.22 hundred million yuan.  So the reconciliation is not
 * vacuous - it is the two disagreements that are the point of it. */
#ifndef TDX_NEWBOND_H
#define TDX_NEWBOND_H

#include <stddef.h>

#include "tdx_bonds.h"
#include "tdx_error.h"
#include "tdx_jsn.h"
#include "tdx_subscription.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_NEWBOND_PROJECTION_RESOURCE "list/gxjty_zq_xkzz102_1.jsn"
#define TDX_NEWBOND_ROWS_MAX 1024
/* Room for a market prefix plus a six-digit code, as everywhere else in the project. */
#define TDX_NEWBOND_CODE_MAX 24

typedef struct tdx_newbond_row {
    /* sgdm: the subscription code, which is this list's preferred key. */
    tdx_bond_text subscription_code;
    tdx_bond_text subscription_name;
    /* sgrq, raw and compacted; the compacted form is what can be compared. */
    tdx_bond_text subscription_date_text;
    char subscription_date[12];
    tdx_bond_text issue_date_text;
    char issue_date[12];
    int has_issue_price_yuan;
    double issue_price_yuan;
    tdx_bond_text issue_type;
    int has_issue_size_100m_yuan;
    double issue_size_100m_yuan;
    int has_stock_rights_yuan;
    double stock_rights_yuan;
    int has_conversion_price_yuan;
    double conversion_price_yuan;
    tdx_bond_text conversion_available;
    tdx_bond_text conversion_start_date;
    tdx_bond_text conversion_end_date;
    int has_shareholder_placement_ratio;
    double shareholder_placement_ratio;
    tdx_bond_text lottery_date;
    int has_lottery_rate_pct;
    double lottery_rate_pct;
    tdx_bond_text region;
    tdx_bond_text plan_progress;
    tdx_bond_text progress_date;

    /* $ZQDM/$SC: the underlying, this list's fallback key. */
    int has_underlying;
    int stock_market_id;
    char stock_security_id[TDX_NEWBOND_CODE_MAX];
    tdx_bond_text stock_code;
} tdx_newbond_row;

/* The reference's compact_date_prefix: the digits, stopping at eight. */
int tdx_newbond_compact_date(const char *text, size_t length, char *out, size_t capacity);

int tdx_newbond_normalize(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                          tdx_newbond_row *out, size_t capacity, size_t *out_count,
                          size_t *skipped_count, tdx_error *err);

/* --- the reconciliation ----------------------------------------------- */

typedef struct tdx_newbond_match {
    int matched;
    /* "subscription-code", "underlying-only" or "none". */
    const char *match_method;
    /* The subscription row it matched, or NULL. */
    const tdx_subscription_row *primary;
    int date_mismatch;
    int size_mismatch;
    int has_size_delta;
    double size_delta_100m_yuan;
} tdx_newbond_match;

typedef struct tdx_newbond_reconciliation {
    size_t primary_rows;
    size_t projection_rows;
    size_t exact_code_matches;
    size_t underlying_only_matches;
    size_t unmatched_projection_rows;
    size_t issue_size_mismatch_count;
    size_t subscription_date_mismatch_count;
    size_t hybrid_or_stale_count;
    /* True when every projection row matched by code and nothing disagreed. */
    int exact_projection;
    /* One per projection row, in the projection's order. */
    tdx_newbond_match *matches;
    size_t match_count;
    /* The subscription codes of the unmatched rows, in the projection's order. */
    char (*unmatched_codes)[TDX_NEWBOND_CODE_MAX];
    size_t unmatched_code_count;
} tdx_newbond_reconciliation;

int tdx_newbond_reconcile(const tdx_subscription_row *primary, size_t primary_count,
                          const tdx_newbond_row *projection, size_t projection_count,
                          tdx_newbond_reconciliation *out, tdx_error *err);
void tdx_newbond_reconciliation_free(tdx_newbond_reconciliation *reconciliation);

#ifdef __cplusplus
}
#endif

#endif /* TDX_NEWBOND_H */
