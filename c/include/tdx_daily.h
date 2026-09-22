/* tdx_daily.h - the local .day daily-bar files.
 *
 * A .day file is a flat array of 32-byte records with no header:
 *
 *   0   u32  date, YYYYMMDD
 *   4   i32  open      (all four prices are SIGNED, which the reference reads and this
 *   8   i32  high       does too; the date and volume are unsigned)
 *  12   i32  low
 *  16   i32  close
 *  20   f32  amount
 *  24   u32  volume
 *  28   u32  reserved
 *
 * THE PRICE SCALE IS NOT A CONSTANT.  The reference divides every price by 100, which is
 * right for a stock and wrong for everything else, and the difference is a hundredfold.
 * Measured against the live prices for the same securities, the scale is exactly
 *
 *     100 * tdx_price_divisor(code)
 *
 * the same table the wire path already uses: 100 for a stock, 1000 for a fund, 10000 for a
 * convertible bond or a repo.  So a fund and a bond come out of the reference's reader ten
 * and a hundred times too large respectively - and still look like numbers.  Four divisor
 * classes were measured, each within a few per cent of its predicted scale, and the
 * deviation is explained by the day file ending in June while the live price is today's.
 *
 * Two things were measured before deciding what to refuse, over 1,130 files and 4,053,117
 * records:
 *
 *   * every date is a real calendar date, so enforcing that rejects nothing real - and a
 *     date that is not one is a decode failure, not a data quirk;
 *   * no record has a zero or negative price, so a non-positive price is COUNTED and
 *     reported rather than silently passed through, but it is not an error: the sample
 *     cannot prove it never happens, and refusing real data is worse than reporting it. */
#ifndef TDX_DAILY_H
#define TDX_DAILY_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_DAILY_RECORD_SIZE 32
#define TDX_DAILY_BARS_MAX 200000
#define TDX_DAILY_PATH_MAX 512

/* The divisor a .day record's prices are stored in: 100 times the wire divisor table's
 * value for the same code. */
int tdx_daily_scale_divisor(const char *code);

typedef struct tdx_daily_bar {
    uint32_t date;
    double open;
    double high;
    double low;
    double close;
    double amount;
    uint32_t volume;
} tdx_daily_bar;

/* Parses a .day file.  code selects the price scale and may be NULL, in which case the
 * stock scale applies - which is right for a stock and wrong for anything else, so a
 * caller with a code should pass it. */
int tdx_daily_parse(const uint8_t *data, size_t size, const char *code, tdx_daily_bar *out,
                    size_t capacity, size_t *out_count, size_t *suspicious_prices,
                    tdx_error *err);

/* Builds <root>/vipdoc/<market>/lday/<market><code>.day, the layout the terminal uses. */
int tdx_daily_locate(const char *root, int market_id, const char *code, char *out,
                     size_t capacity, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_DAILY_H */
