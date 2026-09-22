/* tdx_pending_json.c - JSONL rendering for the pending issue list. */
#include "tdx_pending_json.h"

#include <stdio.h>
#include <string.h>

#include "tdx_format.h"

#define APPEND_LITERAL(buf, err, text) tdx_buf_append((buf), (text), sizeof(text) - 1, (err))
/* The argument must be a string literal.  This macro sizes it with sizeof, and a
 * computed expression decays to a pointer, so `cond ? "true" : "false"` would
 * copy sizeof(char*) - 1 bytes.  Pass an expression to tdx_buf_append_printf. */

static int append_text(tdx_buf *out, const tdx_bond_text *text, tdx_error *err) {
    char scratch[1024];

    if (!text->present || !text->data)
        return APPEND_LITERAL(out, err, "null");
    if (text->length >= sizeof(scratch)) {
        tdx_error_set(err, "a pending text field is %zu bytes, longer than this renderer holds",
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

int tdx_pending_format(tdx_buf *out, const tdx_pending_row *row, const char *resource,
                       size_t row_index, tdx_error *err) {
    if (!out || !row) {
        tdx_error_set(err, "pending rendering needs a buffer and a row");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"pending_convertible_bond\",\"resource\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, resource ? resource : "", err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"row\":%zu,\"underlying\":{\"market\":", row_index) !=
        TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, row->market, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"market_id\":%d,\"security_id\":", row->market_id) !=
        TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, row->security_id, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"code\":") != TDX_OK ||
        append_text(out, &row->code, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "},\"plan\":{\"issue_type\":") != TDX_OK ||
        append_text(out, &row->issue_type, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"planned_size_100m_yuan\":") != TDX_OK ||
        append_number(out, row->has_planned_size_100m_yuan, row->planned_size_100m_yuan, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"progress\":") != TDX_OK ||
        append_text(out, &row->plan_progress, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"progress_date\":") != TDX_OK ||
        append_text(out, &row->progress_date, err) != TDX_OK)
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
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"schedule\":{\"subscription_date\":") != TDX_OK ||
        append_text(out, &row->subscription_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"issue_date\":") != TDX_OK ||
        append_text(out, &row->issue_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"lottery_rate\":") != TDX_OK ||
        append_number(out, row->has_lottery_rate, row->lottery_rate, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"lottery_date\":") != TDX_OK ||
        append_text(out, &row->lottery_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"issue_price_yuan\":") != TDX_OK ||
        append_number(out, row->has_issue_price_yuan, row->issue_price_yuan, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "},\"subscription\":{\"code\":") != TDX_OK ||
        append_text(out, &row->subscription_code, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"name\":") != TDX_OK ||
        append_text(out, &row->subscription_name, err) != TDX_OK)
        return TDX_ERR;
    /* Two braces: one closes the subscription object, one closes the row.  Forgetting
     * the second is a mistake this project has now made five times, which is why every
     * JSON test in it checks brace balance. */
    if (APPEND_LITERAL(out, err, "}}") != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_pending_format_reconciliation(tdx_buf *out,
                                      const tdx_pending_reconciliation *reconciliation,
                                      const char *primary_resource,
                                      const char *projection_resource, const char *endpoint,
                                      tdx_error *err) {
    size_t index;

    if (!out || !reconciliation) {
        tdx_error_set(err, "the pending reconciliation report needs a buffer and a report");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"pending_reconciliation\",\"primary_resource\":") !=
        TDX_OK)
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
                              "\"primary_securities\":%zu,\"projection_securities\":%zu,"
                              "\"common_securities\":%zu,\"primary_only_count\":%zu,"
                              "\"projection_only_count\":%zu,\"exact_security_set\":%s",
                              reconciliation->primary_rows, reconciliation->projection_rows,
                              reconciliation->primary_securities,
                              reconciliation->projection_securities,
                              reconciliation->common_securities,
                              reconciliation->primary_only_count,
                              reconciliation->projection_only_count,
                              reconciliation->exact_security_set ? "true" : "false") != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"primary_only\":[") != TDX_OK)
        return TDX_ERR;
    for (index = 0; index < reconciliation->primary_only_count; ++index) {
        if (index > 0 && tdx_buf_push(out, ',', err) != TDX_OK)
            return TDX_ERR;
        if (tdx_format_json_string(out, reconciliation->primary_only[index], err) != TDX_OK)
            return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "],\"projection_only\":[") != TDX_OK)
        return TDX_ERR;
    for (index = 0; index < reconciliation->projection_only_count; ++index) {
        if (index > 0 && tdx_buf_push(out, ',', err) != TDX_OK)
            return TDX_ERR;
        if (tdx_format_json_string(out, reconciliation->projection_only[index], err) != TDX_OK)
            return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "]}") != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_pending_format_summary(tdx_buf *out, size_t rows, size_t skipped, size_t projections,
                               size_t rows_common_with_every_projection,
                               const char *primary_resource, const char *endpoint,
                               tdx_error *err) {
    if (!out) {
        tdx_error_set(err, "the pending summary needs a buffer");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"pending_summary\",\"primary_resource\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, primary_resource ? primary_resource : "", err) != TDX_OK)
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
                              ",\"rows\":%zu,\"rows_skipped\":%zu,\"projections_reconciled\":%zu,"
                              "\"projections_agreeing_on_every_security\":%zu}",
                              rows, skipped, projections,
                              rows_common_with_every_projection) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}
