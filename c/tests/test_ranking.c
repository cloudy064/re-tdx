/* test_ranking.c - the 0x054B category ranking.
 *
 * The response body is built IN THE TEST rather than captured, because what needs asserting
 * precisely is the delta arithmetic: five of the prices arrive as an offset from the record's
 * own close, so a record whose previous close is ABOVE its last price is the case that tells
 * a delta reader from an absolute one.  A capture cannot be relied on to contain the exact
 * numbers that make that visible, and it cannot be embedded anyway at 80 records a page.
 *
 * The live check is in output/ranking_verification_evidence.txt: a full page of eighty
 * records, sorted by change descending, whose derived changes are non-increasing in every
 * adjacent pair.  That is the same property from the other side - if the deltas were read as
 * absolute prices the derived changes would be nonsense rather than a descending sequence.
 *
 * The varint bytes here are COMPUTED from the values by a helper, not written by hand: three
 * rounds of this project have been caught out by hand-encoded binary fields, and the helper
 * is the answer to that rather than more care. */
#include <stdio.h>
#include <string.h>

#include "tdx_ranking.h"
#include "tdx_ranking_json.h"
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

/* --- a response builder, so no varint is ever written by hand ---------- */

typedef struct body {
    unsigned char bytes[1024];
    size_t length;
} body;

static void put_u16(body *out, uint16_t value) {
    out->bytes[out->length++] = (unsigned char)(value & 0xff);
    out->bytes[out->length++] = (unsigned char)(value >> 8);
}

static void put_u32(body *out, uint32_t value) {
    out->bytes[out->length++] = (unsigned char)(value & 0xff);
    out->bytes[out->length++] = (unsigned char)((value >> 8) & 0xff);
    out->bytes[out->length++] = (unsigned char)((value >> 16) & 0xff);
    out->bytes[out->length++] = (unsigned char)((value >> 24) & 0xff);
}

static void put_f32(body *out, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    put_u32(out, bits);
}

/* The sign-magnitude varint: the low six bits of the first byte, continued in seven-bit
 * groups, with bit 0x40 carrying the sign. */
static void put_varint(body *out, int64_t value) {
    uint64_t magnitude = value < 0 ? (uint64_t)(-value) : (uint64_t)value;
    unsigned char first = (unsigned char)(magnitude & 0x3f);
    unsigned char current;
    int shift = 6;
    magnitude >>= 6;
    if (value < 0)
        first |= 0x40;
    if (magnitude)
        first |= 0x80;
    out->bytes[out->length++] = first;
    while (magnitude) {
        current = (unsigned char)(magnitude & 0x7f);
        magnitude >>= 7;
        if (magnitude)
            current |= 0x80;
        out->bytes[out->length++] = current;
        shift += 7;
    }
    (void)shift;
}

/* One record, with the deltas expressed as the differences a caller would want to see. */
static void put_record(body *out, int market, const char *code, int64_t close_raw,
                       int64_t previous_delta, int64_t open_delta, int64_t high_delta,
                       int64_t low_delta, int64_t bid_delta, int64_t ask_delta) {
    size_t index;
    out->bytes[out->length++] = (unsigned char)market;
    for (index = 0; index < 6; ++index)
        out->bytes[out->length++] = (unsigned char)code[index];
    put_u16(out, 7); /* active1 */
    put_varint(out, close_raw);
    put_varint(out, previous_delta);
    put_varint(out, open_delta);
    put_varint(out, high_delta);
    put_varint(out, low_delta);
    put_varint(out, 0);     /* server time */
    put_varint(out, 0);     /* raw price */
    put_varint(out, 1000);  /* total hand */
    put_varint(out, 10);    /* current hand */
    put_u32(out, 0);        /* amount */
    put_varint(out, 5);     /* inside dish */
    put_varint(out, 6);     /* outer disc */
    put_varint(out, 7);     /* one raw */
    put_varint(out, 1234);  /* opening amount, in hundreds of yuan */
    put_varint(out, bid_delta);
    put_varint(out, ask_delta);
    put_varint(out, 11);    /* bid volume */
    put_varint(out, 12);    /* ask volume */
    /* the 56-byte tail */
    put_u16(out, 42);                 /* status or sort raw */
    put_u16(out, (uint16_t)(int16_t)-250); /* rise speed, -2.50 */
    put_u16(out, (uint16_t)(int16_t)125);  /* short turnover, 1.25 */
    put_f32(out, 3.5f);               /* two-minute amount */
    put_u16(out, (uint16_t)(int16_t)-75);  /* opening rush, -0.75 */
    for (index = 0; index < 10; ++index)
        out->bytes[out->length++] = (unsigned char)(0xa0 + index);
    put_f32(out, 1.25f);              /* volume rise speed */
    put_f32(out, 0.5f);               /* depth */
    for (index = 0; index < 24; ++index)
        out->bytes[out->length++] = (unsigned char)index;
    put_u16(out, 9);                  /* active2 */
}

