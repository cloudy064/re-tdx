/* tdx_capital.c - 0x000F share-capital changes, ported from the C++ tool. */
#include "tdx_capital.h"

#include <math.h>
#include <string.h>

/* The wire view of a share count arrives in ten-thousands. */
#define TDX_CAPITAL_SHARE_SCALE 10000.0

static double read_f32(const uint8_t *data) {
    float raw;
    memcpy(&raw, data, sizeof(raw));
    return (double)raw;
}

const char *tdx_capital_category_key(unsigned category) {
    switch (category) {
    case 1:
        return "ex_rights_dividend";
    case 2:
        return "bonus_rights_listed";
    case 3:
        return "non_tradable_listed";
    case 4:
        return "state_share_placement";
    case 5:
        return "share_capital_change";
    case 6:
        return "seasoned_new_issue";
    case 7:
        return "share_buyback";
    case 8:
        return "seasoned_issue_listed";
    case 9:
        return "transferred_placement_listed";
    case 10:
        return "convertible_bond_listed";
    case 11:
        return "share_expansion_or_shrink";
    case 12:
        return "non_tradable_shrink";
    case 13:
        return "subscription_warrant";
    case 14:
        return "put_warrant";
    case 15:
        return "restructuring_adjustment";
    default:
        return "unknown";
    }
}

