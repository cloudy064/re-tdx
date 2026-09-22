/* test_panorama.c - the market panorama.
 *
 * The registry is generated from the reference's table, so what needs asserting is that it
 * arrived whole: ten views and a hundred and twelve field mappings.  A generator that silently
 * stopped matching would shrink the table, and these counts are what makes that fail loudly
 * rather than quietly.
 *
 * The projection is exercised on a SYNTHETIC document with controlled cells, because what
 * matters is the mapping - a column named "syl" arriving as the field "pe_ttm", an absent column
 * becoming null rather than an empty string, a row that cannot name its security being skipped -
 * and a live resource cannot be made to contain exactly those cases on demand.
 *
 * The live check is in output/panorama_verification_evidence.txt: all ten resources fetched,
 * 28,503 rows, and every one of the ten views' key columns and all 112 field columns found in
 * the resources' own headers. */
#include <stdio.h>
#include <string.h>

#include "tdx_jsn.h"
#include "tdx_panorama.h"
#include "tdx_panorama_json.h"
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

/* The counts the extractor reported.  Written here as literals so a table that shrank fails. */
#define EXPECTED_VIEWS 10
#define EXPECTED_FIELDS 112

static tdx_error error;

static const char *text_of(const tdx_buf *buffer) {
    static char scratch[16384];
    size_t copy = buffer->len < sizeof(scratch) - 1 ? buffer->len : sizeof(scratch) - 1;
    if (copy && buffer->data)
        memcpy(scratch, buffer->data, copy);
    scratch[copy] = '\0';
    return scratch;
}

static void test_registry(void) {
    size_t index;
    size_t fields = 0;
    size_t widest = 0;

    CHECK(tdx_panorama_view_count == EXPECTED_VIEWS, "%d views, got %zu", EXPECTED_VIEWS,
          tdx_panorama_view_count);
    for (index = 0; index < tdx_panorama_view_count; ++index) {
        const tdx_panorama_view *view = &tdx_panorama_views[index];
        CHECK(view->id && *view->id, "view %zu has an id", index);
        CHECK(view->label && *view->label, "and a label");
        CHECK(view->resource && *view->resource, "and a resource");
        CHECK(view->market_field && view->code_field, "and both key columns");
        CHECK(view->field_count > 0 && view->field_count <= TDX_PANORAMA_FIELDS_MAX,
              "view %s declares %zu fields, which the row buffer must hold", view->id,
              view->field_count);
        CHECK(view->fields != NULL, "and a field table");
        fields += view->field_count;
        if (view->field_count > widest)
            widest = view->field_count;
    }
    CHECK(fields == EXPECTED_FIELDS, "%d field mappings, got %zu", EXPECTED_FIELDS, fields);
    /* The widest view is what sets the buffer: eighteen, not the sixteen that looked generous. */
    CHECK(widest == 18, "the widest view has 18 fields, got %zu", widest);
    CHECK(TDX_PANORAMA_FIELDS_MAX >= widest, "and the buffer holds it");

    /* Every resource is a JSN path under the prefix, and no two views share one. */
    for (index = 0; index < tdx_panorama_view_count; ++index) {
        size_t other;
        CHECK(strncmp(tdx_panorama_views[index].resource, "list/", 5) == 0,
              "view %s names a list resource: %s", tdx_panorama_views[index].id,
              tdx_panorama_views[index].resource);
        for (other = index + 1; other < tdx_panorama_view_count; ++other)
            CHECK(strcmp(tdx_panorama_views[index].resource,
                         tdx_panorama_views[other].resource) != 0,
                  "views %s and %s share a resource", tdx_panorama_views[index].id,
                  tdx_panorama_views[other].id);
    }

    /* Finding by id, case-insensitively, and refusing what is not there. */
    CHECK(tdx_panorama_find("quality-rating") != NULL, "quality-rating is found");
    CHECK(tdx_panorama_find("QUALITY-RATING") == tdx_panorama_find("quality-rating"),
          "the id is case-insensitive");
    CHECK(tdx_panorama_find("capital-flow") != NULL, "capital-flow is found");
    CHECK(tdx_panorama_find("catalog") == NULL, "catalog is not a registry entry: it is a view");
    CHECK(tdx_panorama_find("nosuchview") == NULL, "an unknown id is refused");
    CHECK(tdx_panorama_find("") == NULL && tdx_panorama_find(NULL) == NULL,
          "and an empty or null one");
    /* The one view that keys on different columns, which is why the names travel in the
     * registry rather than being assumed. */
    {
        const tdx_panorama_view *lhb = tdx_panorama_find("lhb-overview");
        CHECK(lhb != NULL, "lhb-overview is found");
        if (lhb) {
            CHECK(strcmp(lhb->market_field, "$SC1") == 0 && strcmp(lhb->code_field, "$ZQDM1") == 0,
                  "and it keys on %s/%s rather than $SC/$ZQDM", lhb->market_field,
                  lhb->code_field);
            CHECK(strcmp(tdx_panorama_views[0].market_field, "$SC") == 0,
                  "while the first view keys on $SC");
        }
    }
}

