/* test_daily.c - the local .day daily-bar files.
 *
 * The fixture is a verbatim prefix of two real files from the terminal's own vipdoc tree:
 * a stock and a convertible bond.  They are the two scales that matter, because a reader
 * with one fixed scale - which is what the reference has - is wrong by a hundredfold on
 * one of them and still prints numbers.
 *
 * The scale is not a stock/bond special case.  It is 100 times the divisor the wire path
 * already uses, so a stock is hundredths, a fund is thousandths and a bond or repo is
 * ten-thousandths.  test_daily.c checks the function for each class, and the two fixture
 * files are the two ends of the range that can be checked against real bytes.
 *
 * The date check is enforced because a measurement says it is safe: 4,053,117 records
 * across 1,130 real files contain not one implausible date.  A non-positive price is
 * COUNTED rather than refused, because the same measurement cannot prove it never
 * happens elsewhere and refusing real data is worse than reporting it. */
#include <stdio.h>
#include <string.h>

#include "tdx_daily.h"
#include "tdx_daily_json.h"
#include "render_check.h"
#include "daily_fixtures.h"

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

static void test_scale_is_a_function_of_the_wire_divisor(void) {
    /* The four classes, with the wire divisor each one already has. */
    CHECK(tdx_daily_scale_divisor("600519") == 100, "a Shanghai stock is hundredths, got %d",
          tdx_daily_scale_divisor("600519"));
    CHECK(tdx_daily_scale_divisor("000001") == 100, "a Shenzhen stock too, got %d",
          tdx_daily_scale_divisor("000001"));
    CHECK(tdx_daily_scale_divisor("510300") == 1000, "a Shanghai fund is thousandths, got %d",
          tdx_daily_scale_divisor("510300"));
    CHECK(tdx_daily_scale_divisor("159915") == 1000, "a Shenzhen fund too, got %d",
          tdx_daily_scale_divisor("159915"));
    CHECK(tdx_daily_scale_divisor("110075") == 10000,
          "a convertible bond is ten-thousandths, got %d",
          tdx_daily_scale_divisor("110075"));
    CHECK(tdx_daily_scale_divisor("132026") == 10000, "an exchangeable bond too, got %d",
          tdx_daily_scale_divisor("132026"));
    CHECK(tdx_daily_scale_divisor("204001") == 10000, "and a repo, got %d",
          tdx_daily_scale_divisor("204001"));
    /* A code the table does not know falls back to the stock scale, which is what the
     * wire path does as well. */
    CHECK(tdx_daily_scale_divisor("999999") == 100, "an unknown code is hundredths, got %d",
          tdx_daily_scale_divisor("999999"));
    CHECK(tdx_daily_scale_divisor(NULL) == 100, "and no code at all, got %d",
          tdx_daily_scale_divisor(NULL));
}

