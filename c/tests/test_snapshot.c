/* test_snapshot.c - 0x054C whole-universe L1 snapshot.
 *
 * The fixture is a live three-record reply for sz000623, sz000001 and sh600000,
 * captured together with 0x0547 answers for the same securities.  Eleven of the
 * twelve shared fields matched exactly between the two independent commands; the
 * twelfth, open_amount, differs by a factor of ten in resolution because the two
 * commands really do ship it at different granularity - the C++ decoder itself
 * scales it by 100 on this path and by 10 on the depth path.  That is asserted
 * below so the difference is recorded rather than mistaken for a bug. */
#include <stdio.h>
#include <string.h>

#include "tdx_snapshot.h"
#include "tdx_snapshot_json.h"
#include "snapshot_fixtures.h"

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

/* --- fund iopv predicate ---------------------------------------------- */

static void test_fund_iopv_predicate(void) {
    tdx_code code;

    code = make_code(0, "158000");
    CHECK(tdx_snapshot_is_fund_iopv(&code) == 1, "a Shenzhen 158 fund has a net value");
    code = make_code(0, "159915");
    CHECK(tdx_snapshot_is_fund_iopv(&code) == 1, "a Shenzhen 159 fund has a net value");
    code = make_code(0, "000623");
    CHECK(tdx_snapshot_is_fund_iopv(&code) == 0, "an ordinary Shenzhen stock does not");

    /* Shanghai needs both a listed prefix and a trailing zero. */
    code = make_code(1, "510300");
    CHECK(tdx_snapshot_is_fund_iopv(&code) == 1, "a Shanghai 510 fund has a net value");
    code = make_code(1, "588000");
    CHECK(tdx_snapshot_is_fund_iopv(&code) == 1, "a Shanghai 588 fund has a net value");
    code = make_code(1, "510301");
    CHECK(tdx_snapshot_is_fund_iopv(&code) == 0, "a Shanghai 510 code not ending in 0 does not");
    code = make_code(1, "600000");
    CHECK(tdx_snapshot_is_fund_iopv(&code) == 0, "an ordinary Shanghai stock does not");
    code = make_code(1, "519999");
    CHECK(tdx_snapshot_is_fund_iopv(&code) == 0, "519 is not in the fund prefix list");
    code = make_code(2, "430047");
    CHECK(tdx_snapshot_is_fund_iopv(&code) == 0, "Beijing never carries a fund net value");
    CHECK(tdx_snapshot_is_fund_iopv(NULL) == 0, "a null security is not a fund");
}

/* --- request ---------------------------------------------------------- */

static void test_request(void) {
    tdx_code codes[3];
    tdx_buf request;
    tdx_error error;
    error.message[0] = '\0';
    tdx_buf_init(&request);

    codes[0] = make_code(0, "000623");
    codes[1] = make_code(0, "000001");
    codes[2] = make_code(1, "600000");
    CHECK(tdx_snapshot_build_request(codes, 3, &request, &error) == TDX_OK, "build: %s",
          error.message);
    CHECK(request.len == TDX_SNAPSHOT_REQUEST_FIXED + 3 * TDX_SNAPSHOT_CODE_STRIDE,
          "the body is %zu bytes, expected %u", request.len,
          (unsigned)(TDX_SNAPSHOT_REQUEST_FIXED + 3 * TDX_SNAPSHOT_CODE_STRIDE));
    if (request.len == 31) {
        size_t index;
        int zero_head = 1;
        CHECK(request.data[0] == 5, "the fixed marker at +0 is 5, got %u",
              (unsigned)request.data[0]);
        for (index = 1; index <= 7; ++index)
            if (request.data[index] != 0)
                zero_head = 0;
        CHECK(zero_head, "the seven bytes after the marker must be zero");
        CHECK(tdx_u16le(request.data + 8) == 3, "the count sits at +8");
        CHECK(request.data[10] == 0 && memcmp(request.data + 11, "000623", 6) == 0,
              "the first security is market + code from +10");
        CHECK(request.data[17] == 0 && memcmp(request.data + 18, "000001", 6) == 0,
              "the second security follows at +17");
        CHECK(request.data[24] == 1 && memcmp(request.data + 25, "600000", 6) == 0,
              "the third security carries market 1");
    }

    CHECK(tdx_snapshot_build_request(codes, 0, &request, &error) == TDX_ERR,
          "an empty batch must be refused");
    CHECK(tdx_snapshot_build_request(NULL, 1, &request, &error) == TDX_ERR,
          "a null list must be refused");
    codes[0] = make_code(3, "000623");
    CHECK(tdx_snapshot_build_request(codes, 1, &request, &error) == TDX_ERR,
          "market 3 must be refused");
    codes[0] = make_code(0, "00062a");
    CHECK(tdx_snapshot_build_request(codes, 1, &request, &error) == TDX_ERR,
          "a non-numeric code must be refused");
    tdx_buf_free(&request);
}

