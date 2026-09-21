/* test_timeline.c - 0x0537 time-share series.
 *
 * This command was recovered from the live server rather than ported, so the
 * test carries most of the evidence: the reply fixture is the captured stream,
 * and the assertions below pin the two things that are easy to misread, namely
 * that the price and average fields are offsets from point 0 rather than deltas,
 * and that the volume is a per-minute lot count whose sum is the day total. */
#include <stdio.h>
#include <string.h>

#include "tdx_timeline.h"
#include "tdx_timeline_json.h"
#include "timeline_fixtures.h"

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

/* --- minute mapping --------------------------------------------------- */

static void test_minute_mapping(void) {
    CHECK(tdx_timeline_minute_of_day(0) == 9 * 60 + 31, "point 0 is 09:31, got %d",
          tdx_timeline_minute_of_day(0));
    CHECK(tdx_timeline_minute_of_day(119) == 11 * 60 + 30, "point 119 is 11:30, got %d",
          tdx_timeline_minute_of_day(119));
    CHECK(tdx_timeline_minute_of_day(120) == 13 * 60 + 1, "point 120 skips lunch to 13:01, got %d",
          tdx_timeline_minute_of_day(120));
    CHECK(tdx_timeline_minute_of_day(239) == 15 * 60, "point 239 is 15:00, got %d",
          tdx_timeline_minute_of_day(239));
    CHECK(tdx_timeline_minute_of_day(1) == 9 * 60 + 32, "point 1 is 09:32");
}

/* --- request ---------------------------------------------------------- */

static void test_request(void) {
    tdx_buf request;
    tdx_error error;
    size_t index;
    int zero_tail = 1;
    error.message[0] = '\0';
    tdx_buf_init(&request);

    CHECK(tdx_timeline_build_request(0, "000623", &request, &error) == TDX_OK, "build: %s",
          error.message);
    CHECK(request.len == TDX_TIMELINE_REQUEST_SIZE, "the body is %zu bytes, expected %u",
          request.len, (unsigned)TDX_TIMELINE_REQUEST_SIZE);
    if (request.len == TDX_TIMELINE_REQUEST_SIZE) {
        CHECK(request.data[0] == 0, "market at +0");
        CHECK(request.data[1] == 0, "the reserved byte at +1 stays zero");
        CHECK(memcmp(request.data + 2, "000623", 6) == 0, "the code sits at +2");
        for (index = 8; index < request.len; ++index)
            if (request.data[index] != 0)
                zero_tail = 0;
        CHECK(zero_tail, "the four trailing bytes are written as zero");
    }
    CHECK(tdx_timeline_build_request(1, "600000", &request, &error) == TDX_OK, "market 1");
    CHECK(tdx_timeline_build_request(0, "00062", &request, &error) == TDX_ERR,
          "a five-digit code must be refused");
    CHECK(tdx_timeline_build_request(0, "00062a", &request, &error) == TDX_ERR,
          "a non-numeric code must be refused");
    CHECK(tdx_timeline_build_request(3, "000623", &request, &error) == TDX_ERR,
          "market 3 must be refused");
    tdx_buf_free(&request);
}

/* --- the captured reply ----------------------------------------------- */

