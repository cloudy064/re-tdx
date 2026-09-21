/* test_finance.c - 0x0010 batch fundamental data.
 *
 * What this test does and does not claim is deliberate.  The captured reply is a
 * live six-record one, and the assertions below cover only what external facts
 * confirm:
 *
 *   * the six listing dates match the real listings of the six companies;
 *   * Kweichow Moutai's total share count and ICBC's H-share count come out right,
 *     which is what pins the 10000 share scale;
 *   * the balance-sheet envelope holds (assets >= net assets).
 *
 * The share-class slots after total_shares are NOT asserted, because they do not
 * survive the same treatment: ICBC reports 4.27e12 "legal person" shares against
 * 3.56e11 total, and three large banks all report 19,660,000 "national" shares.
 * Those fields are bound exactly as the C++ reference binds them, so the gap is
 * the reference's, not this port's, and inventing a different binding here would
 * be worse than reporting it. */
#include <stdio.h>
#include <string.h>

#include "tdx_finance.h"
#include "tdx_finance_json.h"
#include "finance_fixtures.h"

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

static tdx_code make_code(int market, const char *code) {
    tdx_code result;
    memset(&result, 0, sizeof(result));
    result.market_id = market;
    memcpy(result.code, code, 6);
    result.code[6] = '\0';
    return result;
}

/* --- request ---------------------------------------------------------- */

static void test_request(void) {
    tdx_code codes[3];
    tdx_buf request;
    tdx_error error;
    error.message[0] = '\0';
    tdx_buf_init(&request);

    codes[0] = make_code(0, "000001");
    codes[1] = make_code(1, "600000");
    codes[2] = make_code(2, "430047");
    CHECK(tdx_finance_build_request(codes, 3, &request, &error) == TDX_OK, "build: %s",
          error.message);
    CHECK(request.len == TDX_FINANCE_REQUEST_FIXED + 3 * TDX_FINANCE_CODE_STRIDE,
          "the body is %zu bytes, expected %u", request.len,
          (unsigned)(TDX_FINANCE_REQUEST_FIXED + 3 * TDX_FINANCE_CODE_STRIDE));
    if (request.len == 23) {
        CHECK(tdx_u16le(request.data) == 3, "the count leads the body");
        CHECK(request.data[2] == 0 && memcmp(request.data + 3, "000001", 6) == 0,
              "the first security is market + code from +2");
        CHECK(request.data[9] == 1 && memcmp(request.data + 10, "600000", 6) == 0,
              "the second security follows at +9");
        CHECK(request.data[16] == 2 && memcmp(request.data + 17, "430047", 6) == 0,
              "the third carries market 2");
    }
    CHECK(tdx_finance_build_request(codes, 0, &request, &error) == TDX_ERR,
          "an empty batch must be refused");
    CHECK(tdx_finance_build_request(NULL, 1, &request, &error) == TDX_ERR,
          "a null list must be refused");
    codes[0] = make_code(3, "000001");
    CHECK(tdx_finance_build_request(codes, 1, &request, &error) == TDX_ERR,
          "market 3 must be refused");
    codes[0] = make_code(0, "00000a");
    CHECK(tdx_finance_build_request(codes, 1, &request, &error) == TDX_ERR,
          "a non-numeric code must be refused");
    tdx_buf_free(&request);
}

/* --- the captured reply ----------------------------------------------- */

