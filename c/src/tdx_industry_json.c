/* tdx_industry_json.c - JSONL rendering for the industry resource. */
#include "tdx_industry_json.h"

#include <stdio.h>
#include <string.h>

#include "tdx_format.h"

/* The argument must be a string literal: sizeof sizes it, and a computed expression decays
 * to a pointer, so `cond ? "a" : "b"` would copy sizeof(char*) - 1 bytes. */
#define INDUSTRY_LITERAL(buf, err, text) tdx_buf_append((buf), (text), sizeof(text) - 1, (err))

static int append_number(tdx_buf *out, int has, double value, tdx_error *err) {
    if (!has)
        return INDUSTRY_LITERAL(out, err, "null");
    return tdx_buf_append_printf(out, err, "%.6f", value);
}

static int append_text(tdx_buf *out, const tdx_bond_text *text, tdx_error *err) {
    char scratch[256];
    if (!text->present || !text->data)
        return INDUSTRY_LITERAL(out, err, "null");
    if (text->length >= sizeof(scratch)) {
        tdx_error_set(err, "an industry text field is %zu bytes, longer than this renderer "
                           "holds", text->length);
        return TDX_ERR;
    }
    memcpy(scratch, text->data, text->length);
    scratch[text->length] = '\0';
    return tdx_format_json_string(out, scratch, err);
}

int tdx_industry_format(tdx_buf *out, const tdx_industry *industry, size_t index,
                        tdx_error *err) {
    if (!out || !industry) {
        tdx_error_set(err, "rendering an industry needs a buffer and an industry");
        return TDX_ERR;
    }
    if (INDUSTRY_LITERAL(out, err, "{\"type\":\"industry\",\"index\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, "%zu,\"code\":\"%s\",\"security_id\":\"%s\","
                                        "\"market_id\":%d,\"name\":",
                              index, industry->code, industry->security_id,
                              industry->market_id) != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, industry->name, err) != TDX_OK)
        return TDX_ERR;
    if (INDUSTRY_LITERAL(out, err, ",\"pe_ttm\":") != TDX_OK ||
        append_number(out, industry->has_pe, industry->pe, err) != TDX_OK)
        return TDX_ERR;
    if (INDUSTRY_LITERAL(out, err, ",\"pb_mrq\":") != TDX_OK ||
        append_number(out, industry->has_pb, industry->pb, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"rows\":%zu,\"declared\":%zu,\"counts_agree\":%s,"
                              "\"inconsistent\":%s}",
                              industry->row_count, industry->declared_count,
                              industry->counts_agree ? "true" : "false",
                              industry->inconsistent ? "true" : "false") != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_industry_format_row(tdx_buf *out, const tdx_industry_row *row, size_t index,
                            tdx_error *err) {
    if (!out || !row) {
        tdx_error_set(err, "rendering a stock-to-industry row needs a buffer and a row");
        return TDX_ERR;
    }
    if (INDUSTRY_LITERAL(out, err, "{\"type\":\"stock_industry\",\"index\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              "%zu,\"security_id\":\"%s\",\"stock_code\":\"%.*s\","
                              "\"stock_market_id\":%d,\"industry_code\":\"%s\","
                              "\"industry_security_id\":\"%s\",\"industry_market_id\":%d,"
                              "\"industry_name\":",
                              index, row->stock_security_id, (int)row->stock_code.length,
                              row->stock_code.data ? row->stock_code.data : "",
                              row->stock_market_id, row->industry_code,
                              row->industry_security_id, row->industry_market_id) != TDX_OK)
        return TDX_ERR;
    if (append_text(out, &row->industry_name, err) != TDX_OK)
        return TDX_ERR;
    if (INDUSTRY_LITERAL(out, err, ",\"pe_ttm\":") != TDX_OK ||
        append_number(out, row->has_pe, row->pe, err) != TDX_OK)
        return TDX_ERR;
    if (INDUSTRY_LITERAL(out, err, ",\"pb_mrq\":") != TDX_OK ||
        append_number(out, row->has_pb, row->pb, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"declared_count\":%zu,\"theme_count\":%zu}",
                              row->declared_count, row->theme_count) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_industry_format_summary(tdx_buf *out, const char *resource, size_t rows,
                                size_t skipped, size_t industries, size_t agreeing,
                                size_t inconsistent, const char *endpoint, tdx_error *err) {
    if (!out) {
        tdx_error_set(err, "the industry summary needs a buffer");
        return TDX_ERR;
    }
    if (INDUSTRY_LITERAL(out, err, "{\"type\":\"industry_summary\",\"resource\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, resource ? resource : "", err) != TDX_OK)
        return TDX_ERR;
    if (INDUSTRY_LITERAL(out, err, ",\"endpoint\":") != TDX_OK)
        return TDX_ERR;
    if (endpoint && *endpoint) {
        if (tdx_format_json_string(out, endpoint, err) != TDX_OK)
            return TDX_ERR;
    } else if (INDUSTRY_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err,
                              ",\"rows\":%zu,\"rows_skipped\":%zu,\"industries\":%zu,"
                              "\"industries_whose_counts_agree\":%zu,"
                              "\"industries_inconsistent\":%zu}",
                              rows, skipped, industries, agreeing, inconsistent) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}
