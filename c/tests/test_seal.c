/* test_seal.c - the sealed-order figure.
 *
 * The three paths do not occur together in one capture, so all three are built here: the
 * auction imbalance, the auction's populated second level, and the continuous case.  The
 * continuous case is the one whose DEFINITION is worth pinning down - a security is sealed
 * because the opposite side has no level 1 at all, not because its price is near the limit,
 * so the test asserts that a quote with both sides populated is NOT sealed even when its
 * last price is exactly the limit.
 *
 * The live check is in output/seal_verification_evidence.txt: of 96 securities walked, one
 * was sealed - 000504 at 12.11, which was exactly its upper limit, with 410,742 lots queued
 * and an amount of 497,408,562 yuan.  The amount, the ratio and the limit were each
 * re-derived from that record independently and agreed. */
#include <stdio.h>
#include <string.h>

#include "tdx_limit.h"
#include "tdx_seal.h"
#include "tdx_seal_json.h"
#include "render_check.h"

static int failures = 0;

#define CHECK(condition, ...)                                                        \
    do {                                                                             \
        if (!(condition)) {                                                          \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                              \
            printf(__VA_ARGS__);                                                     \
            printf("\n");                                                            \
            failures++;                                                              \
        }                                                                            \
    } while (0)

static tdx_error error;

static const char *text_of(const tdx_buf *buffer) {
    static char scratch[8192];
    size_t copy = buffer->len < sizeof(scratch) - 1 ? buffer->len : sizeof(scratch) - 1;
    if (copy && buffer->data)
        memcpy(scratch, buffer->data, copy);
    scratch[copy] = '\0';
    return scratch;
}

/* A security the limit rules cover, with a known hundred-share lot. */
static tdx_code equity(void) {
    tdx_code code;
    memset(&code, 0, sizeof(code));
    code.market_id = 0;
    snprintf(code.code, sizeof(code.code), "000504");
    return code;
}

static tdx_limit_prices limits_of(double upper, double lower) {
    tdx_limit_prices limits;
    memset(&limits, 0, sizeof(limits));
    limits.available = 1;
    limits.rate = 0.10;
    limits.upper = upper;
    limits.lower = lower;
    limits.security_class = 0;
    snprintf(limits.source, sizeof(limits.source), "test");
    return limits;
}

static void set_level(tdx_seal_level *level, double price, int64_t volume) {
    level->price = price;
    level->volume_hand = volume;
}

