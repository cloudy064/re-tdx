/* test_download.c - MD5, the 0x02C5/0x06B9 framing, and the day path layout.
 *
 * The transfer loop itself is exercised against a real server by the evidence
 * run recorded in output/; what is pinned here is every byte the reverse
 * engineering produced, because a wrong offset or field width would otherwise
 * only show up as a mysterious "no such resource" from the peer. */
#include <stdio.h>
#include <string.h>

#include "tdx_download.h"
#include "tdx_md5.h"
#include "tdx_quote.h"
#include "tdx_zst_day.h"

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

/* --- MD5 --------------------------------------------------------------- */

static void check_digest(const char *text, const char *expected) {
    char actual[TDX_MD5_HEX_SIZE];
    tdx_md5_hex_of(text, strlen(text), actual);
    CHECK(strcmp(actual, expected) == 0, "md5(%s) = %s, expected %s", text, actual, expected);
}

static void test_md5_vectors(void) {
    /* RFC 1321, appendix A.5. */
    check_digest("", "d41d8cd98f00b204e9800998ecf8427e");
    check_digest("a", "0cc175b9c0f1b6a831c399e269772661");
    check_digest("abc", "900150983cd24fb0d6963f7d28e17f72");
    check_digest("message digest", "f96b697d7cb7938d525a2f31aaf161d0");
    check_digest("abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b");
    check_digest("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
                 "d174ab98d277d9f5a5611c2c9f419d9f");
    check_digest("12345678901234567890123456789012345678901234567890123456789012345678901234567890",
                 "57edf4a22be3c955ac49da2e2107b67a");
}

/* The padding branch changes at 55/56/57 and at 63/64/65 bytes, so a one-shot
 * digest must equal a digest fed in 7-byte pieces across that whole range. */
static void test_md5_streaming_matches(void) {
    uint8_t data[256];
    size_t length;
    size_t index;
    for (index = 0; index < sizeof(data); ++index)
        data[index] = (uint8_t)(index * 31u + 7u);
    for (length = 0; length <= 200; ++length) {
        tdx_md5 context;
        uint8_t digest[TDX_MD5_DIGEST_SIZE];
        char streamed[TDX_MD5_HEX_SIZE];
        char one_shot[TDX_MD5_HEX_SIZE];
        size_t offset = 0;
        tdx_md5_hex_of(data, length, one_shot);
        tdx_md5_init(&context);
        while (offset < length) {
            size_t piece = length - offset < 7 ? length - offset : 7;
            tdx_md5_update(&context, data + offset, piece);
            offset += piece;
        }
        tdx_md5_final(&context, digest);
        tdx_md5_hex(digest, streamed);
        CHECK(strcmp(streamed, one_shot) == 0, "length %zu: %s vs %s", length, streamed,
              one_shot);
    }
}

/* --- path validation --------------------------------------------------- */

static void test_path_validation(void) {
    tdx_error error;
    error.message[0] = '\0';
    CHECK(tdx_download_path_valid("hishf/date/20260612/sz000623.img", 100, &error) == TDX_OK,
          "a real resource path must be accepted: %s", error.message);
    CHECK(tdx_download_path_valid("a.jsn", 40, &error) == TDX_OK, "a bare file name is fine");
    CHECK(tdx_download_path_valid("", 40, &error) == TDX_ERR, "an empty path must fail");
    CHECK(tdx_download_path_valid("/a/b", 40, &error) == TDX_ERR, "an absolute path must fail");
    CHECK(tdx_download_path_valid("a/b/", 40, &error) == TDX_ERR,
          "a trailing separator must fail");
    CHECK(tdx_download_path_valid("a//b", 40, &error) == TDX_ERR,
          "an empty segment must fail");
    CHECK(tdx_download_path_valid("../a", 40, &error) == TDX_ERR, ".. must fail");
    CHECK(tdx_download_path_valid("a/../b", 40, &error) == TDX_ERR, ".. in the middle must fail");
    CHECK(tdx_download_path_valid("a/./b", 40, &error) == TDX_ERR, ". must fail");
    CHECK(tdx_download_path_valid("C:/x", 40, &error) == TDX_ERR, "a drive letter must fail");
    CHECK(tdx_download_path_valid("a\u00e9b", 40, &error) == TDX_ERR, "non-ASCII must fail");
    {
        char too_long[64];
        memset(too_long, 'a', sizeof(too_long));
        too_long[39] = '/';
        too_long[40] = 'b';
        too_long[41] = '\0';
        CHECK(tdx_download_path_valid(too_long, 40, &error) == TDX_ERR,
              "a path of 41 bytes must not fit a 40 byte field");
        too_long[39] = '\0';
        CHECK(tdx_download_path_valid(too_long, 40, &error) == TDX_OK,
              "39 bytes must fit a 40 byte field");
    }
}

