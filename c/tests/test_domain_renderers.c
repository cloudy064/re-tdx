#include <stdio.h>
#include <math.h>
#include <string.h>
#include "tdx_bonds_json.h"
#include "tdx_convertible_json.h"
#include "tdx_jsn_json.h"
#include "tdx_limit_json.h"

static int failures, emit;
#define CHECK(c) do { if (!(c)) { printf("FAIL %d: %s\n", __LINE__, #c); failures++; } } while (0)
static int parse_line(const tdx_buf *line, tdx_json_doc *doc) {
    int okay = tdx_json_parse(line->data, line->len, doc, NULL) == TDX_OK;
    CHECK(okay);
    if (okay && emit) {
        CHECK(fwrite(line->data, 1, line->len, stdout) == line->len);
        putchar('\n');
    }
    return okay;
}
static const tdx_json_node *member(const tdx_json_doc *doc, const char *key) {
    return tdx_json_member(doc, tdx_json_root(doc), key);
}
static void text_equals(const tdx_json_doc *doc, const char *key, const char *value, size_t length) {
    const tdx_json_node *node = member(doc, key);
    const char *text = tdx_json_text(doc, node);
    CHECK(node && text && node->text_length == length && !memcmp(text, value, length));
}
static void test_limits(void) {
    const char source[] = "C:\\quoted\"dir\\rules\n.dat";
    tdx_limit_rules rules;
    tdx_limit_prices prices = {0};
    tdx_code security = {0};
    tdx_buf line = {0};
    tdx_json_doc json = {0};
    tdx_limit_rules_default(&rules);
    memcpy(rules.source, source, sizeof(source));
    CHECK(tdx_limit_format_rules(&line, &rules, NULL) == TDX_OK);
    if (parse_line(&line, &json)) text_equals(&json, "source", source, sizeof(source) - 1);
    tdx_buf_clear(&line);
    memcpy(prices.source, source, sizeof(source));
    memcpy(security.code, "600519", 7);
    security.market_id = 1;
    prices.security_class = 3;
    prices.rate = 0.1; prices.upper = 11; prices.lower = 9;
    CHECK(tdx_limit_format_prices(&line, &security, 10, &prices, NULL) == TDX_OK);
    if (parse_line(&line, &json)) {
        const tdx_json_node *upper = member(&json, "upper");
        text_equals(&json, "source", source, sizeof(source) - 1);
        CHECK(upper && upper->type == TDX_JSON_NUMBER && upper->number == 11);
    }
    tdx_json_doc_free(&json); tdx_buf_free(&line);
}
static void test_jsn(void) {
    const char input[] =
        "[{\"colheader\":[\"type\",\"type_cell\",\"type_cell_2\",\"type\",\"row\","
        "\"quote\\\"\\\\\",\"type\\u0000tail\",\"dup\",\"dup\"],"
        "\"data\":[[\"A\",\"B\",\"C\",\"D\",\"E\",\"F\",\"G\",\"H\",\"I\"]]}]";
    const char resource[] = "bi/windows\\quote\".jsn";
    const char endpoint[] = "host\\name\"\n";
    tdx_jsn_document document = {0};
    tdx_json_doc json = {0};
    tdx_buf line = {0};
    size_t renamed = 0;
    CHECK(tdx_jsn_parse((const uint8_t *)input, sizeof(input) - 1, &document, NULL) == TDX_OK);
    CHECK(tdx_jsn_format_row(&line, &document, 0, 0, resource, &renamed, NULL) == TDX_OK);
    CHECK(renamed == 4);
    if (parse_line(&line, &json)) {
        const tdx_json_node *root = tdx_json_root(&json);
        CHECK(root && root->child_count == 13);
        text_equals(&json, "type", "jsn_row", 7);
        text_equals(&json, "type_cell", "B", 1);
        text_equals(&json, "type_cell_2", "C", 1);
        text_equals(&json, "type_cell_3", "A", 1);
        text_equals(&json, "type_cell_4", "D", 1);
        text_equals(&json, "row_cell", "E", 1);
        text_equals(&json, "dup_cell_2", "I", 1);
    }
    tdx_buf_clear(&line);
    {
        tdx_jsn_summary summary = {resource, 123, "md5\"\\", 1, 1, renamed, endpoint};
        CHECK(tdx_jsn_format_summary(&line, &summary, NULL) == TDX_OK);
        if (parse_line(&line, &json)) text_equals(&json, "endpoint", endpoint, sizeof(endpoint) - 1);
    }
    /* A reserved key alone keeps the established suffix and field value. */
    {
        const char ordinary[] = "[{\"colheader\":[\"type\"],\"data\":[[\"bond\"]]}]";
        CHECK(tdx_jsn_parse((const uint8_t *)ordinary, sizeof(ordinary)-1, &document, NULL) == TDX_OK);
        tdx_buf_clear(&line);
        CHECK(tdx_jsn_format_row(&line, &document, 0, 0, "plain", &renamed, NULL) == TDX_OK);
        CHECK(renamed == 1);
        CHECK(tdx_json_parse(line.data, line.len, &json, NULL) == TDX_OK);
        text_equals(&json, "type_cell", "bond", 4);
    }
    tdx_json_doc_free(&json); tdx_jsn_document_free(&document); tdx_buf_free(&line);
}
static void test_convertible_summary(void) {
    tdx_convertible_join_summary summary = {8, "host\\quoted\"", 10, 9, 8, 7, 2, 1, 3, 4};
    tdx_buf line = {0};
    tdx_json_doc json = {0};
    CHECK(tdx_convertible_format_join_summary(&line, &summary, NULL) == TDX_OK);
    if (parse_line(&line, &json)) {
        const tdx_json_node *value = member(&json, "projection_fields_used");
        CHECK(value && value->number == 4);
        text_equals(&json, "endpoint", summary.endpoint, strlen(summary.endpoint));
    }
    tdx_json_doc_free(&json); tdx_buf_free(&line);
}
static void test_bond_schedule(void) {
    const char input[] =
        "[{\"colheader\":[\"FXRQXL\",\"FXLLXL\",\"SYFXRQXL\",\"SYFXLLXL\"],"
        "\"data\":[[\"20270101,odd\\\"\\\\date\\u0000x\",\"0.02,0.03\",\"20280101\",\"0.04\"]]}]";
    const char name[] = "Bond\0name";
    tdx_bond_row bond = {0};
    tdx_jsn_document document = {0};
    tdx_json_doc json = {0};
    tdx_buf line = {0};
    memcpy(bond.market, "SH", 3); bond.market_id = 1;
    bond.has_face_value_yuan = 1; bond.face_value_yuan = INFINITY;
    bond.name.data = name; bond.name.length = sizeof(name)-1; bond.name.present = 1;
    CHECK(tdx_jsn_parse((const uint8_t *)input, sizeof(input)-1, &document, NULL) == TDX_OK);
    CHECK(tdx_bonds_format_with_schedule(&line, &bond, "resource\\path", &document,
                                          &document.groups[0], 0, 0, 10, NULL) == TDX_OK);
    if (parse_line(&line, &json)) {
        const tdx_json_node *schedule = member(&json, "coupon_schedule");
        const tdx_json_node *first = tdx_json_at(&json, schedule, 0);
        const tdx_json_node *date = tdx_json_member(&json, first, "date");
        CHECK(date && date->type == TDX_JSON_NUMBER && date->number == 20270101);
        CHECK(schedule && schedule->child_count == 2);
        text_equals(&json, "name", name, sizeof(name)-1);
    }
    tdx_json_doc_free(&json); tdx_jsn_document_free(&document); tdx_buf_free(&line);
}
static void test_bond_list_and_summary(void) {
    const char values[] = "line\n\0x,1\0tail,inf,nan,1e999,12.5";
    const char scale[] = "scale\"\\\n";
    tdx_bond_text text = {0};
    tdx_buf line = {0};
    tdx_json_doc json = {0};
    tdx_convertible_row row = {0};
    text.data = values; text.length = sizeof(values) - 1; text.present = 1;
    CHECK(tdx_bonds_format_comma_array(&line, &text, 1, NULL) == TDX_OK);
    if (parse_line(&line, &json)) {
        const tdx_json_node *root = tdx_json_root(&json);
        const tdx_json_node *first = tdx_json_at(&json, root, 0);
        CHECK(root && root->child_count == 6);
        CHECK(first && first->text_length == 7);
    }
    tdx_buf_clear(&line);
    CHECK(tdx_bonds_format_summary(&line, 1, 1, 0, 0, "resource", scale, "host", NULL) == TDX_OK);
    if (parse_line(&line, &json)) text_equals(&json, "scale", scale, sizeof(scale) - 1);
    tdx_buf_clear(&line);
    row.has_face_value = 1; row.face_value = INFINITY;
    CHECK(tdx_convertible_format(&line, &row, "resource", 0, 0, NULL) == TDX_OK);
    CHECK(parse_line(&line, &json));
    tdx_json_doc_free(&json); tdx_buf_free(&line);
}
int main(int argc, char **argv) {
    emit = argc > 1 && strcmp(argv[1], "--emit-json-vectors") == 0;
    test_limits(); test_jsn(); test_convertible_summary(); test_bond_schedule();
    test_bond_list_and_summary();
    if (!emit) printf("domain renderer checks: %d failures\n", failures);
    return failures ? 1 : 0;
}
