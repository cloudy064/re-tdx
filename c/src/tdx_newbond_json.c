/* tdx_newbond_json.c - JSONL rendering for the projection and its reconciliation. */
#include "tdx_newbond_json.h"

#include <stdio.h>
#include <string.h>

#include "tdx_format.h"

#define APPEND_LITERAL(buf, err, text) tdx_buf_append((buf), (text), sizeof(text) - 1, (err))
/* The argument must be a string literal.  This macro sizes it with sizeof, and a
 * computed expression decays to a pointer, so `cond ? "true" : "false"` would
 * copy sizeof(char*) - 1 bytes.  Pass an expression to tdx_buf_append_printf. */

static int append_text(tdx_buf *out, const tdx_bond_text *text, tdx_error *err) {
    char scratch[512];

    if (!text->present || !text->data)
        return APPEND_LITERAL(out, err, "null");
    if (text->length >= sizeof(scratch)) {
        tdx_error_set(err, "a projection text field is %zu bytes, longer than this renderer "
                           "holds",
                      text->length);
        return TDX_ERR;
    }
    memcpy(scratch, text->data, text->length);
    scratch[text->length] = '\0';
    return tdx_format_json_string(out, scratch, err);
}

static int append_number(tdx_buf *out, int has, double value, tdx_error *err) {
    if (!has)
        return APPEND_LITERAL(out, err, "null");
    return tdx_buf_append_printf(out, err, "%.6f", value);
}

static int append_cstr(tdx_buf *out, const char *text, tdx_error *err) {
    if (!text || !*text)
        return APPEND_LITERAL(out, err, "null");
    return tdx_format_json_string(out, text, err);
}