/* --- the captured reply ----------------------------------------------- */

static void test_parse_captured(void) {
    tdx_snapshot records[8];
    size_t count = 0;
    uint16_t header_raw = 0;
    tdx_error error;
    error.message[0] = '\0';

    CHECK(tdx_snapshot_parse(snapshot_reply, sizeof(snapshot_reply), 3, records, 8, &count,
                             &header_raw, &error) == TDX_OK,
          "parse: %s", error.message);
    CHECK(count == 3, "three records, got %zu", count);
    CHECK(header_raw == 0, "the uninterpreted header word is zero here, got %u",
          (unsigned)header_raw);
    if (count != 3)
        return;

    CHECK(records[0].security.market_id == 0 &&
              strcmp(records[0].security.code, "000623") == 0,
          "the first record is SZ000623");
    CHECK(records[0].active == 2783, "active %u", (unsigned)records[0].active);
    CHECK(records[0].last == 17.75, "last %.4f", records[0].last);
    CHECK(records[0].previous == 17.56, "previous %.4f", records[0].previous);
    CHECK(records[0].open == 17.55, "open %.4f", records[0].open);
    CHECK(records[0].high == 17.78, "high %.4f", records[0].high);
    CHECK(records[0].low == 17.53, "low %.4f", records[0].low);
    CHECK(records[0].total_hand == 87389, "total hand %lld",
          (long long)records[0].total_hand);
    CHECK(records[0].current_hand == 1097, "current hand %lld",
          (long long)records[0].current_hand);
    CHECK(records[0].inside == 37816 && records[0].outside == 49573,
          "dishes %lld/%lld", (long long)records[0].inside, (long long)records[0].outside);
    CHECK(records[0].amount > 154285183.0 && records[0].amount < 154285185.0,
          "amount %.2f", records[0].amount);
    CHECK(records[0].auction_imbalance_hand == 0, "imbalance %lld",
          (long long)records[0].auction_imbalance_hand);
    /* The snapshot ships open_amount ten times coarser than the depth command:
     * its raw is 1/10 of the depth raw and the scale on top is 10x bigger, so
     * both decoders land on the same yuan figure this way. */
    CHECK(records[0].open_amount > 1935799.0 && records[0].open_amount < 1935801.0,
          "open amount %.2f", records[0].open_amount);
    CHECK(records[0].has_fund_iopv == 0, "an ordinary stock carries no fund net value");
    CHECK(records[0].tail_size > 0, "the record leaves %zu bytes unread, which is reported",
          records[0].tail_size);

    CHECK(records[1].security.market_id == 0 && strcmp(records[1].security.code, "000001") == 0,
          "the second record is SZ000001");
    CHECK(records[1].last == 11.73, "second last %.4f", records[1].last);
    CHECK(records[2].security.market_id == 1 && strcmp(records[2].security.code, "600000") == 0,
          "the third record is SH600000");
    CHECK(records[2].last == 9.01, "third last %.4f", records[2].last);
    CHECK(records[2].low == 8.91, "third low %.4f", records[2].low);

    /* Every record must satisfy its own envelope. */
    {
        size_t index;
        for (index = 0; index < count; ++index) {
            CHECK(records[index].high >= records[index].low, "record %zu high >= low", index);
            CHECK(records[index].last >= records[index].low &&
                      records[index].last <= records[index].high,
                  "record %zu last %.4f inside [%.4f, %.4f]", index, records[index].last,
                  records[index].low, records[index].high);
            CHECK(records[index].total_hand >= records[index].current_hand,
                  "record %zu cumulative >= current", index);
        }
    }
}

