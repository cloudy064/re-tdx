/* test_limits.c - the 0x0452 special price-limit list.
 *
 * The fixture is a real 15-byte page holding ONE record, because that is what the
 * server answers: a request from index 0 comes back with a single row and the walk
 * advances by explicit index.  Getting that wrong is the bug this port actually
 * made - treating a one-record page as "the list ended" stops after the first row -
 * so the fixture deliberately preserves the real shape rather than a convenient
 * multi-record one.
 *
 * The parser still accepts `count` records, because the length rule is
 * 2 + count * 13 and the reference decodes a list.  That path is exercised with a
 * body built in this test from the captured record repeated, and it is labelled
 * synthetic so it is never mistaken for a capture.
 *
 * What the values mean was checked live, not assumed: for 13 consecutive records
 * the midpoint (up + down) / 2 equalled the previous close that 0x054C reported
 * for the same security at the same time - exactly for 11 of them and within half a
 * cent for the other two - and the implied band was about ten percent for every
 * one.  So these are real limit prices.  Why a security is on the list is NOT
 * claimed here, because the fields do not show it. */
#include <stdio.h>
#include <string.h>

#include "tdx_limits.h"
#include "limits_fixtures.h"

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

static void test_request(void) {
    tdx_buf request = {0};
    tdx_error error;
    error.message[0] = '\0';
    tdx_buf_init(&request);

    CHECK(tdx_limits_build_request(0, &request, &error) == TDX_OK, "build: %s", error.message);
    CHECK(request.len == TDX_LIMITS_REQUEST_SIZE, "the body is %zu bytes, expected %d",
          request.len, TDX_LIMITS_REQUEST_SIZE);
    if (request.len == TDX_LIMITS_REQUEST_SIZE) {
        size_t index;
        int all_zero = 1;
        CHECK(tdx_u16le(request.data) == 0, "the start index leads the body");
        for (index = 2; index < request.len; ++index)
            if (request.data[index] != 0)
                all_zero = 0;
        CHECK(all_zero, "the twelve bytes after the index are zero");
    }
    /* The index lives in a 16-bit field, so it is checked rather than truncated. */
    CHECK(tdx_limits_build_request(11, &request, &error) == TDX_OK, "index 11");
    CHECK(tdx_u16le(request.data) == 11, "the index travels little endian");
    CHECK(tdx_limits_build_request(65536, &request, &error) == TDX_ERR,
          "an index above the 16-bit field must be refused");
    CHECK(tdx_limits_build_request(0, NULL, &error) == TDX_ERR,
          "a null buffer must be refused");
    tdx_buf_free(&request);
}

static void test_parse_captured(void) {
    tdx_limit_record records[8];
    unsigned indices[8];
    size_t count = 0;
    tdx_error error;
    error.message[0] = '\0';

    CHECK(tdx_limits_parse(limits_page, sizeof(limits_page), 0, records, 8, &count, indices,
                           &error) == TDX_OK,
          "parse: %s", error.message);
    CHECK(count == LIMITS_PAGE_COUNT, "the page holds %d record, got %zu", LIMITS_PAGE_COUNT,
          count);
    if (count != LIMITS_PAGE_COUNT)
        return;

    /* SZ000010, the code arriving as the NUMBER 10 and rendered as six digits. */
    CHECK(records[0].security.market_id == 0, "market 0, got %d", records[0].security.market_id);
    CHECK(records[0].code_number == 10, "the code number is 10, got %u",
          (unsigned)records[0].code_number);
    CHECK(strcmp(records[0].security.code, "000010") == 0, "rendered as 000010, got %s",
          records[0].security.code);
    CHECK(indices[0] == 0, "the record is numbered 0, got %u", indices[0]);

    CHECK(records[0].limit_up > 1.739 && records[0].limit_up < 1.741,
          "limit up is 1.74, got %.4f", records[0].limit_up);
    CHECK(records[0].limit_down > 1.419 && records[0].limit_down < 1.421,
          "limit down is 1.42, got %.4f", records[0].limit_down);
    CHECK(records[0].limit_up > records[0].limit_down, "up is above down");
    /* The midpoint was the previous close on live data, which is why the pair is
     * readable as a band around it rather than as two unrelated numbers. */
    CHECK((records[0].limit_up + records[0].limit_down) / 2.0 > 1.579 &&
              (records[0].limit_up + records[0].limit_down) / 2.0 < 1.581,
          "the midpoint is 1.58, got %.4f",
          (records[0].limit_up + records[0].limit_down) / 2.0);
}

