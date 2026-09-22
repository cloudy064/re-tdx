/* test_capital.c - 0x000F share-capital changes and ex-rights events.
 *
 * The fixture is Ping An Bank's entire share-capital history (81 records) captured
 * live.  Three independent facts pin the decode, and all three are asserted here
 * because all three were established from evidence rather than assumed:
 *
 *   * the 2024 record is the "10 for 7.19" dividend the C++ reference recorded,
 *     which fixes the record layout, category 1, and the per-ten-shares basis;
 *   * consecutive share-capital records chain exactly - the shares a company has
 *     after one event are the shares it has before the next - and the live capture
 *     chains on all 26 comparable pairs, which is what pins the slot order
 *     (circulating before, total before, circulating after, total after);
 *   * the float32 view and the wire view agree on all 324 slots, so the slots are
 *     a single float32 and not two quantities sharing four bytes. */
#include <stdio.h>
#include <string.h>

#include "tdx_capital.h"
#include "tdx_capital_json.h"
#include "capital_fixtures.h"
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

#define MAX_RECORDS 400

static tdx_code make_code(int market, const char *code) {
    tdx_code result;
    memset(&result, 0, sizeof(result));
    result.market_id = market;
    memcpy(result.code, code, 6);
    result.code[6] = '\0';
    return result;
}

static void test_category_keys(void) {
    CHECK(strcmp(tdx_capital_category_key(1), "ex_rights_dividend") == 0, "category 1");
    CHECK(strcmp(tdx_capital_category_key(15), "restructuring_adjustment") == 0, "category 15");
    CHECK(strcmp(tdx_capital_category_key(0), "unknown") == 0, "category 0 is unknown");
    CHECK(strcmp(tdx_capital_category_key(16), "unknown") == 0, "category 16 is unknown");
}

static void test_request(void) {
    tdx_code security;
    tdx_buf request = {0};
    tdx_error error;
    error.message[0] = '\0';
    tdx_buf_init(&request);

    security = make_code(0, "000001");
    CHECK(tdx_capital_build_request(&security, &request, &error) == TDX_OK, "build: %s",
          error.message);
    CHECK(request.len == 9, "the body is %zu bytes, expected 9", request.len);
    if (request.len == 9) {
        /* The count is the constant 1: the reply header names one security. */
        CHECK(tdx_u16le(request.data) == 1, "the count is 1");
        CHECK(request.data[2] == 0 && memcmp(request.data + 3, "000001", 6) == 0,
              "market + code follow the count");
    }
    security = make_code(1, "600000");
    CHECK(tdx_capital_build_request(&security, &request, &error) == TDX_OK, "second build");
    CHECK(request.data[2] == 1 && memcmp(request.data + 3, "600000", 6) == 0,
          "market 1 travels in the body");

    CHECK(tdx_capital_build_request(NULL, &request, &error) == TDX_ERR,
          "a null security must be refused");
    security = make_code(3, "000001");
    CHECK(tdx_capital_build_request(&security, &request, &error) == TDX_ERR,
          "market 3 must be refused");
    security = make_code(0, "00000a");
    CHECK(tdx_capital_build_request(&security, &request, &error) == TDX_ERR,
          "a non-numeric code must be refused");
    tdx_buf_free(&request);
}

