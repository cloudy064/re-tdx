/* test_jsn.c - the JSN resource table format.
 *
 * The fixture is the whole raw GBK payload of bi/list/zq_tx201.jsn (the
 * discount-bond list) as a node sent it, md5 b5a03f5b0a848424085902a2557974cb.  So
 * this test drives the real chain: GBK conversion through the platform code page,
 * JSON parsing, and the group/row/column layer - on the bytes, not on a
 * reconstruction of them.
 *
 * The format's one real invariant is that a row is exactly as wide as its header.
 * A row that is short would leave every cell after the gap labelled by the wrong
 * column, which is worse than a failure, so that is refused and tested with a
 * synthetic document (no captured resource has a malformed row). */
#include <stdio.h>
#include <string.h>

#include "tdx_jsn.h"
#include "jsn_fixtures.h"

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


/* tdx_buf carries a length, not a terminator, so a comparison must copy and
 * terminate.  Reading buffer->data with strcmp directly reads whatever the
 * allocation held before, which is a mistake this test made and this helper is
 * here to prevent. */
static const char *text_of(const tdx_buf *buffer) {
    static char scratch[65536];
    size_t copy = buffer->len < sizeof(scratch) - 1 ? buffer->len : sizeof(scratch) - 1;
    if (copy && buffer->data)
        memcpy(scratch, buffer->data, copy);
    scratch[copy] = '\0';
    return scratch;
}

static void test_remote_path(void) {
    char path[128];
    tdx_error error;
    error.message[0] = '\0';

    CHECK(tdx_jsn_remote_path("list/zq_tx201.jsn", "bi", path, sizeof(path), &error) == TDX_OK,
          "path: %s", error.message);
    CHECK(strcmp(path, "bi/list/zq_tx201.jsn") == 0, "the prefix is applied once: %s", path);
    /* A leading slash and an already-prefixed resource must not double up. */
    CHECK(tdx_jsn_remote_path("/list/zq_tx201.jsn", "bi", path, sizeof(path), &error) == TDX_OK,
          "a leading slash is normalised");
    CHECK(strcmp(path, "bi/list/zq_tx201.jsn") == 0, "still one prefix: %s", path);
    CHECK(tdx_jsn_remote_path("bi/list/zq_tx201.jsn", "bi", path, sizeof(path), &error) == TDX_OK,
          "an already-prefixed resource");
    CHECK(strcmp(path, "bi/list/zq_tx201.jsn") == 0, "not prefixed twice: %s", path);
    CHECK(tdx_jsn_remote_path("list/x.jsn", NULL, path, sizeof(path), &error) == TDX_OK,
          "a null prefix defaults to bi");
    CHECK(strcmp(path, "bi/list/x.jsn") == 0, "the default prefix is bi: %s", path);
    CHECK(tdx_jsn_remote_path("list/x.jsn", "bi", path, 8, &error) == TDX_ERR,
          "a short buffer must be refused");
    CHECK(tdx_jsn_remote_path(NULL, "bi", path, sizeof(path), &error) == TDX_ERR,
          "a null resource must be refused");
}

static void test_gbk_conversion(void) {
    tdx_buf utf8;
    tdx_error error;
    error.message[0] = '\0';
    tdx_buf_init(&utf8);

    CHECK(tdx_jsn_gbk_to_utf8(jsn_gbk, sizeof(jsn_gbk), &utf8, &error) == TDX_OK,
          "the fixture converts from GBK: %s", error.message);
    CHECK(utf8.len == JSN_FIXTURE_UTF8_BYTES, "the conversion is %d bytes, got %zu",
          JSN_FIXTURE_UTF8_BYTES, utf8.len);
    /* GBK is two bytes per Chinese character, UTF-8 is three, so a payload with
     * Chinese in it must grow - and the growth is the evidence that the non-ASCII
     * bytes were really converted rather than passed through. */
    CHECK(utf8.len > sizeof(jsn_gbk), "the UTF-8 form is longer than the GBK one");
    /* The JSON is ASCII except inside strings, so the converted text must still be
     * parseable JSON with the same structure. */
    {
        const char *text = (const char *)utf8.data;
        CHECK(strstr(text, "\"colheader\"") != NULL, "the converted text is still JSON");
    }
    CHECK(tdx_jsn_gbk_to_utf8(NULL, 0, &utf8, &error) == TDX_OK,
          "an empty payload converts to nothing");
    CHECK(tdx_jsn_gbk_to_utf8(jsn_gbk, sizeof(jsn_gbk), NULL, &error) == TDX_ERR,
          "a null output must be refused");
    tdx_buf_free(&utf8);
}

