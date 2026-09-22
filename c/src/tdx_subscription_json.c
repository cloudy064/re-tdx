/* tdx_subscription_json.c - JSONL rendering for subscription events. */
#include "tdx_subscription_json.h"

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
        tdx_error_set(err, "a subscription text field is %zu bytes, longer than this renderer "
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

/* A market/code identity as its own object, the shape the rest of the project uses. */
static int append_identity(tdx_buf *out, const char *market, int market_id,
                           const char *security_id, const tdx_bond_text *code,
                           const tdx_bond_text *name, tdx_error *err) {
    if (APPEND_LITERAL(out, err, "{\"market\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, market, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"market_id\":%d,\"security_id\":", market_id) != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, security_id, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"code\":") != TDX_OK ||
        append_text(out, code, err) != TDX_OK)
        return TDX_ERR;
    if (name) {
        if (APPEND_LITERAL(out, err, ",\"name\":") != TDX_OK ||
            append_text(out, name, err) != TDX_OK)
            return TDX_ERR;
    }
    return tdx_buf_push(out, '}', err);
}

int tdx_subscription_format(tdx_buf *out, const tdx_subscription_row *row, const char *resource,
                            size_t row_index, tdx_error *err) {
    tdx_bond_text no_name;

    if (!out || !row) {
        tdx_error_set(err, "subscription rendering needs a buffer and a row");
        return TDX_ERR;
    }
    memset(&no_name, 0, sizeof(no_name));
    if (APPEND_LITERAL(out, err, "{\"type\":\"convertible_bond_subscription\",\"resource\":") !=
        TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, resource ? resource : "", err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"row\":%zu,\"kind\":\"convertible-bond-subscription\","
                                        "\"event_id\":",
                              row_index) != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, row->event_id, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"bond\":") != TDX_OK ||
        append_identity(out, row->bond_market, row->bond_market_id, row->bond_security_id,
                        &row->bond_code, &row->bond_name, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"underlying\":") != TDX_OK ||
        append_identity(out, row->stock_market, row->stock_market_id, row->stock_security_id,
                        &row->stock_code, &no_name, err) != TDX_OK)
        return TDX_ERR;

    if (APPEND_LITERAL(out, err, ",\"subscription\":{\"date\":") != TDX_OK ||
        append_text(out, &row->subscription_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"code\":") != TDX_OK ||
        append_text(out, &row->subscription_code, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"limit_10k_yuan\":") != TDX_OK ||
        append_number(out, row->has_subscription_limit_10k_yuan, row->subscription_limit_10k_yuan,
                      err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"conversion_start_date\":") != TDX_OK ||
        append_text(out, &row->conversion_start_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        return TDX_ERR;

    /* The inputs and the two derived metrics sit together, because a reader has to be
     * able to see what the derivation had to work with. */
    if (APPEND_LITERAL(out, err, ",\"valuation\":{\"underlying_close_yuan\":") != TDX_OK ||
        append_number(out, row->has_underlying_close_yuan, row->underlying_close_yuan, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"conversion_price_yuan\":") != TDX_OK ||
        append_number(out, row->has_conversion_price_yuan, row->conversion_price_yuan, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"conversion_value_yuan\":") != TDX_OK ||
        append_number(out, row->has_conversion_value_yuan, row->conversion_value_yuan, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"bond_close_yuan\":") != TDX_OK ||
        append_number(out, row->has_bond_close_yuan, row->bond_close_yuan, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"conversion_premium_pct\":") != TDX_OK ||
        append_number(out, row->has_conversion_premium_pct, row->conversion_premium_pct, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        return TDX_ERR;

    if (APPEND_LITERAL(out, err, ",\"issue\":{\"size_100m_yuan\":") != TDX_OK ||
        append_number(out, row->has_issue_size_100m_yuan, row->issue_size_100m_yuan, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"lottery_date\":") != TDX_OK ||
        append_text(out, &row->lottery_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"lottery_rate_pct\":") != TDX_OK ||
        append_number(out, row->has_lottery_rate_pct, row->lottery_rate_pct, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"listing_date\":") != TDX_OK ||
        append_text(out, &row->listing_date, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"listed\":%s}",
                              row->listed ? "true" : "false") != TDX_OK)
        return TDX_ERR;
    /* Close the row.  The brace-balance check in the test is what catches a missing
     * one, which this project has needed five times. */
    return tdx_buf_push(out, '}', err);
}

int tdx_subscription_format_summary(tdx_buf *out, size_t rows, size_t skipped, size_t listed,
                                    size_t with_conversion_value, size_t with_premium,
                                    const char *resource, const char *endpoint, tdx_error *err) {
    if (!out) {
        tdx_error_set(err, "the subscription summary needs a buffer");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"convertible_subscription_summary\","
                                "\"resource\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, resource ? resource : "", err) != TDX_OK)
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
                              ",\"rows\":%zu,\"rows_skipped\":%zu,\"rows_listed\":%zu,"
                              "\"rows_with_conversion_value\":%zu,\"rows_with_premium\":%zu}",
                              rows, skipped, listed, with_conversion_value, with_premium) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}