static void test_stock_prefix(void) {
    tdx_daily_bar bars[DAILY_STOCK_PREFIX_RECORDS + 2];
    size_t count = 0;
    size_t suspicious = 0;

    error.message[0] = '\0';
    CHECK(tdx_daily_parse(daily_stock_prefix, DAILY_STOCK_PREFIX_BYTES, DAILY_STOCK_CODE, bars,
                          DAILY_STOCK_PREFIX_RECORDS + 2, &count, &suspicious,
                          &error) == TDX_OK,
          "the stock prefix parses: %s", error.message);
    CHECK(count == DAILY_STOCK_PREFIX_RECORDS, "%d bars, got %zu",
          DAILY_STOCK_PREFIX_RECORDS, count);
    CHECK(suspicious == 0, "and no price is non-positive, got %zu", suspicious);

    /* The values the bytes encode, at the stock scale. */
    CHECK(bars[0].date == 20010827, "the first bar is 20010827, got %u", bars[0].date);
    CHECK(bars[0].open == 34.51, "open 34.51, got %.4f", bars[0].open);
    CHECK(bars[0].high == 37.78, "high 37.78, got %.4f", bars[0].high);
    CHECK(bars[0].low == 32.85, "low 32.85, got %.4f", bars[0].low);
    CHECK(bars[0].close == 35.55, "close 35.55, got %.4f", bars[0].close);
    CHECK(bars[1].close == 36.86, "the second close is 36.86, got %.4f", bars[1].close);
    CHECK(bars[0].volume == 40631800, "volume 40631800, got %u", bars[0].volume);
    /* The amount is a float in the file, read as one. */
    CHECK(bars[0].amount > 1410347100.0 && bars[0].amount < 1410347200.0,
          "amount about 1.41e9, got %.2f", bars[0].amount);

    /* A CAPACITY SMALLER THAN THE FILE is refused rather than truncated. */
    error.message[0] = '\0';
    CHECK(tdx_daily_parse(daily_stock_prefix, DAILY_STOCK_PREFIX_BYTES, DAILY_STOCK_CODE, bars,
                          1, &count, &suspicious, &error) == TDX_ERR,
          "a one-bar buffer for ten bars must be refused: %s", error.message);
    /* A size that is not a multiple of 32 is not this format. */
    error.message[0] = '\0';
    CHECK(tdx_daily_parse(daily_stock_prefix, DAILY_STOCK_PREFIX_BYTES - 1, DAILY_STOCK_CODE,
                          bars, DAILY_STOCK_PREFIX_RECORDS + 2, &count, &suspicious,
                          &error) == TDX_ERR,
          "a size that is not a multiple of 32 is refused");
    CHECK(strstr(error.message, "multiple") != NULL, "and says why: %s", error.message);
    CHECK(tdx_daily_parse(NULL, 0, NULL, bars, 4, &count, &suspicious, &error) == TDX_ERR,
          "no bytes is refused");
}

static void test_bond_prefix_is_not_a_hundred_times_too_large(void) {
    tdx_daily_bar bars[DAILY_BOND_PREFIX_RECORDS + 2];
    size_t count = 0;
    size_t suspicious = 0;

    error.message[0] = '\0';
    CHECK(tdx_daily_parse(daily_bond_prefix, DAILY_BOND_PREFIX_BYTES, DAILY_BOND_CODE, bars,
                          DAILY_BOND_PREFIX_RECORDS + 2, &count, &suspicious,
                          &error) == TDX_OK,
          "the bond prefix parses: %s", error.message);
    CHECK(count == DAILY_BOND_PREFIX_RECORDS, "%d bars, got %zu", DAILY_BOND_PREFIX_RECORDS,
          count);
    /* A bond lists near par, so its first bar is the check that the scale is right: at the
     * stock scale this reads 11023.00 instead of 110.23. */
    CHECK(bars[0].date == 20201103, "the bond's first bar is 20201103, got %u", bars[0].date);
    CHECK(bars[0].open > 110.22 && bars[0].open < 110.24,
          "a bond lists near par: open 110.23, got %.4f", bars[0].open);
    CHECK(bars[0].close > 116.99 && bars[0].close < 117.01, "close 117.00, got %.4f",
          bars[0].close);
    CHECK(bars[0].open < 1000.0,
          "and certainly not the hundredfold 11023 the stock scale would give, got %.4f",
          bars[0].open);
    /* THE SAME BYTES AT THE STOCK SCALE, to show the difference is real rather than a
     * property of the fixture. */
    {
        tdx_daily_bar wrong[DAILY_BOND_PREFIX_RECORDS + 2];
        error.message[0] = '\0';
        CHECK(tdx_daily_parse(daily_bond_prefix, DAILY_BOND_PREFIX_BYTES, DAILY_STOCK_CODE,
                              wrong, DAILY_BOND_PREFIX_RECORDS + 2, &count, &suspicious,
                              &error) == TDX_OK,
              "parsing bond bytes as a stock works: %s", error.message);
        CHECK(wrong[0].open > 11022.0 && wrong[0].open < 11024.0,
              "and gives 11023, the hundredfold error: got %.4f", wrong[0].open);
        CHECK(wrong[0].open / bars[0].open > 99.9 && wrong[0].open / bars[0].open < 100.1,
              "exactly a hundred times too large: ratio %.2f", wrong[0].open / bars[0].open);
    }
}

