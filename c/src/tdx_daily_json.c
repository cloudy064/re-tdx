/* tdx_daily_json.c - JSONL rendering for the .day daily bars. */
#include "tdx_daily_json.h"

#include <stdio.h>
#include <string.h>

#include "tdx_format.h"

/* The argument must be a string literal: sizeof sizes it, and a computed expression decays
 * to a pointer, so `cond ? "true" : "false"` would copy sizeof(char*) - 1 bytes. */
#define DAILY_LITERAL(buf, err, text) tdx_buf_append((buf), (text), sizeof(text) - 1, (err))

int tdx_daily_format_bar(tdx_buf *out, const tdx_daily_bar *bar, const char *security_id,
                         int scale_divisor, size_t index, tdx_error *err) {
    if (!out || !bar) {
        tdx_error_set(err, "rendering a daily bar needs a buffer and a bar");
        return TDX_ERR;
    }
    if (DAILY_LITERAL(out, err, "{\"type\":\"daily_bar\",\"security_id\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, security_id ? security_id : "", err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"index\":%zu,\"scale_divisor\":%d,\"date\":%u,"
                              "\"open\":%.4f,\"high\":%.4f,\"low\":%.4f,\"close\":%.4f,"
                              "\"amount\":%.2f,\"volume\":%u}",
                              index, scale_divisor, bar->date, bar->open, bar->high, bar->low,
                              bar->close, bar->amount, bar->volume) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_daily_format_summary(tdx_buf *out, const char *source, const char *security_id,
                             int scale_divisor, size_t bytes, size_t bars,
                             size_t suspicious_prices, size_t first_date, size_t last_date,
                             tdx_error *err) {
    if (!out) {
        tdx_error_set(err, "the daily summary needs a buffer");
        return TDX_ERR;
    }
    if (DAILY_LITERAL(out, err, "{\"type\":\"daily_summary\",\"source\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, source ? source : "", err) != TDX_OK)
        return TDX_ERR;
    if (DAILY_LITERAL(out, err, ",\"security_id\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, security_id ? security_id : "", err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"scale_divisor\":%d,\"bytes\":%zu,\"record_size\":%d,"
                              "\"bars\":%zu,\"suspicious_prices\":%zu",
                              scale_divisor, bytes, TDX_DAILY_RECORD_SIZE, bars,
                              suspicious_prices) != TDX_OK)
        return TDX_ERR;
    if (first_date && last_date) {
        if (tdx_buf_append_printf(out, err, ",\"first_date\":%zu,\"last_date\":%zu}", first_date,
                                  last_date) != TDX_OK)
            return TDX_ERR;
    } else if (DAILY_LITERAL(out, err, ",\"first_date\":null,\"last_date\":null}") != TDX_OK) {
        return TDX_ERR;
    }
    return TDX_OK;
}
