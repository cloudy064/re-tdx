/* tdx_newbond.c - the new-bond projection and its reconciliation. */
#include "tdx_newbond.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A difference smaller than this is not a disagreement: the column is in 100m yuan, so
 * 0.01 is one million. */
#define TDX_NEWBOND_SIZE_TOLERANCE 0.01

int tdx_newbond_compact_date(const char *text, size_t length, char *out, size_t capacity) {
    size_t index;
    size_t written = 0;

    if (!out || capacity < 9)
        return 0;
    out[0] = '\0';
    if (!text)
        return 0;
    /* Digits only, hyphens and the weekday text dropped, stopping at eight: the
     * projection's "2026-06-26" followed by the weekday becomes "20260626". */
    for (index = 0; index < length && written < 8; ++index) {
        if (text[index] >= '0' && text[index] <= '9')
            out[written++] = text[index];
        else if (text[index] != '-' && text[index] != ' ' && text[index] != '\t')
            break; /* anything that is not a separator ends the date */
    }
    out[written] = '\0';
    return written == 8;
}

static int valid_code(const char *code, size_t length) {
    size_t index;
    if (!code || length != 6)
        return 0;
    for (index = 0; index < 6; ++index)
        if (code[index] < '0' || code[index] > '9')
            return 0;
    return 1;
}

static tdx_bond_text cell(const tdx_jsn_document *doc, const tdx_jsn_group *group, size_t row,
                          const char *key) {
    return tdx_bonds_cell_text(doc, group, row, key);
}

static int cell_number(const tdx_jsn_document *doc, const tdx_jsn_group *group, size_t row,
                       const char *key, double *out) {
    return tdx_bonds_cell_number(doc, group, row, key, out);
}

