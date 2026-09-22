/* tdx_limits_json.c - JSONL rendering for the 0x0452 special-limit list. */
#include "tdx_limits_json.h"

#include <stdio.h>
#include <string.h>

#include "tdx_format.h"

#define APPEND_LITERAL(buf, err, text) tdx_buf_append((buf), (text), sizeof(text) - 1, (err))
/* The argument must be a string literal.  This macro sizes it with sizeof, and a
 * computed expression decays to a pointer, so `cond ? "true" : "false"` would
 * copy sizeof(char*) - 1 bytes.  Pass an expression to tdx_buf_append_printf. */

static const char *upper_prefix(int market_id) {
    switch (market_id) {
    case 1:
        return "SH";
    case 2:
        return "BJ";
    default:
        return "SZ";
    }
}

int tdx_limits_format(tdx_buf *out, const tdx_limit_record *record, unsigned index,
                      tdx_error *err) {
    char identity[16];

    if (!out || !record) {
        tdx_error_set(err, "limit rendering needs a buffer and a record");
        return TDX_ERR;
    }
    snprintf(identity, sizeof(identity), "%s%s", upper_prefix(record->security.market_id),
             record->security.code);
    if (APPEND_LITERAL(out, err, "{\"type\":\"special_limit\",\"index\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, "%u,\"security_id\":", index) != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, identity, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"market_id\":%d,\"code\":\"%s\",\"code_number\":%u,"
                              "\"limit_up_price\":%.6f,\"limit_down_price\":%.6f,"
                              "\"band_percent\":%.4f}",
                              record->security.market_id, record->security.code,
                              (unsigned)record->code_number, record->limit_up,
                              record->limit_down,
                              /* The band the two prices imply when they straddle the
                               * midpoint, which is what they do on live data.  It is
                               * derived here so a reader does not have to guess the
                               * rule; it is not a wire field. */
                              (record->limit_up + record->limit_down) > 0.0
                                  ? 100.0 * (record->limit_up - record->limit_down) /
                                        (record->limit_up + record->limit_down)
                                  : 0.0) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_limits_format_summary(tdx_buf *out, const tdx_limit_record *records, size_t count,
                              unsigned start_index, unsigned next_index, const char *endpoint,
                              tdx_error *err) {
    size_t index;
    int all_ordered = 1;

    if (!out || !records) {
        tdx_error_set(err, "the limit summary needs a buffer and the records");
        return TDX_ERR;
    }
    for (index = 0; index < count; ++index)
        if (records[index].limit_up <= records[index].limit_down)
            all_ordered = 0;

    if (APPEND_LITERAL(out, err, "{\"type\":\"special_limit_summary\",\"command\":\"0x0452\"") !=
        TDX_OK)
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
                              ",\"start_index\":%u,\"next_index\":%u,\"record_count\":%zu,"
                              "\"every_band_ordered\":",
                              start_index, next_index, count) != TDX_OK)
        return TDX_ERR;
    if (all_ordered) {
        if (APPEND_LITERAL(out, err, "true") != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "false") != TDX_OK) {
        return TDX_ERR;
    }
    /* Close the summary object.  The brace-balance check in the test is what
     * catches a missing one. */
    return tdx_buf_push(out, '}', err);
}
