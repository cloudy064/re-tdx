/* tdx_seal_json.c - JSONL rendering for the sealed-order figure. */
#include "tdx_seal_json.h"

#include <stdio.h>
#include <string.h>

#include "tdx_format.h"

/* The argument must be a string literal: sizeof sizes it, and a computed expression decays
 * to a pointer, so `cond ? "a" : "b"` would copy sizeof(char*) - 1 bytes. */
#define SEAL_LITERAL(buf, err, text) tdx_buf_append((buf), (text), sizeof(text) - 1, (err))

int tdx_seal_format(tdx_buf *out, const tdx_code *security, const tdx_limit_prices *limits,
                    const tdx_seal_result *seal, const tdx_seal_input *input,
                    const char *as_of, tdx_error *err) {
    if (!out || !security || !limits || !seal || !input) {
        tdx_error_set(err, "rendering a seal needs a buffer, a security, limits, a result and "
                           "its inputs");
        return TDX_ERR;
    }
    if (SEAL_LITERAL(out, err, "{\"type\":\"seal\",\"code\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, security->code, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"market_id\":%d,\"as_of\":", security->market_id) !=
        TDX_OK)
        return TDX_ERR;
    if (as_of && *as_of) {
        if (tdx_format_json_string(out, as_of, err) != TDX_OK)
            return TDX_ERR;
    } else if (SEAL_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    /* The limits first: without them no seal is possible, so they explain the result. */
    if (tdx_buf_append_printf(out, err,
                              ",\"limit\":{\"available\":%s,\"rate\":%.4f,"
                              "\"upper\":%.4f,\"lower\":%.4f,\"security_class\":%d,"
                              "\"source\":",
                              limits->available ? "true" : "false", limits->rate, limits->upper,
                              limits->lower, limits->security_class) != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, limits->source, err) != TDX_OK)
        return TDX_ERR;
    if (SEAL_LITERAL(out, err, "}") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"quote\":{\"last_price\":%.4f,\"total_hand\":%llu,"
                              "\"trade_unit\":%.0f,\"quote_flags\":%llu,"
                              "\"auction_imbalance_hand\":%lld,"
                              "\"bid1_price\":%.4f,\"bid1_volume_hand\":%lld,"
                              "\"ask1_price\":%.4f,\"ask1_volume_hand\":%lld,"
                              "\"bid2_price\":%.4f,\"bid2_volume_hand\":%lld,"
                              "\"ask2_price\":%.4f,\"ask2_volume_hand\":%lld}",
                              input->last_price, (unsigned long long)input->total_hand,
                              input->trade_unit, (unsigned long long)input->quote_flags,
                              (long long)input->auction_imbalance_hand, input->buys[0].price,
                              (long long)input->buys[0].volume_hand, input->sells[0].price,
                              (long long)input->sells[0].volume_hand, input->buys[1].price,
                              (long long)input->buys[1].volume_hand, input->sells[1].price,
                              (long long)input->sells[1].volume_hand) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"seal\":{\"available\":%s,\"direction\":%d,\"mode\":\"%s\","
                              "\"queue_hand\":%lld,\"denominator_hand\":%llu,"
                              "\"amount_yuan\":",
                              seal->available ? "true" : "false", seal->direction,
                              seal->mode ? seal->mode : "not-sealed",
                              (long long)seal->queue_hand,
                              (unsigned long long)seal->denominator_hand) != TDX_OK)
        return TDX_ERR;
    if (!seal->amount_available) {
        if (SEAL_LITERAL(out, err, "null") != TDX_OK)
            return TDX_ERR;
    } else if (tdx_buf_append_printf(out, err, "%.2f", seal->amount_yuan) != TDX_OK) {
        return TDX_ERR;
    }
    if (SEAL_LITERAL(out, err, ",\"ratio\":") != TDX_OK)
        return TDX_ERR;
    if (!seal->ratio_available) {
        if (SEAL_LITERAL(out, err, "null") != TDX_OK)
            return TDX_ERR;
    } else if (tdx_buf_append_printf(out, err, "%.6f", seal->ratio) != TDX_OK) {
        return TDX_ERR;
    }
    return SEAL_LITERAL(out, err, "}}");
}
