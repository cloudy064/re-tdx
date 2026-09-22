/* tdx_snapshot.c - 0x054C whole-universe L1 snapshot, ported from the C++ tool. */
#include "tdx_snapshot.h"

#include <math.h>
#include <string.h>

/* The auxiliary price is only exposed as an indicative net value above this
 * threshold; below it the client treats the field as absent.  The constant comes
 * from the recovered TdxW predicate, so it is kept verbatim rather than
 * rounded to something tidier. */
#define TDX_SNAPSHOT_IOPV_FLOOR 0.00009999999747378752

int tdx_snapshot_is_fund_iopv(const tdx_code *security) {
    static const char *const shanghai_prefixes[] = {
        "510", "511", "512", "513", "515", "516", "517", "518",
        "520", "530", "551", "560", "561", "562", "563", "588", "589",
    };
    size_t index;

    if (!security)
        return 0;
    if (security->market_id == 0)
        return strncmp(security->code, "158", 3) == 0 ||
               strncmp(security->code, "159", 3) == 0;
    if (security->market_id != 1)
        return 0;
    if (strlen(security->code) != 6 || security->code[5] != '0')
        return 0;
    for (index = 0; index < sizeof(shanghai_prefixes) / sizeof(shanghai_prefixes[0]); ++index)
        if (strncmp(security->code, shanghai_prefixes[index], 3) == 0)
            return 1;
    return 0;
}

int tdx_snapshot_build_request(const tdx_code *codes, size_t count, tdx_buf *out,
                               tdx_error *err) {
    size_t index;

    if (!out) {
        tdx_error_set(err, "0x054C needs an output buffer");
        return TDX_ERR;
    }
    if (!codes || count == 0) {
        tdx_error_set(err, "0x054C needs at least one security");
        return TDX_ERR;
    }
    if (count > 0xFFFFu) {
        tdx_error_set(err, "0x054C batch of %zu exceeds the 16-bit count field", count);
        return TDX_ERR;
    }
    for (index = 0; index < count; ++index) {
        const tdx_code *code = &codes[index];
        size_t digit;
        if (code->market_id < 0 || code->market_id > 2) {
            tdx_error_set(err, "snapshot market must be 0, 1 or 2, got %d", code->market_id);
            return TDX_ERR;
        }
        if (strlen(code->code) != 6) {
            tdx_error_set(err, "snapshot needs six-digit codes, got %s", code->code);
            return TDX_ERR;
        }
        for (digit = 0; digit < 6; ++digit)
            if (code->code[digit] < '0' || code->code[digit] > '9') {
                tdx_error_set(err, "snapshot code must be six digits, got %s", code->code);
                return TDX_ERR;
            }
    }

    tdx_buf_clear(out);
    if (tdx_buf_push(out, 5, err) != TDX_OK ||
        tdx_buf_append_zeros(out, 7, err) != TDX_OK ||
        tdx_buf_append_u16le(out, (uint16_t)count, err) != TDX_OK)
        return TDX_ERR;
    for (index = 0; index < count; ++index) {
        if (tdx_buf_push(out, (uint8_t)codes[index].market_id, err) != TDX_OK ||
            tdx_buf_append(out, codes[index].code, 6, err) != TDX_OK)
            return TDX_ERR;
    }
    return TDX_OK;
}

