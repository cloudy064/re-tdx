/* test_minute.c - the local .lc1 one-minute bars.
 *
 * The fixture is two windows of real files, chosen for the decision they test rather than
 * for being tidy.  One is ordinary; the other is a window around a record whose close is
 * ABOVE its high - which the reference refuses and which real files contain.  A fixture
 * without such a record could not test the choice to count rather than refuse, and that
 * choice is the one thing about this format most likely to be got wrong by copying.
 *
 * The scale is worth a word too, because it is the opposite of .day's: those prices are
 * scaled integers needing a per-instrument divisor, and these are floats that are already
 * yuan.  Measured across stocks, bonds, funds, an index and a repo, every ratio against
 * the live price is of order one. */
#include <stdio.h>
#include <string.h>

#include "tdx_minute.h"
#include "tdx_minute_json.h"
#include "render_check.h"
#include "minute_fixtures.h"

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

static void test_date_word(void) {
    uint32_t date = 0;

    /* The encoding: year = word / 2048 + 2004, remainder gives MMDD. */
    CHECK(tdx_lc1_decode_date(43814, &date) == 1 && date == 20250806,
          "word 43814 is 20250806, got %u", date);
    CHECK(tdx_lc1_decode_date(44122, &date) == 1 && date == 20251114,
          "word 44122 is 20251114, got %u", date);
    /* Word 0 is NOT 2004-01-01: its remainder is 0, so the month is 0, which is not a
     * date.  The smallest word that decodes is 101. */
    CHECK(tdx_lc1_decode_date(0, &date) == 0, "word 0 is refused");
    CHECK(tdx_lc1_decode_date(101, &date) == 1 && date == 20040101,
          "word 101 is 20040101, got %u", date);
    /* Month 0 and month 13 are not dates. */
    CHECK(tdx_lc1_decode_date(2048 - 100, &date) == 0, "a month of 0 is refused");
    CHECK(tdx_lc1_decode_date(2048 - 1, &date) == 0, "and a remainder of 2047 is not MMDD");
    CHECK(tdx_lc1_decode_date(0, NULL) == 0, "a null output is refused");
}

static void test_plain_window(void) {
    tdx_lc1_bar bars[MINUTE_PLAIN_RECORDS + 2];
    size_t count = 0;
    size_t violations = 0;

    error.message[0] = '\0';
    CHECK(tdx_lc1_parse(minute_plain, MINUTE_PLAIN_BYTES, bars, MINUTE_PLAIN_RECORDS + 2,
                        &count, &violations, &error) == TDX_OK,
          "the plain window parses: %s", error.message);
    CHECK(count == MINUTE_PLAIN_RECORDS, "%d bars, got %zu", MINUTE_PLAIN_RECORDS, count);
    CHECK(violations == 0, "and none breaks the range, got %zu", violations);

    /* The values the bytes encode.  The close is 1425.940 and not 1425940: these are
     * already yuan, unlike .day's scaled integers. */
    CHECK(bars[0].date == 20250806, "the first bar is 20250806, got %u", bars[0].date);
    CHECK(bars[0].hour == 9 && bars[0].minute == 31, "at 09:31, got %02d:%02d", bars[0].hour,
          bars[0].minute);
    CHECK(bars[0].open == 1429.0, "open 1429.000, got %.3f", bars[0].open);
    CHECK(bars[0].high == 1429.0, "high 1429.000, got %.3f", bars[0].high);
    CHECK(bars[0].low == 1424.0, "low 1424.000, got %.3f", bars[0].low);
    CHECK(bars[0].close > 1425.93 && bars[0].close < 1425.95,
          "close 1425.940, got %.3f", bars[0].close);
    CHECK(bars[0].volume == 61800, "volume 61800, got %u", bars[0].volume);
    CHECK(bars[1].close > 1426.97 && bars[1].close < 1426.99, "the second close is 1426.980");
    /* The trailing words on a stock are zero, which the measurement found for all 16,080
     * records of this file.  They are reported as they are, not named. */
    CHECK(bars[0].extra_1 == 0 && bars[0].extra_2 == 0,
          "a stock's trailing words are zero, got %u/%u", bars[0].extra_1, bars[0].extra_2);
}