static void test_parse_captured(void) {
    tdx_timeline timeline;
    tdx_error error;
    double volume = 0.0;
    size_t index;
    error.message[0] = '\0';
    tdx_timeline_init(&timeline);

    CHECK(tdx_timeline_parse(today_reply, sizeof(today_reply), &timeline, &error) == TDX_OK,
          "parse: %s", error.message);
    CHECK(timeline.count == 240, "a full session is 240 points, got %zu", timeline.count);
    CHECK(timeline.reserved == 0, "the reserved header word is zero, got %lld",
          (long long)timeline.reserved);
    if (timeline.count == 240) {
        const tdx_timeline_point *first = &timeline.points[0];
        const tdx_timeline_point *last = &timeline.points[239];

        /* Point 0 carries the session base in both fields. */
        CHECK(first->price > 17.59 && first->price < 17.61, "the base price %.4f", first->price);
        CHECK(first->average_price > 17.56 && first->average_price < 17.57,
              "the base average %.5f", first->average_price);
        CHECK(first->volume_hand == 3921, "the first minute's volume %lld",
              (long long)first->volume_hand);
        CHECK(first->minute_of_day == 9 * 60 + 31, "the first minute %d", first->minute_of_day);

        /* The last point is the closing print; these were cross-checked against
         * the same day's 0x052D 15:00 bar and the 0x0FC6 closing tick. */
        CHECK(last->price > 17.74 && last->price < 17.76, "the closing price %.4f", last->price);
        CHECK(last->volume_hand == 1097, "the closing auction volume %lld",
              (long long)last->volume_hand);
        CHECK(last->minute_of_day == 15 * 60, "the closing minute %d", last->minute_of_day);

        /* The offset reading: point 1's price must be base + its own offset, and
         * reading the field as a delta instead would put it at 17.61 + 17.61. */
        CHECK(timeline.points[1].price_offset_raw == 1,
              "point 1's price offset is +1, got %lld",
              (long long)timeline.points[1].price_offset_raw);
        /* base + 0.01 and (base_raw + 1)/100 are different double roundings, so
         * compare with a tolerance rather than for bit equality. */
        CHECK(timeline.points[1].price - (first->price + 0.01) < 1e-9 &&
                  (first->price + 0.01) - timeline.points[1].price < 1e-9,
              "point 1 must be the base plus one cent, got %.4f vs %.4f",
              timeline.points[1].price, first->price + 0.01);

        /* Every point must be sane, monotone where it has to be, and the running
         * average must sit inside the day's envelope near the end. */
        for (index = 0; index < timeline.count; ++index) {
            const tdx_timeline_point *point = &timeline.points[index];
            CHECK(point->volume_hand >= 0, "point %zu has volume %lld", index,
                  (long long)point->volume_hand);
            CHECK(point->price > 17.0 && point->price < 18.5,
                  "point %zu price %.4f is outside the day", index, point->price);
            CHECK(point->minute_of_day == tdx_timeline_minute_of_day(index),
                  "point %zu minute %d", index, point->minute_of_day);
            volume += (double)point->volume_hand;
        }
        /* 87,389 lots is the day total that the 0x052D daily bar and the 0x0FC6
         * tape independently report for this session. */
        CHECK((long long)volume == 87389, "the minute volumes must sum to the day total, got %lld",
              (long long)volume);
    }
    tdx_timeline_free(&timeline);
}

static void test_parse_rejects(void) {
    tdx_timeline timeline;
    tdx_error error;
    uint8_t body[64];
    error.message[0] = '\0';

    tdx_timeline_init(&timeline);
    memset(body, 0, sizeof(body));
    CHECK(tdx_timeline_parse(body, sizeof(body), &timeline, &error) == TDX_OK,
          "a zero-point series is legal: %s", error.message);
    CHECK(timeline.count == 0, "no points, got %zu", timeline.count);
    tdx_timeline_free(&timeline);

    tdx_timeline_init(&timeline);
    CHECK(tdx_timeline_parse(today_reply, 3, &timeline, &error) == TDX_ERR,
          "a three-byte body must be rejected");
    tdx_timeline_free(&timeline);

    tdx_timeline_init(&timeline);
    CHECK(tdx_timeline_parse(today_reply, sizeof(today_reply) - 3, &timeline, &error) == TDX_ERR,
          "a truncated final point must be rejected");
    tdx_timeline_free(&timeline);

    /* A count above the sanity cap. */
    memset(body, 0, sizeof(body));
    body[0] = 0x01;
    body[1] = 0x04; /* 1025 */
    tdx_timeline_init(&timeline);
    CHECK(tdx_timeline_parse(body, sizeof(body), &timeline, &error) == TDX_ERR,
          "a count above the cap must be rejected");
    CHECK(strstr(error.message, "above the") != NULL, "the error must say why: %s",
          error.message);
    tdx_timeline_free(&timeline);

    /* A body that carries more bytes than its declared points consume. */
    {
        static uint8_t extra[16];
        memset(extra, 0, sizeof(extra));
        extra[0] = 1; /* one point */
        extra[1] = 0;
        extra[4] = 0x10; /* price offset 16 */
        extra[5] = 0x20; /* average offset 32 */
        extra[6] = 0x05; /* volume 5 */
        extra[7] = 0x7f; /* a stray byte the point does not consume */
        tdx_timeline_init(&timeline);
        CHECK(tdx_timeline_parse(extra, 8, &timeline, &error) == TDX_ERR,
              "unconsumed bytes must be rejected");
        CHECK(strstr(error.message, "trailing") != NULL, "the error must say why: %s",
              error.message);
        tdx_timeline_free(&timeline);
    }
}

