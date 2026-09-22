/* tdx_pending.c - the pending convertible-bond issue list. */
#include "tdx_pending.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int valid_code(const char *code, size_t length) {
    size_t index;
    if (!code || length != 6)
        return 0;
    for (index = 0; index < 6; ++index)
        if (code[index] < '0' || code[index] > '9')
            return 0;
    return 1;
}

/* Reads a cell's text, zero copy, through the shared accessor.  It TRIMS, which is
 * what the reference does to every value and what matters for the columns here that
 * arrive as fixed-width text. */
static tdx_bond_text cell(const tdx_jsn_document *doc, const tdx_jsn_group *group, size_t row,
                          const char *key) {
    return tdx_bonds_cell_text(doc, group, row, key);
}

static int cell_number(const tdx_jsn_document *doc, const tdx_jsn_group *group, size_t row,
                       const char *key, double *out) {
    return tdx_bonds_cell_number(doc, group, row, key, out);
}

int tdx_pending_normalize(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                          tdx_pending_row *out, size_t capacity, size_t *out_count,
                          size_t *skipped_count, tdx_error *err) {
    size_t stored = 0;
    size_t skipped = 0;
    size_t row;

    if (out_count)
        *out_count = 0;
    if (skipped_count)
        *skipped_count = 0;
    if (!doc || !group || !out) {
        tdx_error_set(err, "normalizing the pending list needs a document, a group and an output");
        return TDX_ERR;
    }
    for (row = 0; row < group->row_count; ++row) {
        tdx_bond_text code = cell(doc, group, row, "$ZQDM");
        tdx_bond_text market_text = cell(doc, group, row, "$SC");
        int market = -1;
        char prefix[16];
        const char *market_name;
        tdx_pending_row *item;

        /* A row that cannot be attached to a stock is skipped, not fatal: this is a
         * plan list, and the reference skips it too.  The count of skips is reported
         * so a caller can see that the document held more rows than it produced. */
        if (!valid_code(code.data, code.length)) {
            skipped++;
            continue;
        }
        if (tdx_bonds_market_id(market_text.data, market_text.length, &market, NULL) != TDX_OK) {
            skipped++;
            continue;
        }
        if (tdx_bonds_market_prefix(market, prefix, sizeof(prefix), err) != TDX_OK)
            return TDX_ERR;
        if (stored >= capacity) {
            tdx_error_set(err, "the pending list holds more than %zu rows", capacity);
            return TDX_ERR;
        }
        item = &out[stored];
        memset(item, 0, sizeof(*item));
        market_name = tdx_bonds_market_name(market);
        item->market_id = market;
        snprintf(item->market, sizeof(item->market), "%s", market_name ? market_name : prefix);
        snprintf(item->security_id, sizeof(item->security_id), "%s%.*s", prefix,
                 (int)code.length, code.data);
        item->code = code;

        item->issue_type = cell(doc, group, row, "zzlx");
        item->has_planned_size_100m_yuan =
            cell_number(doc, group, row, "mzgm", &item->planned_size_100m_yuan);
        item->plan_progress = cell(doc, group, row, "fadj");
        item->progress_date = cell(doc, group, row, "date0");
        item->has_stock_rights_yuan =
            cell_number(doc, group, row, "byhq", &item->stock_rights_yuan);
        item->has_conversion_price_yuan =
            cell_number(doc, group, row, "zgj", &item->conversion_price_yuan);
        item->has_shareholder_placement_ratio = cell_number(
            doc, group, row, "gdpsl", &item->shareholder_placement_ratio);
        item->subscription_date = cell(doc, group, row, "sgrq");
        item->issue_date = cell(doc, group, row, "fxrq");
        item->has_lottery_rate = cell_number(doc, group, row, "zql", &item->lottery_rate);
        item->lottery_date = cell(doc, group, row, "zqr");
        item->subscription_code = cell(doc, group, row, "sgdm");
        item->subscription_name = cell(doc, group, row, "sgmc");
        item->has_issue_price_yuan =
            cell_number(doc, group, row, "fxjg", &item->issue_price_yuan);
        stored++;
    }
    if (out_count)
        *out_count = stored;
    if (skipped_count)
        *skipped_count = skipped;
    return TDX_OK;
}