static void test_parse_captured(void) {
    tdx_finance_record records[8];
    size_t count = 0;
    tdx_error error;
    error.message[0] = '\0';

    CHECK(tdx_finance_parse(finance_reply, sizeof(finance_reply), records, 8, &count, &error) ==
              TDX_OK,
          "parse: %s", error.message);
    CHECK(count == 6, "six records, got %zu", count);
    if (count != 6)
        return;

    /* Identity and the six real listing dates. */
    CHECK(records[0].security.market_id == 1 && strcmp(records[0].security.code, "600000") == 0,
          "the first record is SH600000");
    CHECK(records[0].listing_date == 19991110, "Pudong Bank listed 1999-11-10, got %d",
          records[0].listing_date);
    CHECK(records[1].listing_date == 19910403, "Ping An Bank listed 1991-04-03, got %d",
          records[1].listing_date);
    CHECK(records[2].listing_date == 20061027, "ICBC listed 2006-10-27, got %d",
          records[2].listing_date);
    CHECK(records[3].listing_date == 19910129, "Vanke A listed 1991-01-29, got %d",
          records[3].listing_date);
    CHECK(records[4].listing_date == 20010827, "Moutai listed 2001-08-27, got %d",
          records[4].listing_date);
    CHECK(records[5].listing_date == 20180611, "CATL listed 2018-06-11, got %d",
          records[5].listing_date);

    /* The updated date is a recent report date, after every listing date. */
    {
        size_t index;
        for (index = 0; index < count; ++index) {
            CHECK(records[index].updated_date > records[index].listing_date,
                  "record %zu was updated (%d) after it was listed (%d)", index,
                  records[index].updated_date, records[index].listing_date);
            CHECK(records[index].updated_date / 10000 == 2026,
                  "record %zu updated in 2026, got %d", index, records[index].updated_date);
        }
    }

    /* The share scale is pinned by two counts that are public knowledge.  The
     * windows are wide enough to allow a corporate action since the capture but
     * far too narrow to survive a wrong power of ten. */
    CHECK(records[4].total_shares > 1240000000.0 && records[4].total_shares < 1260000000.0,
          "Moutai has about 1.25bn shares, got %.0f", records[4].total_shares);
    CHECK(records[2].h_shares > 86790000000.0 && records[2].h_shares < 86800000000.0,
          "ICBC has about 86.79bn H shares, got %.0f", records[2].h_shares);
    CHECK(records[2].b_shares == 0.0, "ICBC has no B shares, got %.0f", records[2].b_shares);
    CHECK(records[0].circulating_shares == records[0].total_shares,
          "Pudong Bank is fully circulating in this record");

    /* Envelopes that any correct binding must satisfy. */
    {
        size_t index;
        for (index = 0; index < count; ++index) {
            CHECK(records[index].total_assets >= records[index].net_assets,
                  "record %zu total assets >= net assets", index);
            CHECK(records[index].circulating_shares <= records[index].total_shares * 1.000001,
                  "record %zu circulating %.0f <= total %.0f", index,
                  records[index].circulating_shares, records[index].total_shares);
            CHECK(records[index].net_assets_per_share > 0.0,
                  "record %zu net assets per share %.4f", index,
                  records[index].net_assets_per_share);
        }
    }
    /* Moutai's per-share figures are the kind of number that only a correct
     * offset and scale can produce. */
    CHECK(records[4].net_assets_per_share > 150.0 && records[4].net_assets_per_share < 250.0,
          "Moutai net assets per share is around 200, got %.4f",
          records[4].net_assets_per_share);
    CHECK(records[4].eps > 10.0 && records[4].eps < 100.0,
          "Moutai EPS is tens of yuan, got %.4f", records[4].eps);
}

