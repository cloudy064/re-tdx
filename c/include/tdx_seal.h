/* tdx_seal.h - the sealed-order figure: how much is queued at a limit.
 *
 * Three situations produce a seal, and they are not the same situation:
 *
 *   auction-imbalance      before the open there is no last price and the two sides cross at
 *                          one price; the queue is the auction imbalance itself
 *   auction-second-level   the same crossed price but with only level 2 populated, which is
 *                          the queue, and level 1's volume joins the denominator
 *   continuous             during trading the last price IS the limit and one side has no
 *                          level 1 at all - that absence is what "sealed" means
 *
 * The last case is the one worth stating: a sealed limit is detected by the ABSENCE of the
 * opposite side, not by its price.  A quote with a level on both sides is not sealed however
 * close to the limit it is.
 *
 * THE AMOUNT AND THE RATIO ARE SIGNED.  A limit-up seal is positive and a limit-down seal is
 * negative, so a caller can rank by seal amount without first asking the direction.  The
 * denominator is the traded volume, plus level 1's volume in the auction-second-level case,
 * because that volume is part of what the seal is measured against.
 *
 * The amount needs the trade unit - shares per lot - which is 100 for the A shares this can
 * apply to at all.  A security the limit rules do not cover is not price limited, so no seal
 * is possible and the calculation says so rather than producing a figure. */
#ifndef TDX_SEAL_H
#define TDX_SEAL_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_error.h"
#include "tdx_limit.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The A-share lot: the only securities the limit rules cover, so the only ones a seal can
 * apply to. */
#define TDX_SEAL_TRADE_UNIT 100.0

typedef struct tdx_seal_level {
    double price;
    int64_t volume_hand;
} tdx_seal_level;

typedef struct tdx_seal_input {
    double last_price;
    uint64_t total_hand;
    double trade_unit;
    uint64_t quote_flags;
    int64_t auction_imbalance_hand;
    tdx_seal_level buys[2];
    tdx_seal_level sells[2];
} tdx_seal_input;

typedef struct tdx_seal_result {
    int available;
    int amount_available;
    int ratio_available;
    /* 1 limit-up, -1 limit-down, 0 not sealed. */
    int direction;
    double amount_yuan;
    double ratio;
    int64_t queue_hand;
    uint64_t denominator_hand;
    const char *mode;
} tdx_seal_result;

/* The three mode names, exposed so a test can compare them without repeating the strings. */
extern const char *const tdx_seal_mode_not_sealed;
extern const char *const tdx_seal_mode_auction_imbalance;
extern const char *const tdx_seal_mode_auction_second_level;
extern const char *const tdx_seal_mode_continuous;

int tdx_seal_calculate(const tdx_seal_input *input, const tdx_limit_prices *limits,
                       tdx_seal_result *out);

/* Fills an input from a 0x0547 depth record, which carries every field the calculation
 * needs.  Levels beyond the second are ignored, as the reference does. */
void tdx_seal_input_from_depth(const tdx_depth *depth, tdx_seal_input *out);

#ifdef __cplusplus
}
#endif

#endif /* TDX_SEAL_H */
