/* test_minute_pack.c - writing the .lc1 format back.
 *
 * THE DECISIVE TEST IS A ROUND TRIP OVER REAL TERMINAL BYTES.  The fixture in
 * minute_fixtures.h is a window of the terminal's own sh600519.lc1.  Parsing it and packing
 * the result again must reproduce those bytes EXACTLY - not approximately, and not only the
 * fields the reader names.  That is stronger than "my writer and my reader agree", because
 * the input bytes were written by the terminal.
 *
 * The live check in output/lc1_pack_verification_evidence.txt adds two things the fixture
 * cannot: all 16,000 bars of a real 0x0537 download survive the packer with every field
 * unchanged, and all 67 distinct date words found in 200 terminal .lc1 files encode back to
 * themselves.
 *
 * The guards are asserted too, because a writer that quietly truncates is worse than one that
 * refuses: the record's volume field is 32 bits and the online aggregates can exceed it. */
#include <stdio.h>
#include <string.h>

#include "tdx_minute.h"
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


/* The date word is what the terminal itself writes, so encoding and decoding must be exact
 * inverses over the range the format covers. */
static void test_date_word(void) {
    uint16_t word = 0;
    uint32_t date = 0;
    uint32_t day;

    CHECK(tdx_lc1_decode_date(43814, &date) && date == 20250806,
          "43814 is 20250806, got %u", date);
    CHECK(tdx_lc1_encode_date(20250806, &word) && word == 43814,
          "and 20250806 is 43814, got %u", word);
    CHECK(tdx_lc1_decode_date(44122, &date) && date == 20251114,
          "44122 is 20251114, got %u", date);
    CHECK(tdx_lc1_encode_date(20251114, &word) && word == 44122,
          "and back, got %u", word);
    /* The smallest date the format can hold: the year field starts at 2004 and the month
     * and day have to be real, so 2004-01-01 is 101 and word 0 is not a date at all. */
    CHECK(tdx_lc1_encode_date(20040101, &word) && word == 101,
          "20040101 is 101, got %u", word);
    CHECK(!tdx_lc1_encode_date(20031231, &word), "2003 is before the format");
    /* THE LAST DATE THE FORMAT CAN HOLD.  The year field would reach 4051, but the word has
     * 16 bits: (y - 2004) * 2048 + month * 100 + day, so 2035-12-31 is 64719 and 2036-01-01
     * is 65637 - past the word. */
    CHECK(tdx_lc1_encode_date(20351231, &word) && word == 64719,
          "2035-12-31 is the last date and is 64719, got %u", word);
    CHECK(!tdx_lc1_encode_date(20360101, &word),
          "2036-01-01 does not fit the word even though the year does");
    CHECK(tdx_lc1_encode_date(20350102, &word), "while earlier in 2035 does");
    /* A nonexistent date the word cannot express is refused rather than wrapped. */
    CHECK(!tdx_lc1_encode_date(20251301, &word), "month 13 is refused");
    CHECK(!tdx_lc1_encode_date(20250132, &word), "day 32 is refused");
    CHECK(!tdx_lc1_encode_date(20250100, &word), "day 0 is refused");

    /* EXACT INVERSES where the decoder accepts, over six years of real dates. */
    {
        unsigned year;
        unsigned month;
        int checked = 0;
        int round_tripped = 0;
        for (year = 2024; year <= 2030; ++year)
            for (month = 1; month <= 12; ++month)
                for (day = 1; day <= 28; ++day) {
                    uint32_t candidate = year * 10000 + month * 100 + day;
                    uint16_t encoded = 0;
                    uint32_t decoded = 0;
                    if (!tdx_lc1_encode_date(candidate, &encoded))
                        continue;
                    checked++;
                    if (tdx_lc1_decode_date(encoded, &decoded) && decoded == candidate)
                        round_tripped++;
                }
        CHECK(checked > 2000, "the sweep covers real dates, got %d", checked);
        CHECK(checked == round_tripped, "every encoded date decodes back, got %d of %d",
              round_tripped, checked);
    }
}

/* REAL TERMINAL BYTES, PARSED AND WRITTEN BACK. */
static void test_real_bytes_round_trip(void) {
    tdx_lc1_bar bars[64];
    size_t count = 0;
    size_t violations = 0;
    tdx_buf packed;

    error.message[0] = '\0';
    CHECK(tdx_lc1_parse(minute_plain, sizeof(minute_plain), bars, 64, &count, &violations,
                        &error) == TDX_OK,
          "the real window parses: %s", error.message);
    CHECK(count == MINUTE_PLAIN_RECORDS, "%d records, got %zu", MINUTE_PLAIN_RECORDS, count);
    CHECK(sizeof(minute_plain) == MINUTE_PLAIN_BYTES, "the fixture is its stated size");
    CHECK(violations == 0, "with no violations, got %zu", violations);

    tdx_buf_init(&packed);
    CHECK(tdx_lc1_pack(bars, count, &packed, &error) == TDX_OK, "pack: %s", error.message);
    /* 32 bytes each, and THE SAME 32 BYTES THE TERMINAL WROTE. */
    CHECK(packed.len == sizeof(minute_plain), "%zu bytes against the terminal's %zu",
          packed.len, sizeof(minute_plain));
    CHECK(packed.len == count * 32, "and 32 bytes per record, got %zu", packed.len);
    CHECK(packed.len == sizeof(minute_plain) &&
              memcmp(packed.data, minute_plain, sizeof(minute_plain)) == 0,
          "the packed bytes are IDENTICAL to the terminal's own");
    /* If the comparison ever fails, say where rather than only that it did. */
    if (packed.len == sizeof(minute_plain) &&
        memcmp(packed.data, minute_plain, sizeof(minute_plain)) != 0) {
        size_t index;
        for (index = 0; index < sizeof(minute_plain); ++index)
            if (packed.data[index] != minute_plain[index])
                CHECK(0, "first difference at byte %zu: %02x against %02x", index,
                      packed.data[index], minute_plain[index]);
    }

    /* The violation window goes through the same way, and STAYS a violation: the writer does
     * not correct what the reader tolerates. */
    {
        tdx_lc1_bar other[64];
        size_t other_count = 0;
        size_t other_violations = 0;
        tdx_buf again;
        CHECK(tdx_lc1_parse(minute_violation, sizeof(minute_violation), other, 64, &other_count,
                            &other_violations, &error) == TDX_OK,
              "the violation window parses: %s", error.message);
        CHECK(other_violations >= 1, "and still holds its violation, got %zu",
              other_violations);
        tdx_buf_init(&again);
        CHECK(tdx_lc1_pack(other, other_count, &again, &error) == TDX_OK, "pack: %s",
              error.message);
        CHECK(again.len == sizeof(minute_violation) &&
                  memcmp(again.data, minute_violation, sizeof(minute_violation)) == 0,
              "and packs back to the same bytes, violations included");
        tdx_buf_free(&again);
    }
    tdx_buf_free(&packed);
}