int tdx_snapshot_split_records(const uint8_t *data, size_t size, size_t count, size_t *starts,
                               size_t starts_capacity, size_t *out_count, tdx_error *err) {
    size_t found = 0;
    size_t position;

    if (out_count)
        *out_count = 0;
    if (!data || !starts) {
        tdx_error_set(err, "record splitting needs data and a start list");
        return TDX_ERR;
    }
    if (count == 0)
        return TDX_OK;
    for (position = 0; position + 7 <= size; ++position) {
        size_t digit;
        int all_digits = 1;
        if (data[position] > 2)
            continue;
        for (digit = 1; digit <= 6; ++digit)
            if (data[position + digit] < '0' || data[position + digit] > '9') {
                all_digits = 0;
                break;
            }
        if (!all_digits)
            continue;
        if (found >= starts_capacity) {
            tdx_error_set(err, "more than %zu record boundaries in the payload",
                          starts_capacity);
            return TDX_ERR;
        }
        starts[found++] = position;
    }
    if (found != count) {
        tdx_error_set(err,
                      "cannot identify snapshot record boundaries: found %zu for a declared %zu",
                      found, count);
        return TDX_ERR;
    }
    if (starts[0] != 0) {
        tdx_error_set(err, "the first snapshot record does not start at offset 0");
        return TDX_ERR;
    }
    if (out_count)
        *out_count = found;
    return TDX_OK;
}