/* --- the set reconciliation ------------------------------------------- */

/* The distinct security ids of a row list, sorted.  An empty id is not a security and
 * is dropped, which is what a set of ids means. */
static int collect_ids(const tdx_pending_row *rows, size_t count, char (*out)[24],
                       size_t *out_count, size_t capacity, tdx_error *err) {
    size_t index;
    size_t unique = 0;

    for (index = 0; index < count; ++index) {
        size_t scan;
        int seen = 0;
        if (rows[index].security_id[0] == '\0')
            continue;
        for (scan = 0; scan < unique; ++scan)
            if (strcmp(out[scan], rows[index].security_id) == 0) {
                seen = 1;
                break;
            }
        if (seen)
            continue;
        if (unique >= capacity) {
            tdx_error_set(err, "more than %zu distinct securities to reconcile", capacity);
            return TDX_ERR;
        }
        snprintf(out[unique], sizeof(out[unique]), "%s", rows[index].security_id);
        unique++;
    }
    if (out_count)
        *out_count = unique;
    return TDX_OK;
}

static int contains(char (*ids)[24], size_t count, const char *id) {
    size_t index;
    for (index = 0; index < count; ++index)
        if (strcmp(ids[index], id) == 0)
            return 1;
    return 0;
}

int tdx_pending_reconcile(const tdx_pending_row *primary, size_t primary_count,
                          const tdx_pending_row *projection, size_t projection_count,
                          tdx_pending_reconciliation *out, tdx_error *err) {
    char(*primary_ids)[24] = NULL;
    char(*projection_ids)[24] = NULL;
    size_t primary_unique = 0;
    size_t projection_unique = 0;
    size_t index;
    int result = TDX_ERR;

    if (!out) {
        tdx_error_set(err, "the pending reconciliation needs an output");
        return TDX_ERR;
    }
    memset(out, 0, sizeof(*out));
    out->primary_rows = primary_count;
    out->projection_rows = projection_count;
    primary_ids = (char(*)[24])calloc(primary_count ? primary_count : 1, 24);
    projection_ids = (char(*)[24])calloc(projection_count ? projection_count : 1, 24);
    out->primary_only = (char(*)[24])calloc(primary_count ? primary_count : 1, 24);
    out->projection_only = (char(*)[24])calloc(projection_count ? projection_count : 1, 24);
    if (!primary_ids || !projection_ids || !out->primary_only || !out->projection_only) {
        tdx_error_set(err, "out of memory while reconciling the pending lists");
        goto done;
    }
    if (collect_ids(primary, primary_count, primary_ids, &primary_unique, primary_count,
                    err) != TDX_OK)
        goto done;
    if (collect_ids(projection, projection_count, projection_ids, &projection_unique,
                    projection_count, err) != TDX_OK)
        goto done;
    out->primary_securities = primary_unique;
    out->projection_securities = projection_unique;

    /* primary_only keeps the PRIMARY document's order, projection_only the
     * projection's, so a caller can line the report up with the rows it printed. */
    for (index = 0; index < primary_unique; ++index) {
        if (contains(projection_ids, projection_unique, primary_ids[index])) {
            out->common_securities++;
        } else {
            snprintf(out->primary_only[out->primary_only_count], 24, "%s", primary_ids[index]);
            out->primary_only_count++;
        }
    }
    for (index = 0; index < projection_unique; ++index)
        if (!contains(primary_ids, primary_unique, projection_ids[index])) {
            snprintf(out->projection_only[out->projection_only_count], 24, "%s",
                     projection_ids[index]);
            out->projection_only_count++;
        }
    out->exact_security_set = primary_unique == projection_unique &&
                              out->primary_only_count == 0 && out->projection_only_count == 0;
    result = TDX_OK;

done:
    free(primary_ids);
    free(projection_ids);
    if (result != TDX_OK)
        tdx_pending_reconciliation_free(out);
    return result;
}

void tdx_pending_reconciliation_free(tdx_pending_reconciliation *reconciliation) {
    if (!reconciliation)
        return;
    free(reconciliation->primary_only);
    free(reconciliation->projection_only);
    memset(reconciliation, 0, sizeof(*reconciliation));
}