static void test_request(void) {
    tdx_buf request = {0};
    uint16_t value = 0;

    tdx_buf_init(&request);
    error.message[0] = '\0';
    /* The nine fields, in the order the server expects, with reverse 1 for descending. */
    CHECK(tdx_ranking_build_request(6, 0x000E, 0, 80, 0, 0, &request, &error) == TDX_OK,
          "build: %s", error.message);
    CHECK(request.len == TDX_RANKING_REQUEST_SIZE, "the body is 18 bytes, got %zu", request.len);
    CHECK(request.len == 18, "and the constant says 18, got %d", TDX_RANKING_REQUEST_SIZE);
    {
        static const uint16_t expected[9] = {6, 0x000E, 0, 80, 1, 5, 0, 1, 0};
        size_t index;
        for (index = 0; index < 9; ++index) {
            uint16_t got = (uint16_t)(request.data[index * 2] |
                                      ((uint16_t)request.data[index * 2 + 1] << 8));
            CHECK(got == expected[index], "field %zu is %u, got %u", index, expected[index],
                  got);
        }
    }
    /* No sort key means no direction: reverse 0, not 1. */
    CHECK(tdx_ranking_build_request(6, 0, 0, 80, 0, 0, &request, &error) == TDX_OK,
          "build: %s", error.message);
    CHECK(request.data[8] == 0 && request.data[9] == 0,
          "with no sort key reverse is 0, got %u", request.data[8]);
    /* Ascending is 2, and that is not a flag: descending is the default. */
    CHECK(tdx_ranking_build_request(6, 0x000E, 0, 80, 1, 0, &request, &error) == TDX_OK,
          "build: %s", error.message);
    CHECK(request.data[8] == 2, "ascending gives reverse 2, got %u", request.data[8]);
    /* The page size is capped at 80 by the server. */
    error.message[0] = '\0';
    CHECK(tdx_ranking_build_request(6, 0x000E, 0, 81, 0, 0, &request, &error) == TDX_ERR,
          "a page of 81 is refused");
    CHECK(strstr(error.message, "1..80") != NULL, "and says the range: %s", error.message);
    CHECK(tdx_ranking_build_request(6, 0x000E, 0, 0, 0, 0, &request, &error) == TDX_ERR,
          "and so is a page of 0");

    /* The sort table, by name, by number and by hex. */
    CHECK(tdx_ranking_sort_id("change-pct", &value) && value == 0x000E,
          "change-pct is 0x000E, got 0x%04x", value);
    CHECK(tdx_ranking_sort_id("AMOUNT", &value) && value == 0x000A,
          "the name is case-insensitive, got 0x%04x", value);
    CHECK(tdx_ranking_sort_id("seal-amount", &value) && value == 0x001C,
          "seal-amount is 0x001C, got 0x%04x", value);
    CHECK(tdx_ranking_sort_id("0x1234", &value) && value == 0x1234,
          "a hex number reaches a key the table does not name, got 0x%04x", value);
    CHECK(tdx_ranking_sort_id("4660", &value) && value == 4660,
          "and so does a decimal one, got %u", value);
    CHECK(!tdx_ranking_sort_id("nonsense", &value), "a word that is not a key is refused");
    CHECK(tdx_ranking_sort_name(0x000E) != NULL &&
              strcmp(tdx_ranking_sort_name(0x000E), "change-pct") == 0,
          "and the name comes back");
    CHECK(tdx_ranking_sort_name(0x1234) == NULL, "an unknown id has no name");

    /* The category: the reference's spelling, the directory's, and a number. */
    CHECK(tdx_ranking_category_id("a-shares", &value) && value == 6, "a-shares is 6");
    CHECK(tdx_ranking_category_id("a_share", &value) && value == 6,
          "a_share is the directory's spelling and also 6");
    CHECK(tdx_ranking_category_id("6", &value) && value == 6, "and the number");
    CHECK(!tdx_ranking_category_id("etf", &value), "etf is not a ranking category here");
    tdx_buf_free(&request);
}