static void test_parse_rejects(void) {
    tdx_finance_record records[8];
    size_t count = 0;
    tdx_error error;
    uint8_t body[160];
    error.message[0] = '\0';

    CHECK(tdx_finance_parse(finance_reply, 1, records, 8, &count, &error) == TDX_ERR,
          "a one-byte body must be refused");
    CHECK(tdx_finance_parse(finance_reply, sizeof(finance_reply) - 1, records, 8, &count,
                            &error) == TDX_ERR,
          "a body one byte short of count*143 must be refused");
    CHECK(strstr(error.message, "length mismatch") != NULL, "the error must say why: %s",
          error.message);
    CHECK(tdx_finance_parse(finance_reply, sizeof(finance_reply) + 143, records, 8, &count,
                            &error) == TDX_ERR,
          "a body one record too long must be refused");
    CHECK(tdx_finance_parse(finance_reply, sizeof(finance_reply), records, 2, &count, &error) ==
              TDX_ERR,
          "a reply that does not fit the output must be refused");

    /* A record whose market byte is not a market, and one whose code is not
     * digits. */
    memset(body, 0, sizeof(body));
    body[0] = 7;
    memcpy(body + 1, "000001", 6);
    error.message[0] = '\0';
    CHECK(tdx_finance_parse_record(body, TDX_FINANCE_RECORD_SIZE, &records[0], &error) == TDX_ERR,
          "market 7 must be refused");
    CHECK(strstr(error.message, "market") != NULL, "the error must name the field: %s",
          error.message);
    memset(body, 0, sizeof(body));
    body[0] = 0;
    memcpy(body + 1, "00000a", 6);
    error.message[0] = '\0';
    CHECK(tdx_finance_parse_record(body, TDX_FINANCE_RECORD_SIZE, &records[0], &error) == TDX_ERR,
          "a non-numeric code must be refused");

    /* A record of the wrong size is refused even when the outer length agreed. */
    CHECK(tdx_finance_parse_record(body, TDX_FINANCE_RECORD_SIZE - 1, &records[0], &error) ==
              TDX_ERR,
          "a 142-byte record must be refused");

    /* An impossible date. */
    memset(body, 0, sizeof(body));
    body[0] = 0;
    memcpy(body + 1, "000001", 6);
    body[7 + 12] = 0x63; /* listing date low byte, making 0x...63 a bad day */
    body[7 + 13] = 0x00;
    body[7 + 14] = 0x00;
    body[7 + 15] = 0x00; /* raw 0x63 = 99 -> year 0 */
    error.message[0] = '\0';
    CHECK(tdx_finance_parse_record(body, TDX_FINANCE_RECORD_SIZE, &records[0], &error) ==
              TDX_ERR,
          "a non-date must be refused");
    CHECK(strstr(error.message, "date") != NULL, "the error must name the field: %s",
          error.message);

    /* A non-finite float. */
    memset(body, 0, sizeof(body));
    body[0] = 0;
    memcpy(body + 1, "000001", 6);
    body[7 + 16] = 0x00;
    body[7 + 17] = 0x00;
    body[7 + 18] = 0xC0;
    body[7 + 19] = 0x7F; /* total_shares = NaN */
    error.message[0] = '\0';
    CHECK(tdx_finance_parse_record(body, TDX_FINANCE_RECORD_SIZE, &records[0], &error) ==
              TDX_ERR,
          "a NaN field must be refused");
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
    tdx_finance_record records[8];
    tdx_finance_tally tally;
    size_t count = 0;
    tdx_buf line;
    tdx_error error;
    char text[8192];
    size_t copy;

    error.message[0] = '\0';
    tdx_buf_init(&line);
    CHECK(tdx_finance_parse(finance_reply, sizeof(finance_reply), records, 8, &count, &error) ==
              TDX_OK,
          "parse: %s", error.message);

    CHECK(tdx_finance_format(&line, &records[0], &error) == TDX_OK, "render: %s", error.message);
    copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
    memcpy(text, line.data, copy);
    text[copy] = '\0';
    CHECK(braces_balanced(text), "the finance object must be balanced: %s", text);
    CHECK(strstr(text, "\"type\":\"finance\"") != NULL, "the type");
    CHECK(strstr(text, "\"security_id\":\"SH600000\"") != NULL, "the identity");
    CHECK(strstr(text, "\"listing_date\":\"1999-11-10\"") != NULL, "the listing date renders");
    CHECK(strstr(text, "\"shares\":{") != NULL, "the share group");
    CHECK(strstr(text, "\"balance_sheet_yuan\":{") != NULL, "the balance sheet group");
    CHECK(strstr(text, "\"income_statement_yuan\":{") != NULL, "the income group");
    CHECK(strstr(text, "\"cash_flow_yuan\":{") != NULL, "the cash-flow group");

    tdx_buf_clear(&line);
    tdx_finance_tally_init(&tally);
    tdx_finance_tally_add(&tally, records, count);
    CHECK(tally.record_count == 6, "the tally counts %zu records", tally.record_count);
    CHECK(tally.records_with_listing_date == 6, "every record carries a listing date");
    {
        double once = tally.total_shares_sum;
        tdx_finance_tally_add(&tally, records, count);
        CHECK(tally.record_count == 12, "a second batch accumulates, got %zu",
              tally.record_count);
        CHECK(tally.total_shares_sum > once * 1.99 && tally.total_shares_sum < once * 2.01,
              "the share total accumulates: %.0f then %.0f", once, tally.total_shares_sum);
    }

    tdx_buf_clear(&line);
    CHECK(tdx_finance_format_summary(&line, &tally, "1.2.3.4:7709", &error) == TDX_OK,
          "summary render: %s", error.message);
    copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
    memcpy(text, line.data, copy);
    text[copy] = '\0';
    CHECK(braces_balanced(text), "the summary must be balanced: %s", text);
    CHECK(strstr(text, "\"type\":\"finance_summary\"") != NULL, "the summary type");
    CHECK(strstr(text, "\"record_count\":12") != NULL, "the accumulated record count");
    CHECK(strstr(text, "\"records_with_listing_date\":12") != NULL, "the accumulated listing");

    tdx_buf_clear(&line);
    tdx_finance_tally_init(&tally);
    CHECK(tdx_finance_format_summary(&line, &tally, NULL, &error) == TDX_OK,
          "empty summary render: %s", error.message);
    copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
    memcpy(text, line.data, copy);
    text[copy] = '\0';
    CHECK(braces_balanced(text), "an empty summary must be balanced: %s", text);
    CHECK(strstr(text, "\"endpoint\":null") != NULL, "a missing endpoint renders null");
    CHECK(strstr(text, "\"record_count\":0") != NULL, "a zero record count");

    tdx_buf_free(&line);
}

int main(void) {
    test_request();
    test_parse_captured();
    test_parse_rejects();
    test_json_rendering();

    if (failures) {
        printf("%d finance check(s) failed\n", failures);
        return 1;
    }
    printf("finance checks passed\n");
    return 0;
}
