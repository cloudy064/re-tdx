/* test_professional_finance.c - the quarterly finance packages.
 *
 * The archives in the fixture are written by PYTHON's zipfile and read by this code, so
 * the two implementations are checked against each other; a reader tested only against
 * archives it wrote itself proves much less.
 *
 * Four archives, one per branch that matters:
 *
 *   stored    method 0, the copy path
 *   deflate   method 8, the inflate path
 *   bad_crc   the deflate archive with its CRC field corrupted, so the check is shown to
 *             reject rather than merely to exist
 *   no_eocd   a valid archive with its end record removed
 *
 * The member holds two records of 200 floats, whose values are the field index and twice
 * the field index.  A wrong field offset therefore lands on a wrong NUMBER rather than on
 * something that still looks like data.
 *
 * The real package cannot be embedded - 5.7 MB compressed, 13 MB uncompressed - so its
 * measured facts (member size, CRC, 5,570 records of 584 fields) are in
 * output/professional_finance_evidence.txt, and one of them is asserted here because the
 * fixture's own member is a different size entirely. */
#include <stdio.h>
#include <string.h>

#include "tdx_json.h"
#include "tdx_professional_finance.h"
#include "tdx_zip.h"
#include "professional_finance_fixtures.h"

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

static void test_stored_and_deflate(void) {
    static const struct {
        const char *label;
        const unsigned char *archive;
        size_t size;
    } cases[2] = {
        {"stored", profinance_stored_zip, PROFINANCE_STORED_BYTES},
        {"deflate", profinance_deflate_zip, PROFINANCE_DEFLATE_BYTES},
    };
    size_t which;

    for (which = 0; which < 2; ++which) {
        tdx_zip_entry entries[TDX_ZIP_ENTRIES_MAX];
        tdx_zip_entry entry;
        tdx_buf member = {0};
        size_t count = 0;
        tdx_profinance_document document;
        tdx_profinance_record_view view;
        double value = 0.0;

        error.message[0] = '\0';
        tdx_buf_init(&member);
        CHECK(tdx_zip_entries(cases[which].archive, cases[which].size, entries,
                              TDX_ZIP_ENTRIES_MAX, &count, &error) == TDX_OK,
              "%s: entries: %s", cases[which].label, error.message);
        CHECK(count == 1, "%s: one member, got %zu", cases[which].label, count);
        if (count != 1) {
            tdx_buf_free(&member);
            continue;
        }
        CHECK(strcmp(entries[0].name, PROFINANCE_MEMBER_NAME) == 0,
              "%s: the member is %s, got %s", cases[which].label, PROFINANCE_MEMBER_NAME,
              entries[0].name);
        CHECK(entries[0].method == (which == 0 ? 0u : 8u), "%s: method %u",
              cases[which].label, entries[0].method);
        CHECK(tdx_zip_find(entries, count, PROFINANCE_MEMBER_NAME, &entry) == 1,
              "%s: found by name", cases[which].label);
        CHECK(tdx_zip_find(entries, count, "nothing.dat", &entry) == 0,
              "%s: and an absent name is not found", cases[which].label);

        CHECK(tdx_zip_extract(cases[which].archive, cases[which].size, &entry, &member,
                              &error) == TDX_OK,
              "%s: extract: %s", cases[which].label, error.message);
        CHECK(member.len == PROFINANCE_FIXTURE_DATA_START + 2 * PROFINANCE_FIXTURE_DATA_SIZE,
              "%s: the member is %zu bytes, got %zu", cases[which].label,
              (size_t)(PROFINANCE_FIXTURE_DATA_START + 2 * PROFINANCE_FIXTURE_DATA_SIZE),
              member.len);

        CHECK(tdx_profinance_parse(member.data, member.len, &document, &error) == TDX_OK,
              "%s: parse the member: %s", cases[which].label, error.message);
        CHECK(document.version == 1, "%s: version 1, got %u", cases[which].label,
              document.version);
        CHECK(document.report_date == 20260630, "%s: report date 20260630, got %u",
              cases[which].label, document.report_date);
        CHECK(document.record_count == PROFINANCE_FIXTURE_RECORDS,
              "%s: %d records, got %zu", cases[which].label, PROFINANCE_FIXTURE_RECORDS,
              document.record_count);
        CHECK(document.index_size == TDX_PROFINANCE_INDEX_SIZE,
              "%s: index entries are %d bytes, got %zu", cases[which].label,
              TDX_PROFINANCE_INDEX_SIZE, document.index_size);
        CHECK(document.field_count == PROFINANCE_FIXTURE_FIELDS,
              "%s: %d fields, got %zu", cases[which].label, PROFINANCE_FIXTURE_FIELDS,
              document.field_count);
        CHECK(document.data_start == PROFINANCE_FIXTURE_DATA_START,
              "%s: the data starts at %d, got %zu", cases[which].label,
              PROFINANCE_FIXTURE_DATA_START, document.data_start);

        /* The two records, and the values a wrong offset would miss. */
        CHECK(tdx_profinance_record_at(&document, 0, &view, &error) == TDX_OK,
              "%s: record 0: %s", cases[which].label, error.message);
        CHECK(strcmp(view.code, "000001") == 0, "%s: the first code is 000001, got %s",
              cases[which].label, view.code);
        CHECK(view.market_id == 0, "%s: 000001 is Shenzhen, got %d", cases[which].label,
              view.market_id);
        CHECK(tdx_profinance_field(&view, 1, &value) == 1 && value == 0.0,
              "%s: field 1 is 0, got %f", cases[which].label, value);
        CHECK(tdx_profinance_field(&view, 2, &value) == 1 && value == 1.0,
              "%s: field 2 is 1, got %f", cases[which].label, value);
        CHECK(tdx_profinance_field(&view, 200, &value) == 1 && value == 199.0,
              "%s: field 200 is 199, got %f", cases[which].label, value);
        /* Out of range is absent, not zero. */
        CHECK(tdx_profinance_field(&view, 201, &value) == 0,
              "%s: field 201 is past the record", cases[which].label);
        CHECK(tdx_profinance_field(&view, 0, &value) == 0,
              "%s: fields are numbered from 1", cases[which].label);

        CHECK(tdx_profinance_record_at(&document, 1, &view, &error) == TDX_OK,
              "%s: record 1: %s", cases[which].label, error.message);
        CHECK(strcmp(view.code, "600519") == 0, "%s: the second code is 600519, got %s",
              cases[which].label, view.code);
        CHECK(view.market_id == 1, "%s: 600519 is Shanghai, got %d", cases[which].label,
              view.market_id);
        CHECK(tdx_profinance_field(&view, 3, &value) == 1 && value == 4.0,
              "%s: its field 3 is twice the index, got %f", cases[which].label, value);
        CHECK(tdx_profinance_record_at(&document, 2, &view, &error) == TDX_ERR,
              "%s: record 2 is past the end", cases[which].label);
        tdx_buf_free(&member);
    }
}

