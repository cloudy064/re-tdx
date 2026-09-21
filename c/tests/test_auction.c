/* test_auction.c - 0x056A call-auction point series.
 *
 * The two reply fixtures are live captures: one from selector 0 (opening only,
 * 42 points) and one from selector 1 (opening plus closing, 61 points).  They
 * cover both the single-segment and the two-segment shape, and they are the
 * reason the price field is read as a float32 rather than memcpy'd into a double
 * - a first pass did the latter and decoded every price as 0. */
#include <stdio.h>
#include <string.h>

#include "tdx_auction.h"
#include "tdx_auction_json.h"
#include "auction_fixtures.h"

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

/* --- helpers ---------------------------------------------------------- */

static void test_helpers(void) {
    char label[12];
    tdx_error error;
    error.message[0] = '\0';

    CHECK(strcmp(tdx_auction_direction_text(1), "buy") == 0, "positive is buy");
    CHECK(strcmp(tdx_auction_direction_text(-1), "sell") == 0, "negative is sell");
    CHECK(strcmp(tdx_auction_direction_text(0), "balanced") == 0, "zero is balanced");

    CHECK(tdx_auction_time_label(0, label, sizeof(label)) == TDX_OK && strcmp(label, "00:00:00") == 0,
          "midnight, got %s", label);
    CHECK(tdx_auction_time_label(9 * 3600 + 15 * 60, label, sizeof(label)) == TDX_OK &&
              strcmp(label, "09:15:00") == 0,
          "the opening auction start, got %s", label);
    CHECK(tdx_auction_time_label(9 * 60 * 60 + 24 * 60 + 57, label, sizeof(label)) == TDX_OK &&
              strcmp(label, "09:24:57") == 0,
          "the last sample before the match, got %s", label);
    CHECK(tdx_auction_time_label(15 * 3600, label, sizeof(label)) == TDX_OK &&
              strcmp(label, "15:00:00") == 0,
          "the close, got %s", label);
    CHECK(tdx_auction_time_label(-1, label, sizeof(label)) == TDX_ERR, "a negative time must fail");
    CHECK(tdx_auction_time_label(24 * 3600, label, sizeof(label)) == TDX_ERR,
          "24:00:00 must fail");
    CHECK(tdx_auction_time_label(0, label, 3) == TDX_ERR, "a short buffer must fail");
}

/* --- request ---------------------------------------------------------- */

static void test_request(void) {
    tdx_buf request;
    tdx_error error;
    error.message[0] = '\0';
    tdx_buf_init(&request);

    CHECK(tdx_auction_build_request(0, "000623", 0, 0, 200, &request, &error) == TDX_OK,
          "build: %s", error.message);
    CHECK(request.len == TDX_AUCTION_REQUEST_SIZE, "the body is %zu bytes, expected %u",
          request.len, (unsigned)TDX_AUCTION_REQUEST_SIZE);
    if (request.len == TDX_AUCTION_REQUEST_SIZE) {
        CHECK(request.data[0] == 0, "market at +0");
        CHECK(request.data[1] == 0, "the reserved byte at +1");
        CHECK(memcmp(request.data + 2, "000623", 6) == 0, "the code sits at +2");
        CHECK(tdx_u32le(request.data + 8) == 0, "the first constant word at +8");
        CHECK(tdx_u32le(request.data + 12) == 0, "selector at +12");
        CHECK(tdx_u32le(request.data + 16) == 0, "the second constant word at +16");
        CHECK(tdx_u32le(request.data + 20) == 0, "start at +20");
        CHECK(tdx_u32le(request.data + 24) == 200, "limit at +24");
    }
    CHECK(tdx_auction_build_request(1, "600000", 1, 7, 5000, &request, &error) == TDX_OK,
          "a full selector: %s", error.message);
    if (request.len == TDX_AUCTION_REQUEST_SIZE) {
        CHECK(tdx_u32le(request.data + 12) == 1, "selector 1");
        CHECK(tdx_u32le(request.data + 20) == 7, "start 7");
        CHECK(tdx_u32le(request.data + 24) == 5000, "limit 5000");
    }
    CHECK(tdx_auction_build_request(0, "000623", 0, 0, 0, &request, &error) == TDX_ERR,
          "a zero limit must be refused");
    CHECK(tdx_auction_build_request(0, "000623", 0, 0, 5001, &request, &error) == TDX_ERR,
          "a limit above 5000 must be refused");
    CHECK(tdx_auction_build_request(3, "000623", 0, 0, 10, &request, &error) == TDX_ERR,
          "market 3 must be refused");
    CHECK(tdx_auction_build_request(0, "00062", 0, 0, 10, &request, &error) == TDX_ERR,
          "a five-digit code must be refused");
    tdx_buf_free(&request);
}

