/* test_json.c - the bounded JSON parser.
 *
 * The inputs here are GRAMMAR vectors written by hand on purpose: they test the
 * parser against the JSON specification, not against captured data.  Captured
 * bytes belong in data fixtures, and those are generated mechanically; a syntax
 * test has no capture to derive from.
 *
 * Two of the cases below are regressions for bugs this parser actually had:
 *
 *   * a string built from several pieces ("a\tb\u00e9c") must come out contiguous.
 *     An earlier version appended a NUL after every piece, so the value read back
 *     was cut off at the first escape.
 *   * an object member whose value is itself a string needs both the name and the
 *     value.  An earlier version stored them in one field, so one overwrote the
 *     other. */
#include <stdio.h>
#include <string.h>

#include "tdx_json.h"

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

#define PARSE(text) parse_of((const uint8_t *)(text), sizeof(text) - 1)

static tdx_error error;
static tdx_json_doc doc;

static int parse_of(const uint8_t *data, size_t size) {
    /* Every case gets a fresh document, so a failed parse cannot leave state for
     * the next one. */
    error.message[0] = '\0';
    return tdx_json_parse(data, size, &doc, &error);
}

static void test_scalars(void) {
    const tdx_json_node *root;

    CHECK(PARSE("null") == TDX_OK, "null parses: %s", error.message);
    root = tdx_json_root(&doc);
    CHECK(root && root->type == TDX_JSON_NULL, "null is a null");
    tdx_json_doc_free(&doc);

    CHECK(PARSE("true") == TDX_OK, "true parses");
    root = tdx_json_root(&doc);
    CHECK(root && root->type == TDX_JSON_BOOL && root->boolean == 1, "true is true");
    tdx_json_doc_free(&doc);

    CHECK(PARSE("false") == TDX_OK, "false parses");
    root = tdx_json_root(&doc);
    CHECK(root && root->type == TDX_JSON_BOOL && root->boolean == 0, "false is false");
    tdx_json_doc_free(&doc);

    CHECK(PARSE("-12.5e3") == TDX_OK, "a number parses: %s", error.message);
    root = tdx_json_root(&doc);
    CHECK(root && root->type == TDX_JSON_NUMBER && root->number == -12500.0,
          "the number is -12500, got %f", root ? root->number : 0.0);
    tdx_json_doc_free(&doc);

    CHECK(PARSE("0") == TDX_OK, "zero parses");
    CHECK(PARSE("") == TDX_ERR, "an empty input is refused");
    CHECK(PARSE("   ") == TDX_ERR, "whitespace alone is refused");
    tdx_json_doc_free(&doc);
}

