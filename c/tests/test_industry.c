/* test_industry.c - the industry valuation resource.
 *
 * The fixture keeps EVERY ROW of three industries rather than a prefix of the file.  That
 * matters: the property this module rests on is that the declared member list agrees with
 * the rows themselves, and a prefix would cut industries in half so the property would fail
 * in the fixture while holding on the real file.
 *
 * The measured facts, from all 5,567 rows in the live resource:
 *
 *   * 110 industries, and the two membership counts agree for every one of them;
 *   * no industry's rows disagree about their market, name or valuation.
 *
 * Both are asserted here, and the fixture's three industries are a slice of the same
 * property rather than a separate claim. */
#include <stdio.h>
#include <string.h>

#include "tdx_industry.h"
#include "tdx_industry_json.h"
#include "tdx_jsn.h"
#include "render_check.h"
#include "industry_fixtures.h"

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
    static char scratch[16384];
    size_t copy = buffer->len < sizeof(scratch) - 1 ? buffer->len : sizeof(scratch) - 1;
    if (copy && buffer->data)
        memcpy(scratch, buffer->data, copy);
    scratch[copy] = '\0';
    return scratch;
}

static const char *bond_text(const tdx_bond_text *text) {
    static char scratch[512];
    if (!text->present || !text->data)
        return "(absent)";
    if (text->length >= sizeof(scratch))
        return "(too long)";
    memcpy(scratch, text->data, text->length);
    scratch[text->length] = '\0';
    return scratch;
}

static tdx_jsn_document document;
static tdx_industry_row rows[INDUSTRY_FIXTURE_ROWS + 4];
static tdx_industry industries[INDUSTRY_FIXTURE_INDUSTRIES + 4];
static size_t row_count = 0;
static size_t skipped = 0;
static size_t industry_count = 0;

static int load(void) {
    error.message[0] = '\0';
    tdx_jsn_document_init(&document);
    if (tdx_jsn_parse((const uint8_t *)industry_resource, strlen(industry_resource),
                      &document, &error) != TDX_OK) {
        CHECK(0, "the fixture parses: %s", error.message);
        return TDX_ERR;
    }
    if (tdx_industry_parse(&document, &document.groups[0], rows, INDUSTRY_FIXTURE_ROWS + 4,
                           &row_count, &skipped, &error) != TDX_OK) {
        CHECK(0, "parse: %s", error.message);
        return TDX_ERR;
    }
    if (tdx_industry_catalog(rows, row_count, industries, INDUSTRY_FIXTURE_INDUSTRIES + 4,
                             &industry_count, &error) != TDX_OK) {
        CHECK(0, "catalog: %s", error.message);
        return TDX_ERR;
    }
    return TDX_OK;
}

static const tdx_industry *find_industry(const char *code) {
    size_t index;
    for (index = 0; index < industry_count; ++index)
        if (strcmp(industries[index].code, code) == 0)
            return &industries[index];
    return NULL;
}

static void test_shape(void) {
    if (load() != TDX_OK)
        return;
    CHECK(document.groups[0].column_count == INDUSTRY_FIXTURE_COLUMNS, "%d columns, got %zu",
          INDUSTRY_FIXTURE_COLUMNS, document.groups[0].column_count);
    CHECK(INDUSTRY_FIXTURE_COLUMNS == 9, "the resource has 9 columns, header says %d",
          INDUSTRY_FIXTURE_COLUMNS);
    CHECK(INDUSTRY_FIXTURE_SOURCE_ROWS == 5567, "and 5,567 rows live, header says %d",
          INDUSTRY_FIXTURE_SOURCE_ROWS);
    CHECK(row_count == INDUSTRY_FIXTURE_ROWS, "%d rows map, got %zu", INDUSTRY_FIXTURE_ROWS,
          row_count);
    CHECK(skipped == 0, "and none is skipped, got %zu", skipped);
    CHECK(industry_count == INDUSTRY_FIXTURE_INDUSTRIES, "%d industries, got %zu",
          INDUSTRY_FIXTURE_INDUSTRIES, industry_count);
    tdx_jsn_document_free(&document);
}

