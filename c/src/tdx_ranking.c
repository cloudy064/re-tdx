/* tdx_ranking.c - the category ranking over 0x054B. */
#include "tdx_ranking.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tdx_bytes.h"
#include "tdx_quote.h"

typedef struct sort_entry {
    const char *name;
    uint16_t id;
} sort_entry;

/* The reference's own table, names and ids alike. */
static const sort_entry sort_types[] = {
    {"code", 0x0000},          {"price", 0x0006},
    {"amount", 0x000A},        {"change-pct", 0x000E},
    {"seal-amount", 0x001C},   {"opening-amount", 0x001D},
    {"rise-speed", 0x002E},    {"short-turnover", 0x00CC},
    {"volume-rise-speed", 0x00D0}, {"opening-rush", 0x010A},
    {"two-minute-amount", 0x010C}, {"opening-change", 0x0119},
    {"highest-change", 0x011A},    {"lowest-change", 0x011B},
    {"drawdown", 0x011E},      {"attack", 0x011F},
};

/* Lower-cases ASCII in place, as the reference does before comparing, so "AMOUNT" and
 * "amount" are the same key. */
static void lower_ascii(char *text) {
    size_t index;
    for (index = 0; text[index]; ++index)
        if (text[index] >= 'A' && text[index] <= 'Z')
            text[index] = (char)(text[index] - 'A' + 'a');
}

/* Parses a decimal number, returning 0 when the text is not one.  The reference falls back
 * to a number for any key its table does not name, which is how a caller reaches a sort the
 * table has not met. */
static int parse_u16(const char *text, uint16_t *out) {
    unsigned long value = 0;
    size_t index = 0;
    if (!text || !text[0])
        return 0;
    /* A leading 0x means hex, because a caller naming a raw sort id would use it. */
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        index = 2;
        if (!text[index])
            return 0;
        for (; text[index]; ++index) {
            int digit;
            char ch = text[index];
            if (ch >= '0' && ch <= '9')
                digit = ch - '0';
            else if (ch >= 'a' && ch <= 'f')
                digit = ch - 'a' + 10;
            else if (ch >= 'A' && ch <= 'F')
                digit = ch - 'A' + 10;
            else
                return 0;
            value = value * 16 + (unsigned long)digit;
            if (value > 0xFFFF)
                return 0;
        }
    } else {
        for (; text[index]; ++index) {
            if (text[index] < '0' || text[index] > '9')
                return 0;
            value = value * 10 + (unsigned long)(text[index] - '0');
            if (value > 0xFFFF)
                return 0;
        }
    }
    *out = (uint16_t)value;
    return 1;
}

int tdx_ranking_sort_id(const char *name, uint16_t *out) {
    char scratch[32];
    size_t index;
    if (!name || !out)
        return 0;
    snprintf(scratch, sizeof(scratch), "%s", name);
    lower_ascii(scratch);
    for (index = 0; index < sizeof(sort_types) / sizeof(sort_types[0]); ++index)
        if (strcmp(sort_types[index].name, scratch) == 0) {
            *out = sort_types[index].id;
            return 1;
        }
    return parse_u16(scratch, out);
}

const char *tdx_ranking_sort_name(uint16_t id) {
    size_t index;
    for (index = 0; index < sizeof(sort_types) / sizeof(sort_types[0]); ++index)
        if (sort_types[index].id == id)
            return sort_types[index].name;
    return NULL;
}

int tdx_ranking_category_id(const char *name, uint16_t *out) {
    char scratch[32];
    if (!name || !out)
        return 0;
    snprintf(scratch, sizeof(scratch), "%s", name);
    lower_ascii(scratch);
    /* The three spellings the reference accepts for the A-share category.  The Chinese ones
     * are compared as bytes: "\xe6\xb2\xaa\xe6\xb7\xb1a\xe8\x82\xa1" is the four-character
     * spelling and "a\xe8\x82\xa1" the two-character one. */
    /* "a_share" is how the security directory spells it and is the default of --category,
     * so refusing it would make the flag unusable; "a-shares" is the reference's own
     * spelling and the Chinese ones are the terminal's. */
    if (strcmp(scratch, "a_share") == 0 || strcmp(scratch, "a-shares") == 0 ||
        strcmp(scratch, "a\xe8\x82\xa1") == 0 ||
        strcmp(scratch, "\xe6\xb2\xaa\xe6\xb7\xb1" "a\xe8\x82\xa1") == 0) {
        *out = TDX_RANKING_CATEGORY_A_SHARES;
        return 1;
    }
    return parse_u16(scratch, out);
}

