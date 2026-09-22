/* tdx_daily.c - the local .day daily-bar files. */
#include "tdx_daily.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "tdx_quote.h"

int tdx_daily_scale_divisor(const char *code) {
    /* 100 times the wire table: a stock's prices are hundredths, a fund's are thousandths
     * and a bond's or repo's are ten-thousandths. */
    return 100 * tdx_price_divisor(code);
}

static uint32_t u32_at(const uint8_t *data, size_t offset) {
    return (uint32_t)data[offset] | ((uint32_t)data[offset + 1] << 8) |
           ((uint32_t)data[offset + 2] << 16) | ((uint32_t)data[offset + 3] << 24);
}

static int32_t i32_at(const uint8_t *data, size_t offset) {
    return (int32_t)u32_at(data, offset);
}

static float f32_at(const uint8_t *data, size_t offset) {
    float value;
    uint32_t bits = u32_at(data, offset);
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* A real calendar date, including leap years.  Measured over 4,053,117 records in 1,130
 * files: not one is anything else, so this rejects nothing that exists. */
static int date_is_sane(uint32_t date) {
    static const unsigned lengths[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    unsigned year = date / 10000;
    unsigned month = date / 100 % 100;
    unsigned day = date % 100;

    if (year < 1900 || year > 2200 || month < 1 || month > 12 || day < 1)
        return 0;
    if (day > lengths[month]) {
        int leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        return month == 2 && leap && day == 29;
    }
    return 1;
}

int tdx_daily_parse(const uint8_t *data, size_t size, const char *code, tdx_daily_bar *out,
                    size_t capacity, size_t *out_count, size_t *suspicious_prices,
                    tdx_error *err) {
    size_t records;
    size_t index;
    double divisor;

    if (out_count)
        *out_count = 0;
    if (suspicious_prices)
        *suspicious_prices = 0;
    if (!data || !out) {
        tdx_error_set(err, "parsing a .day file needs bytes and an output");
        return TDX_ERR;
    }
    if (size % TDX_DAILY_RECORD_SIZE != 0) {
        tdx_error_set(err, "a %zu-byte .day file is not a multiple of %d", size,
                      TDX_DAILY_RECORD_SIZE);
        return TDX_ERR;
    }
    records = size / TDX_DAILY_RECORD_SIZE;
    if (records > capacity) {
        tdx_error_set(err, "the .day file holds %zu records, more than %zu", records, capacity);
        return TDX_ERR;
    }
    divisor = (double)tdx_daily_scale_divisor(code);
    for (index = 0; index < records; ++index) {
        const uint8_t *record = data + index * TDX_DAILY_RECORD_SIZE;
        tdx_daily_bar *bar = &out[index];
        float amount;

        bar->date = u32_at(record, 0);
        if (!date_is_sane(bar->date)) {
            tdx_error_set(err, ".day record %zu has date %u", index, bar->date);
            return TDX_ERR;
        }
        bar->open = (double)i32_at(record, 4) / divisor;
        bar->high = (double)i32_at(record, 8) / divisor;
        bar->low = (double)i32_at(record, 12) / divisor;
        bar->close = (double)i32_at(record, 16) / divisor;
        amount = f32_at(record, 20);
        /* The amount is a float in the file and not always finite; the reference zeroes it
         * rather than propagating it, and a NaN reaching a caller as a price-like number
         * is exactly the failure worth avoiding. */
        bar->amount = isfinite(amount) ? (double)amount : 0.0;
        bar->volume = u32_at(record, 24);
        /* Counted, not refused: the sample has none, which is not the same as proof that
         * none can exist, and rejecting real data is worse than reporting it. */
        if (!(bar->open > 0.0) || !(bar->high > 0.0) || !(bar->low > 0.0) ||
            !(bar->close > 0.0)) {
            if (suspicious_prices)
                (*suspicious_prices)++;
        }
    }
    if (out_count)
        *out_count = records;
    return TDX_OK;
}

int tdx_daily_locate(const char *root, int market_id, const char *code, char *out,
                     size_t capacity, tdx_error *err) {
    const char *prefix;

    if (!out || capacity == 0) {
        tdx_error_set(err, "locating a .day file needs somewhere to write the path");
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
        /* The extended markets live under a numeric directory, as the reference has it. */
        if (snprintf(out, capacity, "%s/vipdoc/ds/lday/%d#%s.day", root ? root : "", market_id,
                     code ? code : "") >= (int)capacity) {
            tdx_error_set(err, "the .day path does not fit in %zu bytes", capacity);
            return TDX_ERR;
        }
        return TDX_OK;
    }
    if (snprintf(out, capacity, "%s/vipdoc/%s/lday/%s%s.day", root ? root : "", prefix, prefix,
                 code ? code : "") >= (int)capacity) {
        tdx_error_set(err, "the .day path does not fit in %zu bytes", capacity);
        return TDX_ERR;
    }
    return TDX_OK;
}