/* THE PROPERTY THE MODULE RESTS ON: the declared member list and the rows agree. */
static void test_declared_and_measured_agree(void) {
    const tdx_industry *coal;
    size_t index;
    size_t agreeing = 0;

    if (load() != TDX_OK)
        return;
    for (index = 0; index < industry_count; ++index) {
        CHECK(industries[index].counts_agree,
              "industry %s says %zu declared and %zu rows", industries[index].code,
              industries[index].declared_count, industries[index].row_count);
        CHECK(!industries[index].inconsistent,
              "and industry %s has no row disagreeing with the others",
              industries[index].code);
        if (industries[index].counts_agree)
            agreeing++;
    }
    CHECK(agreeing == INDUSTRY_FIXTURE_INDUSTRIES, "all %d agree, got %zu",
          INDUSTRY_FIXTURE_INDUSTRIES, agreeing);

    /* The three industries, with the counts the fixture header records. */
    coal = find_industry(INDUSTRY_FIXTURE_CODE_0);
    CHECK(coal != NULL, "industry %s is present", INDUSTRY_FIXTURE_CODE_0);
    if (coal) {
        CHECK(coal->row_count == 42 && coal->declared_count == 42,
              "880471 holds 42 members both ways, got %zu and %zu", coal->row_count,
              coal->declared_count);
        CHECK(strcmp(coal->security_id, "SH880471") == 0, "its identity is SH880471, got %s",
              coal->security_id);
        CHECK(coal->has_pe && coal->pe > 5.2104 && coal->pe < 5.2106,
              "its P/E is 5.2105, got %f", coal->pe);
        CHECK(coal->has_pb && coal->pb > 0.5301 && coal->pb < 0.5303,
              "and its P/B is 0.5302, got %f", coal->pb);
    }
    {
        const tdx_industry *real_estate = find_industry(INDUSTRY_FIXTURE_CODE_1);
        CHECK(real_estate != NULL, "industry %s is present", INDUSTRY_FIXTURE_CODE_1);
        if (real_estate) {
            CHECK(real_estate->row_count == 26 && real_estate->declared_count == 26,
                  "880483 holds 26 both ways, got %zu and %zu", real_estate->row_count,
                  real_estate->declared_count);
            CHECK(real_estate->has_pe && real_estate->pe > 11.5823 && real_estate->pe < 11.5825,
                  "its P/E is 11.5824, got %f", real_estate->pe);
        }
    }
    {
        const tdx_industry *regional = find_industry(INDUSTRY_FIXTURE_CODE_2);
        CHECK(regional != NULL, "industry %s is present", INDUSTRY_FIXTURE_CODE_2);
        if (regional) {
            CHECK(regional->row_count == 43 && regional->declared_count == 43,
                  "880484 holds 43 both ways, got %zu and %zu", regional->row_count,
                  regional->declared_count);
            /* A NEGATIVE P/E is real: an industry whose members lose money in aggregate.
             * It must parse as a number rather than being treated as absent. */
            CHECK(regional->has_pe && regional->pe < -14.53 && regional->pe > -14.54,
                  "a negative P/E of -14.5387 parses, got %f", regional->pe);
            CHECK(regional->has_pb && regional->pb > 5.2730 && regional->pb < 5.2732,
                  "and its P/B is 5.2731, got %f", regional->pb);
        }
    }
    tdx_jsn_document_free(&document);
}

/* The two lists inside a row use DIFFERENT separators, and counting them the same way gave
 * every row exactly one theme - a plausible number that would have shipped unnoticed. */
