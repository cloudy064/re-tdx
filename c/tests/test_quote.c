/* test_quote.c - security identity, primitive codecs and the 0x0547 decoder. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "tdx_quote.h"

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

static int nearly(double left, double right) { return fabs(left - right) < 1e-9; }

static void append_varint(tdx_buf *buffer, int64_t value) {
    int negative = value < 0;
    uint64_t magnitude = negative ? (uint64_t)(-value) : (uint64_t)value;
    uint8_t first = (uint8_t)(magnitude & 0x3Fu);
    uint64_t remaining = magnitude >> 6;
    if (negative)
        first |= 0x40u;
    if (remaining)
        first |= 0x80u;
    (void)tdx_buf_push(buffer, first, NULL);
    while (remaining) {
        uint8_t byte = (uint8_t)(remaining & 0x7Fu);
        remaining >>= 7;
        if (remaining)
            byte |= 0x80u;
        (void)tdx_buf_push(buffer, byte, NULL);
    }
}

static void test_code_parse(void) {
    static const struct {
        const char *text;
        int market;
        const char *code;
    } cases[] = {
        {"sz000001", 0, "000001"}, {"SZ000001", 0, "000001"},
        {"sh600000", 1, "600000"}, {"SH600000", 1, "600000"},
        {"bj830799", 2, "830799"}, {"sz:000001", 0, "000001"},
        {"1:600000", 1, "600000"}, {"600000", 1, "600000"},
        {"000001", 0, "000001"},   {"830799", 2, "830799"},
        {"900901", 1, "900901"},   {"880471", 1, "880471"},
        {"000300", 0, "000300"},
    };
    size_t index;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        tdx_code code;
        tdx_error error;
        error.message[0] = '\0';
        CHECK(tdx_code_parse(cases[index].text, &code, &error) == TDX_OK,
              "parse(%s) failed: %s", cases[index].text, error.message);
        CHECK(code.market_id == cases[index].market, "parse(%s) market is %d",
              cases[index].text, code.market_id);
        CHECK(strcmp(code.code, cases[index].code) == 0, "parse(%s) code is %s",
              cases[index].text, code.code);
    }
}

static void test_code_parse_rejects(void) {
    static const char *cases[] = {"sz00000", "sz0000011", "sz00a001",
                                  "qq000001", "sz", ""};
    size_t index;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        tdx_code code;
        tdx_error error;
        error.message[0] = '\0';
        CHECK(tdx_code_parse(cases[index], &code, &error) == TDX_ERR,
              "parse(%s) should have failed", cases[index]);
    }
}

static void test_code_id(void) {
    tdx_code code;
    char text[16];
    tdx_error error;
    error.message[0] = '\0';
    CHECK(tdx_code_parse("sh600000", &code, &error) == TDX_OK, "parse failed");
    tdx_code_id(&code, text, sizeof(text));
    CHECK(strcmp(text, "SH600000") == 0, "code id is %s", text);
}

static void test_price_divisor(void) {
    CHECK(tdx_price_divisor("000001") == 1, "plain stock divisor");
    CHECK(tdx_price_divisor("600000") == 1, "plain Shanghai divisor");
    CHECK(tdx_price_divisor("159915") == 10, "Shenzhen fund divisor");
    CHECK(tdx_price_divisor("510300") == 10, "Shanghai fund divisor");
    CHECK(tdx_price_divisor("113050") == 100, "convertible bond divisor");
    CHECK(tdx_price_divisor("204001") == 100, "repo divisor");
    CHECK(tdx_price_divisor("131800") == 100, "1318 prefix divisor");
    CHECK(tdx_price_divisor("131801") == 100, "1318 prefix divisor, six digits");
    /* Measured against the local day files: every 132xxx code that still trades
     * decodes at about 100 times par without this rule, while four control families
     * come out at a ratio of about 1.  The rule is "132", not "13": only that
     * segment was measured, and the rest of the family has no live code here. */
    CHECK(tdx_price_divisor("132024") == 100, "exchangeable bond divisor");
    CHECK(tdx_price_divisor("132026") == 100, "and the other live one");
    CHECK(tdx_price_divisor("130001") == 1,
          "while the rest of the 13 family is left as measured-unknown");
    CHECK(tdx_price_divisor("130800") == 1, "unlisted prefix stays at one");
}