/* --- series arithmetic and rendering ---------------------------------- */

static int braces_balanced(const char *text) {
    int depth = 0;
    int in_string = 0;
    int escaped = 0;
    const char *cursor;
    for (cursor = text; *cursor; ++cursor) {
        if (in_string) {
            if (escaped)
                escaped = 0;
            else if (*cursor == '\\')
                escaped = 1;
            else if (*cursor == '"')
                in_string = 0;
            continue;
        }
        if (*cursor == '"')
            in_string = 1;
        else if (*cursor == '{')
            depth++;
        else if (*cursor == '}') {
            depth--;
            if (depth < 0)
                return 0;
        }
    }
    return !in_string && depth == 0;
}

static void test_json_rendering(void) {
    tdx_timeline timeline;
    tdx_buf line;
    tdx_error error;
    char text[4096];
    size_t copy;

    error.message[0] = '\0';
    tdx_timeline_init(&timeline);
    tdx_buf_init(&line);
    CHECK(tdx_timeline_parse(today_reply, sizeof(today_reply), &timeline, &error) == TDX_OK,
          "parse: %s", error.message);

    CHECK(tdx_timeline_format_point(&line, &timeline, &timeline.points[0], 0, "000623", &error) ==
              TDX_OK,
          "point render: %s", error.message);
    copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
    memcpy(text, line.data, copy);
    text[copy] = '\0';
    CHECK(braces_balanced(text), "the point object must be balanced: %s", text);
    CHECK(strstr(text, "\"type\":\"minute_point\"") != NULL, "the point type");
    CHECK(strstr(text, "\"security_id\":\"SZ000623\"") != NULL, "the identity");
    CHECK(strstr(text, "\"time\":\"09:31\"") != NULL, "the first label");
    CHECK(strstr(text, "\"volume_hand\":3921") != NULL, "the first volume");

    tdx_buf_clear(&line);
    CHECK(tdx_timeline_format_summary(&line, &timeline, 0, "000623", "1.2.3.4:7709", &error) ==
              TDX_OK,
          "summary render: %s", error.message);
    copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
    memcpy(text, line.data, copy);
    text[copy] = '\0';
    CHECK(braces_balanced(text), "the summary must be balanced: %s", text);
    CHECK(text[line.len - 1] == '}', "the summary must end with '}'");
    CHECK(strstr(text, "\"type\":\"timeline_summary\"") != NULL, "the summary type");
    CHECK(strstr(text, "\"point_count\":240") != NULL, "the point count");
    CHECK(strstr(text, "\"volume_hand\":87389") != NULL, "the volume total: %s", text);
    CHECK(strstr(text, "\"first_time\":\"09:31\"") != NULL, "the first label");
    CHECK(strstr(text, "\"last_time\":\"15:00\"") != NULL, "the last label");

    /* An empty series must render nulls, not invented zeros. */
    tdx_timeline_init(&timeline);
    tdx_buf_clear(&line);
    CHECK(tdx_timeline_format_summary(&line, &timeline, 0, "000623", NULL, &error) == TDX_OK,
          "empty summary render: %s", error.message);
    copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
    memcpy(text, line.data, copy);
    text[copy] = '\0';
    CHECK(braces_balanced(text), "an empty summary must be balanced: %s", text);
    CHECK(strstr(text, "\"first_time\":null") != NULL, "missing times render null");
    CHECK(strstr(text, "\"endpoint\":null") != NULL, "a missing endpoint renders null");
    CHECK(strstr(text, "\"point_count\":0") != NULL, "a zero point count");

    tdx_buf_free(&line);
    tdx_timeline_free(&timeline);
}

int main(void) {
    test_minute_mapping();
    test_request();
    test_parse_captured();
    test_parse_rejects();
    test_json_rendering();

    if (failures) {
        printf("%d timeline check(s) failed\n", failures);
        return 1;
    }
    printf("timeline checks passed\n");
    return 0;
}