static void test_separators_differ(void) {
    size_t index;
    size_t declared_seen = 0;
    size_t themes_over_one = 0;
    size_t themes_seen = 0;

    if (load() != TDX_OK)
        return;
    for (index = 0; index < row_count; ++index) {
        if (rows[index].declared_count > 1)
            declared_seen++;
        if (rows[index].theme_count > 0)
            themes_seen++;
        if (rows[index].theme_count > 1)
            themes_over_one++;
    }
    CHECK(declared_seen > 0, "some row declares more than one member, got %zu", declared_seen);
    /* $S_ZQDM separates with an ASCII comma and sszt with U+3001.  If they were counted
     * alike, every theme count would be 1 and this would be zero. */
    CHECK(themes_over_one > 0,
          "and some row carries more than one THEME, got %zu of %zu rows - if this is zero "
          "the ideographic comma is not being counted",
          themes_over_one, themes_seen);
    /* A row's declared list is the whole industry, so it is far longer than its themes. */
    for (index = 0; index < row_count; ++index) {
        if (rows[index].declared_count > 1 && rows[index].theme_count > 0) {
            CHECK(rows[index].declared_count > rows[index].theme_count,
                  "the declared member list (%zu) is longer than the theme list (%zu)",
                  rows[index].declared_count, rows[index].theme_count);
            break;
        }
    }
    /* A row names the stock it is about and the industry it belongs to. */
    CHECK(row_count > 0, "there are rows");
    if (row_count > 0) {
        int found_industry = 0;
        for (index = 0; index < row_count; ++index)
            if (strcmp(rows[index].industry_code, INDUSTRY_FIXTURE_CODE_0) == 0) {
                found_industry = 1;
                CHECK(strlen(rows[index].stock_security_id) == 8,
                      "a row names an 8-character security id, got %s",
                      rows[index].stock_security_id);
            }
        CHECK(found_industry, "some row belongs to %s", INDUSTRY_FIXTURE_CODE_0);
    }
    tdx_jsn_document_free(&document);
}

static void test_skips_and_inconsistency(void) {
    /* SYNTHETIC rows for the two behaviours a capture cannot be relied on to contain:
     * a row that cannot name both sides, and an industry whose rows disagree. */
    static const char *json =
        "[{\"colheader\":[\"$ZQDM\",\"$SC\",\"TDXHY\",\"$ZQDM1\",\"$SC1\",\"hyPE\",\"hyPB\","
        "\"$S_ZQDM\",\"sszt\"],\"data\":["
        "[\"000001\",\"0\",\"A\",\"880471\",\"1\",\"5.0\",\"0.5\",\"0|000001,0|000002\",\"\"],"
        "[\"000002\",\"0\",\"A\",\"880471\",\"1\",\"5.0\",\"0.5\",\"0|000001,0|000002\",\"\"],"
        "[\"00000\",\"0\",\"B\",\"880472\",\"1\",\"6.0\",\"0.6\",\"\",\"\"],"
        "[\"000003\",\"0\",\"A\",\"880471\",\"1\",\"9.9\",\"0.5\",\"0|000001,0|000002\",\"\"],"
        "[\"000004\",\"sh\",\"C\",\"880473\",\"1\",\"7.0\",\"0.7\",\"\",\"\"]]}]";
    tdx_jsn_document other;
    tdx_industry_row local_rows[8];
    tdx_industry local[8];
    size_t count = 0;
    size_t local_skipped = 0;
    size_t industries_found = 0;
    size_t index;

    error.message[0] = '\0';
    tdx_jsn_document_init(&other);
    if (tdx_jsn_parse((const uint8_t *)json, strlen(json), &other, &error) != TDX_OK) {
        CHECK(0, "parses: %s", error.message);
        tdx_jsn_document_free(&other);
        return;
    }
    CHECK(tdx_industry_parse(&other, &other.groups[0], local_rows, 8, &count, &local_skipped,
                             &error) == TDX_OK,
          "parse: %s", error.message);
    /* Two of the five rows cannot name a code and are skipped rather than failing the
     * file: the reference throws, which would cost a caller everything over one row. */
    CHECK(count == 3, "three of five rows map, got %zu", count);
    CHECK(local_skipped == 2, "and two are skipped and counted, got %zu", local_skipped);

    CHECK(tdx_industry_catalog(local_rows, count, local, 8, &industries_found,
                               &error) == TDX_OK,
          "catalog: %s", error.message);
    CHECK(industries_found == 1, "one industry survives, got %zu", industries_found);
    if (industries_found == 1) {
        /* Two rows agree, the third gives a different P/E: that is recorded, not refused,
         * so the caller keeps the industry and knows to distrust its valuation. */
        CHECK(local[0].inconsistent == 1, "the disagreement is recorded");
        CHECK(local[0].row_count == 3, "with all three stocks counted, got %zu",
              local[0].row_count);
        CHECK(strcmp(local[0].code, "880471") == 0, "under the right code, got %s",
              local[0].code);
    }
    tdx_jsn_document_free(&other);
}