static void test_parse_captured(void) {
    tdx_buf utf8;
    tdx_buf cell;
    tdx_jsn_document document;
    tdx_error error;
    error.message[0] = '\0';
    tdx_buf_init(&utf8);
    tdx_buf_init(&cell);
    tdx_jsn_document_init(&document);

    CHECK(tdx_jsn_gbk_to_utf8(jsn_gbk, sizeof(jsn_gbk), &utf8, &error) == TDX_OK, "convert");
    CHECK(tdx_jsn_parse(utf8.data, utf8.len, &document, &error) == TDX_OK, "parse: %s",
          error.message);
    CHECK(document.group_count == JSN_FIXTURE_GROUPS, "%d group(s), got %zu",
          JSN_FIXTURE_GROUPS, document.group_count);
    CHECK(document.row_count == JSN_FIXTURE_ROWS, "%d rows, got %zu", JSN_FIXTURE_ROWS,
          document.row_count);
    if (document.group_count != 1)
        goto done;

    {
        const tdx_jsn_group *group = &document.groups[0];
        CHECK(group->row_count == JSN_FIXTURE_ROWS, "the group holds every row");
        CHECK(group->column_count == JSN_FIXTURE_COLUMNS, "%d columns, got %zu",
              JSN_FIXTURE_COLUMNS, group->column_count);
        CHECK(group->first_row == 0, "the first group starts at row 0");

        /* The header names come from the resource and are ASCII. */
        CHECK(tdx_jsn_column_name(&document, group, 0) &&
                  strcmp(tdx_jsn_column_name(&document, group, 0), "$ZQDM") == 0,
              "column 0 is the security code: %s", tdx_jsn_column_name(&document, group, 0));
        CHECK(tdx_jsn_column_name(&document, group, group->column_count) == NULL,
              "a column past the end is NULL");

        /* The first row's code and its Chinese short name.  The name is the part
         * that only a correct GBK conversion and a correct JSON parse can produce
         * together; "26贴债39" is the capture's value. */
        tdx_buf_clear(&cell);
        CHECK(tdx_jsn_cell_text(&document, group, 0, 0, &cell, &error) == TDX_OK, "cell 0");
        CHECK(strcmp(text_of(&cell), "020820") == 0,
              "the first bond code is 020820, got %s", text_of(&cell));

        tdx_buf_clear(&cell);
        CHECK(tdx_jsn_cell_text(&document, group, 0, 2, &cell, &error) == TDX_OK, "cell 2");
        {
            /* 26贴债39.  The literal is split because a \x escape eats every
             * hex digit that follows it, so "\xba39" would be ONE escape (0xBA39)
             * and not U+80BA followed by "39". */
            const char *expected = "26\xe8\xb4\xb4\xe5\x80\xba" "39";
            CHECK(strcmp(text_of(&cell), expected) == 0,
                  "the first bond's name decodes from GBK, got %s", text_of(&cell));
        }
        /* LLLX is the rate type; every row of this resource is a discount bond. */
        tdx_buf_clear(&cell);
        CHECK(tdx_jsn_cell_text(&document, group, 0, 6, &cell, &error) == TDX_OK, "cell 6");
        CHECK(strcmp(text_of(&cell), "\xe8\xb4\xb4\xe7\x8e\xb0") == 0,
              "the rate type is the Chinese for discount, got %s", text_of(&cell));

        /* The JSON form of a cell is quoted, and of an empty cell is an empty
         * string, not null: the resource sends "", and flattening that to null
         * would lose the difference. */
        tdx_buf_clear(&cell);
        CHECK(tdx_jsn_cell_json(&document, group, 0, 0, &cell, &error) == TDX_OK, "json cell");
        CHECK(strcmp(text_of(&cell), "\"020820\"") == 0, "a cell renders quoted, got %s",
              text_of(&cell));
        tdx_buf_clear(&cell);
        CHECK(tdx_jsn_cell_json(&document, group, 0, 4, &cell, &error) == TDX_OK, "empty cell");
        CHECK(strcmp(text_of(&cell), "\"\"") == 0, "an empty cell stays an empty string: %s",
              text_of(&cell));

        /* Row membership: every row belongs to group 0, and one past the end does
         * not belong to anything. */
        CHECK(tdx_jsn_group_of_row(&document, 0) == group, "row 0 belongs to the group");
        CHECK(tdx_jsn_group_of_row(&document, group->row_count - 1) == group, "the last row");
        CHECK(tdx_jsn_group_of_row(&document, group->row_count) == NULL,
              "a row past the end belongs to nothing");

        /* Cells in the last row are reachable, which is what a short row would
         * have broken. */
        tdx_buf_clear(&cell);
        CHECK(tdx_jsn_cell_text(&document, group, group->row_count - 1,
                                group->column_count - 1, &cell, &error) == TDX_OK,
              "the last cell of the last row");
    }

done:
    tdx_jsn_document_free(&document);
    tdx_buf_free(&utf8);
    tdx_buf_free(&cell);
}