/* --- the captured replies --------------------------------------------- */

static void test_parse_opening(void) {
    tdx_auction_series series;
    tdx_auction_summary summary;
    tdx_error error;
    error.message[0] = '\0';
    tdx_auction_series_init(&series);

    CHECK(tdx_auction_parse(opening_reply, sizeof(opening_reply), 0, 0, 200, &series, &error) ==
              TDX_OK,
          "parse: %s", error.message);
    CHECK(series.count == 42, "42 points for this session's opening auction, got %zu",
          series.count);
    if (series.count == 42) {
        const tdx_auction_point *first = &series.points[0];
        const tdx_auction_point *last = &series.points[41];
        CHECK(first->minute_of_day == 9 * 60 + 15 && first->second == 0,
              "the series starts at 09:15:00, got %02d:%02d:%02d", first->minute_of_day / 60,
              first->minute_of_day % 60, first->second);
        CHECK(first->time_seconds == 9 * 3600 + 15 * 60, "the first time in seconds %d",
              first->time_seconds);
        CHECK(first->price > 17.55 && first->price < 17.57, "the first virtual price %.4f",
              first->price);
        CHECK(first->matched_volume_hand == 1, "the first matched volume %lld",
              (long long)first->matched_volume_hand);
        CHECK(first->unmatched_signed_hand == -4, "the first imbalance %lld",
              (long long)first->unmatched_signed_hand);
        CHECK(first->unmatched_volume_hand == 4, "the absolute imbalance %lld",
              (long long)first->unmatched_volume_hand);
        CHECK(first->unmatched_direction_raw == -1, "the first direction is sell");
        CHECK(strcmp(tdx_auction_direction_text(first->unmatched_direction_raw), "sell") == 0,
              "the direction text");
        CHECK(first->reserved == 0, "the reserved byte is zero");
        CHECK(first->matched_amount_yuan > 1755.0 && first->matched_amount_yuan < 1757.0,
              "the first matched amount %.2f", first->matched_amount_yuan);

        /* The series stops three seconds before the match, which is expected. */
        CHECK(last->minute_of_day == 9 * 60 + 24 && last->second == 57,
              "the series ends at 09:24:57, got %02d:%02d:%02d", last->minute_of_day / 60,
              last->minute_of_day % 60, last->second);
        CHECK(last->price > 17.51 && last->price < 17.53, "the last virtual price %.4f",
              last->price);
        CHECK(last->matched_volume_hand == 1082, "the last matched volume %lld",
              (long long)last->matched_volume_hand);
        CHECK(last->unmatched_direction_raw == 1, "the last imbalance is on the buy side");
    }

    tdx_auction_summarize(&series, &summary);
    CHECK(summary.point_count == 42, "summary point count %zu", summary.point_count);
    CHECK(summary.opening.has_points == 1 && summary.opening.point_count == 42,
          "the opening segment holds every point, got %zu", summary.opening.point_count);
    CHECK(summary.closing.has_points == 0 && summary.closing.point_count == 0,
          "selector 0 must not report a closing segment");
    CHECK(summary.opening.start_time_seconds == 9 * 3600 + 15 * 60, "the segment start");
    CHECK(summary.opening.end_time_seconds == 9 * 3600 + 24 * 60 + 57, "the segment end");
    CHECK(summary.opening.unmatched_direction_flips > 0,
          "the opening imbalance flips sides during the auction");
    CHECK(summary.opening.matched_volume_monotonic_violations > 0,
          "the virtual matched volume is revised downwards, which is normal");
    CHECK(summary.reserved_nonzero_points == 0, "no record uses the reserved byte");
    CHECK(summary.opening.max_unmatched_volume_hand > 1000,
          "the peak imbalance %lld", (long long)summary.opening.max_unmatched_volume_hand);
    tdx_auction_series_free(&series);
}

