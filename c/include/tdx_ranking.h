/* tdx_ranking.h - the category ranking over 0x054B.
 *
 * A paginated ranking of one category's securities, sorted by one of a fixed set of keys.
 * The request body is nine little-endian u16s:
 *
 *   category, sort, start, count, reverse, 5, filter_raw, 1, 0
 *
 * where reverse is 0 when no sort key was given, 2 for ascending and 1 for descending - so
 * "descending" is the default and the value is not simply a flag.
 *
 * A RECORD IS NOT FIXED-LENGTH, and it is not varint-only either:
 *
 *   0        u8    market id, 0..2
 *   1..6     char[6]  digits
 *   7        u16   a word the reference calls active1
 *   9        nine varints: close, then four DELTAS from it (previous close, open, high,
 *            low), then the server time, a raw price, and two hand counts
 *   then     u32   the amount, in the wire's own scaled form
 *   then     eight varints: inside dish, outer disc, one raw, the opening amount in yuan
 *            (scaled by 100), the bid and ask prices as DELTAS from the close, and their
 *            volumes
 *   then     56 bytes of fixed tail, which is where the speeds, the two floats and two
 *            unmodelled byte runs live
 *
 * THE PRICES ARE DELTAS FROM THE CLOSE.  Five of them arrive as an offset from the record's
 * own close rather than as absolute values, so a reader that treats each varint as a price
 * gets one right and four wrong.  The scale is the same one the wire path already uses:
 * raw / 100 / the divisor table for the code.
 *
 * TWO RUNS OF BYTES IN THE TAIL ARE NOT MODELLED: ten bytes at offset 12 and twenty-four at
 * offset 30.  They are reported as hex rather than given a meaning they have not earned -
 * the same treatment the two unmodelled tails in the L1 commands get. */
#ifndef TDX_RANKING_H
#define TDX_RANKING_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bonds.h"
#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_CMD_CATEGORY_QUOTES 0x054B
#define TDX_RANKING_CATEGORY_A_SHARES 6
#define TDX_RANKING_REQUEST_SIZE 18
#define TDX_RANKING_PAGE_MAX 80
#define TDX_RANKING_RECORDS_MAX 4096
#define TDX_RANKING_HEX_MAX 64

/* The named sort keys.  An unknown name is parsed as a number, which is how the reference
 * lets a caller reach a key the table does not name. */
int tdx_ranking_sort_id(const char *name, uint16_t *out);
int tdx_ranking_category_id(const char *name, uint16_t *out);
/* The name of a sort key, or NULL when the table does not list it. */
const char *tdx_ranking_sort_name(uint16_t id);

/* Builds the 18-byte body.  count must be 1..80, which the server enforces. */
int tdx_ranking_build_request(uint16_t category, uint16_t sort, uint16_t start, uint16_t count,
                              int ascending, uint16_t filter_raw, tdx_buf *out, tdx_error *err);

typedef struct tdx_ranking_record {
    int market_id;
    char security_id[24];
    char code[8];
    uint16_t active1;
    uint16_t active2;

    double last_price;
    double pre_close_price;
    double open_price;
    double high_price;
    double low_price;
    double amount;
    double open_amount_yuan;
    double bid1_price;
    double ask1_price;

    int64_t server_time_raw;
    int64_t neg_price_raw;
    int64_t total_hand;
    int64_t current_hand;
    int64_t inside_dish;
    int64_t outer_disc;
    int64_t after_outer_raw;
    int64_t bid1_volume_hand;
    int64_t ask1_volume_hand;

    uint16_t status_or_sort_raw;
    double rise_speed;
    double short_turnover;
    double two_minute_amount;
    double opening_rush;
    double volume_rise_speed;
    double depth;

    /* The two runs the reference leaves unmodelled, as hex. */
    char extra_pair_hex[TDX_RANKING_HEX_MAX];
    char extra_meta_hex[TDX_RANKING_HEX_MAX];
} tdx_ranking_record;

typedef struct tdx_ranking_page {
    uint16_t header;
    size_t records;
} tdx_ranking_page;

/* Parses one response body.  Trailing bytes are an error rather than ignored: the record
 * walk has to land exactly on the end or the layout is not what this reader thinks it is.
 * code_length selects the price divisor, as the wire path does. */
int tdx_ranking_parse(const uint8_t *payload, size_t size, tdx_ranking_record *out,
                      size_t capacity, tdx_ranking_page *page, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_RANKING_H */