/* --- 0x02C5 / 0x06B9 request bodies ------------------------------------ */

static void test_info_request(void) {
    tdx_buf request;
    tdx_error error;
    const char *path = "hishf/date/20260612/sz000623.img";
    error.message[0] = '\0';
    tdx_buf_init(&request);
    CHECK(tdx_download_info_request(path, &request, &error) == TDX_OK, "build: %s",
          error.message);
    CHECK(request.len == TDX_DOWNLOAD_INFO_PATH_SIZE, "0x02C5 body is %zu bytes, expected %u",
          request.len, (unsigned)TDX_DOWNLOAD_INFO_PATH_SIZE);
    if (request.len == TDX_DOWNLOAD_INFO_PATH_SIZE) {
        CHECK(memcmp(request.data, path, strlen(path)) == 0, "the path must lead the body");
        CHECK(request.data[strlen(path)] == 0, "the path must be NUL terminated");
        CHECK(request.data[TDX_DOWNLOAD_INFO_PATH_SIZE - 1] == 0, "the body must be NUL padded");
    }
    tdx_buf_free(&request);

    /* A path that exactly fills the field is still not terminated. */
    tdx_buf_init(&request);
    {
        char exact[64];
        memset(exact, 'a', 40);
        exact[40] = '\0';
        CHECK(tdx_download_info_request(exact, &request, &error) == TDX_ERR,
              "a 40 byte path must be rejected, there is no room for a terminator");
    }
    tdx_buf_free(&request);
}

static void test_chunk_request(void) {
    tdx_buf request;
    tdx_error error;
    const char *path = "hishf/date/20260612/sz000623.img";
    error.message[0] = '\0';
    tdx_buf_init(&request);
    CHECK(tdx_download_chunk_request(path, 0x0000BBAAu, 30000, &request, &error) == TDX_OK,
          "build: %s", error.message);
    CHECK(request.len == TDX_DOWNLOAD_CHUNK_REQUEST_SIZE,
          "0x06B9 body is %zu bytes, expected %u", request.len,
          (unsigned)TDX_DOWNLOAD_CHUNK_REQUEST_SIZE);
    if (request.len == TDX_DOWNLOAD_CHUNK_REQUEST_SIZE) {
        CHECK(tdx_u32le(request.data) == 0x0000BBAAu, "offset must be little endian at +0");
        CHECK(tdx_u32le(request.data + 4) == 30000u, "length must be little endian at +4");
        CHECK(memcmp(request.data + 8, path, strlen(path)) == 0, "the path must start at +8");
        CHECK(request.data[8 + strlen(path)] == 0, "the path must be NUL terminated");
        CHECK(request.data[8 + TDX_DOWNLOAD_CHUNK_PATH_SIZE - 1] == 0,
              "the path field must be NUL padded to 100 bytes");
        {
            size_t index;
            int zeroed = 1;
            for (index = 8 + TDX_DOWNLOAD_CHUNK_PATH_SIZE;
                 index < TDX_DOWNLOAD_CHUNK_REQUEST_SIZE; ++index)
                if (request.data[index] != 0)
                    zeroed = 0;
            CHECK(zeroed, "the 200 bytes after the path must be zero");
        }
    }
    tdx_buf_clear(&request);
    CHECK(tdx_download_chunk_request(path, 0, 0, &request, &error) == TDX_ERR,
          "a zero length chunk must be rejected");
    CHECK(tdx_download_chunk_request(path, 0, TDX_DOWNLOAD_CHUNK_MAX + 1, &request, &error) ==
              TDX_ERR,
          "a chunk above 30000 bytes must be rejected");
    tdx_buf_free(&request);
}