static void test_violation_is_counted_not_refused(void) {
    tdx_lc1_bar bars[MINUTE_VIOLATION_RECORDS + 2];
    size_t count = 0;
    size_t violations = 0;

    error.message[0] = '\0';
    /* THE DECISION UNDER TEST: the reference refuses such a record; this returns it and
     * counts it.  If this ever starts returning TDX_ERR, the reader has begun rejecting
     * files the terminal itself writes. */
    CHECK(tdx_lc1_parse(minute_violation, MINUTE_VIOLATION_BYTES, bars,
                        MINUTE_VIOLATION_RECORDS + 2, &count, &violations,
                        &error) == TDX_OK,
          "a window containing a real violation still parses: %s", error.message);
    CHECK(count == MINUTE_VIOLATION_RECORDS, "%d bars, got %zu", MINUTE_VIOLATION_RECORDS,
          count);
    CHECK(violations == 1, "and exactly one is counted, got %zu", violations);

    /* The offending record itself: the 15:00 closing minute, whose close is 2608.780
     * against a high of 2608.770.  A closing auction is a plausible reason for a close a
     * hundredth outside the range, which is why refusing it would be wrong. */
    CHECK(MINUTE_VIOLATION_AT == 2, "the fixture says the violation is at index 2, got %d",
          MINUTE_VIOLATION_AT);
    if (MINUTE_VIOLATION_AT < (int)count) {
        const tdx_lc1_bar *bar = &bars[MINUTE_VIOLATION_AT];
        CHECK(bar->date == 20251015, "it is dated 20251015, got %u", bar->date);
        CHECK(bar->hour == 15 && bar->minute == 0, "at 15:00, got %02d:%02d", bar->hour,
              bar->minute);
        CHECK(bar->close > bar->high,
              "with the close above the high: %.3f against %.3f", bar->close, bar->high);
        CHECK(bar->close > 2608.77 && bar->close < 2608.79, "close 2608.780, got %.3f",
              bar->close);
        CHECK(bar->high > 2608.76 && bar->high < 2608.78, "high 2608.770, got %.3f",
              bar->high);
        CHECK(bar->volume == 19439897, "and the closing auction's volume, got %u",
              bar->volume);
        /* The values are passed through unchanged: counting is not correcting. */
        CHECK(bar->open > 2608.03 && bar->open < 2608.05, "open 2608.040, got %.3f",
              bar->open);
    }
    /* A null violation counter is allowed: a caller that does not care need not ask. */
    error.message[0] = '\0';
    CHECK(tdx_lc1_parse(minute_violation, MINUTE_VIOLATION_BYTES, bars,
                        MINUTE_VIOLATION_RECORDS + 2, &count, NULL, &error) == TDX_OK,
          "a null counter is allowed: %s", error.message);
}

