/* tdx_minute_json.c - JSONL rendering for the .lc1 minute bars. */
#include "tdx_minute_json.h"

#include <stdio.h>
#include <string.h>

#include "tdx_format.h"

/* The argument must be a string literal: sizeof sizes it, and a computed expression decays
 * to a pointer, so `cond ? "a" : "b"` would copy sizeof(char*) - 1 bytes. */
#define MINUTE_LITERAL(buf, err, text) tdx_buf_append((buf), (text), sizeof(text) - 1, (err))

int tdx_lc1_format_bar(tdx_buf *out, const tdx_lc1_bar *bar, const char *security_id,
                       size_t index, tdx_error *err) {
    if (!out || !bar) {
        tdx_error_set(err, "rendering a minute bar needs a buffer and a bar");
        return TDX_ERR;
    }
    if (MINUTE_LITERAL(out, err, "{\"type\":\"minute_bar\",\"security_id\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, security_id ? security_id : "", err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"index\":%zu,\"date\":%u,\"time\":\"%02d:%02d\","
                              "\"open\":%.3f,\"high\":%.3f,\"low\":%.3f,\"close\":%.3f,"
                              "\"amount\":%.2f,\"volume\":%u,\"extra_1\":%u,\"extra_2\":%u}",
                              index, bar->date, bar->hour, bar->minute, bar->open, bar->high,
                              bar->low, bar->close, bar->amount, bar->volume, bar->extra_1,
                              bar->extra_2) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_lc1_format_summary(tdx_buf *out, const char *source, const char *security_id,
                           size_t bytes, size_t bars, size_t ohlc_violations,
                           size_t distinct_dates, size_t first_date, size_t last_date,
                           tdx_error *err) {
    if (!out) {
        tdx_error_set(err, "the minute summary needs a buffer");
        return TDX_ERR;
    }
    if (MINUTE_LITERAL(out, err, "{\"type\":\"minute_summary\",\"source\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, source ? source : "", err) != TDX_OK)
        return TDX_ERR;
    if (MINUTE_LITERAL(out, err, ",\"security_id\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, security_id ? security_id : "", err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"bytes\":%zu,\"record_size\":%d,\"bars\":%zu,"
                              "\"distinct_dates\":%zu,\"ohlc_violations\":%zu",
                              bytes, TDX_LC1_RECORD_SIZE, bars, distinct_dates,
                              ohlc_violations) != TDX_OK)
        return TDX_ERR;
    if (first_date && last_date) {
        if (tdx_buf_append_printf(out, err, ",\"first_date\":%zu,\"last_date\":%zu}",
                                  first_date, last_date) != TDX_OK)
            return TDX_ERR;
    } else if (MINUTE_LITERAL(out, err, ",\"first_date\":null,\"last_date\":null}") != TDX_OK) {
        return TDX_ERR;
    }
    return TDX_OK;
}
