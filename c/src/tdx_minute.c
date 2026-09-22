/* tdx_minute.c - the local .lc1 one-minute bars. */
#include "tdx_minute.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint16_t u16_at(const uint8_t *data, size_t offset) {
    return (uint16_t)((uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8));
}

static uint32_t u32_at(const uint8_t *data, size_t offset) {
    return (uint32_t)data[offset] | ((uint32_t)data[offset + 1] << 8) |
           ((uint32_t)data[offset + 2] << 16) | ((uint32_t)data[offset + 3] << 24);
}

static float f32_at(const uint8_t *data, size_t offset) {
    float value;
    uint32_t bits = u32_at(data, offset);
    memcpy(&value, &bits, sizeof(value));
    return value;
}

int tdx_lc1_decode_date(uint16_t word, uint32_t *out) {
    static const unsigned lengths[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    unsigned year;
    unsigned remainder;
    unsigned month;
    unsigned day;

    if (!out)
        return 0;
    year = (unsigned)(word / 2048) + 2004u;
    remainder = (unsigned)(word % 2048);
    month = remainder / 100;
    day = remainder % 100;
    if (year < 2004 || year > 2200 || month < 1 || month > 12 || day < 1)
        return 0;
    if (day > lengths[month]) {
        int leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        if (!(month == 2 && leap && day == 29))
            return 0;
    }
    *out = year * 10000u + month * 100u + day;
    return 1;
}

int tdx_lc1_parse(const uint8_t *data, size_t size, tdx_lc1_bar *out, size_t capacity,
                  size_t *out_count, size_t *ohlc_violations, tdx_error *err) {
    size_t records;
    size_t index;
    size_t violations = 0;

    if (out_count)
        *out_count = 0;
    if (ohlc_violations)
        *ohlc_violations = 0;
    if (!data || !out) {
        tdx_error_set(err, "parsing a .lc1 file needs bytes and an output");
        return TDX_ERR;
    }
    if (size % TDX_LC1_RECORD_SIZE != 0) {
        tdx_error_set(err, "a %zu-byte .lc1 file is not a multiple of %d", size,
                      TDX_LC1_RECORD_SIZE);
        return TDX_ERR;
    }
    records = size / TDX_LC1_RECORD_SIZE;
    if (records > capacity) {
        tdx_error_set(err, "the .lc1 file holds %zu records, more than %zu", records, capacity);
        return TDX_ERR;
    }
    for (index = 0; index < records; ++index) {
        const uint8_t *record = data + index * TDX_LC1_RECORD_SIZE;
        tdx_lc1_bar *bar = &out[index];
        uint16_t minute_word;
        int hour;
        int minute;
        double highest;
        double lowest;

        if (!tdx_lc1_decode_date(u16_at(record, 0), &bar->date)) {
            tdx_error_set(err, ".lc1 record %zu has date word %u", index, u16_at(record, 0));
            return TDX_ERR;
        }
        minute_word = u16_at(record, 2);
        hour = minute_word / 60;
        minute = minute_word % 60;
        /* Not one of the 1,440 minutes of a day: the record has gone out of step.  The scan
         * found no such record in 13.9 million, so refusing this rejects nothing real. */
        if (minute_word >= 1440) {
            tdx_error_set(err, ".lc1 record %zu has minute word %u (%d:%02d)", index,
                          minute_word, hour, minute);
            return TDX_ERR;
        }
        bar->hour = hour;
        bar->minute = minute;
        bar->open = (double)f32_at(record, 4);
        bar->high = (double)f32_at(record, 8);
        bar->low = (double)f32_at(record, 12);
        bar->close = (double)f32_at(record, 16);
        bar->amount = (double)f32_at(record, 20);
        bar->volume = u32_at(record, 24);
        bar->extra_1 = u16_at(record, 28);
        bar->extra_2 = u16_at(record, 30);
        /* A non-finite value is a decode failure: the scan found none, and a NaN reaching
         * a caller as a price is the failure worth refusing. */
        if (!isfinite(bar->open) || !isfinite(bar->high) || !isfinite(bar->low) ||
            !isfinite(bar->close) || !isfinite(bar->amount)) {
            tdx_error_set(err, ".lc1 record %zu does not hold finite numbers", index);
            return TDX_ERR;
        }
        /* COUNTED, not refused: real files contain such records, and refusing them would
         * reject what the terminal itself writes. */
        highest = bar->open;
        if (bar->low > highest)
            highest = bar->low;
        if (bar->close > highest)
            highest = bar->close;
        lowest = bar->open;
        if (bar->high < lowest)
            lowest = bar->high;
        if (bar->close < lowest)
            lowest = bar->close;
        if (bar->high + 1e-5 < highest || bar->low - 1e-5 > lowest)
            violations++;
    }
    if (out_count)
        *out_count = records;
    if (ohlc_violations)
        *ohlc_violations = violations;
    return TDX_OK;
}

int tdx_lc1_locate(const char *root, int market_id, const char *code, char *out,
                   size_t capacity, tdx_error *err) {
    const char *prefix;

    if (!out || capacity == 0) {
        tdx_error_set(err, "locating a .lc1 file needs somewhere to write the path");
        return TDX_ERR;
    }
    switch (market_id) {
    case 0:
        prefix = "sz";
        break;
    case 1:
        prefix = "sh";
        break;
    case 2:
        prefix = "bj";
        break;
    default:
        tdx_error_set(err, "a .lc1 file lives under sz, sh or bj, not market %d", market_id);
        return TDX_ERR;
    }
    if (snprintf(out, capacity, "%s/vipdoc/%s/minline/%s%s.lc1", root ? root : "", prefix,
                 prefix, code ? code : "") >= (int)capacity) {
        tdx_error_set(err, "the .lc1 path does not fit in %zu bytes", capacity);
        return TDX_ERR;
    }
    return TDX_OK;
}

/* ------------------------------------------------------------------ */
/* writing the format back                                             */
/* ------------------------------------------------------------------ */

int tdx_lc1_encode_date(uint32_t date, uint16_t *out) {
    unsigned year;
    unsigned month;
    unsigned day;
    unsigned word;

    if (!out)
        return 0;
    year = date / 10000;
    month = (date / 100) % 100;
    day = date % 100;
    /* THE BOUND IS THE 16-BIT WORD, NOT THE YEAR FIELD.  The year would reach 4051 but
     * (y - 2004) * 2048 + month * 100 + day has to fit 65535, which makes the last date the
     * format can hold 2035-12-31: 31 * 2048 + 1231 = 64719.  A caller writing a later date
     * would otherwise get a word the reader decodes as some other year entirely. */
    if (year < 2004 || month < 1 || month > 12 || day < 1 || day > 31)
        return 0;
    word = (year - 2004) * 2048 + month * 100 + day;
    if (word > 0xFFFFu)
        return 0;
    *out = (uint16_t)word;
    return 1;
}

int tdx_lc1_from_kline(const tdx_kline_bar *bar, tdx_lc1_bar *out, tdx_error *err) {
    uint16_t date_word;
    unsigned minute_word;

    if (!bar || !out) {
        tdx_error_set(err, "converting a kline bar needs a bar and an output");
        return TDX_ERR;
    }
    memset(out, 0, sizeof(*out));
    if (!tdx_lc1_encode_date((uint32_t)bar->date, &date_word)) {
        tdx_error_set(err, "the date %d cannot be written as an .lc1 date word", bar->date);
        return TDX_ERR;
    }
    minute_word = (unsigned)(bar->hour * 60 + bar->minute);
    if (bar->hour < 0 || bar->hour > 23 || bar->minute < 0 || bar->minute > 59 ||
        minute_word >= 1440) {
        tdx_error_set(err, "%02d:%02d is not a minute of the day", bar->hour, bar->minute);
        return TDX_ERR;
    }
    /* The record's volume is 32 bits wide, and the online aggregates can exceed it.  A
     * truncation here would write a plausible but wrong file, so it is refused. */
    if (bar->volume < 0 || bar->volume > 0xFFFFFFFFLL) {
        tdx_error_set(err, "a volume of %lld does not fit the .lc1 record's 32 bits",
                      (long long)bar->volume);
        return TDX_ERR;
    }
    out->date = (uint32_t)bar->date;
    out->hour = bar->hour;
    out->minute = bar->minute;
    out->open = bar->open;
    out->high = bar->high;
    out->low = bar->low;
    out->close = bar->close;
    out->amount = bar->amount;
    out->volume = (uint32_t)bar->volume;
    out->extra_1 = bar->extra_1;
    out->extra_2 = bar->extra_2;
    return TDX_OK;
}

int tdx_lc1_pack(const tdx_lc1_bar *bars, size_t count, tdx_buf *out, tdx_error *err) {
    size_t index;

    if (!out || (!bars && count)) {
        tdx_error_set(err, "packing .lc1 needs a buffer and the bars");
        return TDX_ERR;
    }
    for (index = 0; index < count; ++index) {
        uint16_t date_word;
        unsigned minute_word = (unsigned)(bars[index].hour * 60 + bars[index].minute);
        if (!tdx_lc1_encode_date(bars[index].date, &date_word)) {
            tdx_error_set(err, "bar %zu holds the date %u, which has no .lc1 date word", index,
                          bars[index].date);
            return TDX_ERR;
        }
        if (bars[index].hour < 0 || bars[index].hour > 23 || bars[index].minute < 0 ||
            bars[index].minute > 59 || minute_word >= 1440) {
            tdx_error_set(err, "bar %zu is at %02d:%02d, which is not a minute of the day",
                          index, bars[index].hour, bars[index].minute);
            return TDX_ERR;
        }
        {
            /* 32 bytes, in the order the reader expects. */
            uint8_t record[32];
            uint32_t amount_bits;
            uint32_t volume_bits = bars[index].volume;
            float open = (float)bars[index].open;
            float high = (float)bars[index].high;
            float low = (float)bars[index].low;
            float close = (float)bars[index].close;
            float amount = (float)bars[index].amount;
            size_t position = 0;
            record[position++] = (uint8_t)(date_word & 0xff);
            record[position++] = (uint8_t)(date_word >> 8);
            record[position++] = (uint8_t)(minute_word & 0xff);
            record[position++] = (uint8_t)((minute_word >> 8) & 0xff);
            memcpy(&amount_bits, &open, sizeof(amount_bits));
            record[position++] = (uint8_t)(amount_bits & 0xff);
            record[position++] = (uint8_t)((amount_bits >> 8) & 0xff);
            record[position++] = (uint8_t)((amount_bits >> 16) & 0xff);
            record[position++] = (uint8_t)((amount_bits >> 24) & 0xff);
            memcpy(&amount_bits, &high, sizeof(amount_bits));
            record[position++] = (uint8_t)(amount_bits & 0xff);
            record[position++] = (uint8_t)((amount_bits >> 8) & 0xff);
            record[position++] = (uint8_t)((amount_bits >> 16) & 0xff);
            record[position++] = (uint8_t)((amount_bits >> 24) & 0xff);
            memcpy(&amount_bits, &low, sizeof(amount_bits));
            record[position++] = (uint8_t)(amount_bits & 0xff);
            record[position++] = (uint8_t)((amount_bits >> 8) & 0xff);
            record[position++] = (uint8_t)((amount_bits >> 16) & 0xff);
            record[position++] = (uint8_t)((amount_bits >> 24) & 0xff);
            memcpy(&amount_bits, &close, sizeof(amount_bits));
            record[position++] = (uint8_t)(amount_bits & 0xff);
            record[position++] = (uint8_t)((amount_bits >> 8) & 0xff);
            record[position++] = (uint8_t)((amount_bits >> 16) & 0xff);
            record[position++] = (uint8_t)((amount_bits >> 24) & 0xff);
            memcpy(&amount_bits, &amount, sizeof(amount_bits));
            record[position++] = (uint8_t)(amount_bits & 0xff);
            record[position++] = (uint8_t)((amount_bits >> 8) & 0xff);
            record[position++] = (uint8_t)((amount_bits >> 16) & 0xff);
            record[position++] = (uint8_t)((amount_bits >> 24) & 0xff);
            record[position++] = (uint8_t)(volume_bits & 0xff);
            record[position++] = (uint8_t)((volume_bits >> 8) & 0xff);
            record[position++] = (uint8_t)((volume_bits >> 16) & 0xff);
            record[position++] = (uint8_t)((volume_bits >> 24) & 0xff);
            record[position++] = (uint8_t)(bars[index].extra_1 & 0xff);
            record[position++] = (uint8_t)(bars[index].extra_1 >> 8);
            record[position++] = (uint8_t)(bars[index].extra_2 & 0xff);
            record[position++] = (uint8_t)(bars[index].extra_2 >> 8);
            if (tdx_buf_append(out, record, sizeof(record), err) != TDX_OK)
                return TDX_ERR;
        }
    }
    return TDX_OK;
}