static void test_projection(void) {
    /* A synthetic resource: the first view's key columns and two of its fields, plus the cases
     * worth controlling - an absent column and a row that cannot name its security. */
    static const char *json =
        "[{\"colheader\":[\"$ZQDM\",\"$SC\",\"date\",\"zxfs\",\"syl\"],\"data\":["
        "[\"002086\",\"0\",\"20260918\",\"75\",\"12.5\"],"
        "[\"600519\",\"1\",\"20260920\",\"90\",\"\"],"
        "[\"00000x\",\"0\",\"20260921\",\"50\",\"1.0\"],"
        "[\"300750\",\"0\",\"20260922\",\"60\",\"\"]]}]";
    tdx_jsn_document doc;
    tdx_panorama_row rows[8];
    const tdx_panorama_view *view = tdx_panorama_find("quality-rating");
    size_t count = 0;
    size_t skipped = 0;
    size_t field;

    error.message[0] = '\0';
    tdx_jsn_document_init(&doc);
    CHECK(tdx_jsn_parse((const uint8_t *)json, strlen(json), &doc, &error) == TDX_OK,
          "the synthetic resource parses: %s", error.message);
    CHECK(tdx_panorama_project(view, &doc, &doc.groups[0], rows, 8, &count, &skipped,
                               &error) == TDX_OK,
          "project: %s", error.message);
    /* The third row cannot name its security, so it is skipped and counted. */
    CHECK(count == 3, "three of four rows project, got %zu", count);
    CHECK(skipped == 1, "and one is skipped, got %zu", skipped);
    CHECK(strcmp(rows[0].security_id, "SZ002086") == 0, "the identity is built, got %s",
          rows[0].security_id);
    CHECK(strcmp(rows[1].security_id, "SH600519") == 0, "on both markets, got %s",
          rows[1].security_id);

    /* The mapping: the registry's first field is latest_date, from the resource's "date". */
    {
        size_t date_field = 0;
        size_t score_field = 0;
        for (field = 0; field < view->field_count; ++field) {
            if (strcmp(view->fields[field].name, "latest_date") == 0)
                date_field = field;
            if (strcmp(view->fields[field].name, "latest_score") == 0)
                score_field = field;
        }
        CHECK(rows[0].present[date_field] && rows[0].values[date_field].length == 8,
              "the date arrives as an eight-character value");
        CHECK(rows[0].present[score_field], "the score is present");
        CHECK(rows[0].values[score_field].data &&
                  memcmp(rows[0].values[score_field].data, "75", 2) == 0,
              "and its value is the resource's own text, not a reformatted number");
    }
    /* A FIELD WHOSE COLUMN THE RESOURCE DOES NOT CARRY IS ABSENT.  This synthetic resource has
     * three of the view's nine columns, so six fields are absent by construction - which is the
     * reachable form of "absent", since the reader refuses a row that is short.  A field whose
     * column IS there is present whether or not its cell holds anything. */
    {
        size_t pe_field = 0;
        size_t present = 0;
        size_t absent = 0;
        for (field = 0; field < view->field_count; ++field)
            if (strcmp(view->fields[field].name, "pe_ttm") == 0)
                pe_field = field;
        for (field = 0; field < view->field_count; ++field) {
            if (rows[0].present[field])
                present++;
            else
                absent++;
        }
        /* date, zxfs and syl are the three the resource carries. */
        CHECK(present == 3, "three of the nine fields have a column here, got %zu", present);
        CHECK(absent == 6, "and six do not, got %zu", absent);
        CHECK(rows[0].present[pe_field] && rows[0].values[pe_field].length == 4,
              "the value of the present one is the resource's text");
        CHECK(memcmp(rows[0].values[pe_field].data, "12.5", 4) == 0,
              "which reads 12.5");
        /* AN EMPTY CELL IS NO VALUE, the same as a column the resource does not carry, and it
         * renders as null.  That is this project's convention everywhere: an unknown value is
         * null rather than an empty string, so a caller never has to guess which of the two it
         * is looking at. */
        CHECK(!rows[1].present[pe_field] && rows[1].values[pe_field].length == 0,
              "an empty cell counts as no value, as does a column the resource lacks");
    }
    tdx_jsn_document_free(&doc);
}