static void test_continuous(void) {
    tdx_seal_input input;
    tdx_seal_result seal;
    tdx_limit_prices limits = limits_of(12.11, 9.91);

    /* AT THE UPPER LIMIT WITH NO ASK.  The absence is what makes it sealed. */
    memset(&input, 0, sizeof(input));
    input.last_price = 12.11;
    input.total_hand = 55812;
    input.trade_unit = 100.0;
    set_level(&input.buys[0], 12.11, 410742);
    CHECK(tdx_seal_calculate(&input, &limits, &seal) == 1, "the calculation runs");
    CHECK(seal.available == 1, "and reports itself available");
    CHECK(seal.direction == 1, "a limit-up seal is direction 1, got %d", seal.direction);
    CHECK(strcmp(seal.mode, tdx_seal_mode_continuous) == 0, "in continuous mode, got %s",
          seal.mode);
    CHECK(seal.queue_hand == 410742, "the queue is the bid's volume, got %lld",
          (long long)seal.queue_hand);
    CHECK(seal.denominator_hand == 55812, "the denominator is the traded volume, got %llu",
          (unsigned long long)seal.denominator_hand);
    /* 12.11 * 410742 * 100 = 497,408,562 yuan. */
    CHECK(seal.amount_available && seal.amount_yuan > 497408561.9 &&
              seal.amount_yuan < 497408562.1,
          "the amount is 497408562.00, got %.2f", seal.amount_yuan);
    /* 410742 / 55812 = 7.359385. */
    CHECK(seal.ratio_available && seal.ratio > 7.3593 && seal.ratio < 7.3595,
          "the ratio is 7.359385, got %.6f", seal.ratio);
    /* A limit-up seal is POSITIVE, so a caller can rank by amount without asking direction. */
    CHECK(seal.amount_yuan > 0 && seal.ratio > 0, "and both are positive for a limit-up");

    /* NOT SEALED EVEN AT THE LIMIT, because both sides are populated. */
    set_level(&input.sells[0], 12.11, 1234);
    CHECK(tdx_seal_calculate(&input, &limits, &seal) == 1, "the calculation runs");
    CHECK(seal.direction == 0, "both sides populated is NOT a seal, got %d", seal.direction);
    CHECK(strcmp(seal.mode, tdx_seal_mode_not_sealed) == 0, "mode not-sealed, got %s",
          seal.mode);
    CHECK(seal.queue_hand == 0, "with no queue, got %lld", (long long)seal.queue_hand);
    CHECK(seal.amount_yuan == 0.0, "and no amount");

    /* AT THE LOWER LIMIT WITH NO BID: the mirror image, and both figures NEGATIVE. */
    memset(&input, 0, sizeof(input));
    input.last_price = 9.91;
    input.total_hand = 20000;
    input.trade_unit = 100.0;
    set_level(&input.sells[0], 9.91, 5000);
    CHECK(tdx_seal_calculate(&input, &limits, &seal) == 1, "runs");
    CHECK(seal.direction == -1, "a limit-down seal is direction -1, got %d", seal.direction);
    CHECK(seal.queue_hand == 5000, "the queue is the ask's volume, got %lld",
          (long long)seal.queue_hand);
    CHECK(seal.amount_yuan < 0 && seal.amount_yuan > -4955000.1 &&
              seal.amount_yuan < -4954999.9,
          "the amount is -4955000, got %.2f", seal.amount_yuan);
    CHECK(seal.ratio < 0 && seal.ratio > -0.25001 && seal.ratio < -0.24999,
          "and the ratio is -0.25, got %.6f", seal.ratio);

    /* A REJECTED QUOTE STATE takes the continuous path out of play. */
    input.quote_flags = 0x1C;
    CHECK(tdx_seal_calculate(&input, &limits, &seal) == 1, "runs");
    CHECK(seal.direction == 0, "a rejected state is not sealed, got %d", seal.direction);
    CHECK(strcmp(seal.mode, tdx_seal_mode_not_sealed) == 0, "and says not-sealed");
    /* THE MASK KEEPS FOUR BITS AND COMPARES THEM, so it is not "any rejected flag": 0x3C &
     * 0x3C is 0x3C, which is not 0x1C, and the seal stands. */
    input.quote_flags = 0x3C;
    CHECK(tdx_seal_calculate(&input, &limits, &seal) == 1 && seal.direction == -1,
          "0x3C is NOT a rejected state, got %d", seal.direction);
    input.quote_flags = 0x1D;
    CHECK(tdx_seal_calculate(&input, &limits, &seal) == 1 && seal.direction == 0,
          "0x1D is: the low four bits match 0x1C");
    /* Bits above the mask are ignored, so 0x5C is the same state as 0x1C. */
    input.quote_flags = 0x5C;
    CHECK(tdx_seal_calculate(&input, &limits, &seal) == 1 && seal.direction == 0,
          "and so is 0x5C, since the mask drops the bit above it");
    input.quote_flags = 0x0C;
    CHECK(tdx_seal_calculate(&input, &limits, &seal) == 1 && seal.direction == -1,
          "while a different value leaves the seal in place, got %d", seal.direction);

    /* A price that is NEAR but not AT the limit is not a seal. */
    memset(&input, 0, sizeof(input));
    input.last_price = 12.10;
    input.total_hand = 1000;
    input.trade_unit = 100.0;
    set_level(&input.buys[0], 12.10, 500);
    CHECK(tdx_seal_calculate(&input, &limits, &seal) == 1 && seal.direction == 0,
          "12.10 against a 12.11 limit is not sealed, got %d", seal.direction);
}

static void test_auction(void) {
    tdx_seal_input input;
    tdx_seal_result seal;
    tdx_limit_prices limits = limits_of(12.11, 9.91);

    /* BEFORE THE OPEN: no last price, and the two sides cross at the limit.  The queue is
     * the auction imbalance, not either side's volume. */
    memset(&input, 0, sizeof(input));
    input.total_hand = 0;
    input.trade_unit = 100.0;
    input.auction_imbalance_hand = 4200;
    set_level(&input.buys[0], 12.11, 999);
    set_level(&input.sells[0], 12.11, 999);
    CHECK(tdx_seal_calculate(&input, &limits, &seal) == 1, "runs");
    CHECK(seal.direction == 1, "a limit-up auction is direction 1, got %d", seal.direction);
    CHECK(strcmp(seal.mode, tdx_seal_mode_auction_imbalance) == 0,
          "in auction-imbalance mode, got %s", seal.mode);
    CHECK(seal.queue_hand == 4200, "the queue is the imbalance, got %lld",
          (long long)seal.queue_hand);
    /* Nothing has traded, so the ratio is unavailable rather than zero. */
    CHECK(seal.ratio_available == 0, "an untraded session has no ratio");
    CHECK(seal.denominator_hand == 0, "and no denominator");

    /* A NEGATIVE imbalance at the lower limit. */
    input.auction_imbalance_hand = -3000;
    set_level(&input.buys[0], 9.91, 500);
    set_level(&input.sells[0], 9.91, 500);
    CHECK(tdx_seal_calculate(&input, &limits, &seal) == 1 && seal.direction == -1,
          "a negative imbalance at the floor, got %d", seal.direction);
    CHECK(seal.queue_hand == 3000, "whose queue is the magnitude, got %lld",
          (long long)seal.queue_hand);

    /* The imbalance's SIGN must agree with the limit it is at, or nothing is claimed. */
    input.auction_imbalance_hand = -3000;
    set_level(&input.buys[0], 12.11, 500);
    set_level(&input.sells[0], 12.11, 500);
    CHECK(tdx_seal_calculate(&input, &limits, &seal) == 1 && seal.direction == 0,
          "a negative imbalance at the ceiling is not a seal, got %d", seal.direction);
}