int tdx_snapshot_parse_record(const uint8_t *record, size_t size, tdx_snapshot *out,
                              tdx_error *err) {
    size_t offset = 9;
    int64_t current_delta;
    int64_t previous_delta;
    int64_t open_delta;
    int64_t high_delta;
    int64_t low_delta;
    int64_t open_amount_raw;
    int divisor;
    double scale;
    size_t digit;

    if (!record || !out) {
        tdx_error_set(err, "snapshot record needs data and an output");
        return TDX_ERR;
    }
    if (size < 9) {
        tdx_error_set(err, "snapshot record is %zu bytes, expected at least 9", size);
        return TDX_ERR;
    }
    memset(out, 0, sizeof(*out));
    out->security.market_id = record[0];
    if (out->security.market_id < 0 || out->security.market_id > 2) {
        tdx_error_set(err, "snapshot record has market %d", out->security.market_id);
        return TDX_ERR;
    }
    for (digit = 0; digit < 6; ++digit) {
        char ch = (char)record[1 + digit];
        if (ch < '0' || ch > '9') {
            tdx_error_set(err, "snapshot record code is not six digits");
            return TDX_ERR;
        }
        out->security.code[digit] = ch;
    }
    out->security.code[6] = '\0';
    out->active = tdx_u16le(record + 7);

    if (tdx_varint_decode(record, size, &offset, &current_delta, err) != TDX_OK ||
        tdx_varint_decode(record, size, &offset, &previous_delta, err) != TDX_OK ||
        tdx_varint_decode(record, size, &offset, &open_delta, err) != TDX_OK ||
        tdx_varint_decode(record, size, &offset, &high_delta, err) != TDX_OK ||
        tdx_varint_decode(record, size, &offset, &low_delta, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_varint_decode(record, size, &offset, &out->time_raw, err) != TDX_OK ||
        tdx_varint_decode(record, size, &offset, &out->auxiliary_price_delta_raw, err) != TDX_OK ||
        tdx_varint_decode(record, size, &offset, &out->total_hand, err) != TDX_OK ||
        tdx_varint_decode(record, size, &offset, &out->current_hand, err) != TDX_OK)
        return TDX_ERR;
    if (offset + 4 > size) {
        tdx_error_set(err, "snapshot record %s has no amount field", out->security.code);
        return TDX_ERR;
    }
    out->amount = tdx_wire_number(tdx_u32le(record + offset));
    offset += 4;
    if (tdx_varint_decode(record, size, &offset, &out->inside, err) != TDX_OK ||
        tdx_varint_decode(record, size, &offset, &out->outside, err) != TDX_OK ||
        tdx_varint_decode(record, size, &offset, &out->auction_imbalance_hand, err) != TDX_OK ||
        tdx_varint_decode(record, size, &offset, &open_amount_raw, err) != TDX_OK)
        return TDX_ERR;
    out->open_amount = (double)open_amount_raw * 100.0;

    divisor = tdx_price_divisor(out->security.code);
    scale = (double)divisor * 1000.0;
    /* The four other prices are deltas against the current price. */
    out->last = (double)current_delta * 10.0 / scale;
    out->previous = (double)(previous_delta + current_delta) * 10.0 / scale;
    out->open = (double)(open_delta + current_delta) * 10.0 / scale;
    out->high = (double)(high_delta + current_delta) * 10.0 / scale;
    out->low = (double)(low_delta + current_delta) * 10.0 / scale;

    if (tdx_snapshot_is_fund_iopv(&out->security)) {
        double auxiliary = out->last +
                           (double)out->auxiliary_price_delta_raw / (scale / 10.0);
        if (auxiliary > TDX_SNAPSHOT_IOPV_FLOOR) {
            out->has_fund_iopv = 1;
            out->fund_iopv = auxiliary / 10.0;
        }
    }
    if (!isfinite(out->last) || !isfinite(out->previous) || !isfinite(out->open) ||
        !isfinite(out->high) || !isfinite(out->low) || !isfinite(out->amount)) {
        tdx_error_set(err, "snapshot record %s carries a non-finite value", out->security.code);
        return TDX_ERR;
    }
    out->tail_size = size > offset ? size - offset : 0;
    return TDX_OK;
}

int tdx_snapshot_parse(const uint8_t *payload, size_t size, size_t requested, tdx_snapshot *out,
                       size_t out_capacity, size_t *out_count, uint16_t *header_raw,
                       tdx_error *err) {
    size_t starts[TDX_SNAPSHOT_BATCH_MAX];
    size_t found = 0;
    size_t count;
    size_t index;

    if (out_count)
        *out_count = 0;
    if (!payload || !out) {
        tdx_error_set(err, "0x054C reply needs a body and an output");
        return TDX_ERR;
    }
    if (size < 4) {
        tdx_error_set(err, "0x054C reply is %zu bytes, expected at least 4", size);
        return TDX_ERR;
    }
    if (header_raw)
        *header_raw = tdx_u16le(payload);
    count = (size_t)tdx_u16le(payload + 2);
    if (count > requested) {
        tdx_error_set(err, "0x054C reply declares %zu records for %zu requested", count,
                      requested);
        return TDX_ERR;
    }
    if (count > out_capacity) {
        tdx_error_set(err, "0x054C reply declares %zu records, the output holds %zu", count,
                      out_capacity);
        return TDX_ERR;
    }
    if (count > sizeof(starts) / sizeof(starts[0])) {
        tdx_error_set(err, "0x054C reply declares %zu records, above the %zu batch limit", count,
                      sizeof(starts) / sizeof(starts[0]));
        return TDX_ERR;
    }
    if (tdx_snapshot_split_records(payload + 4, size - 4, count, starts,
                                   sizeof(starts) / sizeof(starts[0]), &found, err) != TDX_OK)
        return TDX_ERR;
    for (index = 0; index < found; ++index) {
        size_t start = starts[index];
        size_t end = index + 1 < found ? starts[index + 1] : size - 4;
        if (tdx_snapshot_parse_record(payload + 4 + start, end - start, &out[index], err) !=
            TDX_OK)
            return TDX_ERR;
    }
    if (out_count)
        *out_count = found;
    return TDX_OK;
}

int tdx_snapshot_fetch(tdx_connection *connection, const tdx_code *codes, size_t count,
                       tdx_snapshot *out, size_t out_capacity, size_t *out_count,
                       tdx_error *err) {
    tdx_buf request = {0};
    tdx_buf response = {0};
    int result = TDX_ERR;

    if (!connection || !out) {
        tdx_error_set(err, "snapshot fetch needs a session and an output");
        return TDX_ERR;
    }
    tdx_buf_init(&request);
    tdx_buf_init(&response);
    if (tdx_snapshot_build_request(codes, count, &request, err) != TDX_OK)
        goto done;
    if (tdx_connection_call(connection, TDX_CMD_SNAPSHOT, request.data, request.len, &response,
                            err) != TDX_OK)
        goto done;
    result = tdx_snapshot_parse(response.data, response.len, count, out, out_capacity, out_count,
                                NULL, err);

done:
    tdx_buf_free(&request);
    tdx_buf_free(&response);
    return result;
}