static void test_shape_rejects(void) {
    tdx_jsn_document document;
    tdx_error error;
    error.message[0] = '\0';

    /* A root that is not an array. */
    {
        const char *text = "{\"colheader\":[],\"data\":[]}";
        CHECK(tdx_jsn_parse((const uint8_t *)text, strlen(text), &document, &error) == TDX_ERR,
              "a non-array root must be refused");
        CHECK(strstr(error.message, "array of groups") != NULL, "the error says why: %s",
              error.message);
        tdx_jsn_document_free(&document);
    }
    /* A group without the two arrays. */
    {
        const char *text = "[{\"colheader\":[\"a\"]}]";
        error.message[0] = '\0';
        CHECK(tdx_jsn_parse((const uint8_t *)text, strlen(text), &document, &error) == TDX_ERR,
              "a group without data must be refused");
        CHECK(strstr(error.message, "colheader/data") != NULL, "the error says why: %s",
              error.message);
        tdx_jsn_document_free(&document);
    }
    /* THE INVARIANT: a row narrower than its header. */
    {
        const char *text = "[{\"colheader\":[\"a\",\"b\"],\"data\":[[\"1\"],[\"2\",\"3\"]]}]";
        error.message[0] = '\0';
        CHECK(tdx_jsn_parse((const uint8_t *)text, strlen(text), &document, &error) == TDX_ERR,
              "a short row must be refused");
        CHECK(strstr(error.message, "mislabelled") != NULL, "the error says why: %s",
              error.message);
        tdx_jsn_document_free(&document);
    }
    /* A row wider than its header is equally wrong. */
    {
        const char *text = "[{\"colheader\":[\"a\"],\"data\":[[\"1\",\"2\"]]}]";
        error.message[0] = '\0';
        CHECK(tdx_jsn_parse((const uint8_t *)text, strlen(text), &document, &error) == TDX_ERR,
              "a wide row must be refused");
        tdx_jsn_document_free(&document);
    }
    /* A row that is not an array. */
    {
        const char *text = "[{\"colheader\":[\"a\"],\"data\":[\"1\"]}]";
        error.message[0] = '\0';
        CHECK(tdx_jsn_parse((const uint8_t *)text, strlen(text), &document, &error) == TDX_ERR,
              "a non-array row must be refused");
        CHECK(strstr(error.message, "not an array") != NULL, "the error says why: %s",
              error.message);
        tdx_jsn_document_free(&document);
    }
    /* Valid but empty resources are allowed. */
    {
        const char *text = "[]";
        CHECK(tdx_jsn_parse((const uint8_t *)text, strlen(text), &document, &error) == TDX_OK,
              "an empty resource parses: %s", error.message);
        CHECK(document.group_count == 0 && document.row_count == 0, "and holds nothing");
        CHECK(tdx_jsn_group_of_row(&document, 0) == NULL, "and has no row 0");
        tdx_jsn_document_free(&document);
    }
    {
        const char *text = "[{\"colheader\":[],\"data\":[]}]";
        CHECK(tdx_jsn_parse((const uint8_t *)text, strlen(text), &document, &error) == TDX_OK,
              "a group with no columns or rows parses: %s", error.message);
        CHECK(document.group_count == 1 && document.row_count == 0,
              "one group, no rows");
        tdx_jsn_document_free(&document);
    }
}

int main(void) {
    test_remote_path();
    test_gbk_conversion();
    test_parse_captured();
    test_shape_rejects();

    if (failures) {
        printf("%d jsn check(s) failed\n", failures);
        return 1;
    }
    printf("jsn checks passed\n");
    return 0;
}