static void test_guards(void) {
    tdx_lc1_bar bar;
    tdx_buf packed;
    tdx_kline_bar kline;

    memset(&bar, 0, sizeof(bar));
    bar.date = 20260922;
    bar.hour = 14;
    bar.minute = 35;
    bar.open = 12.34;
    bar.high = 12.50;
    bar.low = 12.30;
    bar.close = 12.45;
    bar.amount = 1234567.0;
    bar.volume = 100;
    bar.extra_1 = 7;
    bar.extra_2 = 9;

    tdx_buf_init(&packed);
    CHECK(tdx_lc1_pack(&bar, 1, &packed, &error) == TDX_OK, "pack: %s", error.message);
    CHECK(packed.len == 32, "one record, got %zu", packed.len);
    CHECK((packed.data[0] | (packed.data[1] << 8)) ==
              (uint16_t)((2026 - 2004) * 2048 + 922),
          "the date word is what the format says");
    CHECK((packed.data[2] | (packed.data[3] << 8)) == 14 * 60 + 35,
          "and the minute word");
    CHECK(packed.data[28] == 7 && packed.data[29] == 0 && packed.data[30] == 9,
          "the two trailing words are carried, not dropped");

    /* A MINUTE THAT IS NOT A MINUTE. */
    bar.minute = 60;
    error.message[0] = '\0';
    CHECK(tdx_lc1_pack(&bar, 1, &packed, &error) == TDX_ERR, "minute 60 is refused");
    CHECK(strstr(error.message, "minute of the day") != NULL, "and says why: %s",
          error.message);
    bar.minute = 35;
    bar.hour = 24;
    CHECK(tdx_lc1_pack(&bar, 1, &packed, &error) == TDX_ERR, "hour 24 is refused");
    bar.hour = 14;

    /* A DATE WITH NO WORD. */
    bar.date = 20031231;
    CHECK(tdx_lc1_pack(&bar, 1, &packed, &error) == TDX_ERR, "a date before the format");
    bar.date = 20260922;

    /* THE DOWNLOAD PATH, and its guard: a volume past the record's 32 bits. */
    memset(&kline, 0, sizeof(kline));
    kline.date = 20260922;
    kline.hour = 9;
    kline.minute = 31;
    kline.open = 100.5;
    kline.high = 101.0;
    kline.low = 100.0;
    kline.close = 100.75;
    kline.amount = 5000.0;
    kline.volume = 12345;
    kline.extra_1 = 3;
    kline.extra_2 = 4;
    {
        tdx_lc1_bar converted;
        CHECK(tdx_lc1_from_kline(&kline, &converted, &error) == TDX_OK, "convert: %s",
              error.message);
        CHECK(converted.date == 20260922 && converted.hour == 9 && converted.minute == 31,
              "the date and time carry over");
        CHECK(converted.volume == 12345, "and the volume, got %u", converted.volume);
        CHECK(converted.extra_1 == 3 && converted.extra_2 == 4, "and both trailing words");
        CHECK(tdx_lc1_from_kline(&kline, &converted, &error) == TDX_OK, "runs again");
    }
    {
        tdx_lc1_bar guarded;
        kline.volume = 0x100000000LL;
        error.message[0] = '\0';
        CHECK(tdx_lc1_from_kline(&kline, &guarded, &error) == TDX_ERR,
              "a volume past 32 bits is refused rather than truncated");
        CHECK(strstr(error.message, "32 bits") != NULL, "and says so: %s", error.message);
        /* The largest volume that DOES fit is accepted, so the guard is a boundary and not a
         * blanket refusal. */
        kline.volume = 0xFFFFFFFFLL;
        CHECK(tdx_lc1_from_kline(&kline, &guarded, &error) == TDX_OK,
              "while the largest volume that does fit is accepted: %s", error.message);
        CHECK(guarded.volume == 0xFFFFFFFFu, "and it survives at %u", guarded.volume);
        kline.volume = -1;
        CHECK(tdx_lc1_from_kline(&kline, &guarded, &error) == TDX_ERR,
              "and a negative volume is refused");
    }
    tdx_buf_free(&packed);
}

int main(void) {
    test_date_word();
    test_real_bytes_round_trip();
    test_guards();

    if (failures) {
        printf("%d minute pack check(s) failed\n", failures);
        return 1;
    }
    printf("minute pack checks passed\n");
    return 0;
}