static void test_prices_are_deltas(void) {
    body out = {{0}, 0};
    tdx_ranking_record records[4];
    tdx_ranking_page page;

    /* Two records.  The first falls: its previous close is ABOVE its last price, so the
     * delta is negative.  A reader that treats the varints as absolute prices produces
     * 2.00 for the previous close instead of 99.00, which is the whole point. */
    put_u16(&out, 1);
    put_u16(&out, 2);
    put_record(&out, 0, "000001", 10000, -100, -50, 200, -300, -10, 20);
    put_record(&out, 1, "600519", 500000, 500, 100, 900, -700, -100, 100);

    error.message[0] = '\0';
    CHECK(tdx_ranking_parse(out.bytes, out.length, records, 4, &page, &error) == TDX_OK,
          "parse: %s", error.message);
    CHECK(page.header == 1 && page.records == 2, "two records, got %zu", page.records);

    /* The first record: close 100.00 with the four prices as offsets from it. */
    CHECK(strcmp(records[0].security_id, "SZ000001") == 0, "the identity is built, got %s",
          records[0].security_id);
    CHECK(records[0].last_price > 99.99 && records[0].last_price < 100.01,
          "close 100.00, got %.4f", records[0].last_price);
    CHECK(records[0].pre_close_price > 98.99 && records[0].pre_close_price < 99.01,
          "previous close 100.00 + (-1.00) = 99.00, got %.4f", records[0].pre_close_price);
    CHECK(records[0].open_price > 99.49 && records[0].open_price < 99.51,
          "open 99.50, got %.4f", records[0].open_price);
    CHECK(records[0].high_price > 101.99 && records[0].high_price < 102.01,
          "high 102.00, got %.4f", records[0].high_price);
    CHECK(records[0].low_price > 96.99 && records[0].low_price < 97.01,
          "low 97.00, got %.4f", records[0].low_price);
    /* The bid and ask deltas too. */
    CHECK(records[0].bid1_price > 99.89 && records[0].bid1_price < 99.91,
          "bid 99.90, got %.4f", records[0].bid1_price);
    CHECK(records[0].ask1_price > 100.19 && records[0].ask1_price < 100.21,
          "ask 100.20, got %.4f", records[0].ask1_price);
    /* A NEGATIVE opening rush survives, so the signed reads are signed. */
    CHECK(records[0].rise_speed < -2.49 && records[0].rise_speed > -2.51,
          "the rise speed is -2.50, got %.4f", records[0].rise_speed);
    CHECK(records[0].short_turnover > 1.24 && records[0].short_turnover < 1.26,
          "the short turnover is 1.25, got %.4f", records[0].short_turnover);
    CHECK(records[0].opening_rush < -0.74 && records[0].opening_rush > -0.76,
          "the opening rush is -0.75, got %.4f", records[0].opening_rush);
    CHECK(records[0].open_amount_yuan > 123399.9 && records[0].open_amount_yuan < 123400.1,
          "the opening amount is 1234 hundreds = 123400, got %.1f",
          records[0].open_amount_yuan);
    CHECK(records[0].active1 == 7 && records[0].active2 == 9,
          "both active words, got %u and %u", records[0].active1, records[0].active2);
    CHECK(records[0].status_or_sort_raw == 42, "the status word is 42, got %u",
          records[0].status_or_sort_raw);
    /* The two runs with no established meaning are kept as hex, and the hex is the bytes. */
    CHECK(strcmp(records[0].extra_pair_hex,
                 "a0a1a2a3a4a5a6a7a8a9") == 0,
          "the first unmodelled run is its ten bytes, got %s", records[0].extra_pair_hex);
    CHECK(strlen(records[0].extra_meta_hex) == 48,
          "the second is twenty-four bytes of hex, got %zu", strlen(records[0].extra_meta_hex));
    CHECK(strncmp(records[0].extra_meta_hex, "0001020304", 10) == 0,
          "and starts with its first bytes, got %s", records[0].extra_meta_hex);

    /* The second record, on a Shanghai stock whose prices are scaled differently: the
     * divisor table applies, so the same raw close means a different price. */
    CHECK(strcmp(records[1].security_id, "SH600519") == 0, "the second is SH600519, got %s",
          records[1].security_id);
    CHECK(records[1].last_price > 4999.99 && records[1].last_price < 5000.01,
          "close 5000.00, got %.4f", records[1].last_price);
    CHECK(records[1].pre_close_price > 5004.99 && records[1].pre_close_price < 5005.01,
          "previous close 5005.00, got %.4f", records[1].pre_close_price);
}