static void test_parse_combined(void) {
    tdx_auction_series series;
    tdx_auction_summary summary;
    tdx_error error;
    error.message[0] = '\0';
    tdx_auction_series_init(&series);

    CHECK(tdx_auction_parse(combined_reply, sizeof(combined_reply), 1, 0, 200, &series, &error) ==
              TDX_OK,
          "parse: %s", error.message);
    CHECK(series.count == 61, "42 opening plus 19 closing points, got %zu", series.count);
    if (series.count == 61) {
        const tdx_auction_point *closing_first = &series.points[42];
        const tdx_auction_point *closing_last = &series.points[60];
        CHECK(closing_first->minute_of_day == 14 * 60 + 57,
              "the closing segment starts at 14:57, got %02d:%02d", closing_first->minute_of_day / 60,
              closing_first->minute_of_day % 60);
        CHECK(closing_first->price > 17.76 && closing_first->price < 17.78,
              "the first closing virtual price %.4f", closing_first->price);
        CHECK(closing_last->minute_of_day == 14 * 60 + 59 && closing_last->second == 51,
              "the closing segment ends at 14:59:51, got %02d:%02d:%02d",
              closing_last->minute_of_day / 60, closing_last->minute_of_day % 60,
              closing_last->second);
        CHECK(closing_last->matched_volume_hand == 686, "the last matched volume %lld",
              (long long)closing_last->matched_volume_hand);
        /* The closing side ends on the sell side, the opening on the buy side. */
        CHECK(closing_last->unmatched_direction_raw == -1, "the closing imbalance is sell");
    }

    tdx_auction_summarize(&series, &summary);
    CHECK(summary.opening.point_count == 42, "the opening segment holds 42, got %zu",
          summary.opening.point_count);
    CHECK(summary.closing.point_count == 19, "the closing segment holds 19, got %zu",
          summary.closing.point_count);
    CHECK(summary.opening.point_count + summary.closing.point_count == summary.point_count,
          "the two segments must account for every point");
    CHECK(summary.has_largest_gap == 1, "a two-segment series has a gap");
    CHECK(summary.largest_gap_seconds == 19932,
          "the gap is the lunch-and-afternoon span, got %d", summary.largest_gap_seconds);
    CHECK(summary.gap_after_time_seconds == 9 * 3600 + 24 * 60 + 57, "the gap starts after 09:24:57");
    CHECK(summary.gap_before_time_seconds == 14 * 3600 + 57 * 60 + 9, "the gap ends at 14:57:09");
    CHECK(summary.closing.unmatched_direction_flips > 0, "the closing imbalance flips too");
    tdx_auction_series_free(&series);
}

static void test_parse_rejects(void) {
    tdx_auction_series series;
    tdx_error error;
    uint8_t body[64];
    error.message[0] = '\0';

    tdx_auction_series_init(&series);
    CHECK(tdx_auction_parse(opening_reply, 1, 0, 0, 200, &series, &error) == TDX_ERR,
          "a one-byte body must be rejected");
    tdx_auction_series_free(&series);

    /* The record count and the body length must agree exactly. */
    tdx_auction_series_init(&series);
    CHECK(tdx_auction_parse(opening_reply, sizeof(opening_reply) - 1, 0, 0, 200, &series,
                            &error) == TDX_ERR,
          "a body one byte short of count*16 must be rejected");
    CHECK(strstr(error.message, "length mismatch") != NULL, "the error must say why: %s",
          error.message);
    tdx_auction_series_free(&series);

    tdx_auction_series_init(&series);
    CHECK(tdx_auction_parse(opening_reply, sizeof(opening_reply) + 16, 0, 0, 200, &series,
                            &error) == TDX_ERR,
          "a body one record too long must be rejected");
    tdx_auction_series_free(&series);

    /* A second of 60 is not a second.  Records start after the 2-byte count, so
     * the record's own offsets are shifted by two. */
    memset(body, 0, sizeof(body));
    body[0] = 1;
    body[1] = 0;
    body[2] = 0x2B; /* minute 555 = 09:15 */
    body[3] = 0x02;
    body[17] = 60; /* record[15] */
    tdx_auction_series_init(&series);
    error.message[0] = '\0';
    CHECK(tdx_auction_parse(body, 2 + 16, 0, 0, 10, &series, &error) == TDX_ERR,
          "a second of 60 must be rejected");
    CHECK(strstr(error.message, "has time") != NULL, "the error must name the field: %s",
          error.message);
    tdx_auction_series_free(&series);

    /* Minute 1440 does not exist. */
    memset(body, 0, sizeof(body));
    body[0] = 1;
    body[1] = 0;
    body[2] = 0xA0;
    body[3] = 0x05; /* 1440 */
    tdx_auction_series_init(&series);
    error.message[0] = '\0';
    CHECK(tdx_auction_parse(body, 2 + 16, 0, 0, 10, &series, &error) == TDX_ERR,
          "minute 1440 must be rejected");
    tdx_auction_series_free(&series);

    /* A negative price: -1.0f sits at record+2, which is body+4. */
    memset(body, 0, sizeof(body));
    body[0] = 1;
    body[1] = 0;
    body[2] = 0x2B;
    body[3] = 0x02;
    body[4] = 0x00;
    body[5] = 0x00;
    body[6] = 0x80;
    body[7] = 0xBF;
    tdx_auction_series_init(&series);
    error.message[0] = '\0';
    CHECK(tdx_auction_parse(body, 2 + 16, 0, 0, 10, &series, &error) == TDX_ERR,
          "a negative price must be rejected");
    CHECK(strstr(error.message, "price") != NULL, "the error must name the field: %s",
          error.message);
    tdx_auction_series_free(&series);

    /* A not-a-number price. */
    memset(body, 0, sizeof(body));
    body[0] = 1;
    body[1] = 0;
    body[2] = 0x2B;
    body[3] = 0x02;
    body[4] = 0x00;
    body[5] = 0x00;
    body[6] = 0xC0;
    body[7] = 0x7F; /* NaN */
    tdx_auction_series_init(&series);
    error.message[0] = '\0';
    CHECK(tdx_auction_parse(body, 2 + 16, 0, 0, 10, &series, &error) == TDX_ERR,
          "a NaN price must be rejected");
    tdx_auction_series_free(&series);
}