int tdx_capital_build_request(const tdx_code *security, tdx_buf *out, tdx_error *err) {
    size_t digit;

    if (!out) {
        tdx_error_set(err, "0x000F needs an output buffer");
        return TDX_ERR;
    }
    if (!security) {
        tdx_error_set(err, "0x000F needs one security");
        return TDX_ERR;
    }
    if (security->market_id < 0 || security->market_id > 2) {
        tdx_error_set(err, "capital market must be 0, 1 or 2, got %d", security->market_id);
        return TDX_ERR;
    }
    if (strlen(security->code) != 6) {
        tdx_error_set(err, "capital needs a six-digit code, got %s", security->code);
        return TDX_ERR;
    }
    for (digit = 0; digit < 6; ++digit)
        if (security->code[digit] < '0' || security->code[digit] > '9') {
            tdx_error_set(err, "capital code must be six digits, got %s", security->code);
            return TDX_ERR;
        }

    tdx_buf_clear(out);
    /* The header says one security, so the count is a constant 1. */
    if (tdx_buf_append_u16le(out, 1, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_push(out, (uint8_t)security->market_id, err) != TDX_OK ||
        tdx_buf_append(out, security->code, 6, err) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_capital_parse_record(const uint8_t *record, size_t size, tdx_capital_record *out,
                             tdx_error *err) {
    size_t digit;
    size_t slot;

    if (!record || !out) {
        tdx_error_set(err, "capital record needs data and an output");
        return TDX_ERR;
    }
    if (size != TDX_CAPITAL_CHANGES_SIZE) {
        tdx_error_set(err, "capital record is %zu bytes, expected %d", size,
                      TDX_CAPITAL_CHANGES_SIZE);
        return TDX_ERR;
    }
    memset(out, 0, sizeof(*out));
    out->security.market_id = record[0];
    if (out->security.market_id < 0 || out->security.market_id > 2) {
        tdx_error_set(err, "capital record has market %d", out->security.market_id);
        return TDX_ERR;
    }
    for (digit = 0; digit < 6; ++digit) {
        char ch = (char)record[1 + digit];
        if (ch < '0' || ch > '9') {
            tdx_error_set(err, "capital record code is not six digits");
            return TDX_ERR;
        }
        out->security.code[digit] = ch;
    }
    out->security.code[6] = '\0';
    out->reserved = record[7];
    {
        uint32_t raw = tdx_u32le(record + 8);
        int year = (int)(raw / 10000u);
        int month = (int)(raw / 100u % 100u);
        int day = (int)(raw % 100u);
        if (raw == 0) {
            out->date = 0;
        } else if (year < 1980 || year > 2200 || month < 1 || month > 12 || day < 1 ||
                   day > 31) {
            tdx_error_set(err, "capital record date is not a date: %u", (unsigned)raw);
            return TDX_ERR;
        } else {
            out->date = (int)raw;
        }
    }
    out->category = record[12];
    if (out->category > 15) {
        tdx_error_set(err, "capital record category is out of range: %u",
                      (unsigned)out->category);
        return TDX_ERR;
    }
    for (slot = 0; slot < TDX_CAPITAL_SLOTS; ++slot) {
        const uint8_t *field = record + 13 + slot * 4;
        double floater = read_f32(field);
        if (!isfinite(floater)) {
            tdx_error_set(err, "capital record slot %zu is not finite", slot);
            return TDX_ERR;
        }
        out->float_values[slot] = floater;
        out->share_values[slot] = tdx_wire_number(tdx_u32le(field)) * TDX_CAPITAL_SHARE_SCALE;
    }
    return TDX_OK;
}

int tdx_capital_parse(const uint8_t *payload, size_t size, const tdx_code *expected,
                      tdx_capital_record *out, size_t capacity, size_t *out_count,
                      size_t *block_count, tdx_error *err) {
    size_t blocks;
    size_t count;
    size_t index;
    int echoed_market;
    char echoed_code[8];

    if (out_count)
        *out_count = 0;
    if (block_count)
        *block_count = 0;
    if (!payload || !out) {
        tdx_error_set(err, "0x000F reply needs a body and an output");
        return TDX_ERR;
    }
    if (size < TDX_CAPITAL_CHANGES_HEADER) {
        tdx_error_set(err, "0x000F reply is %zu bytes, expected at least %d", size,
                      TDX_CAPITAL_CHANGES_HEADER);
        return TDX_ERR;
    }
    blocks = (size_t)tdx_u16le(payload);
    echoed_market = payload[2];
    memcpy(echoed_code, payload + 3, 6);
    echoed_code[6] = '\0';
    count = (size_t)tdx_u16le(payload + 9);
    if (size != TDX_CAPITAL_CHANGES_HEADER + count * TDX_CAPITAL_CHANGES_SIZE) {
        tdx_error_set(err,
                      "0x000F reply length mismatch: %zu bytes for %zu records of %d bytes",
                      size, count, TDX_CAPITAL_CHANGES_SIZE);
        return TDX_ERR;
    }
    if (echoed_market < 0 || echoed_market > 2) {
        tdx_error_set(err, "0x000F reply has market %d", echoed_market);
        return TDX_ERR;
    }
    /* The header names one security, so a reply that names a different one cannot
     * be attributed to the request that produced it. */
    if (expected) {
        if (echoed_market != expected->market_id ||
            strcmp(echoed_code, expected->code) != 0) {
            tdx_error_set(err, "0x000F reply is for %d/%s, not the requested %d/%s",
                          echoed_market, echoed_code, expected->market_id, expected->code);
            return TDX_ERR;
        }
    }
    if (count > capacity) {
        tdx_error_set(err, "0x000F reply holds %zu records, the output holds %zu", count,
                      capacity);
        return TDX_ERR;
    }
    for (index = 0; index < count; ++index) {
        if (tdx_capital_parse_record(payload + TDX_CAPITAL_CHANGES_HEADER +
                                         index * TDX_CAPITAL_CHANGES_SIZE,
                                     TDX_CAPITAL_CHANGES_SIZE, &out[index], err) != TDX_OK)
            return TDX_ERR;
    }
    if (out_count)
        *out_count = count;
    if (block_count)
        *block_count = blocks;
    return TDX_OK;
}

int tdx_capital_fetch(tdx_connection *connection, const tdx_code *security,
                      tdx_capital_record *out, size_t capacity, size_t *out_count,
                      size_t *block_count, tdx_error *err) {
    tdx_buf request;
    tdx_buf response;
    int result = TDX_ERR;

    if (!connection || !out) {
        tdx_error_set(err, "capital fetch needs a session and an output");
        return TDX_ERR;
    }
    tdx_buf_init(&request);
    tdx_buf_init(&response);
    if (tdx_capital_build_request(security, &request, err) != TDX_OK)
        goto done;
    if (tdx_connection_call(connection, TDX_CMD_CAPITAL_CHANGES, request.data, request.len,
                            &response, err) != TDX_OK)
        goto done;
    result = tdx_capital_parse(response.data, response.len, security, out, capacity, out_count,
                               block_count, err);

done:
    tdx_buf_free(&request);
    tdx_buf_free(&response);
    return result;
}
