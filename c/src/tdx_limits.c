/* tdx_limits.c - the 0x0452 special-treatment price-limit list. */
#include "tdx_limits.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static double read_f32(const uint8_t *data) {
    float raw;
    memcpy(&raw, data, sizeof(raw));
    return (double)raw;
}

int tdx_limits_build_request(unsigned start_index, tdx_buf *out, tdx_error *err) {
    if (!out) {
        tdx_error_set(err, "0x0452 needs an output buffer");
        return TDX_ERR;
    }
    if (start_index > TDX_LIMITS_START_INDEX_MAX) {
        tdx_error_set(err, "0x0452 start index must be in 0..%u", TDX_LIMITS_START_INDEX_MAX);
        return TDX_ERR;
    }
    tdx_buf_clear(out);
    if (tdx_buf_append_u16le(out, (uint16_t)start_index, err) != TDX_OK)
        return TDX_ERR;
    /* Twelve zeros follow.  The reference sends them and the server ignores them,
     * but the length is part of the request shape, so they are written explicitly
     * rather than left to chance. */
    return tdx_buf_append_zeros(out, TDX_LIMITS_REQUEST_SIZE - 2, err);
}

int tdx_limits_parse_record(const uint8_t *record, size_t size, unsigned index,
                            tdx_limit_record *out, tdx_error *err) {
    if (!record || !out) {
        tdx_error_set(err, "a limit record needs data and an output");
        return TDX_ERR;
    }
    if (size != TDX_LIMITS_RECORD_SIZE) {
        tdx_error_set(err, "a limit record is %zu bytes, expected %d", size,
                      TDX_LIMITS_RECORD_SIZE);
        return TDX_ERR;
    }
    memset(out, 0, sizeof(*out));
    out->security.market_id = record[0];
    if (out->security.market_id < 0 || out->security.market_id > 2) {
        tdx_error_set(err, "limit record %u has market %d", index, out->security.market_id);
        return TDX_ERR;
    }
    out->code_number = tdx_u32le(record + 1);
    if (out->code_number > 999999u) {
        tdx_error_set(err, "limit record %u has a code number above six digits: %u", index,
                      (unsigned)out->code_number);
        return TDX_ERR;
    }
    /* The wire carries the code as a number; the project identifies securities by a
     * six-digit string, so the conversion happens once, here. */
    snprintf(out->security.code, sizeof(out->security.code), "%06u", (unsigned)out->code_number);
    out->limit_up = read_f32(record + 5);
    out->limit_down = read_f32(record + 9);
    if (!isfinite(out->limit_up) || !isfinite(out->limit_down)) {
        tdx_error_set(err, "limit record %u (%s) carries a non-finite price", index,
                      out->security.code);
        return TDX_ERR;
    }
    if (out->limit_up < out->limit_down) {
        tdx_error_set(err, "limit record %u (%s) has the up price %.4f below the down price %.4f",
                      index, out->security.code, out->limit_up, out->limit_down);
        return TDX_ERR;
    }
    return TDX_OK;
}

int tdx_limits_parse(const uint8_t *payload, size_t size, unsigned index_of_first,
                     tdx_limit_record *out, size_t capacity, size_t *out_count,
                     unsigned *indices, tdx_error *err) {
    size_t count;
    size_t index;

    if (out_count)
        *out_count = 0;
    if (!payload || !out) {
        tdx_error_set(err, "0x0452 reply needs a body and an output");
        return TDX_ERR;
    }
    if (size < 2) {
        tdx_error_set(err, "0x0452 reply is %zu bytes, expected at least 2", size);
        return TDX_ERR;
    }
    count = (size_t)tdx_u16le(payload);
    if (size != 2 + count * TDX_LIMITS_RECORD_SIZE) {
        tdx_error_set(err, "0x0452 reply length mismatch: %zu bytes for %zu records of %d bytes",
                      size, count, TDX_LIMITS_RECORD_SIZE);
        return TDX_ERR;
    }
    if (count > capacity) {
        tdx_error_set(err, "0x0452 reply holds %zu records, the output holds %zu", count,
                      capacity);
        return TDX_ERR;
    }
    for (index = 0; index < count; ++index) {
        if (tdx_limits_parse_record(payload + 2 + index * TDX_LIMITS_RECORD_SIZE,
                                    TDX_LIMITS_RECORD_SIZE,
                                    index_of_first + (unsigned)index, &out[index], err) != TDX_OK)
            return TDX_ERR;
        if (indices)
            indices[index] = index_of_first + (unsigned)index;
    }
    if (out_count)
        *out_count = count;
    return TDX_OK;
}

int tdx_limits_fetch(tdx_connection *connection, unsigned start_index, tdx_limit_record *out,
                     size_t capacity, size_t *out_count, unsigned *indices, tdx_error *err) {
    tdx_buf request;
    tdx_buf response;
    int result = TDX_ERR;

    if (!connection || !out) {
        tdx_error_set(err, "the limit fetch needs a session and an output");
        return TDX_ERR;
    }
    tdx_buf_init(&request);
    tdx_buf_init(&response);
    if (tdx_limits_build_request(start_index, &request, err) != TDX_OK)
        goto done;
    if (tdx_connection_call(connection, TDX_CMD_SPECIAL_LIMITS, request.data, request.len,
                            &response, err) != TDX_OK)
        goto done;
    result = tdx_limits_parse(response.data, response.len, start_index, out, capacity, out_count,
                              indices, err);

done:
    tdx_buf_free(&request);
    tdx_buf_free(&response);
    return result;
}

int tdx_limits_fetch_all(tdx_connection *connection, unsigned start_index, size_t max_records,
                         unsigned max_pages, tdx_limit_record *out, size_t capacity,
                         size_t *out_count, unsigned *next_index, tdx_error *err) {
    unsigned cursor = start_index;
    size_t stored = 0;
    unsigned page;

    if (out_count)
        *out_count = 0;
    if (next_index)
        *next_index = start_index;
    if (!connection || !out) {
        tdx_error_set(err, "the limit walk needs a session and an output");
        return TDX_ERR;
    }
    for (page = 0; page < max_pages; ++page) {
        size_t room = capacity > stored ? capacity - stored : 0;
        size_t got = 0;
        size_t wanted;

        if (room == 0 || stored >= max_records)
            break;
        if (room > TDX_LIMITS_PAGE_MAX)
            room = TDX_LIMITS_PAGE_MAX;
        if (room > max_records - stored)
            room = max_records - stored;
        wanted = room;
        /* Each page is decoded straight into the caller's buffer, so a whole-list
         * walk needs no scratch copy and no static state. */
        if (tdx_limits_fetch(connection, cursor, out + stored, wanted, &got, NULL, err) != TDX_OK)
            return TDX_ERR;
        if (got == 0) {
            /* An empty page is the end of the list. */
            if (next_index)
                *next_index = cursor;
            break;
        }
        stored += got;
        cursor += (unsigned)got;
        if (next_index)
            *next_index = cursor;
        /* NOTE: no "short page means the end" shortcut here.  This command answers
         * ONE row per request - a request from index 0 returns a single record, and
         * the walk advances by an explicit index - so a short page is the normal
         * case, not the end.  Only an empty page ends the list. */
    }
    if (out_count)
        *out_count = stored;
    return TDX_OK;
}