static void test_varint(void) {
    static const struct {
        uint8_t raw[4];
        size_t size;
        int64_t expected;
    } cases[] = {
        {{0x05, 0, 0, 0}, 1, 5},
        {{0x45, 0, 0, 0}, 1, -5},
        {{0x00, 0, 0, 0}, 1, 0},
        {{0x3F, 0, 0, 0}, 1, 63},
        {{0x80, 0x01, 0, 0}, 2, 64},
        {{0xC0, 0x01, 0, 0}, 2, -64},
        {{0x80, 0x80, 0x01, 0}, 3, 8192},
    };
    size_t index;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        size_t offset = 0;
        int64_t value = 0;
        tdx_error error;
        error.message[0] = '\0';
        CHECK(tdx_varint_decode(cases[index].raw, cases[index].size, &offset, &value,
                                &error) == TDX_OK,
              "varint case %zu failed: %s", index, error.message);
        CHECK(value == cases[index].expected, "varint case %zu decoded %lld, expected %lld",
              index, (long long)value, (long long)cases[index].expected);
        CHECK(offset == cases[index].size, "varint case %zu consumed %zu bytes", index,
              offset);
    }
}

static void test_varint_round_trip(void) {
    static const int64_t values[] = {0, 1, -1, 63, 64, -64, 1000, -1000,
                                     123456789LL, -123456789LL};
    size_t index;
    for (index = 0; index < sizeof(values) / sizeof(values[0]); ++index) {
        tdx_buf buffer;
        size_t offset = 0;
        int64_t decoded = 0;
        tdx_error error;
        error.message[0] = '\0';
        tdx_buf_init(&buffer);
        append_varint(&buffer, values[index]);
        CHECK(tdx_varint_decode(buffer.data, buffer.len, &offset, &decoded, &error) == TDX_OK,
              "round trip encode of %lld failed to decode: %s",
              (long long)values[index], error.message);
        CHECK(decoded == values[index], "round trip of %lld produced %lld",
              (long long)values[index], (long long)decoded);
        tdx_buf_free(&buffer);
    }
}

static void test_varint_rejects_truncation(void) {
    const uint8_t raw[] = {0x80};
    size_t offset = 0;
    int64_t value = 0;
    tdx_error error;
    error.message[0] = '\0';
    CHECK(tdx_varint_decode(raw, sizeof(raw), &offset, &value, &error) == TDX_ERR,
          "an unterminated varint must be rejected");
}

static void test_build_depth_request(void) {
    tdx_code codes[2];
    tdx_buf buffer;
    tdx_error error;
    const uint8_t want[] = {0x02, 0x00, 0x00, '0', '0', '0', '0', '0', '1',
                            0x00, 0x00, 0x00, 0x00,
                            0x01, '6', '0', '0', '0', '0', '0',
                            0x00, 0x00, 0x00, 0x00};
    error.message[0] = '\0';
    CHECK(tdx_code_parse("sz000001", &codes[0], &error) == TDX_OK, "parse failed");
    CHECK(tdx_code_parse("sh600000", &codes[1], &error) == TDX_OK, "parse failed");
    CHECK(tdx_quote_build_depth_request(codes, 2, &buffer, &error) == TDX_OK,
          "build failed: %s", error.message);
    CHECK(buffer.len == sizeof(want), "request length is %zu, expected %zu", buffer.len,
          sizeof(want));
    CHECK(buffer.len == sizeof(want) && memcmp(buffer.data, want, sizeof(want)) == 0,
          "request bytes differ from the recovered layout");
    tdx_buf_free(&buffer);
}

/* Builds one synthetic depth record matching the recovered field order. */
static void build_depth_record(tdx_buf *buffer, int market, const char *code,
                              uint16_t active, int64_t current_delta,
                              int64_t previous_delta, int64_t open_delta,
                              int64_t high_delta, int64_t low_delta,
                              uint32_t update_time, int64_t status,
                              int64_t total_hand, int64_t current_hand,
                              uint32_t amount_raw, int64_t inside, int64_t outside,
                              int64_t after_outer, int64_t open_amount,
                              const int64_t *buy_deltas, const int64_t *sell_deltas,
                              const int64_t *buy_volumes, const int64_t *sell_volumes,
                              size_t levels) {
    uint8_t header[9];
    size_t index;
    header[0] = (uint8_t)market;
    memcpy(header + 1, code, 6);
    header[7] = (uint8_t)(active & 0xFFu);
    header[8] = (uint8_t)((active >> 8) & 0xFFu);
    (void)tdx_buf_append(buffer, header, sizeof(header), NULL);
    append_varint(buffer, current_delta);
    append_varint(buffer, previous_delta);
    append_varint(buffer, open_delta);
    append_varint(buffer, high_delta);
    append_varint(buffer, low_delta);
    (void)tdx_buf_append_u32le(buffer, update_time, NULL);
    append_varint(buffer, status);
    append_varint(buffer, total_hand);
    append_varint(buffer, current_hand);
    (void)tdx_buf_append_u32le(buffer, amount_raw, NULL);
    append_varint(buffer, inside);
    append_varint(buffer, outside);
    append_varint(buffer, after_outer);
    append_varint(buffer, open_amount);
    for (index = 0; index < levels; ++index) {
        append_varint(buffer, buy_deltas[index]);
        append_varint(buffer, sell_deltas[index]);
        append_varint(buffer, buy_volumes[index]);
        append_varint(buffer, sell_volumes[index]);
    }
}

