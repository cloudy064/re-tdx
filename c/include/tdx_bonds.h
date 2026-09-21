/* tdx_bonds.h - bond reference rows: the field mapping over the JSN layer.
 *
 * The JSN layer guarantees that a cell is the cell its header names.  This layer
 * is the domain knowledge on top: which header is which field, how a market is
 * spelled, and - the subtle part - what unit the issue-size column is in.
 *
 * The unit depends on WHICH RESOURCE the row came from, and the reference keeps a
 * table of twenty resources for exactly that reason:
 *
 *   list/zqjrz201.jsn        client master, policy financial -> size in 1e8 yuan
 *   list/zq_dfzfz201.jsn     client master, hidden unit      -> no conversion
 *   list/zqgsz201.jsn        client master, hidden unit
 *   list/zqgz201.jsn         client master, hidden unit
 *   list/zqqyz201.jsn        client master, hidden unit
 *   list/zqsmz201.jsn        client master, hidden unit
 *   list/zqzczczq201.jsn     client master, hidden unit
 *   list/zq_*_1.jsn / _2.jsn exchange projections -> OUTSTANDING balance in 1e8
 *   list/zq_jrz201_1.jsn / _2.jsn                  -> issue size in 1e8
 *   anything else                                   -> yuan, as-is
 *
 * So the same column is an issue size in one resource and an outstanding balance in
 * another, and in a third its unit is not recoverable at all.  Reporting a single
 * "size" number would therefore be wrong in at least two of the three cases, which
 * is why the normalised row carries the raw value, the semantics it was read under,
 * and only the converted field that the semantics actually justifies.
 *
 * A reference master also swaps its identity columns: the master names itself in
 * $ZQDM and its underlying in $ZQDM1, while a projection does the opposite.  The
 * mapping follows the profile rather than assuming one ordering.
 *
 * Strings in a normalised row point INTO the JSN document, so the document must
 * outlive the row; that is stated here rather than hidden behind a copy, because
 * copying every name and date of a 42,957-row archive is pure waste. */
#ifndef TDX_BONDS_H
#define TDX_BONDS_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_error.h"
#include "tdx_jsn.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A string that points into the JSN document, with its length. */
typedef struct tdx_bond_text {
    const char *data;
    size_t length;
    int present; /* the column existed and was not empty */
} tdx_bond_text;

typedef enum tdx_bond_scale {
    /* The value is already yuan. */
    TDX_BOND_SCALE_ISSUE_YUAN = 0,
    /* The value is an issue size in hundreds of millions of yuan. */
    TDX_BOND_SCALE_ISSUE_100M_YUAN,
    /* The value is an OUTSTANDING balance in hundreds of millions of yuan. */
    TDX_BOND_SCALE_OUTSTANDING_100M_YUAN,
    /* The client master's unit is not recoverable; no conversion is applied. */
    TDX_BOND_SCALE_CLIENT_MASTER_HIDDEN
} tdx_bond_scale;

typedef struct tdx_bond_profile {
    const char *resource;
    int reference_master;
    tdx_bond_scale scale;
} tdx_bond_profile;