static void test_refusals(void) {
    tdx_daily_bar bars[4];
    size_t count = 0;
    size_t suspicious = 0;
    static unsigned char bad_date[32];
    static unsigned char bad_day[32];
    static unsigned char zero_price[32];

    error.message[0] = '\0';
    /* A date that is present but impossible: the measurement says no real file has one, so
     * this is a decode failure rather than a data quirk. */
    memset(bad_date, 0, sizeof(bad_date));
    bad_date[0] = 0xd3;
    bad_date[1] = 0x29;
    bad_date[2] = 0x35;
    bad_date[3] = 0x01; /* 20261331 */
    CHECK(tdx_daily_parse(bad_date, sizeof(bad_date), "600519", bars, 4, &count, &suspicious,
                          &error) == TDX_ERR,
          "month 13 is refused: %s", error.message);
    CHECK(strstr(error.message, "date") != NULL, "and the message names the date");

    memset(bad_day, 0, sizeof(bad_day));
    bad_day[0] = 0x87;
    bad_day[1] = 0x25;
    bad_day[2] = 0x35;
    bad_day[3] = 0x01; /* 20260231 */
    error.message[0] = '\0';
    CHECK(tdx_daily_parse(bad_day, sizeof(bad_day), "600519", bars, 4, &count, &suspicious,
                          &error) == TDX_ERR,
          "20260231 is refused: %s", error.message);
    /* A real leap day is accepted. */
    memset(bad_day, 0, sizeof(bad_day));
    bad_day[0] = 0x65;
    bad_day[1] = 0xd7;
    bad_day[2] = 0x34;
    bad_day[3] = 0x01; /* 20240229 */
    error.message[0] = '\0';
    CHECK(tdx_daily_parse(bad_day, sizeof(bad_day), "600519", bars, 4, &count, &suspicious,
                          &error) == TDX_OK,
          "a real 29 February is accepted: %s", error.message);

    /* A zero price is COUNTED, not refused: the sample has none, which is not proof that
     * none can exist. */
    memset(zero_price, 0, sizeof(zero_price));
    zero_price[0] = 0x65;
    zero_price[1] = 0xd7;
    zero_price[2] = 0x34;
    zero_price[3] = 0x01; /* 20240229 */
    error.message[0] = '\0';
    CHECK(tdx_daily_parse(zero_price, sizeof(zero_price), "600519", bars, 4, &count,
                          &suspicious, &error) == TDX_OK,
          "a zero price parses: %s", error.message);
    CHECK(suspicious == 1, "and is counted as suspicious, got %zu", suspicious);
    CHECK(bars[0].close == 0.0, "with the value passed through as it is, got %.4f",
          bars[0].close);
}