static void test_parse_captured(void) {
    tdx_capital_record records[MAX_RECORDS];
    size_t count = 0;
    size_t blocks = 0;
    tdx_error error;
    tdx_code security = make_code(0, "000001");
    size_t index;
    size_t chained = 0;
    size_t comparable = 0;
    size_t previous = 0;
    int have_previous = 0;
    int found_719 = 0;

    error.message[0] = '\0';
    CHECK(tdx_capital_parse(capital_reply, sizeof(capital_reply), &security, records,
                            MAX_RECORDS, &count, &blocks, &error) == TDX_OK,
          "parse: %s", error.message);
    CHECK(count == CAPITAL_FIXTURE_COUNT, "the fixture holds %zu records, header said %d", count,
          CAPITAL_FIXTURE_COUNT);
    CHECK(blocks == 1, "the header declares one block, got %zu", blocks);
    if (count != CAPITAL_FIXTURE_COUNT)
        return;

    /* Every record names the security that was asked for. */
    for (index = 0; index < count; ++index) {
        CHECK(records[index].security.market_id == 0 &&
                  strcmp(records[index].security.code, "000001") == 0,
              "record %zu names SZ000001", index);
        CHECK(records[index].date != 0, "record %zu carries a date", index);
        CHECK(records[index].category >= 1 && records[index].category <= 15,
              "record %zu has a known category, got %u", index,
              (unsigned)records[index].category);
    }

    /* The 2024 dividend the reference recorded: 10 for 7.19, on 2024-06-14. */
    for (index = 0; index < count; ++index) {
        if (records[index].category == 1 && records[index].date == 20240614) {
            found_719 = 1;
            CHECK(records[index].float_values[0] > 7.18 && records[index].float_values[0] < 7.20,
                  "the 2024 dividend is 7.19 per ten shares, got %.4f",
                  records[index].float_values[0]);
            /* The two readings must land on the same number. */
            CHECK(records[index].share_values[0] > 71899.0 &&
                      records[index].share_values[0] < 71901.0,
                  "the wire view of the same slot is 71900 shares' worth, got %.0f",
                  records[index].share_values[0]);
        }
    }
    CHECK(found_719, "the 2024-06-14 ex-dividend record must be present");

    /* The share-capital chain: what a company has after one event it has before
     * the next.  Only records that carry counts take part. */
    for (index = 0; index < count; ++index) {
        if (records[index].category == 1 || records[index].category >= 11) {
            have_previous = 0;
            continue;
        }
        if (records[index].share_values[1] <= 0.0 || records[index].share_values[3] <= 0.0) {
            have_previous = 0;
            continue;
        }
        if (have_previous) {
            comparable++;
            if (records[previous].share_values[3] == records[index].share_values[1] &&
                records[previous].share_values[2] == records[index].share_values[0])
                chained++;
        }
        previous = index;
        have_previous = 1;
    }
    CHECK(comparable > 20, "the capture has enough comparable pairs, got %zu", comparable);
    CHECK(chained == comparable, "all %zu consecutive pairs chain, %zu did", comparable, chained);

    /* Every slot, both ways.  They agree on live data, so a divergence here means
     * the decoder changed rather than the server did. */
    {
        size_t slots = 0;
        size_t agreed = 0;
        for (index = 0; index < count; ++index) {
            size_t slot;
            for (slot = 0; slot < TDX_CAPITAL_SLOTS; ++slot) {
                double floater = records[index].float_values[slot];
                double wire = records[index].share_values[slot] / 10000.0;
                double scale = floater < 0 ? -floater : floater;
                double delta = floater - wire;
                if (delta < 0)
                    delta = -delta;
                slots++;
                if (scale < 1e-9 || delta <= scale * 1e-4)
                    agreed++;
            }
        }
        CHECK(slots == count * TDX_CAPITAL_SLOTS, "every slot was compared");
        CHECK(agreed == slots, "the float and wire views agree on %zu of %zu slots", agreed,
              slots);
    }

    /* The first and last dates of the history are what the live capture showed. */
    CHECK(records[0].date == 19900301, "the earliest event is 1990-03-01, got %d",
          records[0].date);
    CHECK(records[count - 1].date > 20260101, "the latest event is recent, got %d",
          records[count - 1].date);
}

