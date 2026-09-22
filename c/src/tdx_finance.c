/* tdx_finance.c - 0x0010 batch fundamental data, ported from the C++ tool. */
#include "tdx_finance.h"

#include <math.h>
#include <string.h>

/* The two wire scales, kept named because mixing them up is the obvious bug:
 * share counts arrive in ten-thousands and money in thousands. */
#define TDX_FINANCE_SHARE_SCALE 10000.0
#define TDX_FINANCE_YUAN_SCALE 1000.0

static double read_f32(const uint8_t *data) {
    float raw;
    memcpy(&raw, data, sizeof(raw));
    return (double)raw;
}

static int finite_value(double value, const char *name, tdx_error *err) {
    if (isfinite(value))
        return 1;
    tdx_error_set(err, "finance record field %s is not finite", name);
    return 0;
}

static int date_word(uint32_t raw, const char *name, int *out, tdx_error *err) {
    int year = (int)(raw / 10000u);
    int month = (int)(raw / 100u % 100u);
    int day = (int)(raw % 100u);
    if (raw == 0) {
        /* A missing date is normal for a freshly listed or delisted security. */
        *out = 0;
        return 1;
    }
    if (year < 1980 || year > 2200 || month < 1 || month > 12 || day < 1 || day > 31) {
        tdx_error_set(err, "finance record %s is not a date: %u", name, (unsigned)raw);
        return 0;
    }
    *out = (int)raw;
    return 1;
}

int tdx_finance_build_request(const tdx_code *codes, size_t count, tdx_buf *out,
                              tdx_error *err) {
    size_t index;

    if (!out) {
        tdx_error_set(err, "0x0010 needs an output buffer");
        return TDX_ERR;
    }
    if (!codes || count == 0) {
        tdx_error_set(err, "0x0010 needs at least one security");
        return TDX_ERR;
    }
    if (count > 0xFFFFu) {
        tdx_error_set(err, "0x0010 batch of %zu exceeds the 16-bit count field", count);
        return TDX_ERR;
    }
    for (index = 0; index < count; ++index) {
        const tdx_code *code = &codes[index];
        size_t digit;
        if (code->market_id < 0 || code->market_id > 2) {
            tdx_error_set(err, "finance market must be 0, 1 or 2, got %d", code->market_id);
            return TDX_ERR;
        }
        if (strlen(code->code) != 6) {
            tdx_error_set(err, "finance needs six-digit codes, got %s", code->code);
            return TDX_ERR;
        }
        for (digit = 0; digit < 6; ++digit)
            if (code->code[digit] < '0' || code->code[digit] > '9') {
                tdx_error_set(err, "finance code must be six digits, got %s", code->code);
                return TDX_ERR;
            }
    }
    tdx_buf_clear(out);
    if (tdx_buf_append_u16le(out, (uint16_t)count, err) != TDX_OK)
        return TDX_ERR;
    for (index = 0; index < count; ++index) {
        if (tdx_buf_push(out, (uint8_t)codes[index].market_id, err) != TDX_OK ||
            tdx_buf_append(out, codes[index].code, 6, err) != TDX_OK)
            return TDX_ERR;
    }
    return TDX_OK;
}