static void append_depth_record_default(tdx_buf *buffer) {
    static const int64_t buy_deltas[5] = {-1, -2, -3, -4, -5};
    static const int64_t sell_deltas[5] = {0, 1, 2, 3, 4};
    static const int64_t buy_volumes[5] = {4119, 4555, 4760, 8637, 12532};
    static const int64_t sell_volumes[5] = {282, 3690, 2008, 1265, 2565};
    build_depth_record(buffer, 0, "000001", 1684, 1166, 4, 4, 6, -6, 105736, 0,
                       385417, 73, 0, 174059, 211359, 0, 263016, buy_deltas,
                       sell_deltas, buy_volumes, sell_volumes, 5);
    /* Three trailing bytes the current client build does not consume. */
    (void)tdx_buf_append(buffer, "\xCD\x10\xAD", 3, NULL);
}

static void test_depth_response(void) {
    tdx_code requested[1];
    tdx_buf payload;
    tdx_depth depths[4];
    size_t count = 0;
    size_t index;
    tdx_error error;
    error.message[0] = '\0';
    CHECK(tdx_code_parse("sz000001", &requested[0], &error) == TDX_OK, "parse failed");

    tdx_buf_init(&payload);
    (void)tdx_buf_append_u16le(&payload, 1, NULL);
    append_depth_record_default(&payload);
    for (index = 0; index < payload.len; ++index)
        payload.data[index] ^= 0x93u;

    CHECK(tdx_quote_parse_depth_response(payload.data, payload.len, requested, 1, depths,
                                         4, &count, &error) == TDX_OK,
          "parse failed: %s", error.message);
    CHECK(count == 1, "parsed %zu records, expected 1", count);
    if (count == 1) {
        const tdx_depth *depth = &depths[0];
        char id[16];
        tdx_code_id(&depth->security, id, sizeof(id));
        CHECK(strcmp(id, "SZ000001") == 0, "security id is %s", id);
        CHECK(depth->active == 1684, "active is %u", depth->active);
        CHECK(nearly(depth->last, 11.66), "last is %.6f", depth->last);
        CHECK(nearly(depth->previous, 11.70), "previous is %.6f", depth->previous);
        CHECK(nearly(depth->open, 11.70), "open is %.6f", depth->open);
        CHECK(nearly(depth->high, 11.72), "high is %.6f", depth->high);
        CHECK(nearly(depth->low, 11.60), "low is %.6f", depth->low);
        CHECK(depth->update_time == 105736, "update time is %u", depth->update_time);
        CHECK(depth->status == 0, "status is %lld", (long long)depth->status);
        CHECK(depth->total_hand == 385417, "total hand is %lld",
              (long long)depth->total_hand);
        CHECK(depth->current_hand == 73, "current hand is %lld",
              (long long)depth->current_hand);
        CHECK(nearly(depth->amount, 0.0), "amount is %.6f", depth->amount);
        CHECK(depth->inside == 174059, "inside is %lld", (long long)depth->inside);
        CHECK(depth->outside == 211359, "outside is %lld", (long long)depth->outside);
        CHECK(nearly(depth->open_amount, 2630160.0), "open amount is %.6f",
              depth->open_amount);
        CHECK(nearly(depth->buys[0].price, 11.65), "buy1 price is %.6f",
              depth->buys[0].price);
        CHECK(depth->buys[0].volume_hand == 4119, "buy1 volume is %lld",
              (long long)depth->buys[0].volume_hand);
        CHECK(nearly(depth->buys[4].price, 11.61), "buy5 price is %.6f",
              depth->buys[4].price);
        CHECK(nearly(depth->sells[0].price, 11.66), "sell1 price is %.6f",
              depth->sells[0].price);
        CHECK(nearly(depth->sells[4].price, 11.70), "sell5 price is %.6f",
              depth->sells[4].price);
        CHECK(depth->sells[4].volume_hand == 2565, "sell5 volume is %lld",
              (long long)depth->sells[4].volume_hand);
        CHECK(depth->tail_size == 3, "tail size is %zu", depth->tail_size);
    }
    tdx_buf_free(&payload);
}

