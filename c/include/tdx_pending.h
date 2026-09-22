/* tdx_pending.h - the pending convertible-bond issue list.
 *
 * The plan list: convertible bonds that have been announced but not yet listed.  A
 * pending bond has no bond code yet, so the identity here is the UNDERLYING STOCK that
 * the bond will convert into - which is also the key the reference reconciles the
 * projections on.
 *
 * Three resources, and the two projections are NOT field-level fallbacks:
 *
 *   primary      list/dfkzz201_1.jsn              153 rows when captured
 *   projection   list/func_kzz102_1.jsn           145 rows
 *   projection   list/gxjty_zq_dfkzz102_1.jsn     154 rows
 *
 * The reference normalises all three with the SAME mapping and then, for each
 * projection, reports a SET reconciliation against the primary: which underlying
 * securities appear in both, which only in the primary and which only in the
 * projection.  So a projection here answers "does this other view agree about who is
 * planning an issue", not "what value should fill this empty field".  That is a
 * different question from the one the exchangeable-bond projection answers, and
 * treating the two the same way would produce a report nobody asked for.
 *
 * The columns are LOWER CASE, unlike every other JSN resource in this project
 * (zzlx, mzgm, fadj, date0, byhq, zgj, gdpsl, sgrq, fxrq, zql, zqr, sgdm, sgmc,
 * fxjg), and a row whose code or market cannot be read is SKIPPED rather than fatal -
 * unlike the listed view, where an unreadable identity is an error.  The difference is
 * deliberate: this list is a plan, and a plan row that cannot be attached to a stock
 * is simply not useful, whereas a listed bond that cannot be identified means the
 * document is wrong. */
#ifndef TDX_PENDING_H
#define TDX_PENDING_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bonds.h"
#include "tdx_error.h"
#include "tdx_jsn.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_PENDING_PRIMARY_RESOURCE "list/dfkzz201_1.jsn"
#define TDX_PENDING_PROJECTION_A_RESOURCE "list/func_kzz102_1.jsn"
#define TDX_PENDING_PROJECTION_B_RESOURCE "list/gxjty_zq_dfkzz102_1.jsn"

/* A bound on how many plan rows a caller holds; the live list is a few hundred. */
#define TDX_PENDING_ROWS_MAX 4096

typedef struct tdx_pending_row {
    /* The underlying stock, which is this list's identity. */
    int market_id;
    char market[16];
    char security_id[24];
    tdx_bond_text code;

    tdx_bond_text issue_type;        /* zzlx */
    int has_planned_size_100m_yuan;
    double planned_size_100m_yuan;   /* mzgm */
    tdx_bond_text plan_progress;     /* fadj */
    tdx_bond_text progress_date;     /* date0 */
    int has_stock_rights_yuan;
    double stock_rights_yuan;        /* byhq */
    int has_conversion_price_yuan;
    double conversion_price_yuan;    /* zgj */
    int has_shareholder_placement_ratio;
    double shareholder_placement_ratio; /* gdpsl */
    tdx_bond_text subscription_date; /* sgrq */
    tdx_bond_text issue_date;        /* fxrq */
    int has_lottery_rate;
    double lottery_rate;             /* zql */
    tdx_bond_text lottery_date;      /* zqr */
    tdx_bond_text subscription_code; /* sgdm */
    tdx_bond_text subscription_name; /* sgmc */
    int has_issue_price_yuan;
    double issue_price_yuan;         /* fxjg */
} tdx_pending_row;

/* Maps one document.  Rows whose code or market cannot be read are skipped, and
 * skipped_count reports how many. */
int tdx_pending_normalize(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                          tdx_pending_row *out, size_t capacity, size_t *out_count,
                          size_t *skipped_count, tdx_error *err);

/* The set reconciliation the reference reports between the primary and one
 * projection, keyed on the underlying security. */
typedef struct tdx_pending_reconciliation {
    size_t primary_rows;
    size_t projection_rows;
    size_t primary_securities;
    size_t projection_securities;
    size_t common_securities;
    /* The two differences, as lists of rows the caller owns.  A row appears here at
     * most once, in the order the document held it. */
    char (*primary_only)[24];
    size_t primary_only_count;
    char (*projection_only)[24];
    size_t projection_only_count;
    int exact_security_set;
} tdx_pending_reconciliation;

int tdx_pending_reconcile(const tdx_pending_row *primary, size_t primary_count,
                          const tdx_pending_row *projection, size_t projection_count,
                          tdx_pending_reconciliation *out, tdx_error *err);
void tdx_pending_reconciliation_free(tdx_pending_reconciliation *reconciliation);

#ifdef __cplusplus
}
#endif

#endif /* TDX_PENDING_H */
