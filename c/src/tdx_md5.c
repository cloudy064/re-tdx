/* tdx_md5.c - straight RFC 1321 MD5. */
#include "tdx_md5.h"

#include <string.h>

/* Per-round left rotation amounts. */
static const unsigned char md5_shift[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

/* floor(2^32 * abs(sin(i + 1))). */
static const uint32_t md5_sine[64] = {
    0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu, 0x4787c62au,
    0xa8304613u, 0xfd469501u, 0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
    0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u, 0xf61e2562u, 0xc040b340u,
    0x265e5a51u, 0xe9b6c7aau, 0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
    0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu, 0xa9e3e905u, 0xfcefa3f8u,
    0x676f02d9u, 0x8d2a4c8au, 0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
    0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u, 0x289b7ec6u, 0xeaa127fau,
    0xd4ef3085u, 0x04881d05u, 0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
    0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u,
    0xffeff47du, 0x85845dd1u, 0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
    0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u};

static uint32_t rotate_left(uint32_t value, unsigned amount) {
    return (value << amount) | (value >> (32u - amount));
}

static uint32_t read_u32le(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static void write_u32le(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8) & 0xFFu);
    out[2] = (uint8_t)((value >> 16) & 0xFFu);
    out[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static void md5_transform(uint32_t state[4], const uint8_t block[64]) {
    uint32_t words[16];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    unsigned index;

    for (index = 0; index < 16; ++index)
        words[index] = read_u32le(block + index * 4u);

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];

    for (index = 0; index < 64; ++index) {
        uint32_t mix;
        unsigned word;
        if (index < 16) {
            mix = (b & c) | (~b & d);
            word = index;
        } else if (index < 32) {
            mix = (d & b) | (~d & c);
            word = (5u * index + 1u) % 16u;
        } else if (index < 48) {
            mix = b ^ c ^ d;
            word = (3u * index + 5u) % 16u;
        } else {
            mix = c ^ (b | ~d);
            word = (7u * index) % 16u;
        }
        mix += a + md5_sine[index] + words[word];
        a = d;
        d = c;
        c = b;
        b += rotate_left(mix, md5_shift[index]);
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

void tdx_md5_init(tdx_md5 *context) {
    if (!context)
        return;
    context->state[0] = 0x67452301u;
    context->state[1] = 0xefcdab89u;
    context->state[2] = 0x98badcfeu;
    context->state[3] = 0x10325476u;
    context->bits = 0;
    context->filled = 0;
    memset(context->block, 0, sizeof(context->block));
}

void tdx_md5_update(tdx_md5 *context, const void *data, size_t size) {
    const uint8_t *cursor = (const uint8_t *)data;
    if (!context || (!data && size))
        return;
    context->bits += (uint64_t)size * 8u;
    while (size) {
        size_t room = sizeof(context->block) - context->filled;
        size_t take = size < room ? size : room;
        memcpy(context->block + context->filled, cursor, take);
        context->filled += take;
        cursor += take;
        size -= take;
        if (context->filled == sizeof(context->block)) {
            md5_transform(context->state, context->block);
            context->filled = 0;
        }
    }
}

void tdx_md5_final(tdx_md5 *context, uint8_t digest[TDX_MD5_DIGEST_SIZE]) {
    static const uint8_t padding[64] = {0x80u};
    uint8_t length_bytes[8];
    uint64_t bits;
    size_t pad;
    unsigned index;

    if (!context || !digest)
        return;
    bits = context->bits;
    for (index = 0; index < 8; ++index)
        length_bytes[index] = (uint8_t)((bits >> (8u * index)) & 0xFFu);

    /* Pad with 0x80 then zeros until 56 mod 64, then the 64-bit bit count. */
    pad = context->filled < 56 ? 56 - context->filled : 120 - context->filled;
    tdx_md5_update(context, padding, pad);
    tdx_md5_update(context, length_bytes, sizeof(length_bytes));

    for (index = 0; index < 4; ++index)
        write_u32le(digest + index * 4u, context->state[index]);
}

void tdx_md5_hex(const uint8_t digest[TDX_MD5_DIGEST_SIZE], char out[TDX_MD5_HEX_SIZE]) {
    static const char digits[] = "0123456789abcdef";
    unsigned index;
    if (!digest || !out)
        return;
    for (index = 0; index < TDX_MD5_DIGEST_SIZE; ++index) {
        out[index * 2u] = digits[(digest[index] >> 4) & 0x0Fu];
        out[index * 2u + 1u] = digits[digest[index] & 0x0Fu];
    }
    out[TDX_MD5_DIGEST_SIZE * 2u] = '\0';
}

void tdx_md5_hex_of(const void *data, size_t size, char out[TDX_MD5_HEX_SIZE]) {
    tdx_md5 context;
    uint8_t digest[TDX_MD5_DIGEST_SIZE];
    tdx_md5_init(&context);
    tdx_md5_update(&context, data, size);
    tdx_md5_final(&context, digest);
    tdx_md5_hex(digest, out);
}