static void test_parse_list(void) {
    /* SYNTHETIC: the captured record repeated three times behind a matching
     * header.  The server answers one row per request, so no such page exists to
     * capture; this only exercises the parser's list form. */
    uint8_t body[2 + 3 * TDX_LIMITS_RECORD_SIZE];
    tdx_limit_record records[8];
    unsigned indices[8];
    size_t count = 0;
    tdx_error error;
    size_t index;
    error.message[0] = '\0';

    body[0] = 3;
    body[1] = 0;
    for (index = 0; index < 3; ++index) {
        memcpy(body + 2 + index * TDX_LIMITS_RECORD_SIZE, limits_page + 2,
               TDX_LIMITS_RECORD_SIZE);
        /* Give each copy a distinct code so the loop is visibly per record. */
        body[2 + index * TDX_LIMITS_RECORD_SIZE + 1] = (uint8_t)(20 + index);
    }
    CHECK(tdx_limits_parse(body, sizeof(body), 5, records, 8, &count, indices, &error) == TDX_OK,
          "parse: %s", error.message);
    CHECK(count == 3, "three records, got %zu", count);
    if (count == 3) {
        CHECK(strcmp(records[0].security.code, "000020") == 0, "the first code");
        CHECK(strcmp(records[2].security.code, "000022") == 0, "the third code");
        /* The page's start index is carried into every record's number. */
        CHECK(indices[0] == 5 && indices[1] == 6 && indices[2] == 7,
              "the numbering continues from the page start: %u %u %u", indices[0], indices[1],
              indices[2]);
        CHECK(records[1].limit_up > 1.739 && records[1].limit_up < 1.741,
              "the prices repeat as constructed");
    }
}

static void test_parse_rejects(void) {
    tdx_limit_record records[8];
    size_t count = 0;
    tdx_error error;
    uint8_t body[2 + TDX_LIMITS_RECORD_SIZE];
    error.message[0] = '\0';

    CHECK(tdx_limits_parse(limits_page, 1, 0, records, 8, &count, NULL, &error) == TDX_ERR,
          "a one-byte body must be refused");
    CHECK(tdx_limits_parse(limits_page, sizeof(limits_page) - 1, 0, records, 8, &count, NULL,
                           &error) == TDX_ERR,
          "a body one byte short must be refused");
    CHECK(strstr(error.message, "length mismatch") != NULL, "the error must say why: %s",
          error.message);
    CHECK(tdx_limits_parse(limits_page, sizeof(limits_page) + TDX_LIMITS_RECORD_SIZE, 0, records,
                           8, &count, NULL, &error) == TDX_ERR,
          "a body one record too long must be refused");
    CHECK(tdx_limits_parse(limits_page, sizeof(limits_page), 0, records, 0, &count, NULL,
                           &error) == TDX_ERR,
          "a page that does not fit the output must be refused");
    CHECK(tdx_limits_parse(NULL, sizeof(limits_page), 0, records, 8, &count, NULL, &error) ==
              TDX_ERR,
          "a null body must be refused");

    /* A market that is not a market. */
    memcpy(body, limits_page, sizeof(body));
    body[2] = 7;
    error.message[0] = '\0';
    CHECK(tdx_limits_parse(body, sizeof(body), 0, records, 8, &count, NULL, &error) == TDX_ERR,
          "market 7 must be refused");
    CHECK(strstr(error.message, "market") != NULL, "the error must name the field: %s",
          error.message);

    /* A code number that cannot be six digits. */
    memcpy(body, limits_page, sizeof(body));
    body[3] = 0x40;
    body[4] = 0x42;
    body[5] = 0x0F; /* 1,000,000 */
    error.message[0] = '\0';
    CHECK(tdx_limits_parse(body, sizeof(body), 0, records, 8, &count, NULL, &error) == TDX_ERR,
          "a seven-digit code number must be refused");
    CHECK(strstr(error.message, "six digits") != NULL, "the error must say why: %s",
          error.message);

    /* An inverted band.  A record is market at +0, code number at +1..4, up at
     * +5..8 and down at +9..12, so behind the count these sit at +2, +3..6, +7..10
     * and +11..14. */
    memcpy(body, limits_page, sizeof(body));
    body[7] = 0x00;
    body[8] = 0x00;
    body[9] = 0x00;
    body[10] = 0x00; /* up = 0.0, below the down price */
    error.message[0] = '\0';
    CHECK(tdx_limits_parse(body, sizeof(body), 0, records, 8, &count, NULL, &error) == TDX_ERR,
          "an up price below the down price must be refused");
    CHECK(strstr(error.message, "below") != NULL, "the error must say why: %s", error.message);

    /* A non-finite price. */
    memcpy(body, limits_page, sizeof(body));
    body[7] = 0x00;
    body[8] = 0x00;
    body[9] = 0xC0;
    body[10] = 0x7F; /* up = NaN */
    error.message[0] = '\0';
    CHECK(tdx_limits_parse(body, sizeof(body), 0, records, 8, &count, NULL, &error) == TDX_ERR,
          "a NaN price must be refused");
    CHECK(strstr(error.message, "non-finite") != NULL, "the error must say why: %s",
          error.message);
}

int main(void) {
    test_request();
    test_parse_captured();
    test_parse_list();
    test_parse_rejects();

    if (failures) {
        printf("%d limits check(s) failed\n", failures);
        return 1;
    }
    printf("limits checks passed\n");
    return 0;
}
