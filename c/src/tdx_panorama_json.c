/* tdx_panorama_json.c - JSONL rendering for the market panorama. */
#include "tdx_panorama_json.h"

#include <stdio.h>
#include <string.h>

#include "tdx_format.h"

/* The argument must be a string literal: sizeof sizes it, and a computed expression decays to a
 * pointer, so `cond ? "a" : "b"` would copy sizeof(char*) - 1 bytes. */
#define PAN_LITERAL(buf, err, text) tdx_buf_append((buf), (text), sizeof(text) - 1, (err))

int tdx_panorama_format_view(tdx_buf *out, const tdx_panorama_view *view, size_t index,
                             tdx_error *err) {
    size_t field;

    if (!out || !view) {
        tdx_error_set(err, "rendering a panorama view needs a buffer and a view");
        return TDX_ERR;
    }
    if (PAN_LITERAL(out, err, "{\"type\":\"panorama_view\",\"index\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, "%zu,\"id\":", index) != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, view->id, err) != TDX_OK)
        return TDX_ERR;
    if (PAN_LITERAL(out, err, ",\"label\":") != TDX_OK ||
        tdx_format_json_string(out, view->label, err) != TDX_OK)
        return TDX_ERR;
    if (PAN_LITERAL(out, err, ",\"resource\":") != TDX_OK ||
        tdx_format_json_string(out, view->resource, err) != TDX_OK)
        return TDX_ERR;
    if (PAN_LITERAL(out, err, ",\"market_field\":") != TDX_OK ||
        tdx_format_json_string(out, view->market_field, err) != TDX_OK)
        return TDX_ERR;
    if (PAN_LITERAL(out, err, ",\"code_field\":") != TDX_OK ||
        tdx_format_json_string(out, view->code_field, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"field_count\":%zu,\"fields\":[",
                              view->field_count) != TDX_OK)
        return TDX_ERR;
    for (field = 0; field < view->field_count; ++field) {
        if (field && PAN_LITERAL(out, err, ",") != TDX_OK)
            return TDX_ERR;
        if (PAN_LITERAL(out, err, "{\"name\":") != TDX_OK ||
            tdx_format_json_string(out, view->fields[field].name, err) != TDX_OK)
            return TDX_ERR;
        if (PAN_LITERAL(out, err, ",\"column\":") != TDX_OK ||
            tdx_format_json_string(out, view->fields[field].column, err) != TDX_OK)
            return TDX_ERR;
        if (PAN_LITERAL(out, err, "}") != TDX_OK)
            return TDX_ERR;
    }
    return PAN_LITERAL(out, err, "]}");
}

int tdx_panorama_format_row(tdx_buf *out, const tdx_panorama_view *view,
                            const tdx_panorama_row *row, size_t index, tdx_error *err) {
    size_t field;
    char scratch[TDX_PANORAMA_TEXT_MAX];

    if (!out || !view || !row) {
        tdx_error_set(err, "rendering a panorama row needs a buffer, a view and a row");
        return TDX_ERR;
    }
    if (PAN_LITERAL(out, err, "{\"type\":\"panorama_row\",\"view\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, view->id, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"index\":%zu,\"security_id\":\"%s\",\"market_id\":%d,"
                              "\"code\":\"%s\",\"fields\":{",
                              index, row->security_id, row->market_id, row->code) != TDX_OK)
        return TDX_ERR;
    for (field = 0; field < view->field_count; ++field) {
        const tdx_bond_text *value = &row->values[field];
        if (field && PAN_LITERAL(out, err, ",") != TDX_OK)
            return TDX_ERR;
        if (tdx_format_json_string(out, view->fields[field].name, err) != TDX_OK)
            return TDX_ERR;
        if (PAN_LITERAL(out, err, ":") != TDX_OK)
            return TDX_ERR;
        /* AN ABSENT FIELD IS null, NOT AN EMPTY STRING: the resource distinguishes "the column
         * is not there for this row" from "the column is empty", and so does this. */
        if (!row->present[field] || !value->data) {
            if (PAN_LITERAL(out, err, "null") != TDX_OK)
                return TDX_ERR;
            continue;
        }
        if (value->length >= sizeof(scratch)) {
            tdx_error_set(err, "field %s of %s is %zu bytes, longer than this renderer holds",
                          view->fields[field].name, row->security_id, value->length);
            return TDX_ERR;
        }
        memcpy(scratch, value->data, value->length);
        scratch[value->length] = '\0';
        if (tdx_format_json_string(out, scratch, err) != TDX_OK)
            return TDX_ERR;
    }
    return PAN_LITERAL(out, err, "}}");
}

int tdx_panorama_format_summary(tdx_buf *out, const char *view_id, const char *resource,
                                size_t rows, size_t skipped, const char *endpoint,
                                tdx_error *err) {
    if (!out) {
        tdx_error_set(err, "the panorama summary needs a buffer");
        return TDX_ERR;
    }
    if (PAN_LITERAL(out, err, "{\"type\":\"panorama_summary\",\"view\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, view_id ? view_id : "", err) != TDX_OK)
        return TDX_ERR;
    if (PAN_LITERAL(out, err, ",\"resource\":") != TDX_OK)
        return TDX_ERR;
    if (resource && *resource) {
        if (tdx_format_json_string(out, resource, err) != TDX_OK)
            return TDX_ERR;
    } else if (PAN_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err, ",\"rows\":%zu,\"rows_skipped\":%zu,\"endpoint\":",
                              rows, skipped) != TDX_OK)
        return TDX_ERR;
    if (endpoint && *endpoint) {
        if (tdx_format_json_string(out, endpoint, err) != TDX_OK)
            return TDX_ERR;
    } else if (PAN_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    return tdx_buf_push(out, '}', err);
}