static void test_parse_rejects(void) {
    tdx_capital_record records[MAX_RECORDS];
    size_t count = 0;
    size_t blocks = 0;
    tdx_error error;
    tdx_code security = make_code(0, "000001");
    tdx_code other = make_code(1, "600000");
    uint8_t body[64];
    error.message[0] = '\0';

    CHECK(tdx_capital_parse(capital_reply, 10, &security, records, MAX_RECORDS, &count, &blocks,
                            &error) == TDX_ERR,
          "a body shorter than the header must be refused");
    CHECK(tdx_capital_parse(capital_reply, sizeof(capital_reply) - 1, &security, records,
                            MAX_RECORDS, &count, &blocks, &error) == TDX_ERR,
          "a body one byte short must be refused");
    CHECK(strstr(error.message, "length mismatch") != NULL, "the error must say why: %s",
          error.message);
    CHECK(tdx_capital_parse(capital_reply, sizeof(capital_reply) + 29, &security, records,
                            MAX_RECORDS, &count, &blocks, &error) == TDX_ERR,
          "a body one record too long must be refused");
    CHECK(tdx_capital_parse(capital_reply, sizeof(capital_reply), &security, records, 4, &count,
                            &blocks, &error) == TDX_ERR,
          "a reply that does not fit the output must be refused");
    /* The header names one security, so a reply for another one cannot be
     * attributed to this request. */
    error.message[0] = '\0';
    CHECK(tdx_capital_parse(capital_reply, sizeof(capital_reply), &other, records, MAX_RECORDS,
                            &count, &blocks, &error) == TDX_ERR,
          "a reply for a different security must be refused");
    CHECK(strstr(error.message, "not the requested") != NULL, "the error must say why: %s",
          error.message);
    /* Without an expectation the parse still works. */
    CHECK(tdx_capital_parse(capital_reply, sizeof(capital_reply), NULL, records, MAX_RECORDS,
                            &count, &blocks, &error) == TDX_OK,
          "parsing without an expected security is allowed: %s", error.message);

    /* Record-level refusals. */
    memset(body, 0, sizeof(body));
    body[0] = 9;
    memcpy(body + 1, "000001", 6);
    CHECK(tdx_capital_parse_record(body, TDX_CAPITAL_CHANGES_SIZE, &records[0], &error) == TDX_ERR,
          "market 9 must be refused");
    CHECK(strstr(error.message, "market") != NULL, "the error must name the field: %s",
          error.message);
    memset(body, 0, sizeof(body));
    body[0] = 0;
    memcpy(body + 1, "00000a", 6);
    CHECK(tdx_capital_parse_record(body, TDX_CAPITAL_CHANGES_SIZE, &records[0], &error) ==
              TDX_ERR,
          "a non-numeric code must be refused");
    CHECK(tdx_capital_parse_record(body, TDX_CAPITAL_CHANGES_SIZE - 1, &records[0], &error) ==
              TDX_ERR,
          "a 28-byte record must be refused");

    /* A category outside the table. */
    memset(body, 0, sizeof(body));
    body[0] = 0;
    memcpy(body + 1, "000001", 6);
    body[12] = 16;
    error.message[0] = '\0';
    CHECK(tdx_capital_parse_record(body, TDX_CAPITAL_CHANGES_SIZE, &records[0], &error) ==
              TDX_ERR,
          "category 16 must be refused");
    CHECK(strstr(error.message, "category") != NULL, "the error must name the field: %s",
          error.message);

    /* An impossible date: low byte 99 gives year 0. */
    memset(body, 0, sizeof(body));
    body[0] = 0;
    memcpy(body + 1, "000001", 6);
    body[8] = 0x63;
    error.message[0] = '\0';
    CHECK(tdx_capital_parse_record(body, TDX_CAPITAL_CHANGES_SIZE, &records[0], &error) ==
              TDX_ERR,
          "a non-date must be refused");
    CHECK(strstr(error.message, "date") != NULL, "the error must name the field: %s",
          error.message);

    /* A non-finite slot. */
    memset(body, 0, sizeof(body));
    body[0] = 0;
    memcpy(body + 1, "000001", 6);
    body[13] = 0x00;
    body[14] = 0x00;
    body[15] = 0xC0;
    body[16] = 0x7F; /* slot 0 = NaN */
    error.message[0] = '\0';
    CHECK(tdx_capital_parse_record(body, TDX_CAPITAL_CHANGES_SIZE, &records[0], &error) ==
              TDX_ERR,
          "a NaN slot must be refused");
    CHECK(strstr(error.message, "not finite") != NULL, "the error must say why: %s",
          error.message);
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
    tdx_capital_record records[MAX_RECORDS];
    tdx_capital_tally tally;
    size_t count = 0;
    size_t blocks = 0;
    tdx_code security = make_code(0, "000001");
    tdx_buf line = {0};
    tdx_error error;
    char text[8192];
    size_t copy;
    size_t index;
    int checked_dividend = 0;
    int checked_shares = 0;

    error.message[0] = '\0';
    tdx_buf_init(&line);
    CHECK(tdx_capital_parse(capital_reply, sizeof(capital_reply), &security, records,
                            MAX_RECORDS, &count, &blocks, &error) == TDX_OK,
          "parse: %s", error.message);

    /* One record of each shape: the dividend event and a share-capital event. */
    for (index = 0; index < count; ++index) {
        if (records[index].category == 1 && records[index].date == 20240614 && !checked_dividend) {
            checked_dividend = 1;
            tdx_buf_clear(&line);
            CHECK(tdx_capital_format(&line, &records[index], &error) == TDX_OK, "render: %s",
                  error.message);
            copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
            memcpy(text, line.data, copy);
            text[copy] = '\0';
            CHECK(braces_balanced(text), "the dividend object must be balanced: %s", text);
    {
        /* Balanced is not the same as parseable; the project's own parser decides. */
        char reason[192];
        CHECK(render_parses(text, reason, sizeof(reason)),
              "and it parses as JSON: %s\n    %s", reason, text);
    }
            CHECK(strstr(text, "\"type\":\"capital_change\"") != NULL, "the type");
            CHECK(strstr(text, "\"security_id\":\"SZ000001\"") != NULL, "the identity");
            CHECK(strstr(text, "\"date\":\"2024-06-14\"") != NULL, "the date renders");
            CHECK(strstr(text, "\"category_name\":\"ex_rights_dividend\"") != NULL,
                  "the category name");
            CHECK(strstr(text, "\"dividend_per_10_shares_yuan\":7.1900") != NULL,
                  "the per-ten-shares dividend: %s", text);
            CHECK(strstr(text, "\"dividend_per_share_yuan\":0.719000") != NULL,
                  "the derived per-share dividend");
        }
        if (records[index].category == 2 && !checked_shares) {
            checked_shares = 1;
            tdx_buf_clear(&line);
            CHECK(tdx_capital_format(&line, &records[index], &error) == TDX_OK, "render: %s",
                  error.message);
            copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
            memcpy(text, line.data, copy);
            text[copy] = '\0';
            CHECK(braces_balanced(text), "the share object must be balanced: %s", text);
    {
        /* Balanced is not the same as parseable; the project's own parser decides. */
        char reason[192];
        CHECK(render_parses(text, reason, sizeof(reason)),
              "and it parses as JSON: %s\n    %s", reason, text);
    }
            CHECK(strstr(text, "\"before_circulating_shares\":") != NULL, "the share block");
            CHECK(strstr(text, "\"after_total_shares\":") != NULL, "the after shares");
            CHECK(strstr(text, "\"float32_values\":[") != NULL, "both readings travel");
        }
    }
    CHECK(checked_dividend && checked_shares, "both record shapes were rendered");

    tdx_capital_tally_init(&tally);
    tdx_capital_tally_add(&tally, records, count);
    CHECK(tally.record_count == count, "the tally counts every record");
    CHECK(tally.securities == 1, "one security was tallied");
    CHECK(tally.earliest_date == 19900301, "the tally keeps the earliest date, got %d",
          tally.earliest_date);
    CHECK(tally.category_counts[1] == 33, "33 ex-rights records, got %zu",
          tally.category_counts[1]);
    CHECK(tally.category_counts[5] == 32, "32 share-capital records, got %zu",
          tally.category_counts[5]);
    /* A second security accumulates rather than replaces. */
    tdx_capital_tally_add(&tally, records, count);
    CHECK(tally.securities == 2, "the security count accumulates, got %zu", tally.securities);
    CHECK(tally.record_count == count * 2, "the record count accumulates");

    tdx_buf_clear(&line);
    CHECK(tdx_capital_format_summary(&line, &tally, 1, "1.2.3.4:7709", &error) == TDX_OK,
          "summary render: %s", error.message);
    copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
    memcpy(text, line.data, copy);
    text[copy] = '\0';
    CHECK(braces_balanced(text), "the summary must be balanced: %s", text);
    {
        /* Balanced is not the same as parseable; the project's own parser decides. */
        char reason[192];
        CHECK(render_parses(text, reason, sizeof(reason)),
              "and it parses as JSON: %s\n    %s", reason, text);
    }
    CHECK(strstr(text, "\"type\":\"capital_summary\"") != NULL, "the summary type");
    CHECK(strstr(text, "\"category_counts\":{") != NULL, "the category histogram");
    CHECK(strstr(text, "\"ex_rights_dividend\":66") != NULL, "the accumulated category count");
    CHECK(strstr(text, "\"earliest_date\":\"1990-03-01\"") != NULL, "the earliest date renders");

    tdx_buf_clear(&line);
    tdx_capital_tally_init(&tally);
    CHECK(tdx_capital_format_summary(&line, &tally, 0, NULL, &error) == TDX_OK,
          "empty summary render: %s", error.message);
    copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
    memcpy(text, line.data, copy);
    text[copy] = '\0';
    CHECK(braces_balanced(text), "an empty summary must be balanced: %s", text);
    {
        /* Balanced is not the same as parseable; the project's own parser decides. */
        char reason[192];
        CHECK(render_parses(text, reason, sizeof(reason)),
              "and it parses as JSON: %s\n    %s", reason, text);
    }
    CHECK(strstr(text, "\"endpoint\":null") != NULL, "a missing endpoint renders null");
    CHECK(strstr(text, "\"record_count\":0") != NULL, "a zero record count");
    CHECK(strstr(text, "\"earliest_date\":null") != NULL, "no dates renders null");

    tdx_buf_free(&line);
}

int main(void) {
    test_category_keys();
    test_request();
    test_parse_captured();
    test_parse_rejects();
    test_json_rendering();

    if (failures) {
        printf("%d capital check(s) failed\n", failures);
        return 1;
    }
    printf("capital checks passed\n");
    return 0;
}
