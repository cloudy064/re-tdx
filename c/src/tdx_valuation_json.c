/* tdx_valuation_json.c - JSONL rendering for the index valuation family. */
#include "tdx_valuation_json.h"

#include <stdio.h>
#include <string.h>

#include "tdx_format.h"

/* The argument must be a string literal: sizeof sizes it, and a computed expression decays
 * to a pointer, so `cond ? "a" : "b"` would copy sizeof(char*) - 1 bytes. */
#define VAL_LITERAL(buf, err, text) tdx_buf_append((buf), (text), sizeof(text) - 1, (err))

static int append_number(tdx_buf *out, int has, double value, tdx_error *err) {
    if (!has)
        return VAL_LITERAL(out, err, "null");
    return tdx_buf_append_printf(out, err, "%.6f", value);
}

static int append_text(tdx_buf *out, const tdx_bond_text *text, tdx_error *err) {
    char scratch[256];
    if (!text->present || !text->data)
        return VAL_LITERAL(out, err, "null");
    if (text->length >= sizeof(scratch)) {
        tdx_error_set(err, "a valuation text field is %zu bytes, longer than this renderer "
                           "holds", text->length);
        return TDX_ERR;
    }
    memcpy(scratch, text->data, text->length);
    scratch[text->length] = '\0';
    return tdx_format_json_string(out, scratch, err);
}

int tdx_valuation_format_index(tdx_buf *out, const tdx_valuation_index *index, size_t row,
                               tdx_error *err) {
    if (!out || !index) {
        tdx_error_set(err, "rendering a valuation index needs a buffer and an index");
        return TDX_ERR;
    }
    if (VAL_LITERAL(out, err, "{\"type\":\"valuation_index\",\"row\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              "%zu,\"security_id\":\"%s\",\"market_id\":%d,"
                              "\"code\":\"%s\",\"detail_id\":\"%s\",\"date\":",
                              row, index->security_id, index->market_id, index->code,
                              index->detail_id) != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, index->date, err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"pe\":") != TDX_OK ||
        append_number(out, index->has_pe, index->pe, err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"pe_percentile\":") != TDX_OK ||
        append_number(out, index->has_pe_percentile, index->pe_percentile, err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"pb\":") != TDX_OK ||
        append_number(out, index->has_pb, index->pb, err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"pb_percentile\":") != TDX_OK ||
        append_number(out, index->has_pb_percentile, index->pb_percentile, err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"dividend_yield\":") != TDX_OK ||
        append_number(out, index->has_dividend_yield, index->dividend_yield, err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"roe\":") != TDX_OK ||
        append_number(out, index->has_roe, index->roe, err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"earnings_yield\":") != TDX_OK ||
        append_number(out, index->has_earnings_yield, index->earnings_yield, err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"valuation_label\":") != TDX_OK ||
        append_text(out, &index->label, err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"returns\":{\"days_5\":") != TDX_OK ||
        append_number(out, index->has_return[0], index->returns[0], err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"days_10\":") != TDX_OK ||
        append_number(out, index->has_return[1], index->returns[1], err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"days_20\":") != TDX_OK ||
        append_number(out, index->has_return[2], index->returns[2], err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"days_30\":") != TDX_OK ||
        append_number(out, index->has_return[3], index->returns[3], err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, "}}") != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_valuation_format_point(tdx_buf *out, const tdx_valuation_point *point,
                               const char *security_id, size_t row, tdx_error *err) {
    if (!out || !point) {
        tdx_error_set(err, "rendering a valuation point needs a buffer and a point");
        return TDX_ERR;
    }
    if (VAL_LITERAL(out, err, "{\"type\":\"valuation_history\",\"security_id\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, security_id ? security_id : "", err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"row\":%zu,\"date\":", row) != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, point->date, err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"pe\":") != TDX_OK ||
        append_number(out, point->has_pe, point->pe, err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"pe_percentile\":") != TDX_OK ||
        append_number(out, point->has_pe_percentile, point->pe_percentile, err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"pb\":") != TDX_OK ||
        append_number(out, point->has_pb, point->pb, err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"pb_percentile\":") != TDX_OK ||
        append_number(out, point->has_pb_percentile, point->pb_percentile, err) != TDX_OK)
        return TDX_ERR;
    /* The closing brace: without it every point is a truncated object, which is exactly what
     * the parse assertion in the test is for. */
    return tdx_buf_push(out, '}', err);
}

int tdx_valuation_format_merge(tdx_buf *out, const tdx_valuation_merge *merge,
                               const char *security_id, const char *pe_resource,
                               const char *pb_resource, tdx_error *err) {
    if (!out || !merge) {
        tdx_error_set(err, "the merge report needs a buffer and a report");
        return TDX_ERR;
    }
    if (VAL_LITERAL(out, err, "{\"type\":\"valuation_history_merge\",\"security_id\":") !=
        TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, security_id ? security_id : "", err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"pe_resource\":") != TDX_OK ||
        tdx_format_json_string(out, pe_resource ? pe_resource : "", err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"pb_resource\":") != TDX_OK ||
        tdx_format_json_string(out, pb_resource ? pb_resource : "", err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"points\":%zu,\"both_sides\":%zu,\"pe_only\":%zu,"
                              "\"pb_only\":%zu,\"complete\":%s}",
                              merge->points, merge->both_sides, merge->pe_only, merge->pb_only,
                              merge->complete ? "true" : "false") != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_valuation_format_fund(tdx_buf *out, const tdx_valuation_fund *fund, size_t row,
                              tdx_error *err) {
    if (!out || !fund) {
        tdx_error_set(err, "rendering a fund needs a buffer and a fund");
        return TDX_ERR;
    }
    if (VAL_LITERAL(out, err, "{\"type\":\"valuation_fund\",\"row\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              "%zu,\"security_id\":\"%s\",\"market_id\":%d,"
                              "\"unit_nav\":",
                              row, fund->security_id, fund->market_id) != TDX_OK)
        return TDX_ERR;
    if (append_number(out, fund->has_unit_nav, fund->unit_nav, err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"premium_pct\":") != TDX_OK ||
        append_number(out, fund->has_premium_pct, fund->premium_pct, err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"size_yuan\":") != TDX_OK ||
        append_number(out, fund->has_size_yuan, fund->size_yuan, err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"fund_type\":") != TDX_OK ||
        append_text(out, &fund->fund_type, err) != TDX_OK)
        return TDX_ERR;
    /* The closing brace, as in the point renderer: the same mistake, in the renderer the
     * test did not cover. */
    return tdx_buf_push(out, '}', err);
}

int tdx_valuation_format_summary(tdx_buf *out, const char *resource, size_t indices,
                                 size_t skipped, const char *endpoint, tdx_error *err) {
    if (!out) {
        tdx_error_set(err, "the valuation summary needs a buffer");
        return TDX_ERR;
    }
    if (VAL_LITERAL(out, err, "{\"type\":\"valuation_summary\",\"resource\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, resource ? resource : "", err) != TDX_OK)
        return TDX_ERR;
    if (VAL_LITERAL(out, err, ",\"endpoint\":") != TDX_OK)
        return TDX_ERR;
    if (endpoint && *endpoint) {
        if (tdx_format_json_string(out, endpoint, err) != TDX_OK)
            return TDX_ERR;
    } else if (VAL_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err, ",\"indices\":%zu,\"rows_skipped\":%zu}",
                              indices, skipped) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}
