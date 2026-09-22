/* tdx_bytes.c - growable byte buffer and little-endian scalars. */
#include "tdx_bytes.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void tdx_error_set(tdx_error *err, const char *format, ...) {
    va_list args;
    if (!err)
        return;
    va_start(args, format);
    vsnprintf(err->message, sizeof(err->message), format, args);
    va_end(args);
}

void tdx_buf_init(tdx_buf *buf) {
    if (!buf)
        return;
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

void tdx_buf_free(tdx_buf *buf) {
    if (!buf)
        return;
    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

int tdx_buf_reserve(tdx_buf *buf, size_t extra, tdx_error *err) {
    size_t needed;
    size_t capacity;
    uint8_t *grown;

    if (!buf) {
        tdx_error_set(err, "buffer is null");
        return TDX_ERR;
    }
    if (extra > SIZE_MAX - buf->len) {
        tdx_error_set(err, "buffer length overflow");
        return TDX_ERR;
    }
    needed = buf->len + extra;
    if (needed <= buf->cap)
        return TDX_OK;

    capacity = buf->cap ? buf->cap : 256;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }
    grown = (uint8_t *)realloc(buf->data, capacity);
    if (!grown) {
        tdx_error_set(err, "out of memory growing buffer to %zu bytes", capacity);
        return TDX_ERR;
    }
    buf->data = grown;
    buf->cap = capacity;
    return TDX_OK;
}

int tdx_buf_push(tdx_buf *buf, uint8_t value, tdx_error *err) {
    if (tdx_buf_reserve(buf, 1, err) != TDX_OK)
        return TDX_ERR;
    buf->data[buf->len++] = value;
    return TDX_OK;
}

int tdx_buf_append(tdx_buf *buf, const void *data, size_t size, tdx_error *err) {
    if (size == 0)
        return TDX_OK;
    if (!data) {
        tdx_error_set(err, "append source is null");
        return TDX_ERR;
    }
    if (tdx_buf_reserve(buf, size, err) != TDX_OK)
        return TDX_ERR;
    memcpy(buf->data + buf->len, data, size);
    buf->len += size;
    return TDX_OK;
}

int tdx_buf_append_zeros(tdx_buf *buf, size_t count, tdx_error *err) {
    if (count == 0)
        return TDX_OK;
    if (tdx_buf_reserve(buf, count, err) != TDX_OK)
        return TDX_ERR;
    memset(buf->data + buf->len, 0, count);
    buf->len += count;
    return TDX_OK;
}

int tdx_buf_append_u16le(tdx_buf *buf, uint16_t value, tdx_error *err) {
    uint8_t raw[2];
    raw[0] = (uint8_t)(value & 0xFFu);
    raw[1] = (uint8_t)((value >> 8) & 0xFFu);
    return tdx_buf_append(buf, raw, sizeof(raw), err);
}

int tdx_buf_append_u32le(tdx_buf *buf, uint32_t value, tdx_error *err) {
    uint8_t raw[4];
    raw[0] = (uint8_t)(value & 0xFFu);
    raw[1] = (uint8_t)((value >> 8) & 0xFFu);
    raw[2] = (uint8_t)((value >> 16) & 0xFFu);
    raw[3] = (uint8_t)((value >> 24) & 0xFFu);
    return tdx_buf_append(buf, raw, sizeof(raw), err);
}

void tdx_buf_clear(tdx_buf *buf) {
    if (buf)
        buf->len = 0;
}

int tdx_buf_append_printf(tdx_buf *buf, tdx_error *err, const char *format, ...) {
    va_list args;
    va_list copy;
    int needed;
    char stack[512];

    if (!buf || !format) {
        tdx_error_set(err, "formatting needs a buffer and a format");
        return TDX_ERR;
    }
    va_start(args, format);
    va_copy(copy, args);
    needed = vsnprintf(stack, sizeof(stack), format, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        tdx_error_set(err, "cannot format the requested text");
        return TDX_ERR;
    }
    if ((size_t)needed < sizeof(stack)) {
        va_end(args);
        return tdx_buf_append(buf, stack, (size_t)needed, err);
    }
    /* vsnprintf writes a terminator in addition to the counted text. */
    if (tdx_buf_reserve(buf, (size_t)needed + 1u, err) != TDX_OK) {
        va_end(args);
        return TDX_ERR;
    }
    vsnprintf((char *)buf->data + buf->len, (size_t)needed + 1, format, args);
    va_end(args);
    buf->len += (size_t)needed;
    return TDX_OK;
}

uint16_t tdx_u16le(const uint8_t *data) {
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

uint32_t tdx_u32le(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

int32_t tdx_i32le(const uint8_t *data) {
    return (int32_t)tdx_u32le(data);
}

float tdx_f32le(const uint8_t *data) {
    uint32_t raw = tdx_u32le(data);
    float value;
    memcpy(&value, &raw, sizeof(value));
    return value;
}

int tdx_ascii_casecmp(const char *left, const char *right) {
    while (*left && *right) {
        int a = *left;
        int b = *right;
        if (a >= 'A' && a <= 'Z')
            a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z')
            b += 'a' - 'A';
        if (a != b)
            return a - b;
        ++left;
        ++right;
    }
    return (int)(unsigned char)*left - (int)(unsigned char)*right;
}

char *tdx_trim(char *text) {
    char *end;
    if (!text)
        return NULL;
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n' ||
           *text == '\f' || *text == '\v')
        ++text;
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' ||
                          end[-1] == '\n' || end[-1] == '\f' || end[-1] == '\v'))
        --end;
    *end = '\0';
    return text;
}
