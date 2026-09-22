/* Shared buffer capacity, lifetime and byte-oriented JSON string contracts. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tdx_bytes.h"
#include "tdx_format.h"

static int failures;
#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, m); failures++; } } while (0)

static void test_format_capacity(void) {
    static const size_t sizes[] = {511, 512, 513, 1023, 1024};
    size_t index;
    for (index = 0; index < sizeof(sizes) / sizeof(sizes[0]); ++index) {
        size_t length = sizes[index];
        size_t prefix;
        for (prefix = 0; prefix < 2; ++prefix) {
            tdx_buf out = {0};
            tdx_error error = {0};
            char *text = (char *)malloc(length + 1);
            CHECK(text != NULL, "allocate test input");
            if (!text)
                return;
            memset(text, 'x', length);
            text[length] = 0;
            CHECK(tdx_buf_reserve(&out, 512, &error) == TDX_OK, "reserve boundary capacity");
            if (prefix)
                CHECK(tdx_buf_push(&out, 'p', &error) == TDX_OK, "append prefix");
            CHECK(tdx_buf_append_printf(&out, &error, "%s", text) == TDX_OK, "format boundary text");
            CHECK(out.len == prefix + length, "length excludes printf terminator");
            CHECK(memcmp(out.data + prefix, text, length) == 0, "formatted bytes preserved");
            if (length >= 512) {
                CHECK(out.cap > out.len, "long printf reserves a byte for the terminator");
                CHECK(out.data[out.len] == 0, "printf terminator lies in the allocation");
            }
            tdx_buf_free(&out);
            free(text);
        }
    }
}

static void test_buffer_lifetime(void) {
    tdx_buf out = {0};
    tdx_error error = {0};
    uint8_t *allocation;
    CHECK(tdx_buf_append(&out, "abcd", 4, &error) == TDX_OK, "append bytes");
    allocation = out.data;
    tdx_buf_clear(&out);
    CHECK(out.data == allocation && out.len == 0, "clear preserves allocation");
    CHECK(tdx_buf_append(&out, "x", 1, &error) == TDX_OK, "reuse buffer");
    CHECK(tdx_buf_reserve(&out, SIZE_MAX, &error) == TDX_ERR, "addition overflow refused");
    CHECK(out.len == 1 && out.data == allocation, "failed reserve preserves valid buffer");
    tdx_buf_free(&out);
    tdx_buf_free(&out);
    CHECK(!out.data && !out.len && !out.cap, "free is idempotent");
}

static void test_json_byte_string(void) {
    const char bytes[] = {'a', '"', '\\', '\n', '\t', '\0', 'z'};
    const char expected[] = "\"a\\\"\\\\\\n\\t\\u0000z\"";
    tdx_buf out = {0};
    tdx_error error = {0};
    CHECK(tdx_format_json_string_n(&out, bytes, sizeof(bytes), &error) == TDX_OK,
          "explicit length string formats");
    CHECK(out.len == sizeof(expected) - 1 && memcmp(out.data, expected, out.len) == 0,
          "control bytes and embedded NUL are escaped, suffix is preserved");
    tdx_buf_clear(&out);
    CHECK(tdx_format_json_string_n(&out, NULL, 0, &error) == TDX_OK, "empty null view formats");
    CHECK(out.len == 2 && memcmp(out.data, "\"\"", 2) == 0, "empty string literal");
    CHECK(tdx_format_json_string_n(&out, NULL, 1, &error) == TDX_ERR, "invalid view is refused");
    tdx_buf_free(&out);
}

static void test_heartbeat_counter_schema(void) {
    const char round[] = "{\"type\":\"heartbeat\",\"round\":12,\"subscribed\":3,\"total_events\":7}";
    const char sequence[] = "{\"type\":\"heartbeat\",\"sequence\":12,\"subscribed\":3,\"total_events\":7}";
    tdx_buf out = {0};
    tdx_error error = {0};
    CHECK(tdx_format_heartbeat_round_event(&out, 12, 3, 7, &error) == TDX_OK,
          "render watch heartbeat");
    CHECK(out.len == sizeof(round) - 1 && memcmp(out.data, round, out.len) == 0,
          "watch retains the round field and existing byte format");
    tdx_buf_clear(&out);
    CHECK(tdx_format_heartbeat_event(&out, 12, 3, 7, &error) == TDX_OK,
          "render hub heartbeat");
    CHECK(out.len == sizeof(sequence) - 1 && memcmp(out.data, sequence, out.len) == 0,
          "hub retains the sequence field and existing byte format");
    tdx_buf_free(&out);
}

int main(void) {
    test_format_capacity();
    test_buffer_lifetime();
    test_json_byte_string();
    test_heartbeat_counter_schema();
    if (!failures)
        puts("byte buffer checks passed");
    return failures ? 1 : 0;
}