typedef struct tdx_bond_row {
    /* Identity.  These are built here, so they are real storage. */
    int market_id;
    char market[16];      /* sz / sh / bj / M<n> */
    char security_id[24]; /* SH020820 */
    tdx_bond_text code;
    tdx_bond_text name;
    int name_resolved;

    /* The master's own instrument id ($ZQDM in a master, null otherwise). */
    tdx_bond_text client_instrument_id;

    /* Classification. */
    tdx_bond_text bond_type;
    tdx_bond_text bond_credit_rating;
    tdx_bond_text issuer_credit_rating;
    tdx_bond_text rate_type;
    tdx_bond_text rate_type_flag;
    tdx_bond_text guarantee_status;

    /* Dates, exactly as the resource spells them (YYYYMMDD in practice, but they
     * are strings on the wire and this layer does not reformat what it cannot
     * verify). */
    tdx_bond_text accrual_start_date;
    tdx_bond_text maturity_date;
    tdx_bond_text next_coupon_date;
    tdx_bond_text last_coupon_date;
    tdx_bond_text listing_date;
    tdx_bond_text conversion_start_date;
    tdx_bond_text conversion_end_date;

    /* Numbers.  has_* distinguishes "the column was absent" from zero. */
    int has_remaining_years;
    double remaining_years;
    int has_current_coupon_rate_pct;
    double current_coupon_rate_pct;
    int has_coupon_frequency_months;
    double coupon_frequency_months;
    int has_face_value_yuan;
    double face_value_yuan;
    int has_issue_price_yuan;
    double issue_price_yuan;
    int has_remaining_coupon_count;
    double remaining_coupon_count;
    int has_conversion_price_yuan;
    double conversion_price_yuan;
    int has_revision_trigger_pct;
    double revision_trigger_pct;
    int has_put_trigger_pct;
    double put_trigger_pct;
    int has_call_trigger_pct;
    double call_trigger_pct;

    /* The size column, read under the resource's own semantics. */
    int has_source_scale;
    double source_scale_raw;
    tdx_bond_scale scale;
    int has_issue_size_yuan;
    double issue_size_yuan;
    int has_issue_size_source_100m;
    double issue_size_source_100m;
    int has_outstanding_balance_yuan;
    double outstanding_balance_yuan;
    int has_outstanding_balance_source_100m;
    double outstanding_balance_source_100m;

    /* The embedded convertible-bond underlying, when the resource names one. */
    int has_underlying;
    int underlying_market_id;
    char underlying_market[16];
    char underlying_security_id[24];
    tdx_bond_text underlying_code;
} tdx_bond_row;

/* The profile for a resource.  A resource the table does not know is read as yuan
 * with no master swap, which is the reference's default too. */
tdx_bond_profile tdx_bonds_profile(const char *resource);

/* The reference's own spelling for a market: 0 sz, 1 sh, 2 or 44 bj, else M<n>. */
int tdx_bonds_market_id(const char *text, size_t length, int *out, tdx_error *err);
const char *tdx_bonds_market_name(int market_id);
/* SZ / SH / BJ, or M<n>: for anything else.  Writes into out (16 bytes is plenty). */
int tdx_bonds_market_prefix(int market_id, char *out, size_t capacity, tdx_error *err);

/* Maps one JSN row.  row_in_group is the row's index inside its group. */
int tdx_bonds_normalize(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                        size_t row_in_group, const char *resource, tdx_bond_row *out,
                        tdx_error *err);

/* One entry of a coupon schedule. */
typedef struct tdx_bond_coupon {
    tdx_bond_text date;
    int has_rate;
    double rate_pct;
} tdx_bond_coupon;

/* Pairs the dates column with the rates column.  A rate whose magnitude is at most
 * one is multiplied by a hundred, because the resource writes some rates as
 * fractions and some as percents and the reference resolves that the same way. */
int tdx_bonds_coupon_schedule(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                              size_t row_in_group, const char *dates_key, const char *rates_key,
                              tdx_bond_coupon *out, size_t capacity, size_t *out_count,
                              size_t *list_length, tdx_error *err);

/* --- column access, shared with the other JSN domain layers ------------------ */

/* Reads a column's cell as text, zero copy.  An absent column, an empty value and
 * a non-string cell all report present = 0; the JSN resources carry their values as
 * strings, so a non-string cell cannot lose a value in practice. */
tdx_bond_text tdx_bonds_cell_text(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                                  size_t row, const char *key);

/* The first of several candidate column names that has a value. */
tdx_bond_text tdx_bonds_first_cell_text(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                                        size_t row, const char *const *keys, size_t key_count);

/* Reads a column's cell as a number; returns 0 when it is absent, empty or not a
 * number, which is how "no value" and "zero" stay distinguishable. */
int tdx_bonds_cell_number(const tdx_jsn_document *doc, const tdx_jsn_group *group, size_t row,
                          const char *key, double *out);

#ifdef __cplusplus
}
#endif

#endif /* TDX_BONDS_H */