int tdx_finance_parse_record(const uint8_t *record, size_t size, tdx_finance_record *out,
                             tdx_error *err) {
    const uint8_t *info;
    size_t digit;

    if (!record || !out) {
        tdx_error_set(err, "finance record needs data and an output");
        return TDX_ERR;
    }
    if (size != TDX_FINANCE_RECORD_SIZE) {
        tdx_error_set(err, "finance record is %zu bytes, expected %d", size,
                      TDX_FINANCE_RECORD_SIZE);
        return TDX_ERR;
    }
    memset(out, 0, sizeof(*out));
    out->security.market_id = record[0];
    if (out->security.market_id < 0 || out->security.market_id > 2) {
        tdx_error_set(err, "finance record has market %d", out->security.market_id);
        return TDX_ERR;
    }
    for (digit = 0; digit < 6; ++digit) {
        char ch = (char)record[1 + digit];
        if (ch < '0' || ch > '9') {
            tdx_error_set(err, "finance record code is not six digits");
            return TDX_ERR;
        }
        out->security.code[digit] = ch;
    }
    out->security.code[6] = '\0';

    info = record + 7;
    out->circulating_shares = read_f32(info) * TDX_FINANCE_SHARE_SCALE;
    out->province_id = tdx_u16le(info + 4);
    out->industry_id = tdx_u16le(info + 6);
    if (!date_word(tdx_u32le(info + 8), "updated_date", &out->updated_date, err))
        return TDX_ERR;
    if (!date_word(tdx_u32le(info + 12), "listing_date", &out->listing_date, err))
        return TDX_ERR;

    out->total_shares = read_f32(info + 16) * TDX_FINANCE_SHARE_SCALE;
    out->national_shares = read_f32(info + 20) * TDX_FINANCE_SHARE_SCALE;
    out->promoter_legal_shares = read_f32(info + 24) * TDX_FINANCE_SHARE_SCALE;
    out->legal_shares = read_f32(info + 28) * TDX_FINANCE_SHARE_SCALE;
    out->b_shares = read_f32(info + 32) * TDX_FINANCE_SHARE_SCALE;
    out->h_shares = read_f32(info + 36) * TDX_FINANCE_SHARE_SCALE;

    out->eps = read_f32(info + 40);
    out->total_assets = read_f32(info + 44) * TDX_FINANCE_YUAN_SCALE;
    out->current_assets = read_f32(info + 48) * TDX_FINANCE_YUAN_SCALE;
    out->fixed_assets = read_f32(info + 52) * TDX_FINANCE_YUAN_SCALE;
    out->intangible_assets = read_f32(info + 56) * TDX_FINANCE_YUAN_SCALE;
    out->shareholder_count = read_f32(info + 60);
    out->current_liabilities = read_f32(info + 64) * TDX_FINANCE_YUAN_SCALE;
    out->long_term_liabilities = read_f32(info + 68) * TDX_FINANCE_YUAN_SCALE;
    out->capital_reserve = read_f32(info + 72) * TDX_FINANCE_YUAN_SCALE;
    out->net_assets = read_f32(info + 76) * TDX_FINANCE_YUAN_SCALE;
    out->revenue = read_f32(info + 80) * TDX_FINANCE_YUAN_SCALE;
    out->main_profit = read_f32(info + 84) * TDX_FINANCE_YUAN_SCALE;
    out->accounts_receivable = read_f32(info + 88) * TDX_FINANCE_YUAN_SCALE;
    out->operating_profit = read_f32(info + 92) * TDX_FINANCE_YUAN_SCALE;
    out->investment_income = read_f32(info + 96) * TDX_FINANCE_YUAN_SCALE;
    out->operating_cash_flow = read_f32(info + 100) * TDX_FINANCE_YUAN_SCALE;
    out->total_cash_flow = read_f32(info + 104) * TDX_FINANCE_YUAN_SCALE;
    out->inventory = read_f32(info + 108) * TDX_FINANCE_YUAN_SCALE;
    out->total_profit = read_f32(info + 112) * TDX_FINANCE_YUAN_SCALE;
    out->after_tax_profit = read_f32(info + 116) * TDX_FINANCE_YUAN_SCALE;
    out->net_profit = read_f32(info + 120) * TDX_FINANCE_YUAN_SCALE;
    out->undistributed_profit = read_f32(info + 124) * TDX_FINANCE_YUAN_SCALE;
    out->net_assets_per_share = read_f32(info + 128);
    out->reserved_2_raw = read_f32(info + 132);

    /* Every value above is a widening of a float32, so only a NaN or an infinity
     * in the wire data can make one non-finite. */
    {
        const struct {
            const char *name;
            double value;
        } checks[] = {
            {"circulating_shares", out->circulating_shares},
            {"total_shares", out->total_shares},
            {"national_shares", out->national_shares},
            {"promoter_legal_shares", out->promoter_legal_shares},
            {"legal_shares", out->legal_shares},
            {"b_shares", out->b_shares},
            {"h_shares", out->h_shares},
            {"eps", out->eps},
            {"total_assets", out->total_assets},
            {"current_assets", out->current_assets},
            {"fixed_assets", out->fixed_assets},
            {"intangible_assets", out->intangible_assets},
            {"shareholder_count", out->shareholder_count},
            {"current_liabilities", out->current_liabilities},
            {"long_term_liabilities", out->long_term_liabilities},
            {"capital_reserve", out->capital_reserve},
            {"net_assets", out->net_assets},
            {"revenue", out->revenue},
            {"main_profit", out->main_profit},
            {"accounts_receivable", out->accounts_receivable},
            {"operating_profit", out->operating_profit},
            {"investment_income", out->investment_income},
            {"operating_cash_flow", out->operating_cash_flow},
            {"total_cash_flow", out->total_cash_flow},
            {"inventory", out->inventory},
            {"total_profit", out->total_profit},
            {"after_tax_profit", out->after_tax_profit},
            {"net_profit", out->net_profit},
            {"undistributed_profit", out->undistributed_profit},
            {"net_assets_per_share", out->net_assets_per_share},
            {"reserved_2", out->reserved_2_raw},
        };
        size_t index;
        for (index = 0; index < sizeof(checks) / sizeof(checks[0]); ++index)
            if (!finite_value(checks[index].value, checks[index].name, err))
                return TDX_ERR;
    }
    return TDX_OK;
}