static void test_strings(void) {
    const tdx_json_node *root;
    const char *text;

    CHECK(PARSE("\"hello\"") == TDX_OK, "a plain string parses");
    root = tdx_json_root(&doc);
    text = tdx_json_text(&doc, root);
    CHECK(text && strcmp(text, "hello") == 0, "the value is hello, got %s", text ? text : "(null)");
    tdx_json_doc_free(&doc);

    /* REGRESSION: several pieces must join into one contiguous value. */
    CHECK(PARSE("\"a\\tb\\u00e9c\"") == TDX_OK, "an escaped string parses: %s", error.message);
    root = tdx_json_root(&doc);
    text = tdx_json_text(&doc, root);
    CHECK(text && strcmp(text, "a\tb\xc3\xa9""c") == 0,
          "the pieces join: a TAB b U+00E9 c, got %s", text ? text : "(null)");
    CHECK(root->text_length == 6, "a, TAB, b, two bytes of U+00E9 and c is 6 bytes, got %zu",
          root ? root->text_length : 0);
    tdx_json_doc_free(&doc);

    /* The four byte escapes, the quote and the backslash. */
    CHECK(PARSE("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"") == TDX_OK, "the escape set parses: %s",
          error.message);
    root = tdx_json_root(&doc);
    text = tdx_json_text(&doc, root);
    CHECK(text && strcmp(text, "\"\\/\b\f\n\r\t") == 0, "every escape decodes");
    tdx_json_doc_free(&doc);

    /* A surrogate pair becomes one four-byte sequence; a lone surrogate becomes
     * U+FFFD rather than invalid UTF-8. */
    CHECK(PARSE("\"\\ud83d\\ude00\"") == TDX_OK, "a surrogate pair parses");
    root = tdx_json_root(&doc);
    text = tdx_json_text(&doc, root);
    CHECK(text && strcmp(text, "\xf0\x9f\x98\x80") == 0, "the pair is U+1F600");
    tdx_json_doc_free(&doc);

    CHECK(PARSE("\"\\ud800\"") == TDX_OK, "a lone surrogate parses");
    root = tdx_json_root(&doc);
    text = tdx_json_text(&doc, root);
    CHECK(text && strcmp(text, "\xef\xbf\xbd") == 0, "a lone surrogate becomes U+FFFD");
    tdx_json_doc_free(&doc);

    CHECK(PARSE("\"unterminated") == TDX_ERR, "an unterminated string is refused");
    CHECK(PARSE("\"\\q\"") == TDX_ERR, "an unknown escape is refused");
    CHECK(PARSE("\"\\u00\"") == TDX_ERR, "a short \\u escape is refused");
    CHECK(PARSE("\"a\tb\"") == TDX_ERR, "a raw control character is refused");
    tdx_json_doc_free(&doc);
}

static void test_containers(void) {
    const tdx_json_node *root;
    const tdx_json_node *member;

    /* REGRESSION: a member whose value is a string needs the name AND the value. */
    CHECK(PARSE("{\"name\":\"value\"}") == TDX_OK, "an object parses: %s", error.message);
    root = tdx_json_root(&doc);
    CHECK(root && root->type == TDX_JSON_OBJECT, "the root is an object");
    CHECK(root && root->child_count == 1, "one member");
    member = tdx_json_member(&doc, root, "name");
    CHECK(member != NULL, "the member is found by name");
    CHECK(member && strcmp(tdx_json_key(&doc, member), "name") == 0,
          "the name survives next to the value");
    CHECK(member && strcmp(tdx_json_text(&doc, member), "value") == 0,
          "the value survives next to the name");
    CHECK(tdx_json_member(&doc, root, "absent") == NULL, "an absent member is NULL");
    tdx_json_doc_free(&doc);

    CHECK(PARSE("[1,2,3]") == TDX_OK, "an array parses");
    root = tdx_json_root(&doc);
    CHECK(root && root->type == TDX_JSON_ARRAY && root->child_count == 3, "three elements");
    CHECK(tdx_json_at(&doc, root, 2) && tdx_json_at(&doc, root, 2)->number == 3.0,
          "the third element is 3");
    CHECK(tdx_json_at(&doc, root, 3) == NULL, "an out-of-range index is NULL");
    tdx_json_doc_free(&doc);

    /* The shape the JSN resources actually use. */
    CHECK(PARSE("[{\"colheader\":[\"a\",\"b\"],\"data\":[[\"1\",\"2\"],[\"3\",\"4\"]]}]") ==
              TDX_OK,
          "the JSN shape parses: %s", error.message);
    root = tdx_json_root(&doc);
    CHECK(root && root->type == TDX_JSON_ARRAY && root->child_count == 1, "one group");
    {
        const tdx_json_node *group = tdx_json_at(&doc, root, 0);
        const tdx_json_node *headers = tdx_json_member(&doc, group, "colheader");
        const tdx_json_node *data = tdx_json_member(&doc, group, "data");
        const tdx_json_node *row = tdx_json_at(&doc, data, 1);
        CHECK(headers && headers->child_count == 2, "two column headers");
        CHECK(data && data->child_count == 2, "two rows");
        CHECK(row && row->child_count == 2, "two cells in the row");
        CHECK(row && strcmp(tdx_json_text(&doc, tdx_json_at(&doc, row, 0)), "3") == 0,
              "the second row's first cell is 3");
    }
    tdx_json_doc_free(&doc);

    /* Empty containers and nesting. */
    CHECK(PARSE("{}") == TDX_OK, "an empty object parses");
    CHECK(PARSE("[]") == TDX_OK, "an empty array parses");
    CHECK(PARSE("[[[[[1]]]]]") == TDX_OK, "nesting parses");
    tdx_json_doc_free(&doc);
}

static void test_rejects(void) {
    CHECK(PARSE("{} {}") == TDX_ERR, "trailing data is refused");
    CHECK(PARSE("[1,]") == TDX_ERR, "a trailing comma is refused");
    CHECK(PARSE("{\"a\":1,}") == TDX_ERR, "a trailing object comma is refused");
    CHECK(PARSE("{\"a\" 1}") == TDX_ERR, "a missing colon is refused");
    CHECK(PARSE("[1 2]") == TDX_ERR, "a missing comma is refused");
    CHECK(PARSE("[1}") == TDX_ERR, "a mismatched closer is refused");
    CHECK(PARSE("01") == TDX_ERR, "a leading zero is refused");
    CHECK(PARSE("tru") == TDX_ERR, "a truncated literal is refused");
    CHECK(PARSE("[") == TDX_ERR, "an unterminated array is refused");
    CHECK(PARSE("@") == TDX_ERR, "an unexpected character is refused");
    CHECK(PARSE("{1:2}") == TDX_ERR, "an unquoted key is refused");
    tdx_json_doc_free(&doc);
    /* The failure message names an offset, which is what makes it actionable. */
    CHECK(PARSE("[1,]") == TDX_ERR, "refused again");
    CHECK(strstr(error.message, "offset") != NULL, "the error carries an offset: %s",
          error.message);
    tdx_json_doc_free(&doc);

    /* The nesting limit is enforced rather than blowing the stack. */
    {
        char deep[2 * TDX_JSON_DEPTH_MAX + 8];
        size_t index;
        for (index = 0; index < TDX_JSON_DEPTH_MAX + 2; ++index)
            deep[index] = '[';
        deep[TDX_JSON_DEPTH_MAX + 2] = '\0';
        error.message[0] = '\0';
        CHECK(tdx_json_parse((const uint8_t *)deep, strlen(deep), &doc, &error) == TDX_ERR,
              "a document past the depth limit is refused");
        CHECK(strstr(error.message, "nests deeper") != NULL, "the error says why: %s",
              error.message);
        tdx_json_doc_free(&doc);
    }
}

static void test_integer(void) {
    long value = 0;
    const tdx_json_node *root;

    CHECK(PARSE("[42,\"43\"]") == TDX_OK, "parses");
    root = tdx_json_root(&doc);
    CHECK(tdx_json_integer(&doc, tdx_json_at(&doc, root, 0), &value) == TDX_OK && value == 42,
          "a number reads as 42, got %ld", value);
    /* The JSN payloads carry numbers as strings, which is why this is accepted. */
    CHECK(tdx_json_integer(&doc, tdx_json_at(&doc, root, 1), &value) == TDX_OK && value == 43,
          "a numeric string reads as 43, got %ld", value);
    CHECK(tdx_json_integer(&doc, NULL, &value) == TDX_ERR, "a null node is refused");
    tdx_json_doc_free(&doc);
}

int main(void) {
    tdx_json_doc_init(&doc);
    test_scalars();
    test_strings();
    test_containers();
    test_rejects();
    test_integer();

    if (failures) {
        printf("%d json check(s) failed\n", failures);
        return 1;
    }
    printf("json checks passed\n");
    return 0;
}