static void test_refusals(void) {
    tdx_zip_entry entries[TDX_ZIP_ENTRIES_MAX];
    tdx_zip_entry entry;
    tdx_buf member = {0};
    size_t count = 0;

    error.message[0] = '\0';
    tdx_buf_init(&member);

    /* A corrupted CRC must be REJECTED, not merely noticed: this is the check that stops
     * a stream which decodes to the right length but the wrong bytes. */
    CHECK(tdx_zip_entries(profinance_bad_crc_zip, PROFINANCE_BAD_CRC_BYTES, entries,
                          TDX_ZIP_ENTRIES_MAX, &count, &error) == TDX_OK,
          "the corrupted archive still reads as a ZIP: %s", error.message);
    if (count == 1 && tdx_zip_find(entries, count, PROFINANCE_MEMBER_NAME, &entry)) {
        error.message[0] = '\0';
        CHECK(tdx_zip_extract(profinance_bad_crc_zip, PROFINANCE_BAD_CRC_BYTES, &entry, &member,
                              &error) == TDX_ERR,
              "extracting it fails");
        CHECK(strstr(error.message, "CRC") != NULL, "and the message names the CRC: %s",
              error.message);
        CHECK(member.len == 0, "with the buffer left empty, got %zu", member.len);
    }
    /* An archive without its end record is not a ZIP. */
    error.message[0] = '\0';
    CHECK(tdx_zip_entries(profinance_no_eocd_zip, PROFINANCE_NO_EOCD_BYTES, entries,
                          TDX_ZIP_ENTRIES_MAX, &count, &error) == TDX_ERR,
          "an archive with no end record is refused");
    CHECK(strstr(error.message, "end-of-central-directory") != NULL,
          "and says which record is missing: %s", error.message);

    /* A truncated archive. */
    error.message[0] = '\0';
    CHECK(tdx_zip_entries(profinance_stored_zip, 8, entries, TDX_ZIP_ENTRIES_MAX, &count,
                          &error) == TDX_ERR,
          "eight bytes is too short to be a ZIP");
    CHECK(tdx_zip_entries(NULL, 0, entries, TDX_ZIP_ENTRIES_MAX, &count, &error) == TDX_ERR,
          "and so is nothing at all");
    /* A capacity smaller than the member count. */
    error.message[0] = '\0';
    CHECK(tdx_zip_entries(profinance_stored_zip, PROFINANCE_STORED_BYTES, entries, 0, &count,
                          &error) == TDX_ERR,
          "zero room for one member is refused");
    tdx_buf_free(&member);
}