int tdx_newbond_format_row(tdx_buf *out, const tdx_newbond_row *row,
                           const tdx_newbond_match *match, size_t row_index, tdx_error *err) {
    if (!out || !row) {
        tdx_error_set(err, "projection rendering needs a buffer and a row");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"new_convertible_bond_projection\",\"row\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, "%zu,\"subscription_code\":", row_index) != TDX_OK)
        return TDX_ERR;
    if (append_text(out, &row->subscription_code, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"subscription_name\":") != TDX_OK ||
        append_text(out, &row->subscription_name, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"underlying\":{\"market_id\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, "%d,\"security_id\":", row->stock_market_id) != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, row->stock_security_id, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        return TDX_ERR;
    /* Both the raw date and the compacted one, because the raw one is what the document
     * says and the compacted one is what cannot be compared wrongly. */
    if (APPEND_LITERAL(out, err, ",\"dates\":{\"subscription_text\":") != TDX_OK ||
        append_text(out, &row->subscription_date_text, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"subscription\":") != TDX_OK ||
        append_cstr(out, row->subscription_date[0] ? row->subscription_date : NULL, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"issue_text\":") != TDX_OK ||
        append_text(out, &row->issue_date_text, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"issue\":") != TDX_OK ||
        append_cstr(out, row->issue_date[0] ? row->issue_date : NULL, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"issue\":{\"type\":") != TDX_OK ||
        append_text(out, &row->issue_type, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"size_100m_yuan\":") != TDX_OK ||
        append_number(out, row->has_issue_size_100m_yuan, row->issue_size_100m_yuan, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"price_yuan\":") != TDX_OK ||
        append_number(out, row->has_issue_price_yuan, row->issue_price_yuan, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"stock_rights_yuan\":") != TDX_OK ||
        append_number(out, row->has_stock_rights_yuan, row->stock_rights_yuan, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"conversion_price_yuan\":") != TDX_OK ||
        append_number(out, row->has_conversion_price_yuan, row->conversion_price_yuan, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"shareholder_placement_ratio\":") != TDX_OK ||
        append_number(out, row->has_shareholder_placement_ratio,
                      row->shareholder_placement_ratio, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"lottery_rate_pct\":") != TDX_OK ||
        append_number(out, row->has_lottery_rate_pct, row->lottery_rate_pct, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"region\":") != TDX_OK ||
        append_text(out, &row->region, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"progress\":") != TDX_OK ||
        append_text(out, &row->plan_progress, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"progress_date\":") != TDX_OK ||
        append_text(out, &row->progress_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        return TDX_ERR;

    /* The match, or the plain statement that there is none. */
    if (!match) {
        if (APPEND_LITERAL(out, err, ",\"subscription_match\":null}") != TDX_OK)
            return TDX_ERR;
        return TDX_OK;
    }
    if (APPEND_LITERAL(out, err, ",\"subscription_match\":{\"matched\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, "%s,\"match_method\":\"%s\"",
                              match->matched ? "true" : "false",
                              match->match_method ? match->match_method : "none") != TDX_OK)
        return TDX_ERR;
    if (match->matched && match->primary) {
        if (APPEND_LITERAL(out, err, ",\"primary_event_id\":") != TDX_OK ||
            append_cstr(out, match->primary->event_id, err) != TDX_OK)
            return TDX_ERR;
        if (APPEND_LITERAL(out, err, ",\"primary_bond\":") != TDX_OK ||
            append_cstr(out, match->primary->bond_security_id, err) != TDX_OK)
            return TDX_ERR;
        if (APPEND_LITERAL(out, err, ",\"primary_subscription_date\":") != TDX_OK ||
            append_text(out, &match->primary->subscription_date, err) != TDX_OK)
            return TDX_ERR;
        if (APPEND_LITERAL(out, err, ",\"primary_issue_size_100m_yuan\":") != TDX_OK ||
            append_number(out, match->primary->has_issue_size_100m_yuan,
                          match->primary->issue_size_100m_yuan, err) != TDX_OK)
            return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err, ",\"subscription_date_mismatch\":%s",
                              match->date_mismatch ? "true" : "false") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"issue_size_mismatch\":%s",
                              match->size_mismatch ? "true" : "false") != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"issue_size_delta_100m_yuan\":") != TDX_OK ||
        append_number(out, match->has_size_delta, match->size_delta_100m_yuan, err) != TDX_OK)
        return TDX_ERR;
    /* Two braces: the match object and the row.  The brace-balance check in the test is
     * what keeps this honest, and this project has needed it six times. */
    if (APPEND_LITERAL(out, err, "}}") != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_newbond_format_reconciliation(tdx_buf *out,
                                      const tdx_newbond_reconciliation *reconciliation,
                                      const char *primary_resource,
                                      const char *projection_resource, const char *endpoint,
                                      tdx_error *err) {
    size_t index;

    if (!out || !reconciliation) {
        tdx_error_set(err, "the projection reconciliation report needs a buffer and a report");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"new_projection_reconciliation\","
                                "\"primary_resource\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, primary_resource ? primary_resource : "", err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"projection_resource\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, projection_resource ? projection_resource : "", err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"endpoint\":") != TDX_OK)
        return TDX_ERR;
    if (endpoint && *endpoint) {
        if (tdx_format_json_string(out, endpoint, err) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err,
                              ",\"primary_rows\":%zu,\"projection_rows\":%zu,"
                              "\"exact_subscription_code_matches\":%zu,"
                              "\"underlying_only_matches\":%zu,"
                              "\"unmatched_projection_rows\":%zu,"
                              "\"subscription_date_mismatch_count\":%zu,"
                              "\"issue_size_mismatch_count\":%zu,\"hybrid_or_stale_count\":%zu,"
                              "\"exact_projection\":%s",
                              reconciliation->primary_rows, reconciliation->projection_rows,
                              reconciliation->exact_code_matches,
                              reconciliation->underlying_only_matches,
                              reconciliation->unmatched_projection_rows,
                              reconciliation->subscription_date_mismatch_count,
                              reconciliation->issue_size_mismatch_count,
                              reconciliation->hybrid_or_stale_count,
                              reconciliation->exact_projection ? "true" : "false") != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"unmatched_subscription_codes\":[") != TDX_OK)
        return TDX_ERR;
    for (index = 0; index < reconciliation->unmatched_code_count; ++index) {
        if (index > 0 && tdx_buf_push(out, ',', err) != TDX_OK)
            return TDX_ERR;
        if (tdx_format_json_string(out, reconciliation->unmatched_codes[index], err) != TDX_OK)
            return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "]}") != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}