static void test_depth_response_two_records(void) {
    tdx_code requested[2];
    tdx_buf payload;
    tdx_depth depths[4];
    size_t count = 0;
    size_t index;
    tdx_error error;
    error.message[0] = '\0';
    CHECK(tdx_code_parse("sz000001", &requested[0], &error) == TDX_OK, "parse failed");
    CHECK(tdx_code_parse("sh600000", &requested[1], &error) == TDX_OK, "parse failed");

    tdx_buf_init(&payload);
    (void)tdx_buf_append_u16le(&payload, 2, NULL);
    append_depth_record_default(&payload);
    {
        static const int64_t buy_deltas[5] = {0, 0, 0, 0, 0};
        static const int64_t sell_deltas[5] = {1, 1, 1, 1, 1};
        static const int64_t buy_volumes[5] = {1, 2, 3, 4, 5};
        static const int64_t sell_volumes[5] = {6, 7, 8, 9, 10};
        build_depth_record(&payload, 1, "600000", 900, 800, 0, 0, 0, 0, 100, 0, 1000,
                           5, 0, 10, 20, 0, 100, buy_deltas, sell_deltas, buy_volumes,
                           sell_volumes, 5);
    }
    for (index = 0; index < payload.len; ++index)
        payload.data[index] ^= 0x93u;

    CHECK(tdx_quote_parse_depth_response(payload.data, payload.len, requested, 2, depths,
                                         4, &count, &error) == TDX_OK,
          "parse failed: %s", error.message);
    CHECK(count == 2, "parsed %zu records, expected 2", count);
    if (count == 2) {
        CHECK(depths[0].total_hand == 385417, "first total hand is %lld",
              (long long)depths[0].total_hand);
        CHECK(depths[1].security.market_id == 1, "second market is %d",
              depths[1].security.market_id);
        CHECK(strcmp(depths[1].security.code, "600000") == 0, "second code is %s",
              depths[1].security.code);
        CHECK(depths[1].total_hand == 1000, "second total hand is %lld",
              (long long)depths[1].total_hand);
        CHECK(nearly(depths[1].last, 8.00), "second last is %.6f", depths[1].last);
    }
    tdx_buf_free(&payload);
}

static void test_depth_response_rejects_bad_count(void) {
    tdx_code requested[1];
    tdx_buf payload;
    tdx_depth depths[4];
    size_t count = 0;
    size_t index;
    tdx_error error;
    error.message[0] = '\0';
    CHECK(tdx_code_parse("sz000001", &requested[0], &error) == TDX_OK, "parse failed");
    tdx_buf_init(&payload);
    (void)tdx_buf_append_u16le(&payload, 5, NULL);
    append_depth_record_default(&payload);
    for (index = 0; index < payload.len; ++index)
        payload.data[index] ^= 0x93u;
    CHECK(tdx_quote_parse_depth_response(payload.data, payload.len, requested, 1, depths,
                                         4, &count, &error) == TDX_ERR,
          "a record count above the request must be rejected");
    tdx_buf_free(&payload);
}

static void test_depth_response_rejects_truncation(void) {
    tdx_code requested[1];
    tdx_buf payload;
    tdx_depth depths[4];
    size_t count = 0;
    size_t index;
    tdx_error error;
    error.message[0] = '\0';
    CHECK(tdx_code_parse("sz000001", &requested[0], &error) == TDX_OK, "parse failed");
    tdx_buf_init(&payload);
    (void)tdx_buf_append_u16le(&payload, 1, NULL);
    append_depth_record_default(&payload);
    /* Drop the final five bytes so the level varints run off the end. */
    payload.len -= 5;
    for (index = 0; index < payload.len; ++index)
        payload.data[index] ^= 0x93u;
    CHECK(tdx_quote_parse_depth_response(payload.data, payload.len, requested, 1, depths,
                                         4, &count, &error) == TDX_ERR,
          "a truncated record must be rejected");
    tdx_buf_free(&payload);
}

static void test_wire_number_zero(void) {
    CHECK(tdx_wire_number(0) == 0.0, "wire number of zero must be zero");
}

int main(void) {
    test_code_parse();
    test_code_parse_rejects();
    test_code_id();
    test_price_divisor();
    test_varint();
    test_varint_round_trip();
    test_varint_rejects_truncation();
    test_build_depth_request();
    test_depth_response();
    test_depth_response_two_records();
    test_depth_response_rejects_bad_count();
    test_depth_response_rejects_truncation();
    test_wire_number_zero();
    if (failures) {
        printf("%d quote check(s) failed\n", failures);
        return 1;
    }
    printf("quote checks passed\n");
    return 0;
}