/* --- 0x02C5 / 0x06B9 replies ------------------------------------------ */

static void put_u32(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8) & 0xFFu);
    out[2] = (uint8_t)((value >> 16) & 0xFFu);
    out[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static void test_parse_info(void) {
    uint8_t body[64];
    tdx_file_info info;
    tdx_error error;
    const char *digest = "d41d8cd98f00b204e9800998ecf8427e";
    error.message[0] = '\0';

    /* The realistic layout: length, has_md5, digest, then unused tail. */
    memset(body, 0, sizeof(body));
    put_u32(body, 141227);
    body[4] = 1;
    memcpy(body + 5, digest, 32);
    CHECK(tdx_download_parse_info(body, sizeof(body), &info, &error) == TDX_OK, "parse: %s",
          error.message);
    CHECK(info.size == 141227, "size %u", (unsigned)info.size);
    CHECK(info.has_md5 == 1, "has_md5");
    CHECK(strcmp(info.md5, digest) == 0, "digest %s", info.md5);

    /* Upper case digests are normalised. */
    memcpy(body + 5, "D41D8CD98F00B204E9800998ECF8427E", 32);
    CHECK(tdx_download_parse_info(body, sizeof(body), &info, &error) == TDX_OK, "parse: %s",
          error.message);
    CHECK(strcmp(info.md5, digest) == 0, "digest must be lower cased, got %s", info.md5);

    /* No digest announced: allowed, md5 stays empty. */
    memset(body, 0, sizeof(body));
    put_u32(body, 4096);
    CHECK(tdx_download_parse_info(body, sizeof(body), &info, &error) == TDX_OK, "parse: %s",
          error.message);
    CHECK(info.has_md5 == 0 && info.md5[0] == '\0', "has_md5 must be 0, digest empty");

    /* Rejections. */
    CHECK(tdx_download_parse_info(body, 37, &info, &error) == TDX_ERR,
          "a 37 byte reply must be rejected");
    /* The exact bytes every public node returns for a historical image: size 0,
     * the "has digest" byte set, and an empty digest field. */
    {
        static const uint8_t absent[38] = {0x00, 0x00, 0x00, 0x00, 0x01};
        CHECK(tdx_download_parse_info(absent, sizeof(absent), &info, &error) == TDX_ERR,
              "a zero-length resource must be rejected");
        CHECK(strstr(error.message, "does not hold") != NULL,
              "a missing resource must say so, got: %s", error.message);
    }
    memset(body, 0, sizeof(body));
    put_u32(body, 4096);
    body[4] = 1;
    memcpy(body + 5, "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz", 32);
    CHECK(tdx_download_parse_info(body, sizeof(body), &info, &error) == TDX_ERR,
          "a non-hex digest must be rejected");
    memset(body, 0, sizeof(body));
    put_u32(body, 4096);
    body[4] = 1;
    memcpy(body + 5, "abc", 3);
    CHECK(tdx_download_parse_info(body, sizeof(body), &info, &error) == TDX_ERR,
          "a short digest must be rejected when one is announced");
    memset(body, 0, sizeof(body));
    put_u32(body, TDX_DOWNLOAD_RESOURCE_MAX + 1u);
    CHECK(tdx_download_parse_info(body, sizeof(body), &info, &error) == TDX_ERR,
          "an oversized resource must be rejected");
}

static void test_parse_chunk(void) {
    uint8_t body[64];
    const uint8_t *data = NULL;
    size_t length = 0;
    tdx_error error;
    error.message[0] = '\0';

    memset(body, 0, sizeof(body));
    put_u32(body, 10);
    memcpy(body + 4, "0123456789", 10);
    CHECK(tdx_download_parse_chunk(body, 14, 10, &data, &length, &error) == TDX_OK, "parse: %s",
          error.message);
    CHECK(length == 10 && data == body + 4, "payload view must start after the length");
    CHECK(memcmp(data, "0123456789", 10) == 0, "payload content");

    /* Trailing slack in the reply is ignored. */
    CHECK(tdx_download_parse_chunk(body, sizeof(body), 10, &data, &length, &error) == TDX_OK,
          "parse: %s", error.message);
    CHECK(length == 10, "length %zu", length);

    /* An empty chunk is legal at this layer; the transfer loop rejects it. */
    memset(body, 0, sizeof(body));
    CHECK(tdx_download_parse_chunk(body, 4, 10, &data, &length, &error) == TDX_OK, "parse: %s",
          error.message);
    CHECK(length == 0, "length %zu", length);

    memset(body, 0, sizeof(body));
    put_u32(body, 11);
    CHECK(tdx_download_parse_chunk(body, 15, 10, &data, &length, &error) == TDX_ERR,
          "more bytes than requested must be rejected");
    CHECK(tdx_download_parse_chunk(body, 3, 10, &data, &length, &error) == TDX_ERR,
          "a reply without a length field must be rejected");
    memset(body, 0, sizeof(body));
    put_u32(body, 10);
    memcpy(body + 4, "01234", 5);
    CHECK(tdx_download_parse_chunk(body, 9, 10, &data, &length, &error) == TDX_ERR,
          "a truncated payload must be rejected");
}

/* --- day resource naming ----------------------------------------------- */

static void test_day_paths(void) {
    tdx_code security;
    char remote[128];
    char local[512];
    tdx_error error;
    error.message[0] = '\0';

    CHECK(tdx_code_parse("sz000623", &security, &error) == TDX_OK, "code: %s", error.message);
    CHECK(tdx_zst_day_build_paths(&security, "20260612", "C:/cache", remote, sizeof(remote),
                                  local, sizeof(local), &error) == TDX_OK,
          "paths: %s", error.message);
    CHECK(strcmp(remote, "hishf/date/20260612/sz000623.img") == 0, "remote %s", remote);
    CHECK(strcmp(local, "C:/cache/sz000623_20260612.img") == 0, "local %s", local);

    CHECK(tdx_code_parse("sh600000", &security, &error) == TDX_OK, "code: %s", error.message);
    CHECK(tdx_zst_day_build_paths(&security, "20250101", "c", remote, sizeof(remote), local,
                                  sizeof(local), &error) == TDX_OK,
          "paths: %s", error.message);
    CHECK(strcmp(remote, "hishf/date/20250101/sh600000.img") == 0, "remote %s", remote);
    CHECK(strcmp(local, "c/sh600000_20250101.img") == 0, "local %s", local);

    CHECK(tdx_code_parse("bj430047", &security, &error) == TDX_OK, "code: %s", error.message);
    CHECK(tdx_zst_day_build_paths(&security, "20250101", "c", remote, sizeof(remote), NULL, 0,
                                  &error) == TDX_OK,
          "paths: %s", error.message);
    CHECK(strcmp(remote, "hishf/date/20250101/bj430047.img") == 0, "remote %s", remote);

    CHECK(tdx_code_parse("sz000623", &security, &error) == TDX_OK, "code: %s", error.message);
    CHECK(tdx_zst_day_build_paths(&security, "2026-06-12", "c", remote, sizeof(remote), NULL,
                                  0, &error) == TDX_ERR,
          "a dashed date must be rejected");
    CHECK(tdx_zst_day_build_paths(&security, "2026061", "c", remote, sizeof(remote), NULL, 0,
                                  &error) == TDX_ERR,
          "a seven-digit date must be rejected");
    CHECK(tdx_zst_day_build_paths(&security, "20261312", "c", remote, sizeof(remote), NULL, 0,
                                  &error) == TDX_ERR,
          "month 13 must be rejected");
    CHECK(tdx_zst_day_build_paths(&security, "20260612", NULL, remote, sizeof(remote), local,
                                  sizeof(local), &error) == TDX_ERR,
          "asking for a cache path without a cache directory must fail");
}

int main(void) {
    test_md5_vectors();
    test_md5_streaming_matches();
    test_path_validation();
    test_info_request();
    test_chunk_request();
    test_parse_info();
    test_parse_chunk();
    test_day_paths();

    if (failures) {
        printf("%d download check(s) failed\n", failures);
        return 1;
    }
    printf("download checks passed\n");
    return 0;
}