int tdx_newbond_normalize(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                          tdx_newbond_row *out, size_t capacity, size_t *out_count,
                          size_t *skipped_count, tdx_error *err) {
    size_t stored = 0;
    size_t skipped = 0;
    size_t row;

    if (out_count)
        *out_count = 0;
    if (skipped_count)
        *skipped_count = 0;
    if (!doc || !group || !out) {
        tdx_error_set(err, "normalizing the new-bond projection needs a document, a group and "
                           "an output");
        return TDX_ERR;
    }
    for (row = 0; row < group->row_count; ++row) {
        tdx_newbond_row *item;
        tdx_bond_text stock_code = cell(doc, group, row, "$ZQDM");
        tdx_bond_text stock_market_text = cell(doc, group, row, "$SC");
        int stock_market = -1;
        char prefix[16];

        if (stored >= capacity) {
            tdx_error_set(err, "the projection holds more than %zu rows", capacity);
            return TDX_ERR;
        }
        /* The UNDERLYING is this list's fallback key, so a row without one is skipped:
         * it could still be matched by subscription code, but the reference skips it
         * and a row that cannot be keyed the same way on both sides is not comparable. */
        if (!valid_code(stock_code.data, stock_code.length)) {
            skipped++;
            continue;
        }
        if (tdx_bonds_market_id(stock_market_text.data, stock_market_text.length, &stock_market,
                                NULL) != TDX_OK) {
            skipped++;
            continue;
        }
        if (tdx_bonds_market_prefix(stock_market, prefix, sizeof(prefix), err) != TDX_OK)
            return TDX_ERR;
        item = &out[stored];
        memset(item, 0, sizeof(*item));
        item->has_underlying = 1;
        item->stock_market_id = stock_market;
        snprintf(item->stock_security_id, sizeof(item->stock_security_id), "%s%.*s", prefix,
                 (int)stock_code.length, stock_code.data);
        item->stock_code = stock_code;

        item->subscription_code = cell(doc, group, row, "sgdm");
        item->subscription_name = cell(doc, group, row, "sgmc");
        item->subscription_date_text = cell(doc, group, row, "sgrq");
        /* The compacted form is what can be compared against the subscription list. */
        if (!tdx_newbond_compact_date(item->subscription_date_text.data,
                                      item->subscription_date_text.length, item->subscription_date,
                                      sizeof(item->subscription_date)) &&
            item->subscription_date_text.present) {
            /* A date that cannot be compacted is not made up from the issue date here;
             * the reference falls back to fxrq, so this does too. */
            item->subscription_date[0] = '\0';
        }
        item->issue_date_text = cell(doc, group, row, "fxrq");
        if (item->subscription_date[0] == '\0')
            tdx_newbond_compact_date(item->issue_date_text.data, item->issue_date_text.length,
                                     item->subscription_date, sizeof(item->subscription_date));
        tdx_newbond_compact_date(item->issue_date_text.data, item->issue_date_text.length,
                                 item->issue_date, sizeof(item->issue_date));
        item->has_issue_price_yuan =
            cell_number(doc, group, row, "fxjg", &item->issue_price_yuan);
        item->issue_type = cell(doc, group, row, "zzlx");
        item->has_issue_size_100m_yuan =
            cell_number(doc, group, row, "mzgm", &item->issue_size_100m_yuan);
        item->has_stock_rights_yuan =
            cell_number(doc, group, row, "byhq", &item->stock_rights_yuan);
        item->has_conversion_price_yuan =
            cell_number(doc, group, row, "zgj", &item->conversion_price_yuan);
        item->conversion_available = cell(doc, group, row, "zg");
        item->conversion_start_date = cell(doc, group, row, "zgqsr");
        item->conversion_end_date = cell(doc, group, row, "zgjzr");
        item->has_shareholder_placement_ratio = cell_number(
            doc, group, row, "gdpsl", &item->shareholder_placement_ratio);
        item->lottery_date = cell(doc, group, row, "zqr");
        item->has_lottery_rate_pct = cell_number(doc, group, row, "zql", &item->lottery_rate_pct);
        item->region = cell(doc, group, row, "ssdy");
        item->plan_progress = cell(doc, group, row, "fadj");
        item->progress_date = cell(doc, group, row, "date0");
        stored++;
    }
    if (out_count)
        *out_count = stored;
    if (skipped_count)
        *skipped_count = skipped;
    return TDX_OK;
}

/* --- the reconciliation ----------------------------------------------- */

/* Finds the subscription row a projection row should be compared against: by code
 * first, preferring the candidate with the same underlying; then by underlying alone. */
static const tdx_subscription_row *match_primary(const tdx_subscription_row *primary,
                                                 size_t count, const tdx_newbond_row *projection,
                                                 int *by_code, int *by_underlying) {
    size_t index;
    const tdx_subscription_row *first_by_code = NULL;

    *by_code = 0;
    *by_underlying = 0;
    /* A code match wins even if the underlying then differs, which is the reference's
     * rule: the code is the stronger key. */
    if (projection->subscription_code.present && projection->subscription_code.length > 0) {
        for (index = 0; index < count; ++index) {
            if (!primary[index].subscription_code.present ||
                primary[index].subscription_code.length != projection->subscription_code.length)
                continue;
            if (memcmp(primary[index].subscription_code.data, projection->subscription_code.data,
                       projection->subscription_code.length) != 0)
                continue;
            if (!first_by_code)
                first_by_code = &primary[index];
            if (projection->has_underlying &&
                strcmp(primary[index].stock_security_id, projection->stock_security_id) == 0) {
                *by_code = 1;
                return &primary[index];
            }
        }
        if (first_by_code) {
            *by_code = 1;
            return first_by_code;
        }
    }
    if (projection->has_underlying) {
        for (index = 0; index < count; ++index)
            if (strcmp(primary[index].stock_security_id, projection->stock_security_id) == 0) {
                *by_underlying = 1;
                return &primary[index];
            }
    }
    return NULL;
}