static void test_auction_second_level(void) {
    tdx_seal_input input;
    tdx_seal_result seal;
    tdx_limit_prices limits = limits_of(12.11, 9.91);

    /* A last price is present, the two sides cross, and ONLY LEVEL 2 is populated: level 2
     * is the queue and level 1's volume joins the denominator. */
    memset(&input, 0, sizeof(input));
    input.last_price = 11.50;
    input.total_hand = 10000;
    input.trade_unit = 100.0;
    set_level(&input.buys[0], 12.11, 777);
    set_level(&input.sells[0], 12.11, 0);
    /* LEVEL 2'S PRICE IS ABSENT AND ITS VOLUME IS NOT: that is the shape this branch exists
     * for, and setting a price here would take the record out of it. */
    set_level(&input.buys[1], 0.0, 5000);
    CHECK(tdx_seal_calculate(&input, &limits, &seal) == 1, "runs");
    CHECK(seal.direction == 1, "direction 1, got %d", seal.direction);
    CHECK(strcmp(seal.mode, tdx_seal_mode_auction_second_level) == 0,
          "in auction-second-level mode, got %s", seal.mode);
    CHECK(seal.queue_hand == 5000, "the queue is level 2, got %lld",
          (long long)seal.queue_hand);
    /* The denominator has level 1's volume added, which is the part that is easy to miss. */
    CHECK(seal.denominator_hand == 10777, "the denominator is 10000 + 777, got %llu",
          (unsigned long long)seal.denominator_hand);
    CHECK(seal.ratio > 0.4639 && seal.ratio < 0.4641, "ratio 5000/10777, got %.6f",
          seal.ratio);

    /* With level 2 on the sell side and the floor, the mirror image. */
    memset(&input, 0, sizeof(input));
    input.last_price = 10.50;
    input.total_hand = 2000;
    input.trade_unit = 100.0;
    set_level(&input.buys[0], 9.91, 0);
    set_level(&input.sells[0], 9.91, 333);
    set_level(&input.sells[1], 0.0, 4444);
    CHECK(tdx_seal_calculate(&input, &limits, &seal) == 1 && seal.direction == -1,
          "a floor-side second level, got %d", seal.direction);
    CHECK(seal.denominator_hand == 2333, "denominator 2000 + 333, got %llu",
          (unsigned long long)seal.denominator_hand);
    CHECK(seal.amount_yuan < 0, "and the amount is negative, got %.2f", seal.amount_yuan);

    /* A LEVEL-2 PRICE TAKES THE RECORD OUT OF THE BRANCH, which is the condition that reads
     * oddly and is worth pinning down. */
    memset(&input, 0, sizeof(input));
    input.last_price = 11.50;
    input.total_hand = 10000;
    input.trade_unit = 100.0;
    set_level(&input.buys[0], 12.11, 777);
    set_level(&input.sells[0], 12.11, 0);
    set_level(&input.buys[1], 12.11, 5000); /* a price, so not this branch */
    CHECK(tdx_seal_calculate(&input, &limits, &seal) == 1 && seal.direction == 0,
          "a populated level-2 price is not the second-level case, got %d", seal.direction);
    /* Nor is it the continuous case, because the ask side has a level. */
    CHECK(strcmp(seal.mode, tdx_seal_mode_not_sealed) == 0, "and it is not sealed at all");
}