static void test_rendering(void) {
    static const char *json =
        "[{\"colheader\":[\"$ZQDM\",\"$SC\",\"date\",\"zxfs\",\"syl\"],\"data\":["
        "[\"002086\",\"0\",\"20260918\",\"75\",\"12.5\"],"
        "[\"600519\",\"1\",\"20260920\",\"90\",\"\"]]}]";
    tdx_jsn_document doc;
    tdx_panorama_row rows[4];
    const tdx_panorama_view *view = tdx_panorama_find("quality-rating");
    size_t count = 0;
    size_t skipped = 0;
    tdx_buf line;
    char reason[192];

    error.message[0] = '\0';
    tdx_jsn_document_init(&doc);
    if (tdx_jsn_parse((const uint8_t *)json, strlen(json), &doc, &error) != TDX_OK ||
        tdx_panorama_project(view, &doc, &doc.groups[0], rows, 4, &count, &skipped, &error) !=
            TDX_OK) {
        CHECK(0, "setup: %s", error.message);
        tdx_jsn_document_free(&doc);
        return;
    }
    tdx_buf_init(&line);

    /* The catalog entry, which is what --view catalog emits. */
    CHECK(tdx_panorama_format_view(&line, &tdx_panorama_views[0], 0, &error) == TDX_OK,
          "render view: %s", error.message);
    CHECK(render_parses(text_of(&line), reason, sizeof(reason)),
          "the view line parses: %s\n    %s", reason, text_of(&line));
    CHECK(strstr(text_of(&line), "\"type\":\"panorama_view\"") != NULL, "the type");
    CHECK(strstr(text_of(&line), "\"field_count\":9") != NULL, "the width");
    CHECK(strstr(text_of(&line), "\"name\":\"latest_score\"") != NULL,
          "and the field list travels with it: %s", text_of(&line));

    /* A projected row: named fields rather than column codes. */
    tdx_buf_clear(&line);
    CHECK(tdx_panorama_format_row(&line, view, &rows[0], 0, &error) == TDX_OK, "render: %s",
          error.message);
    CHECK(render_parses(text_of(&line), reason, sizeof(reason)),
          "the row line parses: %s\n    %s", reason, text_of(&line));
    CHECK(strstr(text_of(&line), "\"type\":\"panorama_row\"") != NULL, "the type");
    CHECK(strstr(text_of(&line), "\"latest_score\":\"75\"") != NULL,
          "the field name, not the column: %s", text_of(&line));
    CHECK(strstr(text_of(&line), "\"zxfs\"") == NULL, "and the column code does not appear");
    /* An empty cell is an empty string, and a missing one is null - the distinction the
     * resource makes and this preserves. */
    CHECK(strstr(text_of(&line), "\"pe_ttm\":\"12.5\"") != NULL,
          "the mapped value: %s", text_of(&line));

    /* A FIELD WITH NO PRESENCE RENDERS AS null, which no well-formed document can produce -
     * the flag is cleared directly to reach that branch. */
    {
        tdx_panorama_row bare = rows[0];
        size_t first_field = 0;
        bare.present[first_field] = 0;
        tdx_buf_clear(&line);
        CHECK(tdx_panorama_format_row(&line, view, &bare, 0, &error) == TDX_OK,
              "render: %s", error.message);
        CHECK(render_parses(text_of(&line), reason, sizeof(reason)),
              "and it parses: %s", reason);
        CHECK(strstr(text_of(&line), "null") != NULL,
              "an absent field is null rather than an empty string: %s", text_of(&line));
        CHECK(strstr(text_of(&line), "\"latest_date\":\"\"") == NULL,
              "and not the empty string");
    }

    tdx_buf_clear(&line);
    CHECK(tdx_panorama_format_summary(&line, "catalog", NULL, 10, 0, NULL, &error) == TDX_OK,
          "summary: %s", error.message);
    CHECK(render_parses(text_of(&line), reason, sizeof(reason)), "the summary parses: %s",
          reason);
    CHECK(strstr(text_of(&line), "\"resource\":null") != NULL,
          "a catalog has no resource: %s", text_of(&line));
    tdx_buf_free(&line);
    tdx_jsn_document_free(&doc);
}

int main(void) {
    /* A crash would discard buffered output, so a test that fails part way through would look
     * like one that never started. */
    setvbuf(stdout, NULL, _IONBF, 0);
    test_registry();
    test_projection();
    test_rendering();

    if (failures) {
        printf("%d panorama check(s) failed\n", failures);
        return 1;
    }
    printf("panorama checks passed\n");
    return 0;
}