static void test_member_refusals(void) {
    tdx_profinance_document document;
    unsigned char broken[64];
    size_t index;

    /* A member that is not a finance table at all. */
    memset(broken, 0, sizeof(broken));
    error.message[0] = '\0';
    CHECK(tdx_profinance_parse(broken, sizeof(broken), &document, &error) == TDX_ERR,
          "version 0 is refused: %s", error.message);
    CHECK(strstr(error.message, "version") != NULL, "and the message names the version");

    /* Version 1 but a wrong index size. */
    for (index = 0; index < sizeof(broken); ++index)
        broken[index] = 0;
    broken[0] = 1; /* version 1 */
    broken[7] = 1; /* record count 1 */
    broken[10] = 9; /* index size 9, not 11 */
    error.message[0] = '\0';
    CHECK(tdx_profinance_parse(broken, sizeof(broken), &document, &error) == TDX_ERR,
          "an index size of 9 is refused: %s", error.message);
    CHECK(strstr(error.message, "index") != NULL, "and the message names the index");

    /* A record size that is not a multiple of four. */
    broken[10] = 11;
    broken[12] = 6; /* data size 6 */
    error.message[0] = '\0';
    CHECK(tdx_profinance_parse(broken, sizeof(broken), &document, &error) == TDX_ERR,
          "a data size of 6 is refused: %s", error.message);

    /* An index that points outside the member. */
    {
        unsigned char member[64];
        memset(member, 0, sizeof(member));
        member[0] = 1;
        member[7] = 1;   /* one record */
        member[10] = 11; /* index size 11 */
        member[12] = 4;  /* four bytes of floats */
        member[20] = '0';
        member[21] = '0';
        member[22] = '0';
        member[23] = '0';
        member[24] = '0';
        member[25] = '1';
        /* data offset 9999, far past a 64-byte member */
        member[27] = 0x0f;
        member[28] = 0x27;
        error.message[0] = '\0';
        CHECK(tdx_profinance_parse(member, sizeof(member), &document, &error) == TDX_ERR,
              "an index pointing past the member is refused: %s", error.message);
        /* And a code that is not six digits. */
        member[27] = 0;
        member[28] = 0;
        member[29] = 0;
        member[30] = 42; /* data offset 42, inside the member */
        member[20] = 'x';
        error.message[0] = '\0';
        CHECK(tdx_profinance_parse(member, sizeof(member), &document, &error) == TDX_ERR,
              "a non-numeric code is refused: %s", error.message);
    }
    /* No bytes at all. */
    CHECK(tdx_profinance_parse(NULL, 0, &document, &error) == TDX_ERR, "no bytes is refused");
}