static void test_unavailable(void) {
    tdx_seal_input input;
    tdx_seal_result seal;
    tdx_limit_prices limits = limits_of(12.11, 9.91);

    memset(&input, 0, sizeof(input));
    input.last_price = 12.11;
    input.total_hand = 100;
    input.trade_unit = 100.0;
    set_level(&input.buys[0], 12.11, 10);

    /* A security the limit rules do not cover has no seal to compute. */
    {
        tdx_limit_prices unavailable;
        memset(&unavailable, 0, sizeof(unavailable));
        CHECK(tdx_seal_calculate(&input, &unavailable, &seal) == 0,
              "an unavailable limit gives no result");
        CHECK(seal.available == 0, "and says so");
    }
    /* A trade unit of zero leaves the amount unavailable but not the direction. */
    memset(&input, 0, sizeof(input));
    input.last_price = 12.11;
    input.total_hand = 100;
    input.trade_unit = 0.0;
    set_level(&input.buys[0], 12.11, 10);
    CHECK(tdx_seal_calculate(&input, &limits, &seal) == 1, "runs");
    CHECK(seal.direction == 1, "still sealed");
    CHECK(seal.amount_available == 0, "but the amount is unavailable");
    CHECK(seal.ratio_available == 1, "while the ratio still works");
    /* No trades means no ratio. */
    input.total_hand = 0;
    CHECK(tdx_seal_calculate(&input, &limits, &seal) == 1 && seal.ratio_available == 0,
          "an untraded security has no ratio");
    CHECK(tdx_seal_calculate(NULL, &limits, &seal) == 0, "a null input is refused");
    CHECK(tdx_seal_calculate(&input, &limits, NULL) == 0, "and a null output");
}

static void test_rendering(void) {
    tdx_seal_input input;
    tdx_seal_result seal;
    tdx_limit_prices limits = limits_of(12.11, 9.91);
    tdx_code code = equity();
    tdx_buf line = {0};
    char reason[192];

    memset(&input, 0, sizeof(input));
    input.last_price = 12.11;
    input.total_hand = 55812;
    input.trade_unit = 100.0;
    set_level(&input.buys[0], 12.11, 410742);
    CHECK(tdx_seal_calculate(&input, &limits, &seal) == 1, "runs");
    tdx_buf_init(&line);
    CHECK(tdx_seal_format(&line, &code, &limits, &seal, &input, "20260922", &error) == TDX_OK,
          "render: %s", error.message);
    CHECK(render_parses(text_of(&line), reason, sizeof(reason)),
          "the seal line parses: %s\n    %s", reason, text_of(&line));
    CHECK(strstr(text_of(&line), "\"type\":\"seal\"") != NULL, "the type");
    CHECK(strstr(text_of(&line), "\"mode\":\"continuous\"") != NULL, "the mode");
    CHECK(strstr(text_of(&line), "\"queue_hand\":410742") != NULL, "the queue");
    /* THE INPUTS TRAVEL WITH THE RESULT: a seal is a claim about a particular quote. */
    CHECK(strstr(text_of(&line), "\"last_price\":12.1100") != NULL, "the quote is echoed");
    CHECK(strstr(text_of(&line), "\"bid1_volume_hand\":410742") != NULL, "including the levels");
    CHECK(strstr(text_of(&line), "\"trade_unit\":100") != NULL, "and the trade unit");
    CHECK(strstr(text_of(&line), "\"rate\":0.1000") != NULL, "and the limit it used");

    /* The unavailable cases render as null rather than as zero, so a caller can tell "no
     * ratio" from "a ratio of nothing". */
    {
        tdx_seal_input untraded;
        tdx_seal_result other;
        memset(&untraded, 0, sizeof(untraded));
        untraded.trade_unit = 100.0;
        untraded.auction_imbalance_hand = 500;
        set_level(&untraded.buys[0], 12.11, 100);
        set_level(&untraded.sells[0], 12.11, 100);
        CHECK(tdx_seal_calculate(&untraded, &limits, &other) == 1, "runs");
        CHECK(other.ratio_available == 0, "no ratio");
        tdx_buf_clear(&line);
        CHECK(tdx_seal_format(&line, &code, &limits, &other, &untraded, NULL, &error) == TDX_OK,
              "render: %s", error.message);
        CHECK(render_parses(text_of(&line), reason, sizeof(reason)), "and it parses: %s",
              reason);
        CHECK(strstr(text_of(&line), "\"ratio\":null") != NULL, "the ratio is null: %s",
              text_of(&line));
        CHECK(strstr(text_of(&line), "\"as_of\":null") != NULL, "and a null date");
    }
    tdx_buf_free(&line);
}

int main(void) {
    test_continuous();
    test_auction();
    test_auction_second_level();
    test_unavailable();
    test_rendering();

    if (failures) {
        printf("%d seal check(s) failed\n", failures);
        return 1;
    }
    printf("seal checks passed\n");
    return 0;
}
