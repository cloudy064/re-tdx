/* tdx_finance_json.c - JSONL rendering for the 0x0010 finance feed. */
#include "tdx_finance_json.h"

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

/* YYYYMMDD as a JSON string, or null when the field carried no date. */
static int append_date(tdx_buf *out, int date, tdx_error *err) {
    if (!date)
        return APPEND_LITERAL(out, err, "null");
    return tdx_buf_append_printf(out, err, "\"%04d-%02d-%02d\"", date / 10000,
                                 date / 100 % 100, date % 100);
}

int tdx_finance_format(tdx_buf *out, const tdx_finance_record *record, tdx_error *err) {
    char identity[16];

    if (!out || !record) {
        tdx_error_set(err, "finance rendering needs a buffer and a record");
        return TDX_ERR;
    }
    snprintf(identity, sizeof(identity), "%s%s", upper_prefix(record->security.market_id),
             record->security.code);
    if (APPEND_LITERAL(out, err, "{\"type\":\"finance\",\"security_id\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, identity, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"province_id\":%u,\"industry_id\":%u,\"updated_date\":",
                              (unsigned)record->province_id,
                              (unsigned)record->industry_id) != TDX_OK)
        return TDX_ERR;
    if (append_date(out, record->updated_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"listing_date\":") != TDX_OK)
        return TDX_ERR;
    if (append_date(out, record->listing_date, err) != TDX_OK)
        return TDX_ERR;

    if (tdx_buf_append_printf(out, err,
                              ",\"shares\":{\"circulating\":%.0f,\"total\":%.0f,"
                              "\"national\":%.0f,\"promoter_legal_person\":%.0f,"
                              "\"legal_person\":%.0f,\"b_share\":%.0f,\"h_share\":%.0f}",
                              record->circulating_shares, record->total_shares,
                              record->national_shares, record->promoter_legal_shares,
                              record->legal_shares, record->b_shares,
                              record->h_shares) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"per_share\":{\"eps\":%.4f,\"net_assets\":%.4f},"
                              "\"shareholder_count\":%.0f",
                              record->eps, record->net_assets_per_share,
                              record->shareholder_count) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"balance_sheet_yuan\":{\"total_assets\":%.0f,"
                              "\"current_assets\":%.0f,\"fixed_assets\":%.0f,"
                              "\"intangible_assets\":%.0f,\"current_liabilities\":%.0f,"
                              "\"long_term_liabilities\":%.0f,\"capital_reserve\":%.0f,"
                              "\"net_assets\":%.0f,\"accounts_receivable\":%.0f,"
                              "\"inventory\":%.0f}",
                              record->total_assets, record->current_assets,
                              record->fixed_assets, record->intangible_assets,
                              record->current_liabilities, record->long_term_liabilities,
                              record->capital_reserve, record->net_assets,
                              record->accounts_receivable, record->inventory) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"income_statement_yuan\":{\"revenue\":%.0f,"
                              "\"main_profit\":%.0f,\"operating_profit\":%.0f,"
                              "\"investment_income\":%.0f,\"total_profit\":%.0f,"
                              "\"after_tax_profit\":%.0f,\"net_profit\":%.0f,"
                              "\"undistributed_profit\":%.0f},"
                              "\"cash_flow_yuan\":{\"operating\":%.0f,\"total\":%.0f},"
                              "\"reserved_2_raw\":%.6f}",
                              record->revenue, record->main_profit, record->operating_profit,
                              record->investment_income, record->total_profit,
                              record->after_tax_profit, record->net_profit,
                              record->undistributed_profit, record->operating_cash_flow,
                              record->total_cash_flow, record->reserved_2_raw) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

void tdx_finance_tally_init(tdx_finance_tally *tally) {
    if (!tally)
        return;
    memset(tally, 0, sizeof(*tally));
}

void tdx_finance_tally_add(tdx_finance_tally *tally, const tdx_finance_record *records,
                           size_t count) {
    size_t index;
    if (!tally || !records)
        return;
    for (index = 0; index < count; ++index) {
        tally->record_count++;
        tally->total_shares_sum += records[index].total_shares;
        tally->total_assets_sum += records[index].total_assets;
        if (records[index].listing_date)
            tally->records_with_listing_date++;
    }
}

int tdx_finance_format_summary(tdx_buf *out, const tdx_finance_tally *tally,
                               const char *endpoint, tdx_error *err) {
    if (!out || !tally) {
        tdx_error_set(err, "finance summary rendering needs a buffer and a tally");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"finance_summary\",\"command\":\"0x0010\"") !=
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
                              ",\"record_count\":%zu,\"records_with_listing_date\":%zu,"
                              "\"total_shares_sum\":%.0f,\"total_assets_sum_yuan\":%.0f}",
                              tally->record_count, tally->records_with_listing_date,
                              tally->total_shares_sum, tally->total_assets_sum) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}
