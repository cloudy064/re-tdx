/* tdx_frame.c - 7709 request encoding and response decoding. */
#include "tdx_frame.h"

#include <string.h>

#include <zlib.h>

static const uint8_t response_prefix[4] = {0xB1, 0xCB, 0x74, 0x00};

int tdx_frame_build_request(uint32_t message_id, uint16_t message_type,
                            const void *body, size_t body_size, uint8_t prefix,
                            tdx_buf *out, tdx_error *err) {
    uint16_t length;

    if (!out) {
        tdx_error_set(err, "request buffer is null");
        return TDX_ERR;
    }
    if (prefix != TDX_REQUEST_PREFIX && prefix != TDX_EXPANSION_REQUEST_PREFIX) {
        tdx_error_set(err, "market-data request prefix must be 0x0C or 0x01");
        return TDX_ERR;
    }
    if (body_size + 2 > TDX_MAX_BODY_BYTES + 2) {
        tdx_error_set(err, "7709 request payload is too large (%zu bytes)", body_size);
        return TDX_ERR;
    }
    if (body_size > 0 && !body) {
        tdx_error_set(err, "request body is null but %zu bytes were requested", body_size);
        return TDX_ERR;
    }
    length = (uint16_t)(body_size + 2);

    tdx_buf_init(out);
    if (tdx_buf_reserve(out, 12 + body_size, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_push(out, prefix, err) != TDX_OK)
        goto fail;
    if (tdx_buf_append_u32le(out, message_id, err) != TDX_OK)
        goto fail;
    if (tdx_buf_push(out, 1, err) != TDX_OK) /* control */
        goto fail;
    if (tdx_buf_append_u16le(out, length, err) != TDX_OK)
        goto fail;
    if (tdx_buf_append_u16le(out, length, err) != TDX_OK)
        goto fail;
    if (tdx_buf_append_u16le(out, message_type, err) != TDX_OK)
        goto fail;
    if (tdx_buf_append(out, body, body_size, err) != TDX_OK)
        goto fail;
    return TDX_OK;

fail:
    tdx_buf_free(out);
    return TDX_ERR;
}

int tdx_frame_decode_header(const uint8_t header[TDX_RESPONSE_HEADER_SIZE],
                            tdx_response_header *out, tdx_error *err) {
    if (!header || !out) {
        tdx_error_set(err, "response header is null");
        return TDX_ERR;
    }
    if (memcmp(header, response_prefix, sizeof(response_prefix)) != 0) {
        tdx_error_set(err, "invalid 7709 response prefix");
        return TDX_ERR;
    }
    out->control = header[4];
    out->message_id = tdx_u32le(header + 5);
    out->message_type = tdx_u16le(header + 10);
    out->wire_size = tdx_u16le(header + 12);
    out->decoded_size = tdx_u16le(header + 14);
    return TDX_OK;
}

int tdx_frame_decode_body(const uint8_t *wire, size_t wire_size,
                          size_t decoded_size, tdx_buf *out, tdx_error *err) {
    if (!out) {
        tdx_error_set(err, "response buffer is null");
        return TDX_ERR;
    }
    if (wire_size > 0 && !wire) {
        tdx_error_set(err, "response body is null but %zu bytes were announced",
                      wire_size);
        return TDX_ERR;
    }
    tdx_buf_init(out);

    if (wire_size == decoded_size) {
        if (tdx_buf_append(out, wire, wire_size, err) != TDX_OK) {
            tdx_buf_free(out);
            return TDX_ERR;
        }
        return TDX_OK;
    }

    if (tdx_buf_reserve(out, decoded_size, err) != TDX_OK) {
        tdx_buf_free(out);
        return TDX_ERR;
    }
    {
        uLongf produced = (uLongf)decoded_size;
        int status;
        if (decoded_size > 0) {
            memset(out->data, 0, decoded_size);
            status = uncompress(out->data, &produced, wire, (uLong)wire_size);
        } else {
            status = Z_OK;
            produced = 0;
        }
        if (status != Z_OK) {
            tdx_buf_free(out);
            tdx_error_set(err, "7709 zlib decompression failed: %d", status);
            return TDX_ERR;
        }
        if (produced != (uLongf)decoded_size) {
            tdx_buf_free(out);
            tdx_error_set(err, "7709 decoded response length mismatch (%lu of %zu)",
                          (unsigned long)produced, decoded_size);
            return TDX_ERR;
        }
        out->len = decoded_size;
    }
    return TDX_OK;
}