int tdx_newbond_reconcile(const tdx_subscription_row *primary, size_t primary_count,
                          const tdx_newbond_row *projection, size_t projection_count,
                          tdx_newbond_reconciliation *out, tdx_error *err) {
    size_t index;
    int result = TDX_ERR;

    if (!out) {
        tdx_error_set(err, "the new-bond reconciliation needs an output");
        return TDX_ERR;
    }
    memset(out, 0, sizeof(*out));
    out->primary_rows = primary_count;
    out->projection_rows = projection_count;
    out->matches = (tdx_newbond_match *)calloc(projection_count ? projection_count : 1,
                                               sizeof(*out->matches));
    out->unmatched_codes = (char(*)[TDX_NEWBOND_CODE_MAX])calloc(
        projection_count ? projection_count : 1, TDX_NEWBOND_CODE_MAX);
    if (!out->matches || !out->unmatched_codes) {
        tdx_error_set(err, "out of memory while reconciling the projection");
        goto done;
    }
    out->match_count = projection_count;

    for (index = 0; index < projection_count; ++index) {
        tdx_newbond_match *match = &out->matches[index];
        int by_code = 0;
        int by_underlying = 0;
        const tdx_subscription_row *found = match_primary(primary, primary_count, &projection[index],
                                                          &by_code, &by_underlying);
        if (!found) {
            match->matched = 0;
            match->match_method = "none";
            snprintf(out->unmatched_codes[out->unmatched_code_count], TDX_NEWBOND_CODE_MAX, "%.*s",
                     (int)(projection[index].subscription_code.present
                               ? projection[index].subscription_code.length
                               : 0),
                     projection[index].subscription_code.data ? projection[index].subscription_code.data
                                                              : "");
            out->unmatched_code_count++;
            continue;
        }
        match->matched = 1;
        match->primary = found;
        if (by_code) {
            match->match_method = "subscription-code";
            out->exact_code_matches++;
        } else {
            /* A fallback match is weaker evidence, so the method is reported. */
            match->match_method = "underlying-only";
            out->underlying_only_matches++;
        }
        /* The dates are compared in their compacted forms.  Comparing raw text reports a
         * mismatch on every row, because the projection's date carries the weekday. */
        if (projection[index].subscription_date[0] != '\0' &&
            found->subscription_date.present && found->subscription_date.length > 0) {
            match->date_mismatch =
                found->subscription_date.length != strlen(projection[index].subscription_date) ||
                memcmp(found->subscription_date.data, projection[index].subscription_date,
                       found->subscription_date.length) != 0;
        }
        if (projection[index].has_issue_size_100m_yuan && found->has_issue_size_100m_yuan) {
            match->has_size_delta = 1;
            match->size_delta_100m_yuan =
                projection[index].issue_size_100m_yuan - found->issue_size_100m_yuan;
            match->size_mismatch =
                fabs(match->size_delta_100m_yuan) > TDX_NEWBOND_SIZE_TOLERANCE;
        }
        if (match->date_mismatch)
            out->subscription_date_mismatch_count++;
        if (match->size_mismatch)
            out->issue_size_mismatch_count++;
        /* "Hybrid or stale" counts rows where either disagrees - the reference counts it
         * per row, not per disagreement, so a row wrong in both counts once. */
        if (match->date_mismatch || match->size_mismatch)
            out->hybrid_or_stale_count++;
    }
    out->unmatched_projection_rows = projection_count - out->exact_code_matches -
                                     out->underlying_only_matches;
    out->exact_projection =
        out->exact_code_matches == projection_count && out->hybrid_or_stale_count == 0;
    result = TDX_OK;

done:
    if (result != TDX_OK)
        tdx_newbond_reconciliation_free(out);
    return result;
}

void tdx_newbond_reconciliation_free(tdx_newbond_reconciliation *reconciliation) {
    if (!reconciliation)
        return;
    free(reconciliation->matches);
    free(reconciliation->unmatched_codes);
    memset(reconciliation, 0, sizeof(*reconciliation));
}