int tdx_ranking_build_request(uint16_t category, uint16_t sort, uint16_t start, uint16_t count,
                              int ascending, uint16_t filter_raw, tdx_buf *out, tdx_error *err) {
    uint16_t reverse;
    static const uint16_t fixed[9] = {0, 0, 0, 0, 0, 5, 0, 1, 0};
    uint16_t fields[9];
    size_t index;

    if (!out) {
        tdx_error_set(err, "building the ranking request needs a buffer");
        return TDX_ERR;
    }
    if (count < 1 || count > TDX_RANKING_PAGE_MAX) {
        tdx_error_set(err, "the ranking page size must be in 1..%d, not %u",
                      TDX_RANKING_PAGE_MAX, count);
        return TDX_ERR;
    }
    /* No sort key means no direction; otherwise 2 is ascending and 1 descending, so a
     * descending sort is the default rather than a flag being set. */
    reverse = sort == 0 ? 0 : (ascending ? 2 : 1);
    for (index = 0; index < 9; ++index)
        fields[index] = fixed[index];
    fields[0] = category;
    fields[1] = sort;
    fields[2] = start;
    fields[3] = count;
    fields[4] = reverse;
    fields[6] = filter_raw;
    tdx_buf_clear(out);
    for (index = 0; index < 9; ++index)
        if (tdx_buf_append_u16le(out, fields[index], err) != TDX_OK)
            return TDX_ERR;
    if (out->len != TDX_RANKING_REQUEST_SIZE) {
        tdx_error_set(err, "the ranking request is %zu bytes, expected %d", out->len,
                      TDX_RANKING_REQUEST_SIZE);
        return TDX_ERR;
    }
    return TDX_OK;
}

/* --- the response ----------------------------------------------------- */