static void test_refusals(void) {
    body out = {{0}, 0};
    tdx_ranking_record records[4];
    tdx_ranking_page page;

    /* A response shorter than its header. */
    error.message[0] = '\0';
    CHECK(tdx_ranking_parse(out.bytes, 2, records, 4, &page, &error) == TDX_ERR,
          "two bytes is too short");
    /* A page claiming more than eighty records. */
    put_u16(&out, 1);
    put_u16(&out, 81);
    error.message[0] = '\0';
    CHECK(tdx_ranking_parse(out.bytes, out.length, records, 4, &page, &error) == TDX_ERR,
          "a page of 81 records is refused");
    CHECK(strstr(error.message, "more than the 80") != NULL, "and says the cap: %s",
          error.message);

    /* TRAILING BYTES: the walk has to land exactly on the end, or the layout is not what
     * this reader thinks it is. */
    out.length = 0;
    put_u16(&out, 1);
    put_u16(&out, 1);
    put_record(&out, 0, "000001", 10000, -100, -50, 200, -300, -10, 20);
    put_u16(&out, 0xbeef);
    error.message[0] = '\0';
    CHECK(tdx_ranking_parse(out.bytes, out.length, records, 4, &page, &error) == TDX_ERR,
          "two trailing bytes are refused");
    CHECK(strstr(error.message, "trailing") != NULL, "and the message says so: %s",
          error.message);

    /* A market the reference does not allow. */
    out.length = 0;
    put_u16(&out, 1);
    put_u16(&out, 1);
    put_record(&out, 5, "000001", 10000, -100, -50, 200, -300, -10, 20);
    error.message[0] = '\0';
    CHECK(tdx_ranking_parse(out.bytes, out.length, records, 4, &page, &error) == TDX_ERR,
          "market 5 is refused: %s", error.message);
    /* A code that is not six digits. */
    out.length = 0;
    put_u16(&out, 1);
    put_u16(&out, 1);
    put_record(&out, 0, "00000x", 10000, -100, -50, 200, -300, -10, 20);
    error.message[0] = '\0';
    CHECK(tdx_ranking_parse(out.bytes, out.length, records, 4, &page, &error) == TDX_ERR,
          "a non-digit code is refused: %s", error.message);
    /* A record whose tail is cut short. */
    out.length = 0;
    put_u16(&out, 1);
    put_u16(&out, 1);
    {
        body full = {{0}, 0};
        put_record(&full, 0, "000001", 10000, -100, -50, 200, -300, -10, 20);
        memcpy(out.bytes + out.length, full.bytes, full.length - 10);
        out.length += full.length - 10;
    }
    error.message[0] = '\0';
    CHECK(tdx_ranking_parse(out.bytes, out.length, records, 4, &page, &error) == TDX_ERR,
          "a truncated tail is refused: %s", error.message);
    /* A capacity smaller than the page. */
    out.length = 0;
    put_u16(&out, 1);
    put_u16(&out, 2);
    put_record(&out, 0, "000001", 10000, -100, -50, 200, -300, -10, 20);
    put_record(&out, 1, "600519", 500000, 500, 100, 900, -700, -100, 100);
    error.message[0] = '\0';
    CHECK(tdx_ranking_parse(out.bytes, out.length, records, 1, &page, &error) == TDX_ERR,
          "a one-record buffer for two records is refused");
}