static void test_rendering(void) {
    tdx_buf line;
    char reason[192];

    if (load() != TDX_OK)
        return;
    tdx_buf_init(&line);
    CHECK(tdx_industry_format(&line, &industries[0], 0, &error) == TDX_OK, "render: %s",
          error.message);
    CHECK(render_parses(text_of(&line), reason, sizeof(reason)),
          "the industry line parses: %s\n    %s", reason, text_of(&line));
    CHECK(strstr(text_of(&line), "\"type\":\"industry\"") != NULL, "the type");
    CHECK(strstr(text_of(&line), "\"counts_agree\":true") != NULL,
          "the agreement flag travels with the counts: %s", text_of(&line));
    CHECK(strstr(text_of(&line), "\"inconsistent\":false") != NULL, "and the consistency one");

    tdx_buf_clear(&line);
    CHECK(tdx_industry_format_row(&line, &rows[0], 0, &error) == TDX_OK, "row render: %s",
          error.message);
    CHECK(render_parses(text_of(&line), reason, sizeof(reason)),
          "the row parses: %s\n    %s", reason, text_of(&line));
    CHECK(strstr(text_of(&line), "\"type\":\"stock_industry\"") != NULL, "the type");
    CHECK(strstr(text_of(&line), "\"declared_count\":") != NULL, "the declared count");
    CHECK(strstr(text_of(&line), "\"theme_count\":") != NULL, "and the theme count");

    tdx_buf_clear(&line);
    CHECK(tdx_industry_format_summary(&line, TDX_INDUSTRY_RESOURCE, 5567, 0, 110, 110, 0,
                                      "1.2.3.4:7709", &error) == TDX_OK,
          "summary: %s", error.message);
    CHECK(render_parses(text_of(&line), reason, sizeof(reason)),
          "the summary parses: %s\n    %s", reason, text_of(&line));
    CHECK(strstr(text_of(&line), "\"industries_whose_counts_agree\":110") != NULL,
          "the agreement count: %s", text_of(&line));
    /* A row whose industry name is absent renders it as null rather than as an empty
     * string, so a caller can tell "no name" from "empty name". */
    {
        tdx_industry_row bare;
        memset(&bare, 0, sizeof(bare));
        snprintf(bare.stock_security_id, sizeof(bare.stock_security_id), "SZ000001");
        snprintf(bare.industry_code, sizeof(bare.industry_code), "880471");
        snprintf(bare.industry_security_id, sizeof(bare.industry_security_id), "SH880471");
        tdx_buf_clear(&line);
        CHECK(tdx_industry_format_row(&line, &bare, 0, &error) == TDX_OK, "bare row: %s",
              error.message);
        CHECK(render_parses(text_of(&line), reason, sizeof(reason)), "and it parses: %s",
              reason);
        CHECK(strstr(text_of(&line), "\"industry_name\":null") != NULL,
              "with a null name: %s", text_of(&line));
    }
    tdx_buf_free(&line);
    tdx_jsn_document_free(&document);
}

int main(void) {
    test_shape();
    test_declared_and_measured_agree();
    test_separators_differ();
    test_skips_and_inconsistency();
    test_rendering();

    if (failures) {
        printf("%d industry check(s) failed\n", failures);
        return 1;
    }
    printf("industry checks passed\n");
    return 0;
}