static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static float read_f32(const uint8_t *data) {
    float value;
    uint32_t bits = read_u32(data);
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* raw / 100 / the divisor table, which is the same scale the wire path uses. */
static double ranking_price(int64_t raw, const char *code) {
    return (double)raw / 100.0 / (double)tdx_price_divisor(code);
}

static int hex_of(const uint8_t *data, size_t size, char *out, size_t capacity) {
    static const char digits[] = "0123456789abcdef";
    size_t index;
    if (capacity < size * 2 + 1)
        return 0;
    for (index = 0; index < size; ++index) {
        out[index * 2] = digits[data[index] >> 4];
        out[index * 2 + 1] = digits[data[index] & 0x0f];
    }
    out[size * 2] = '\0';
    return 1;
}

int tdx_ranking_parse(const uint8_t *payload, size_t size, tdx_ranking_record *out,
                      size_t capacity, tdx_ranking_page *page, tdx_error *err) {
    size_t offset;
    size_t count;
    size_t index;

    if (page)
        memset(page, 0, sizeof(*page));
    if (!payload || !out) {
        tdx_error_set(err, "parsing the ranking response needs bytes and an output");
        return TDX_ERR;
    }
    if (size < 4) {
        tdx_error_set(err, "a %zu-byte ranking response is shorter than its header", size);
        return TDX_ERR;
    }
    count = read_u16(payload + 2);
    if (page)
        page->header = read_u16(payload);
    if (count > TDX_RANKING_PAGE_MAX) {
        tdx_error_set(err, "the ranking response claims %zu records, more than the %d a page "
                           "may hold", count, TDX_RANKING_PAGE_MAX);
        return TDX_ERR;
    }
    if (count > capacity) {
        tdx_error_set(err, "the ranking response holds %zu records, more than %zu", count,
                      capacity);
        return TDX_ERR;
    }
    offset = 4;
    for (index = 0; index < count; ++index) {
        tdx_ranking_record *record = &out[index];
        int64_t values[9];
        size_t value;
        int64_t close_raw;
        const uint8_t *tail;

        memset(record, 0, sizeof(*record));
        if (offset + 9 > size) {
            tdx_error_set(err, "ranking record %zu has an incomplete header", index);
            return TDX_ERR;
        }
        record->market_id = payload[offset];
        if (record->market_id < 0 || record->market_id > 2) {
            tdx_error_set(err, "ranking record %zu names market %d", index, record->market_id);
            return TDX_ERR;
        }
        {
            size_t digit;
            for (digit = 0; digit < 6; ++digit)
                if (payload[offset + 1 + digit] < '0' || payload[offset + 1 + digit] > '9') {
                    tdx_error_set(err, "ranking record %zu does not hold a six-digit code",
                                  index);
                    return TDX_ERR;
                }
            memcpy(record->code, payload + offset + 1, 6);
            record->code[6] = '\0';
        }
        record->active1 = read_u16(payload + offset + 7);
        {
            const char *prefix = record->market_id == 0 ? "SZ" :
                                 record->market_id == 1 ? "SH" : "BJ";
            snprintf(record->security_id, sizeof(record->security_id), "%s%s", prefix,
                     record->code);
        }
        offset += 9;

        for (value = 0; value < 9; ++value)
            if (tdx_varint_decode(payload, size, &offset, &values[value], err) != TDX_OK) {
                tdx_error_set(err, "ranking record %s: %s", record->code, err->message);
                return TDX_ERR;
            }
        /* FIVE PRICES COME AS DELTAS FROM THE CLOSE.  Reading each varint as an absolute
         * price gets the close right and the other four wrong. */
        close_raw = values[0];
        record->last_price = ranking_price(close_raw, record->code);
        record->pre_close_price = ranking_price(close_raw + values[1], record->code);
        record->open_price = ranking_price(close_raw + values[2], record->code);
        record->high_price = ranking_price(close_raw + values[3], record->code);
        record->low_price = ranking_price(close_raw + values[4], record->code);
        record->server_time_raw = values[5];
        record->neg_price_raw = values[6];
        record->total_hand = values[7];
        record->current_hand = values[8];

        if (offset + 4 > size) {
            tdx_error_set(err, "ranking record %s has no amount", record->code);
            return TDX_ERR;
        }
        record->amount = tdx_wire_number(read_u32(payload + offset));
        offset += 4;

        if (tdx_varint_decode(payload, size, &offset, &record->inside_dish, err) != TDX_OK ||
            tdx_varint_decode(payload, size, &offset, &record->outer_disc, err) != TDX_OK ||
            tdx_varint_decode(payload, size, &offset, &record->after_outer_raw, err) != TDX_OK) {
            tdx_error_set(err, "ranking record %s: %s", record->code, err->message);
            return TDX_ERR;
        }
        {
            int64_t open_amount = 0;
            int64_t bid = 0;
            int64_t ask = 0;
            if (tdx_varint_decode(payload, size, &offset, &open_amount, err) != TDX_OK ||
                tdx_varint_decode(payload, size, &offset, &bid, err) != TDX_OK ||
                tdx_varint_decode(payload, size, &offset, &ask, err) != TDX_OK ||
                tdx_varint_decode(payload, size, &offset, &record->bid1_volume_hand, err) !=
                    TDX_OK ||
                tdx_varint_decode(payload, size, &offset, &record->ask1_volume_hand, err) !=
                    TDX_OK) {
                tdx_error_set(err, "ranking record %s: %s", record->code, err->message);
                return TDX_ERR;
            }
            record->open_amount_yuan = (double)open_amount * 100.0;
            record->bid1_price = ranking_price(close_raw + bid, record->code);
            record->ask1_price = ranking_price(close_raw + ask, record->code);
        }

        /* The 56-byte fixed tail. */
        if (offset + 56 > size) {
            tdx_error_set(err, "ranking record %s has an incomplete tail", record->code);
            return TDX_ERR;
        }
        tail = payload + offset;
        record->status_or_sort_raw = read_u16(tail);
        record->rise_speed = (double)(int16_t)read_u16(tail + 2) / 100.0;
        record->short_turnover = (double)(int16_t)read_u16(tail + 4) / 100.0;
        record->two_minute_amount = (double)read_f32(tail + 6);
        record->opening_rush = (double)(int16_t)read_u16(tail + 10) / 100.0;
        /* Offsets 12 and 30 are the two runs with no established meaning; they are kept as
         * hex rather than interpreted. */
        if (!hex_of(tail + 12, 10, record->extra_pair_hex, sizeof(record->extra_pair_hex)) ||
            !hex_of(tail + 30, 24, record->extra_meta_hex, sizeof(record->extra_meta_hex))) {
            tdx_error_set(err, "the ranking hex buffers are too small");
            return TDX_ERR;
        }
        record->volume_rise_speed = (double)read_f32(tail + 22);
        record->depth = (double)read_f32(tail + 26);
        record->active2 = read_u16(tail + 54);
        offset += 56;

        if (!isfinite(record->last_price) || !isfinite(record->pre_close_price) ||
            !isfinite(record->open_price) || !isfinite(record->high_price) ||
            !isfinite(record->low_price) || !isfinite(record->amount) ||
            !isfinite(record->two_minute_amount) || !isfinite(record->volume_rise_speed) ||
            !isfinite(record->depth)) {
            tdx_error_set(err, "ranking record %s holds a non-finite number", record->code);
            return TDX_ERR;
        }
    }
    /* Trailing bytes mean the walk is not the layout this reader thinks it is, so they are
     * an error rather than something to skip. */
    if (offset != size) {
        tdx_error_set(err, "the ranking response has %zu trailing bytes", size - offset);
        return TDX_ERR;
    }
    if (page)
        page->records = count;
    return TDX_OK;
}
