/* tdx_minute.h - the local .lc1 one-minute bars.
 *
 * A .lc1 file is a flat array of 32-byte records with no header:
 *
 *   0   u16  date word:  year = word / 2048 + 2004, remainder = word % 2048 gives
 *            month = remainder / 100 and day = remainder % 100
 *   2   u16  minute word, hour * 60 + minute
 *   4   f32  open
 *   8   f32  high
 *  12   f32  low
 *  16   f32  close
 *  20   f32  amount
 *  24   u32  volume
 *  28   u16  a word whose meaning is not established
 *  30   u16  a second such word
 *
 * THE PRICES NEED NO SCALE, unlike .day's.  Measured against the live price of the same
 * security across stocks, convertible bonds, funds, an index and a repo, every ratio is
 * of order one - so these floats are already yuan.  This is worth stating because the
 * previous module needed exactly the opposite treatment and a reader might reasonably
 * expect the same rule here.
 *
 * THE OHLC RANGE IS NOT ENFORCED.  The reference refuses a record whose high is below its
 * close, and a scan of 13,905,607 records in 884 real files found SIX that it would
 * refuse - sh000043 has a close of 2608.78 against a high of 2608.77, and sh000689 a
 * close of 1113.96 against a high of 1113.95.  So copying that check would reject files
 * the terminal itself writes.  Violations are COUNTED and reported instead; the same scan
 * found no bad size, no bad date word, no bad minute word and no non-finite value, so
 * those four ARE enforced.
 *
 * THE TWO TRAILING WORDS ARE REPORTED, NOT NAMED.  Measured: every one of sh600519's
 * 16,080 records has both set to zero, while sh000001's carry values that move from minute
 * to minute.  Something instrument-dependent is going on, and inventing a name for it
 * would be worse than handing the caller the two numbers. */
#ifndef TDX_MINUTE_H
#define TDX_MINUTE_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_LC1_RECORD_SIZE 32
#define TDX_LC1_BARS_MAX 200000
#define TDX_LC1_PATH_MAX 512

typedef struct tdx_lc1_bar {
    uint32_t date; /* YYYYMMDD, decoded from the date word */
    int hour;
    int minute;
    double open;
    double high;
    double low;
    double close;
    double amount;
    uint32_t volume;
    uint16_t extra_1;
    uint16_t extra_2;
} tdx_lc1_bar;

/* Decodes the date word.  Returns 0 when it is not a real date. */
int tdx_lc1_decode_date(uint16_t word, uint32_t *out);

/* Parses a .lc1 file.  ohlc_violations may be NULL and counts the records whose high is
 * below, or low above, one of the other three prices - which real files do contain. */
int tdx_lc1_parse(const uint8_t *data, size_t size, tdx_lc1_bar *out, size_t capacity,
                  size_t *out_count, size_t *ohlc_violations, tdx_error *err);

/* Builds <root>/vipdoc/<market>/minline/<market><code>.lc1. */
int tdx_lc1_locate(const char *root, int market_id, const char *code, char *out,
                   size_t capacity, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_MINUTE_H */