/* --- rendering -------------------------------------------------------- */

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
    tdx_auction_series series;
    tdx_auction_summary summary;
    tdx_buf line;
    tdx_error error;
    char text[4096];
    size_t copy;

    error.message[0] = '\0';
    tdx_auction_series_init(&series);
    tdx_buf_init(&line);
    CHECK(tdx_auction_parse(combined_reply, sizeof(combined_reply), 1, 0, 200, &series, &error) ==
              TDX_OK,
          "parse: %s", error.message);
    tdx_auction_summarize(&series, &summary);

    CHECK(tdx_auction_format_point(&line, &series.points[0], 0, "000623", "20260921", &error) ==
              TDX_OK,
          "point render: %s", error.message);
    copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
    memcpy(text, line.data, copy);
    text[copy] = '\0';
    CHECK(braces_balanced(text), "the point object must be balanced: %s", text);
    CHECK(strstr(text, "\"type\":\"auction_point\"") != NULL, "the point type");
    CHECK(strstr(text, "\"security_id\":\"SZ000623\"") != NULL, "the identity");
    CHECK(strstr(text, "\"time\":\"09:15:00\"") != NULL, "the time label");
    CHECK(strstr(text, "\"unmatched_direction\":\"sell\"") != NULL, "the direction");
    CHECK(strstr(text, "\"unmatched_volume_hand\":4") != NULL, "the absolute imbalance");

    tdx_buf_clear(&line);
    CHECK(tdx_auction_format_summary(&line, &series, &summary, 0, "000623", "20260921",
                                     "1.2.3.4:7709", &error) == TDX_OK,
          "summary render: %s", error.message);
    copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
    memcpy(text, line.data, copy);
    text[copy] = '\0';
    CHECK(braces_balanced(text), "the summary must be balanced: %s", text);
    CHECK(text[line.len - 1] == '}', "the summary must end with '}'");
    CHECK(strstr(text, "\"type\":\"auction_summary\"") != NULL, "the summary type");
    CHECK(strstr(text, "\"point_count\":61") != NULL, "the point count");
    CHECK(strstr(text, "\"selector\":1") != NULL, "the selector");
    CHECK(strstr(text, "\"opening\":{") != NULL && strstr(text, "\"closing\":{") != NULL,
          "both segments must be present: %s", text);
    CHECK(strstr(text, "\"largest_gap_seconds\":19932") != NULL, "the segment gap");

    /* An empty series renders nulls rather than invented values. */
    tdx_auction_series_init(&series);
    tdx_auction_summarize(&series, &summary);
    tdx_buf_clear(&line);
    CHECK(tdx_auction_format_summary(&line, &series, &summary, 0, "000623", NULL, NULL, &error) ==
              TDX_OK,
          "empty summary render: %s", error.message);
    copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
    memcpy(text, line.data, copy);
    text[copy] = '\0';
    CHECK(braces_balanced(text), "an empty summary must be balanced: %s", text);
    CHECK(strstr(text, "\"point_count\":0") != NULL, "a zero point count");
    CHECK(strstr(text, "\"has_points\":false") != NULL, "an absent segment is reported as absent");

    tdx_buf_free(&line);
    tdx_auction_series_free(&series);
}

int main(void) {
    test_helpers();
    test_request();
    test_parse_opening();
    test_parse_combined();
    test_parse_rejects();
    test_json_rendering();

    if (failures) {
        printf("%d auction check(s) failed\n", failures);
        return 1;
    }
    printf("auction checks passed\n");
    return 0;
}
