/* tdx_seal.c - the sealed-order figure: how much is queued at a limit. */
#include "tdx_seal.h"

#include <math.h>
#include <string.h>

const char *const tdx_seal_mode_not_sealed = "not-sealed";
const char *const tdx_seal_mode_auction_imbalance = "auction-imbalance";
const char *const tdx_seal_mode_auction_second_level = "auction-second-level";
const char *const tdx_seal_mode_continuous = "continuous";

/* A price counts as present above this, and two prices count as the same within that.  The
 * second is the tighter of the two in the reference, which matters because a limit price and
 * a quoted price can differ in the last place of a float. */
static const double seal_present = 0.00009999999747378752;
static const double seal_match = 0.000090000001;

static int present(double value) {
    return value > seal_present;
}

static int same_price(double left, double right, double tolerance) {
    return fabs(left - right) < tolerance;
}

static int64_t absolute_queue(int64_t value) {
    /* The reference guards the theoretical INT64_MIN; so does this. */
    if (value >= 0)
        return value;
    if (value == INT64_MIN)
        return INT64_MAX;
    return -value;
}

void tdx_seal_input_from_depth(const tdx_depth *depth, tdx_seal_input *out) {
    size_t index;
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    if (!depth)
        return;
    out->last_price = depth->last;
    out->total_hand = depth->total_hand > 0 ? (uint64_t)depth->total_hand : 0;
    out->trade_unit = TDX_SEAL_TRADE_UNIT;
    out->quote_flags = depth->status > 0 ? (uint64_t)depth->status : 0;
    /* The reference falls back to the field after the outer volume when the auction imbalance
     * is absent, which two different builds of the record put there. */
    out->auction_imbalance_hand = depth->auction_imbalance_hand != 0
                                      ? depth->auction_imbalance_hand
                                      : depth->unknown_after_outer;
    for (index = 0; index < 2; ++index) {
        out->buys[index].price = depth->buys[index].price;
        out->buys[index].volume_hand = depth->buys[index].volume_hand;
        out->sells[index].price = depth->sells[index].price;
        out->sells[index].volume_hand = depth->sells[index].volume_hand;
    }
}

int tdx_seal_calculate(const tdx_seal_input *input, const tdx_limit_prices *limits,
                       tdx_seal_result *out) {
    const tdx_seal_level *bid1;
    const tdx_seal_level *ask1;
    const tdx_seal_level *bid2;
    const tdx_seal_level *ask2;
    int direction = 0;
    int64_t queue = 0;
    uint64_t denominator;
    int rejected_state;

    if (!out)
        return 0;
    memset(out, 0, sizeof(*out));
    out->mode = tdx_seal_mode_not_sealed;
    if (!input || !limits || !limits->available || !isfinite(input->last_price) ||
        !isfinite(input->trade_unit))
        return 0;
    out->available = 1;
    out->ratio_available = input->total_hand != 0;
    out->amount_available = input->trade_unit > 0.0;

    bid1 = &input->buys[0];
    ask1 = &input->sells[0];
    bid2 = &input->buys[1];
    ask2 = &input->sells[1];
    denominator = input->total_hand;

    if (!present(input->last_price) && present(bid1->price) &&
        same_price(bid1->price, ask1->price, seal_present)) {
        /* Before the open: the two sides cross at one price and there is no trade yet. */
        if (same_price(bid1->price, limits->upper, seal_match) &&
            input->auction_imbalance_hand > 0) {
            direction = 1;
            queue = absolute_queue(input->auction_imbalance_hand);
            out->mode = tdx_seal_mode_auction_imbalance;
        } else if (same_price(bid1->price, limits->lower, seal_match) &&
                   input->auction_imbalance_hand < 0) {
            direction = -1;
            queue = absolute_queue(input->auction_imbalance_hand);
            out->mode = tdx_seal_mode_auction_imbalance;
        }
    } else if (present(input->last_price) && present(bid1->price) &&
               same_price(bid1->price, ask1->price, seal_present) &&
               !present(bid2->price) && !present(ask2->price)) {
        /* A crossed auction price with only level 2 populated: level 2 IS the queue, and
         * level 1's volume joins what the seal is measured against. */
        if (bid2->volume_hand && same_price(bid1->price, limits->upper, seal_present)) {
            direction = 1;
            queue = bid2->volume_hand;
            denominator += (uint64_t)bid1->volume_hand;
            out->mode = tdx_seal_mode_auction_second_level;
        } else if (ask2->volume_hand && same_price(bid1->price, limits->lower, seal_present)) {
            direction = -1;
            queue = ask2->volume_hand;
            denominator += (uint64_t)ask1->volume_hand;
            out->mode = tdx_seal_mode_auction_second_level;
        }
    }

    /* A rejected quote state means the continuous path is not read. */
    rejected_state = (input->quote_flags & 0x3CU) == 0x1CU;
    if (!direction && !rejected_state &&
        same_price(input->last_price, limits->upper, seal_match) && present(bid1->price) &&
        !present(ask1->price)) {
        /* Sealed at the upper limit: the ABSENCE of the ask is what says so. */
        direction = 1;
        queue = bid1->volume_hand;
        out->mode = tdx_seal_mode_continuous;
    } else if (!direction && !rejected_state &&
               same_price(input->last_price, limits->lower, seal_match) &&
               !present(bid1->price) && present(ask1->price)) {
        direction = -1;
        queue = ask1->volume_hand;
        out->mode = tdx_seal_mode_continuous;
    }

    out->direction = direction;
    out->queue_hand = queue;
    out->denominator_hand = denominator;
    if (!direction)
        return 1;
    {
        /* The price the seal sits at: the bid for a limit-up, the ask for a limit-down. */
        const double price = direction > 0 ? bid1->price : ask1->price;
        if (out->amount_available)
            out->amount_yuan =
                (double)direction * price * (double)queue * input->trade_unit;
    }
    if (denominator != 0) {
        out->ratio_available = 1;
        out->ratio = (double)direction * (double)queue / (double)denominator;
    } else {
        out->ratio_available = 0;
    }
    return 1;
}
