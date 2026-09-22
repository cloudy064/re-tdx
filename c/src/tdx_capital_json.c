/* tdx_capital_json.c - JSONL rendering for the 0x000F capital-change feed. */
#include "tdx_capital_json.h"

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

static int append_date(tdx_buf *out, int date, tdx_error *err) {
    if (!date)
        return APPEND_LITERAL(out, err, "null");
    return tdx_buf_append_printf(out, err, "\"%04d-%02d-%02d\"", date / 10000, date / 100 % 100,
                                 date % 100);
}

/* The category-specific block.  Categories 1, 11..15 carry event values; every
 * other category carries share counts before and after the event. */
static int append_details(tdx_buf *out, const tdx_capital_record *record, tdx_error *err) {
    const double *value = record->float_values;
    const double *shares = record->share_values;

    if (APPEND_LITERAL(out, err, ",\"details\":{") != TDX_OK)
        return TDX_ERR;
    switch (record->category) {
    case 1:
        /* The first slot is per TEN shares, which is what the server sends; the
         * per-share figure is derived here so a caller cannot inflate a dividend
         * tenfold by using the wrong basis. */
        if (tdx_buf_append_printf(out, err,
                                  "\"dividend_per_10_shares_yuan\":%.4f,"
                                  "\"dividend_per_share_yuan\":%.6f,"
                                  "\"rights_price_yuan\":%.4f,"
                                  "\"bonus_transfer_per_10_shares\":%.4f,"
                                  "\"rights_per_10_shares\":%.4f",
                                  value[0], value[0] / 10.0, value[1], value[2],
                                  value[3]) != TDX_OK)
            return TDX_ERR;
        break;
    case 11:
    case 12:
        if (tdx_buf_append_printf(out, err, "\"shrink_ratio\":%.6f", value[2]) != TDX_OK)
            return TDX_ERR;
        break;
    case 13:
    case 14:
        if (tdx_buf_append_printf(out, err, "\"exercise_price_yuan\":%.4f,\"warrant_units\":%.4f",
                                  value[0], value[2]) != TDX_OK)
            return TDX_ERR;
        break;
    case 15:
        if (tdx_buf_append_printf(out, err,
                                  "\"value_1\":%.4f,\"value_2\":%.4f,\"value_3\":%.4f,"
                                  "\"value_4\":%.4f",
                                  value[0], value[1], value[2], value[3]) != TDX_OK)
            return TDX_ERR;
        break;
    default:
        if (tdx_buf_append_printf(out, err,
                                  "\"before_circulating_shares\":%.0f,"
                                  "\"before_total_shares\":%.0f,"
                                  "\"after_circulating_shares\":%.0f,"
                                  "\"after_total_shares\":%.0f",
                                  shares[0], shares[1], shares[2], shares[3]) != TDX_OK)
            return TDX_ERR;
        break;
    }
    /* Close the details object this function opened.  The brace-balance check in
     * the test is what catches it when a branch forgets. */
    return tdx_buf_push(out, '}', err);
}

int tdx_capital_format(tdx_buf *out, const tdx_capital_record *record, tdx_error *err) {
    char identity[16];

    if (!out || !record) {
        tdx_error_set(err, "capital rendering needs a buffer and a record");
        return TDX_ERR;
    }
    snprintf(identity, sizeof(identity), "%s%s", upper_prefix(record->security.market_id),
             record->security.code);
    if (APPEND_LITERAL(out, err, "{\"type\":\"capital_change\",\"security_id\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, identity, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"date\":") != TDX_OK)
        return TDX_ERR;
    if (append_date(out, record->date, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"category\":%u,\"category_name\":\"%s\"",
                              (unsigned)record->category,
                              tdx_capital_category_key(record->category)) != TDX_OK)
        return TDX_ERR;
    if (append_details(out, record, err) != TDX_OK)
        return TDX_ERR;
    /* Both readings of the four slots travel with every record.  They agree on all
     * live evidence, so a caller can see that rather than take it on trust. */
    if (tdx_buf_append_printf(out, err,
                              ",\"float32_values\":[%.6f,%.6f,%.6f,%.6f],"
                              "\"share_values\":[%.0f,%.0f,%.0f,%.0f],\"reserved\":%u}",
                              record->float_values[0], record->float_values[1],
                              record->float_values[2], record->float_values[3],
                              record->share_values[0], record->share_values[1],
                              record->share_values[2], record->share_values[3],
                              (unsigned)record->reserved) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

void tdx_capital_tally_init(tdx_capital_tally *tally) {
    if (!tally)
        return;
    memset(tally, 0, sizeof(*tally));
}

void tdx_capital_tally_add(tdx_capital_tally *tally, const tdx_capital_record *records,
                           size_t count) {
    size_t index;
    if (!tally || !records)
        return;
    tally->securities++;
    for (index = 0; index < count; ++index) {
        tally->record_count++;
        if (records[index].category < 16)
            tally->category_counts[records[index].category]++;
        if (records[index].date) {
            if (!tally->earliest_date || records[index].date < tally->earliest_date)
                tally->earliest_date = records[index].date;
            if (records[index].date > tally->latest_date)
                tally->latest_date = records[index].date;
        }
    }
}

int tdx_capital_format_summary(tdx_buf *out, const tdx_capital_tally *tally, size_t requested,
                               const char *endpoint, tdx_error *err) {
    unsigned category;

    if (!out || !tally) {
        tdx_error_set(err, "capital summary rendering needs a buffer and a tally");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"capital_summary\",\"command\":\"0x000F\"") !=
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
                              ",\"securities\":%zu,\"requested\":%zu,\"record_count\":%zu",
                              tally->securities, requested, tally->record_count) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"earliest_date\":") != TDX_OK)
        return TDX_ERR;
    if (append_date(out, tally->earliest_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"latest_date\":") != TDX_OK)
        return TDX_ERR;
    if (append_date(out, tally->latest_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"category_counts\":{") != TDX_OK)
        return TDX_ERR;
    for (category = 1; category <= 15; ++category) {
        if (tdx_buf_append_printf(out, err, "%s\"%s\":%zu", category == 1 ? "" : ",",
                                  tdx_capital_category_key(category),
                                  tally->category_counts[category]) != TDX_OK)
            return TDX_ERR;
    }
    /* Two braces: one closes category_counts, one closes the summary object.
     * Forgetting the second is a mistake this project has made before, which is
     * why the test checks brace balance rather than trusting the eye. */
    if (APPEND_LITERAL(out, err, "}}") != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}