/* --- record boundary recovery ----------------------------------------- */

static void test_split_records(void) {
    size_t starts[8];
    size_t found = 0;
    tdx_error error;
    error.message[0] = '\0';

    /* The three real boundaries sit at 0, 105 and 210 in the record area. */
    CHECK(tdx_snapshot_split_records(snapshot_reply + 4, sizeof(snapshot_reply) - 4, 3, starts,
                                     8, &found, &error) == TDX_OK,
          "split: %s", error.message);
    CHECK(found == 3, "three boundaries, got %zu", found);
    if (found == 3) {
        CHECK(starts[0] == 0, "the first boundary is at 0, got %zu", starts[0]);
        CHECK(starts[1] > starts[0] && starts[2] > starts[1], "boundaries must ascend");
        CHECK(starts[2] < sizeof(snapshot_reply) - 4, "the last boundary is inside the payload");
    }

    /* Declaring a different count than the payload holds must fail. */
    CHECK(tdx_snapshot_split_records(snapshot_reply + 4, sizeof(snapshot_reply) - 4, 2, starts,
                                     8, &found, &error) == TDX_ERR,
          "a count that disagrees with the payload must be refused");
    CHECK(strstr(error.message, "cannot identify") != NULL, "the error must say why: %s",
          error.message);

    /* A payload that does not begin with a record. */
    CHECK(tdx_snapshot_split_records(snapshot_reply + 5, sizeof(snapshot_reply) - 5, 3, starts,
                                     8, &found, &error) == TDX_ERR,
          "a shifted payload must be refused");

    /* Zero records is legal and yields no boundaries. */
    CHECK(tdx_snapshot_split_records(snapshot_reply + 4, sizeof(snapshot_reply) - 4, 0, starts,
                                     8, &found, &error) == TDX_OK,
          "a zero count needs no boundaries");
    CHECK(found == 0, "no boundaries for a zero count, got %zu", found);
}