static void test_rendering(void) {
    body out = {{0}, 0};
    tdx_ranking_record records[4];
    tdx_ranking_page page;
    tdx_buf line = {0};
    char reason[192];

    put_u16(&out, 1);
    put_u16(&out, 1);
    put_record(&out, 0, "000001", 10000, -100, -50, 200, -300, -10, 20);
    error.message[0] = '\0';
    CHECK(tdx_ranking_parse(out.bytes, out.length, records, 4, &page, &error) == TDX_OK,
          "parse: %s", error.message);
    tdx_buf_init(&line);
    CHECK(tdx_ranking_format(&line, &records[0], 0, 6, 0x000E, &error) == TDX_OK,
          "render: %s", error.message);
    CHECK(render_parses(text_of(&line), reason, sizeof(reason)),
          "the ranking line parses: %s\n    %s", reason, text_of(&line));
    CHECK(strstr(text_of(&line), "\"type\":\"ranking\"") != NULL, "the type");
    CHECK(strstr(text_of(&line), "\"sort_name\":\"change-pct\"") != NULL, "the sort name");
    CHECK(strstr(text_of(&line), "\"pre_close_price\":99.0000") != NULL,
          "the delta-decoded price: %s", text_of(&line));
    CHECK(strstr(text_of(&line), "\"extra_pair_hex\":\"a0a1") != NULL,
          "and the unmodelled run as hex");

    tdx_buf_clear(&line);
    CHECK(tdx_ranking_format_summary(&line, 6, 0x000E, 0, 80, 1, "1.2.3.4:7709", &error) == TDX_OK,
          "summary: %s", error.message);
    CHECK(render_parses(text_of(&line), reason, sizeof(reason)), "the summary parses: %s",
          reason);
    CHECK(strstr(text_of(&line), "\"ascending\":false") != NULL, "the direction: %s",
          text_of(&line));
    CHECK(strstr(text_of(&line), "\"records\":80") != NULL, "the count");
    /* An unnamed sort id renders null rather than being dropped. */
    tdx_buf_clear(&line);
    CHECK(tdx_ranking_format_summary(&line, 6, 0x1234, 1, 0, 0, NULL, &error) == TDX_OK,
          "summary: %s", error.message);
    CHECK(render_parses(text_of(&line), reason, sizeof(reason)), "and it parses: %s", reason);
    CHECK(strstr(text_of(&line), "\"sort_name\":null") != NULL, "with a null name: %s",
          text_of(&line));
    CHECK(strstr(text_of(&line), "\"endpoint\":null") != NULL, "and a null endpoint");
    tdx_buf_free(&line);
}

int main(void) {
    test_request();
    test_prices_are_deltas();
    test_refusals();
    test_rendering();

    if (failures) {
        printf("%d ranking check(s) failed\n", failures);
        return 1;
    }
    printf("ranking checks passed\n");
    return 0;
}
