/* test_frame.c - request encoding and response header/body decoding. */
#include <stdio.h>
#include <string.h>

#include <zlib.h>

#include "tdx_frame.h"
#include "tdx_quote.h"

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

static void expect_bytes(const char *label, const tdx_buf *buffer, const uint8_t *want,
                         size_t want_size) {
    size_t index;
    if (buffer->len != want_size) {
        printf("FAIL %s: length %zu, expected %zu\n", label, buffer->len, want_size);
        failures++;
        return;
    }
    for (index = 0; index < want_size; ++index) {
        if (buffer->data[index] != want[index]) {
            printf("FAIL %s: byte %zu is 0x%02X, expected 0x%02X\n", label, index,
                   buffer->data[index], want[index]);
            failures++;
            return;
        }
    }
}

static void test_request_without_body(void) {
    tdx_buf buffer;
    tdx_error error;
    /* 0x0C | message id | control | length x2 | command */
    const uint8_t want[] = {0x0C, 0x01, 0x08, 0x64, 0x01, 0x01,
                            0x02, 0x00, 0x02, 0x00, 0x47, 0x05};
    error.message[0] = '\0';
    CHECK(tdx_frame_build_request(0x01640801u, TDX_CMD_DEPTH, NULL, 0,
                                  TDX_REQUEST_PREFIX, &buffer, &error) == TDX_OK,
          "build failed: %s", error.message);
    expect_bytes("depth request header", &buffer, want, sizeof(want));
    tdx_buf_free(&buffer);
}

static void test_request_with_body(void) {
    tdx_buf buffer;
    tdx_error error;
    const uint8_t body[] = {0x9A, 0x1C};
    const uint8_t want[] = {0x0C, 0x00, 0x00, 0x00, 0x00, 0x01,
                            0x04, 0x00, 0x04, 0x00, 0x0D, 0x00, 0x9A, 0x1C};
    error.message[0] = '\0';
    CHECK(tdx_frame_build_request(0, TDX_HANDSHAKE_TYPE, body, sizeof(body),
                                  TDX_REQUEST_PREFIX, &buffer, &error) == TDX_OK,
          "build failed: %s", error.message);
    expect_bytes("handshake request", &buffer, want, sizeof(want));
    tdx_buf_free(&buffer);
}

static void test_request_rejects_bad_prefix(void) {
    tdx_buf buffer;
    tdx_error error;
    error.message[0] = '\0';
    CHECK(tdx_frame_build_request(1, TDX_CMD_DEPTH, NULL, 0, 0x7F, &buffer,
                                  &error) == TDX_ERR,
          "a non 0x0C/0x01 prefix must be rejected");
}

static void test_header_round_trip(void) {
    const uint8_t header[TDX_RESPONSE_HEADER_SIZE] = {
        0xB1, 0xCB, 0x74, 0x00, 0x05, 0x11, 0x22, 0x33,
        0x44, 0x7F, 0x47, 0x05, 0x2C, 0x00, 0xD0, 0x07};
    tdx_response_header decoded;
    tdx_error error;
    error.message[0] = '\0';
    CHECK(tdx_frame_decode_header(header, &decoded, &error) == TDX_OK, "decode failed");
    CHECK(decoded.control == 0x05, "control is %u", decoded.control);
    CHECK(decoded.message_id == 0x44332211u, "message id is 0x%08X", decoded.message_id);
    CHECK(decoded.message_type == 0x0547, "command is 0x%04X", decoded.message_type);
    CHECK(decoded.wire_size == 44, "wire size is %u", decoded.wire_size);
    CHECK(decoded.decoded_size == 2000, "decoded size is %u", decoded.decoded_size);
}

static void test_header_rejects_bad_prefix(void) {
    uint8_t header[TDX_RESPONSE_HEADER_SIZE] = {0};
    tdx_response_header decoded;
    tdx_error error;
    error.message[0] = '\0';
    header[0] = 0x0C;
    CHECK(tdx_frame_decode_header(header, &decoded, &error) == TDX_ERR,
          "an unexpected response prefix must be rejected");
}

static void test_body_passthrough(void) {
    const uint8_t wire[] = {1, 2, 3, 4};
    tdx_buf out;
    tdx_error error;
    error.message[0] = '\0';
    CHECK(tdx_frame_decode_body(wire, sizeof(wire), sizeof(wire), &out, &error) == TDX_OK,
          "passthrough failed: %s", error.message);
    CHECK(out.len == sizeof(wire), "passthrough length is %zu", out.len);
    CHECK(memcmp(out.data, wire, sizeof(wire)) == 0, "passthrough bytes differ");
    tdx_buf_free(&out);
}

static void test_body_inflate(void) {
    const char *expected = "7709 quote body, inflated through the frame decoder";
    uint8_t compressed[256];
    uLongf compressed_size = sizeof(compressed);
    tdx_buf out;
    tdx_error error;

    error.message[0] = '\0';
    CHECK(compress(compressed, &compressed_size, (const Bytef *)expected,
                   (uLong)strlen(expected)) == Z_OK,
          "test fixture compress() failed");
    CHECK(compressed_size != (uLongf)strlen(expected),
          "the fixture must actually be compressed");

    CHECK(tdx_frame_decode_body(compressed, (size_t)compressed_size,
                                strlen(expected), &out, &error) == TDX_OK,
          "inflate failed: %s", error.message);
    CHECK(out.len == strlen(expected), "inflated length is %zu", out.len);
    CHECK(out.data && memcmp(out.data, expected, out.len) == 0,
          "inflated bytes differ from the original");
    tdx_buf_free(&out);
}

static void test_body_rejects_corrupt_stream(void) {
    const uint8_t corrupt[] = {0x78, 0x9C, 0x11, 0x22, 0x33, 0x44, 0x55};
    tdx_buf out;
    tdx_error error;
    error.message[0] = '\0';
    CHECK(tdx_frame_decode_body(corrupt, sizeof(corrupt), 64, &out, &error) == TDX_ERR,
          "a corrupt zlib stream must be rejected");
}

int main(void) {
    test_request_without_body();
    test_request_with_body();
    test_request_rejects_bad_prefix();
    test_header_round_trip();
    test_header_rejects_bad_prefix();
    test_body_passthrough();
    test_body_inflate();
    test_body_rejects_corrupt_stream();
    if (failures) {
        printf("%d frame check(s) failed\n", failures);
        return 1;
    }
    printf("frame checks passed\n");
    return 0;
}