static void test_refusals(void) {
    tdx_lc1_bar bars[4];
    size_t count = 0;
    size_t violations = 0;
    unsigned char record[TDX_LC1_RECORD_SIZE];
    /* A word for 20260101, which is a real date: 22 * 2048 + 101. */
    const uint16_t good_word = (uint16_t)(22 * 2048 + 101);
    const uint16_t bad_word = (uint16_t)(100 * 2048 + 101); /* year 2104 with month 1 ... */
    const uint16_t month_zero = (uint16_t)(22 * 2048 + 1);

    error.message[0] = '\0';
    /* A size that is not a multiple of 32 is not this format. */
    CHECK(tdx_lc1_parse(minute_plain, MINUTE_PLAIN_BYTES - 1, bars, 8, &count, &violations,
                        &error) == TDX_ERR,
          "a size that is not a multiple of 32 is refused");
    CHECK(strstr(error.message, "multiple") != NULL, "and says why: %s", error.message);
    /* Capacity is enforced rather than overrun. */
    error.message[0] = '\0';
    CHECK(tdx_lc1_parse(minute_plain, MINUTE_PLAIN_BYTES, bars, 1, &count, &violations,
                        &error) == TDX_ERR,
          "a one-bar buffer for ten bars is refused");
    CHECK(tdx_lc1_parse(NULL, 0, bars, 4, &count, &violations, &error) == TDX_ERR,
          "no bytes is refused");

    /* A minute word of 1440 or more is not a time of day. */
    memset(record, 0, sizeof(record));
    record[0] = (uint8_t)(good_word & 0xff);
    record[1] = (uint8_t)(good_word >> 8);
    record[2] = 0xa0;
        record[3] = 0x05; /* 1440 */ /* the first word that is not a minute of the day */
    error.message[0] = '\0';
    CHECK(tdx_lc1_parse(record, sizeof(record), bars, 4, &count, &violations,
                        &error) == TDX_ERR,
          "minute word 1440 is refused: %s", error.message);
    CHECK(strstr(error.message, "minute") != NULL, "and the message names the minute");
    /* 1439 is the last minute of the day and IS accepted. */
    memset(record, 0, sizeof(record));
    record[0] = (uint8_t)(good_word & 0xff);
    record[1] = (uint8_t)(good_word >> 8);
    record[2] = 0x9f;
        record[3] = 0x05; /* 1439 */ /* 23:59, the last minute of the day */
    for (count = 4; count < 20; ++count)
        record[count] = 0; /* prices are all zero, which is finite */
    error.message[0] = '\0';
    CHECK(tdx_lc1_parse(record, sizeof(record), bars, 4, &count, &violations,
                        &error) == TDX_OK,
          "minute word 1439 is accepted: %s", error.message);
    CHECK(count == 1 && bars[0].hour == 23 && bars[0].minute == 59,
          "and reads as 23:59, got %02d:%02d", bars[0].hour, bars[0].minute);

    /* A date word that is not a date.  The scan found none in 13.9 million records, so
     * this is a decode failure rather than a data quirk. */
    memset(record, 0, sizeof(record));
    record[0] = (uint8_t)(month_zero & 0xff);
    record[1] = (uint8_t)(month_zero >> 8);
    error.message[0] = '\0';
    CHECK(tdx_lc1_parse(record, sizeof(record), bars, 4, &count, &violations,
                        &error) == TDX_ERR,
          "a date word with month 0 is refused: %s", error.message);
    CHECK(strstr(error.message, "date word") != NULL, "and the message names the date word");
    /* The message must carry the word the test wrote, or the check could pass because
     * something else was rejected. */
    {
        char expected[32];
        snprintf(expected, sizeof(expected), "%u", month_zero);
        CHECK(strstr(error.message, expected) != NULL,
              "and carries the word the test wrote (%s): %s", expected, error.message);
    }
    /* A year far outside the range. */
    memset(record, 0, sizeof(record));
    record[0] = (uint8_t)(bad_word & 0xff);
    record[1] = (uint8_t)(bad_word >> 8);
    error.message[0] = '\0';
    CHECK(tdx_lc1_parse(record, sizeof(record), bars, 4, &count, &violations, &error) == TDX_OK ||
              strstr(error.message, "date word") != NULL,
          "a far-future year is either accepted as a real year or refused as a date word");

    /* A non-finite price is a decode failure: the scan found none, and a NaN reaching a
     * caller as a price is the failure worth refusing. */
    memset(record, 0, sizeof(record));
    record[0] = (uint8_t)(good_word & 0xff);
    record[1] = (uint8_t)(good_word >> 8);
    record[4] = 0x00;
    record[5] = 0x00;
    record[6] = 0xc0;
    record[7] = 0x7f; /* a quiet NaN in the open */
    error.message[0] = '\0';
    CHECK(tdx_lc1_parse(record, sizeof(record), bars, 4, &count, &violations,
                        &error) == TDX_ERR,
          "a NaN price is refused: %s", error.message);
    CHECK(strstr(error.message, "finite") != NULL, "and the message says so");
}