static void test_parse_rejects(void) {
    tdx_snapshot records[8];
    size_t count = 0;
    tdx_error error;
    uint8_t body[64];
    error.message[0] = '\0';

    CHECK(tdx_snapshot_parse(snapshot_reply, 3, 3, records, 8, &count, NULL, &error) == TDX_ERR,
          "a three-byte body must be refused");
    CHECK(tdx_snapshot_parse(snapshot_reply, sizeof(snapshot_reply), 2, records, 8, &count, NULL,
                             &error) == TDX_ERR,
          "a reply with more records than were requested must be refused");
    CHECK(tdx_snapshot_parse(snapshot_reply, sizeof(snapshot_reply), 3, records, 2, &count, NULL,
                             &error) == TDX_ERR,
          "a reply that does not fit the output must be refused");

    /* A record whose market byte is not 0..2. */
    memset(body, 0, sizeof(body));
    body[0] = 9;
    memcpy(body + 1, "000623", 6);
    tdx_snapshot_parse_record(body, 9, &records[0], &error);
    CHECK(tdx_snapshot_parse_record(body, 9, &records[0], &error) == TDX_ERR,
          "market 9 must be refused");
    CHECK(strstr(error.message, "market") != NULL, "the error must name the field: %s",
          error.message);

    /* A record whose code is not six digits. */
    memset(body, 0, sizeof(body));
    body[0] = 0;
    memcpy(body + 1, "00062a", 6);
    error.message[0] = '\0';
    CHECK(tdx_snapshot_parse_record(body, 9, &records[0], &error) == TDX_ERR,
          "a non-numeric code must be refused");
    CHECK(strstr(error.message, "six digits") != NULL, "the error must say why: %s",
          error.message);

    /* A record header that is too short. */
    CHECK(tdx_snapshot_parse_record(body, 8, &records[0], &error) == TDX_ERR,
          "an eight-byte record must be refused");

    /* A truncated real record: the varints run out. */
    CHECK(tdx_snapshot_parse_record(snapshot_reply + 4, 12, &records[0], &error) == TDX_ERR,
          "a truncated record must be refused");
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
    tdx_snapshot records[8];
    tdx_snapshot_tally tally;
    size_t count = 0;
    tdx_buf line;
    tdx_error error;
    char text[4096];
    size_t copy;

    error.message[0] = '\0';
    tdx_buf_init(&line);
    CHECK(tdx_snapshot_parse(snapshot_reply, sizeof(snapshot_reply), 3, records, 8, &count, NULL,
                             &error) == TDX_OK,
          "parse: %s", error.message);

    CHECK(tdx_snapshot_format(&line, &records[0], &error) == TDX_OK, "render: %s", error.message);
    copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
    memcpy(text, line.data, copy);
    text[copy] = '\0';
    CHECK(braces_balanced(text), "the snapshot object must be balanced: %s", text);
    CHECK(strstr(text, "\"type\":\"snapshot\"") != NULL, "the type");
    CHECK(strstr(text, "\"security_id\":\"SZ000623\"") != NULL, "the identity");
    CHECK(strstr(text, "\"last_price\":17.750000") != NULL, "the last price");
    CHECK(strstr(text, "\"total_hand\":87389") != NULL, "the cumulative volume");
    CHECK(strstr(text, "\"fund_iopv\":null") != NULL, "an empty net value renders null");

    /* The tally is what a multi-batch run can keep, and adding the same records
     * twice must double every total rather than replace it. */
    tdx_snapshot_tally_init(&tally);
    tdx_snapshot_tally_add(&tally, records, count);
    CHECK(tally.record_count == 3, "the tally counts %zu records", tally.record_count);
    CHECK(tally.records_with_tail == 3, "every record reported a tail");
    CHECK(tally.records_with_fund_iopv == 0, "none of these is a fund");
    {
        double once = tally.amount_sum;
        double hands = tally.total_hand_sum;
        tdx_snapshot_tally_add(&tally, records, count);
        CHECK(tally.record_count == 6, "a second batch accumulates, got %zu", tally.record_count);
        CHECK(tally.amount_sum > once * 1.99 && tally.amount_sum < once * 2.01,
              "the amount accumulates: %.2f then %.2f", once, tally.amount_sum);
        CHECK(tally.total_hand_sum > hands * 1.99 && tally.total_hand_sum < hands * 2.01,
              "the volume accumulates");
    }

    tdx_buf_clear(&line);
    CHECK(tdx_snapshot_format_summary(&line, &tally, 3, "1.2.3.4:7709", &error) == TDX_OK,
          "summary render: %s", error.message);
    copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
    memcpy(text, line.data, copy);
    text[copy] = '\0';
    CHECK(braces_balanced(text), "the summary must be balanced: %s", text);
    CHECK(strstr(text, "\"type\":\"snapshot_summary\"") != NULL, "the summary type");
    CHECK(strstr(text, "\"requested\":3") != NULL, "the requested count");
    CHECK(strstr(text, "\"record_count\":6") != NULL, "the accumulated record count");
    CHECK(strstr(text, "\"records_with_tail\":6") != NULL, "the accumulated tail count");

    tdx_buf_clear(&line);
    tdx_snapshot_tally_init(&tally);
    CHECK(tdx_snapshot_format_summary(&line, &tally, 0, NULL, &error) == TDX_OK,
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
    test_fund_iopv_predicate();
    test_request();
    test_parse_captured();
    test_split_records();
    test_parse_rejects();
    test_json_rendering();

    if (failures) {
        printf("%d snapshot check(s) failed\n", failures);
        return 1;
    }
    printf("snapshot checks passed\n");
    return 0;
}