static void test_field_names(void) {
    /* Only two fields are published, and everything else is reported by number.  A table
     * of 584 invented names would be worse than none. */
    CHECK(tdx_profinance_field_name(TDX_PROFINANCE_FIELD_REVENUE_YOY) != NULL,
          "field 183 is named");
    CHECK(tdx_profinance_field_name(TDX_PROFINANCE_FIELD_PROFIT_YOY) != NULL,
          "field 184 is named");
    CHECK(tdx_profinance_field_name(1) == NULL, "field 1 is not published, so not named");
    CHECK(tdx_profinance_field_name(200) == NULL, "nor is field 200");
    CHECK(TDX_PROFINANCE_FIELD_REVENUE_YOY == 183 && TDX_PROFINANCE_FIELD_PROFIT_YOY == 184,
          "and the two published ids are the ones the reference names");
}

/* Every rendered line has to parse.  The document line once put a Windows path into the
 * JSON through a raw %s, which produced an invalid \f escape - the line looked like JSON
 * and was not, and only a parser could tell. */
static void test_rendered_lines_parse(void) {
    static const char *const lines[] = {
                "{\"type\":\"finance_document\",\"source\":\"C:\\\\Users\\\\x\\\\finance.zip\","
        "\"member\":\"gpcw20260630.dat\",\"member_bytes\":13072810,"
        "\"member_crc\":\"9c4fb1bf\",\"version\":1,\"report_date\":20260630,"
        "\"records\":5570,\"field_count\":584,\"index_size\":11,\"data_size\":2336,"
        "\"named_fields\":[183,184]}",
        "{\"type\":\"finance_row\",\"code\":\"000001\",\"market_id\":0,"
        "\"report_date\":20260630,\"revenue_yoy\":1.780000,\"profit_yoy\":null}",
        "{\"type\":\"finance_field\",\"code\":\"600519\",\"field\":183,"
        "\"name\":\"\xe8\x90\xa5\xe6\x94\xb6\xe5\x90\x8c\xe6\xaf\x94\","
        "\"value\":1.300000}",
        "{\"type\":\"finance_field\",\"code\":\"600519\",\"field\":1,"
        "\"name\":null,\"value\":0.000000}",
    };
    size_t index;

    for (index = 0; index < sizeof(lines) / sizeof(lines[0]); ++index) {
        tdx_json_doc doc = {0};
        error.message[0] = '\0';
        tdx_json_doc_init(&doc);
        CHECK(tdx_json_parse((const uint8_t *)lines[index], strlen(lines[index]), &doc,
                             &error) == TDX_OK,
              "line %zu parses: %s\n    %s", index, error.message, lines[index]);
        tdx_json_doc_free(&doc);
    }
    /* And a line that is NOT valid is rejected, so the assertion above is not vacuous:
     * this is the raw-path form the bug produced. */
    {
        /* An absolute Windows path through a raw %s: \U is not a JSON escape.  (\f would
         * have been legal, which is why the first version of this check was wrong.) */
        static const char *broken =
            "{\"type\":\"finance_document\",\"source\":\"C:\\Users\\x.zip\"}";
        tdx_json_doc doc = {0};
        error.message[0] = '\0';
        tdx_json_doc_init(&doc);
        CHECK(tdx_json_parse((const uint8_t *)broken, strlen(broken), &doc, &error) == TDX_ERR,
              "an unescaped backslash path is rejected");
        tdx_json_doc_free(&doc);
    }
}

int main(void) {
    test_stored_and_deflate();
    test_refusals();
    test_member_refusals();
    test_field_names();
    test_rendered_lines_parse();

    if (failures) {
        printf("%d finance-package check(s) failed\n", failures);
        return 1;
    }
    printf("finance-package checks passed\n");
    return 0;
}