static void test_locate(void) {
    char path[TDX_DAILY_PATH_MAX];

    error.message[0] = '\0';
    CHECK(tdx_daily_locate("C:\\new_tdx", 1, "600519", path, sizeof(path), &error) == TDX_OK,
          "locate: %s", error.message);
    CHECK(strstr(path, "vipdoc/sh/lday/sh600519.day") != NULL,
          "the Shanghai layout, got %s", path);
    CHECK(tdx_daily_locate("C:\\new_tdx", 0, "000001", path, sizeof(path), &error) == TDX_OK &&
              strstr(path, "vipdoc/sz/lday/sz000001.day") != NULL,
          "the Shenzhen layout, got %s", path);
    CHECK(tdx_daily_locate("C:\\new_tdx", 2, "430047", path, sizeof(path), &error) == TDX_OK &&
              strstr(path, "vipdoc/bj/lday/bj430047.day") != NULL,
          "the Beijing layout, got %s", path);
    /* An extended market goes under a numeric directory, as the reference has it. */
    CHECK(tdx_daily_locate("C:\\new_tdx", 44, "1234", path, sizeof(path), &error) == TDX_OK &&
              strstr(path, "vipdoc/ds/lday/44#1234.day") != NULL,
          "an extended market, got %s", path);
    /* A buffer too small is refused rather than truncated into a wrong path. */
    error.message[0] = '\0';
    CHECK(tdx_daily_locate("C:\\new_tdx", 1, "600519", path, 8, &error) == TDX_ERR,
          "a short buffer is refused: %s", error.message);
}

static void test_rendering(void) {
    tdx_daily_bar bars[DAILY_STOCK_PREFIX_RECORDS + 2];
    size_t count = 0;
    size_t suspicious = 0;
    tdx_buf line;
    char reason[192];
    const char *text;

    error.message[0] = '\0';
    CHECK(tdx_daily_parse(daily_stock_prefix, DAILY_STOCK_PREFIX_BYTES, DAILY_STOCK_CODE, bars,
                          DAILY_STOCK_PREFIX_RECORDS + 2, &count, &suspicious,
                          &error) == TDX_OK,
          "parse: %s", error.message);
    tdx_buf_init(&line);

    CHECK(tdx_daily_format_bar(&line, &bars[0], "SH600519", 100, 0, &error) == TDX_OK,
          "render: %s", error.message);
    text = text_of(&line);
    CHECK(render_parses(text, reason, sizeof(reason)), "the bar parses: %s\n    %s", reason,
          text);
    CHECK(strstr(text, "\"type\":\"daily_bar\"") != NULL, "the type");
    CHECK(strstr(text, "\"scale_divisor\":100") != NULL, "the divisor travels with the bar");
    CHECK(strstr(text, "\"date\":20010827") != NULL, "the date");
    CHECK(strstr(text, "\"open\":34.5100") != NULL, "the open: %s", text);
    CHECK(strstr(text, "\"volume\":40631800") != NULL, "the volume");

    tdx_buf_clear(&line);
    CHECK(tdx_daily_format_summary(&line, "C:\\new_tdx\\vipdoc\\sh\\lday\\sh600519.day",
                                   "SH600519", 100, 189984, 5937, 0, 20010827, 20260610,
                                   &error) == TDX_OK,
          "summary: %s", error.message);
    text = text_of(&line);
    CHECK(render_parses(text, reason, sizeof(reason)),
          "the summary parses, backslashes and all: %s\n    %s", reason, text);
    CHECK(strstr(text, "\"bars\":5937") != NULL, "the bar count: %s", text);
    CHECK(strstr(text, "\"first_date\":20010827") != NULL, "the first date");
    CHECK(strstr(text, "\"suspicious_prices\":0") != NULL, "the suspicious count");
    /* An empty file renders nulls rather than zero dates. */
    tdx_buf_clear(&line);
    CHECK(tdx_daily_format_summary(&line, "x.day", "SH600519", 100, 0, 0, 0, 0, 0,
                                   &error) == TDX_OK,
          "empty summary: %s", error.message);
    CHECK(strstr(text_of(&line), "\"first_date\":null") != NULL,
          "an empty file has no first date: %s", text_of(&line));
    tdx_buf_free(&line);
}

int main(void) {
    test_scale_is_a_function_of_the_wire_divisor();
    test_stock_prefix();
    test_bond_prefix_is_not_a_hundred_times_too_large();
    test_refusals();
    test_locate();
    test_rendering();

    if (failures) {
        printf("%d daily check(s) failed\n", failures);
        return 1;
    }
    printf("daily checks passed\n");
    return 0;
}