static void test_locate(void) {
    char path[TDX_LC1_PATH_MAX];

    error.message[0] = '\0';
    CHECK(tdx_lc1_locate("C:\\new_tdx", 1, "600519", path, sizeof(path), &error) == TDX_OK,
          "locate: %s", error.message);
    CHECK(strstr(path, "vipdoc/sh/minline/sh600519.lc1") != NULL,
          "the Shanghai layout, got %s", path);
    CHECK(tdx_lc1_locate("C:\\new_tdx", 0, "000001", path, sizeof(path), &error) == TDX_OK &&
              strstr(path, "vipdoc/sz/minline/sz000001.lc1") != NULL,
          "the Shenzhen layout, got %s", path);
    CHECK(tdx_lc1_locate("C:\\new_tdx", 2, "430047", path, sizeof(path), &error) == TDX_OK &&
              strstr(path, "vipdoc/bj/minline/bj430047.lc1") != NULL,
          "the Beijing layout, got %s", path);
    /* The extended markets have no minute file, so the market id is refused rather than
     * turned into a path that cannot exist. */
    error.message[0] = '\0';
    CHECK(tdx_lc1_locate("C:\\new_tdx", 44, "1234", path, sizeof(path), &error) == TDX_ERR,
          "market 44 is refused: %s", error.message);
    CHECK(tdx_lc1_locate("C:\\new_tdx", 1, "600519", path, 8, &error) == TDX_ERR,
          "a short buffer is refused");
}

static void test_rendering(void) {
    tdx_lc1_bar bars[MINUTE_VIOLATION_RECORDS + 2];
    size_t count = 0;
    size_t violations = 0;
    tdx_buf line;
    char reason[192];
    const char *text;

    error.message[0] = '\0';
    CHECK(tdx_lc1_parse(minute_violation, MINUTE_VIOLATION_BYTES, bars,
                        MINUTE_VIOLATION_RECORDS + 2, &count, &violations,
                        &error) == TDX_OK,
          "parse: %s", error.message);
    tdx_buf_init(&line);
    CHECK(tdx_lc1_format_bar(&line, &bars[MINUTE_VIOLATION_AT], "SH000043",
                             (size_t)MINUTE_VIOLATION_AT, &error) == TDX_OK,
          "render: %s", error.message);
    text = text_of(&line);
    CHECK(render_parses(text, reason, sizeof(reason)), "the bar parses: %s\n    %s", reason,
          text);
    CHECK(strstr(text, "\"type\":\"minute_bar\"") != NULL, "the type");
    CHECK(strstr(text, "\"date\":20251015") != NULL, "the date");
    CHECK(strstr(text, "\"time\":\"15:00\"") != NULL, "the time: %s", text);
    CHECK(strstr(text, "\"close\":2608.780") != NULL, "the close, above the high");

    tdx_buf_clear(&line);
    CHECK(tdx_lc1_format_summary(&line, "C:\\new_tdx\\vipdoc\\sh\\minline\\sh000043.lc1",
                                 "SH000043", 514560, 16080, 1, 67, 20250806, 20251114,
                                 &error) == TDX_OK,
          "summary: %s", error.message);
    text = text_of(&line);
    CHECK(render_parses(text, reason, sizeof(reason)),
          "the summary parses, backslashes and all: %s\n    %s", reason, text);
    CHECK(strstr(text, "\"bars\":16080") != NULL, "the bar count");
    CHECK(strstr(text, "\"ohlc_violations\":1") != NULL,
          "and the violation count travels with it: %s", text);
    tdx_buf_clear(&line);
    CHECK(tdx_lc1_format_summary(&line, "x.lc1", "SH600519", 0, 0, 0, 0, 0, 0,
                                 &error) == TDX_OK,
          "an empty summary renders");
    CHECK(render_parses(text_of(&line), reason, sizeof(reason)), "and parses: %s", reason);
    CHECK(strstr(text_of(&line), "\"first_date\":null") != NULL, "with a null first date");
    tdx_buf_free(&line);
}

int main(void) {
    test_date_word();
    test_plain_window();
    test_violation_is_counted_not_refused();
    test_refusals();
    test_locate();
    test_rendering();

    if (failures) {
        printf("%d minute check(s) failed\n", failures);
        return 1;
    }
    printf("minute checks passed\n");
    return 0;
}