int tdx_finance_parse(const uint8_t *payload, size_t size, tdx_finance_record *out,
                      size_t capacity, size_t *out_count, tdx_error *err) {
    size_t count;
    size_t index;

    if (out_count)
        *out_count = 0;
    if (!payload || !out) {
        tdx_error_set(err, "0x0010 reply needs a body and an output");
        return TDX_ERR;
    }
    if (size < 2) {
        tdx_error_set(err, "0x0010 reply is %zu bytes, expected at least 2", size);
        return TDX_ERR;
    }
    count = (size_t)tdx_u16le(payload);
    if (size != 2 + count * TDX_FINANCE_RECORD_SIZE) {
        tdx_error_set(err,
                      "0x0010 reply length mismatch: %zu bytes for %zu records of %d bytes",
                      size, count, TDX_FINANCE_RECORD_SIZE);
        return TDX_ERR;
    }
    if (count > capacity) {
        tdx_error_set(err, "0x0010 reply holds %zu records, the output holds %zu", count,
                      capacity);
        return TDX_ERR;
    }
    for (index = 0; index < count; ++index) {
        if (tdx_finance_parse_record(payload + 2 + index * TDX_FINANCE_RECORD_SIZE,
                                     TDX_FINANCE_RECORD_SIZE, &out[index], err) != TDX_OK)
            return TDX_ERR;
    }
    if (out_count)
        *out_count = count;
    return TDX_OK;
}

int tdx_finance_fetch(tdx_connection *connection, const tdx_code *codes, size_t count,
                      tdx_finance_record *out, size_t capacity, size_t *out_count,
                      tdx_error *err) {
    tdx_buf request = {0};
    tdx_buf response = {0};
    int result = TDX_ERR;

    if (!connection || !out) {
        tdx_error_set(err, "finance fetch needs a session and an output");
        return TDX_ERR;
    }
    tdx_buf_init(&request);
    tdx_buf_init(&response);
    if (tdx_finance_build_request(codes, count, &request, err) != TDX_OK)
        goto done;
    if (tdx_connection_call(connection, TDX_CMD_FINANCE, request.data, request.len, &response,
                            err) != TDX_OK)
        goto done;
    result = tdx_finance_parse(response.data, response.len, out, capacity, out_count, err);

done:
    tdx_buf_free(&request);
    tdx_buf_free(&response);
    return result;
}
