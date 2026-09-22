/* tdx_download.c - port of the verified C++ 709/1721 transfer path. */
#include "tdx_download.h"

#include <stdio.h>
#include <string.h>

#include "tdx_md5.h"

int tdx_download_path_valid(const char *remote_path, size_t field_size, tdx_error *err) {
    size_t length;
    size_t index;
    size_t segment_start = 0;

    if (!remote_path || !*remote_path) {
        tdx_error_set(err, "resource path is empty");
        return TDX_ERR;
    }
    length = strlen(remote_path);
    if (length >= field_size) {
        tdx_error_set(err, "resource path is %zu bytes, the protocol field holds %zu", length,
                      field_size - 1);
        return TDX_ERR;
    }
    if (remote_path[0] == '/') {
        tdx_error_set(err, "resource path must be relative: %s", remote_path);
        return TDX_ERR;
    }
    for (index = 0; index < length; ++index) {
        unsigned char byte = (unsigned char)remote_path[index];
        if (byte < 0x20 || byte > 0x7E) {
            tdx_error_set(err, "resource path must be printable ASCII: %s", remote_path);
            return TDX_ERR;
        }
        if (byte == ':') {
            tdx_error_set(err, "resource path must not carry a drive letter: %s", remote_path);
            return TDX_ERR;
        }
    }
    for (index = 1; index <= length; ++index) {
        if (index < length && remote_path[index] != '/')
            continue;
        {
            size_t segment_length = index - segment_start;
            const char *segment = remote_path + segment_start;
            if (segment_length == 0) {
                tdx_error_set(err, "resource path has an empty segment: %s", remote_path);
                return TDX_ERR;
            }
            if (segment[0] == '.' &&
                (segment_length == 1 || (segment_length == 2 && segment[1] == '.'))) {
                tdx_error_set(err, "resource path must not contain . or ..: %s", remote_path);
                return TDX_ERR;
            }
        }
        segment_start = index + 1;
    }
    return TDX_OK;
}

/* Writes the path into a fixed-width, zero padded protocol field. */
static int fixed_path(const char *remote_path, size_t field_size, uint8_t *out,
                      tdx_error *err) {
    if (tdx_download_path_valid(remote_path, field_size, err) != TDX_OK)
        return TDX_ERR;
    memset(out, 0, field_size);
    memcpy(out, remote_path, strlen(remote_path));
    return TDX_OK;
}

int tdx_download_info_request(const char *remote_path, tdx_buf *out, tdx_error *err) {
    uint8_t field[TDX_DOWNLOAD_INFO_PATH_SIZE];
    if (!out) {
        tdx_error_set(err, "0x02C5 needs an output buffer");
        return TDX_ERR;
    }
    if (fixed_path(remote_path, sizeof(field), field, err) != TDX_OK)
        return TDX_ERR;
    tdx_buf_clear(out);
    return tdx_buf_append(out, field, sizeof(field), err);
}

int tdx_download_chunk_request(const char *remote_path, uint32_t offset, uint32_t length,
                              tdx_buf *out, tdx_error *err) {
    uint8_t body[TDX_DOWNLOAD_CHUNK_REQUEST_SIZE];
    if (!out) {
        tdx_error_set(err, "0x06B9 needs an output buffer");
        return TDX_ERR;
    }
    if (length < 1 || length > TDX_DOWNLOAD_CHUNK_MAX) {
        tdx_error_set(err, "0x06B9 chunk length %u is outside 1..%u", (unsigned)length,
                      (unsigned)TDX_DOWNLOAD_CHUNK_MAX);
        return TDX_ERR;
    }
    memset(body, 0, sizeof(body));
    body[0] = (uint8_t)(offset & 0xFFu);
    body[1] = (uint8_t)((offset >> 8) & 0xFFu);
    body[2] = (uint8_t)((offset >> 16) & 0xFFu);
    body[3] = (uint8_t)((offset >> 24) & 0xFFu);
    body[4] = (uint8_t)(length & 0xFFu);
    body[5] = (uint8_t)((length >> 8) & 0xFFu);
    body[6] = (uint8_t)((length >> 16) & 0xFFu);
    body[7] = (uint8_t)((length >> 24) & 0xFFu);
    if (fixed_path(remote_path, TDX_DOWNLOAD_CHUNK_PATH_SIZE, body + 8, err) != TDX_OK)
        return TDX_ERR;
    tdx_buf_clear(out);
    return tdx_buf_append(out, body, sizeof(body), err);
}

int tdx_download_parse_info(const uint8_t *body, size_t size, tdx_file_info *out,
                            tdx_error *err) {
    size_t limit;
    size_t length;
    size_t index;

    if (!body || !out) {
        tdx_error_set(err, "0x02C5 reply needs a body and an output");
        return TDX_ERR;
    }
    if (size < 38) {
        tdx_error_set(err, "0x02C5 reply is %zu bytes, expected at least 38", size);
        return TDX_ERR;
    }
    memset(out, 0, sizeof(*out));
    out->size = tdx_u32le(body);
    /* A zero length is how a node says "I do not hold this resource": every one
     * of the 48 public HQHOSTs answers the historical image path with
     * 00 00 00 00 01 followed by 33 zero bytes, i.e. size 0 and an empty digest.
     * Checking it first keeps that from being reported as a malformed digest. */
    if (out->size == 0) {
        tdx_error_set(err, "the server does not hold this resource (zero length)");
        return TDX_ERR;
    }
    if (out->size > TDX_DOWNLOAD_RESOURCE_MAX) {
        tdx_error_set(err, "the resource is %u bytes, above the %u byte safety limit",
                      (unsigned)out->size, (unsigned)TDX_DOWNLOAD_RESOURCE_MAX);
        return TDX_ERR;
    }
    out->has_md5 = body[4] != 0;

    limit = size < 37 ? size : 37;
    length = 5;
    while (length < limit && body[length] != 0)
        ++length;
    length -= 5;
    if (length >= sizeof(out->md5))
        length = sizeof(out->md5) - 1;
    memcpy(out->md5, body + 5, length);
    out->md5[length] = '\0';
    for (index = 0; index < length; ++index) {
        char ch = out->md5[index];
        if (ch >= 'A' && ch <= 'F') {
            out->md5[index] = (char)(ch - 'A' + 'a');
            continue;
        }
        if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))
            continue;
        tdx_error_set(err, "0x02C5 announced a malformed digest: %s", out->md5);
        return TDX_ERR;
    }
    if (out->has_md5 && length != 32) {
        tdx_error_set(err, "0x02C5 announced a %zu-character digest, expected 32", length);
        return TDX_ERR;
    }
    return TDX_OK;
}

int tdx_download_parse_chunk(const uint8_t *body, size_t size, uint32_t requested,
                             const uint8_t **data, size_t *length, tdx_error *err) {
    uint32_t declared;
    if (!body || !data || !length) {
        tdx_error_set(err, "0x06B9 reply needs a body and output views");
        return TDX_ERR;
    }
    if (size < 4) {
        tdx_error_set(err, "0x06B9 reply carries no length field");
        return TDX_ERR;
    }
    declared = tdx_u32le(body);
    if (declared > requested) {
        tdx_error_set(err, "0x06B9 returned %u bytes for a %u byte request", (unsigned)declared,
                      (unsigned)requested);
        return TDX_ERR;
    }
    if (size - 4 < (size_t)declared) {
        tdx_error_set(err, "0x06B9 reply is truncated: %u declared, %zu available",
                      (unsigned)declared, size - 4);
        return TDX_ERR;
    }
    *data = body + 4;
    *length = (size_t)declared;
    return TDX_OK;
}

int tdx_download_info(tdx_connection *connection, const char *remote_path,
                      tdx_file_info *out, tdx_error *err) {
    tdx_buf request;
    tdx_buf response;
    int result = TDX_ERR;

    if (!connection || !out) {
        tdx_error_set(err, "0x02C5 needs a session and an output");
        return TDX_ERR;
    }
    tdx_buf_init(&request);
    tdx_buf_init(&response);
    if (tdx_download_info_request(remote_path, &request, err) != TDX_OK)
        goto done;
    if (tdx_connection_call(connection, TDX_CMD_FILE_INFO, request.data, request.len, &response,
                            err) != TDX_OK)
        goto done;
    result = tdx_download_parse_info(response.data, response.len, out, err);

done:
    tdx_buf_free(&request);
    tdx_buf_free(&response);
    return result;
}

int tdx_download_fetch(tdx_connection *connection, const char *remote_path,
                       const tdx_file_info *info, int verify_md5, tdx_buf *out,
                       tdx_error *err) {
    tdx_buf request;
    tdx_buf response;
    int result = TDX_ERR;

    if (!connection || !info || !out) {
        tdx_error_set(err, "0x06B9 needs a session, a resource description and a buffer");
        return TDX_ERR;
    }
    tdx_buf_init(&request);
    tdx_buf_init(&response);
    tdx_buf_clear(out);
    if (info->size == 0 || info->size > TDX_DOWNLOAD_RESOURCE_MAX) {
        tdx_error_set(err, "resource size is outside the supported transfer limit");
        goto done;
    }
    if (tdx_buf_reserve(out, info->size ? info->size : 1u, err) != TDX_OK)
        goto done;

    while (out->len < info->size) {
        size_t offset = out->len;
        size_t wanted = info->size - offset;
        const uint8_t *payload = NULL;
        size_t payload_size = 0;

        if (wanted > TDX_DOWNLOAD_CHUNK_MAX)
            wanted = TDX_DOWNLOAD_CHUNK_MAX;
        if (tdx_download_chunk_request(remote_path, (uint32_t)offset, (uint32_t)wanted,
                                       &request, err) != TDX_OK)
            goto done;
        if (tdx_connection_call(connection, TDX_CMD_FILE_CHUNK, request.data, request.len,
                                &response, err) != TDX_OK)
            goto done;
        if (tdx_download_parse_chunk(response.data, response.len, (uint32_t)wanted, &payload,
                                     &payload_size, err) != TDX_OK)
            goto done;
        if (payload_size == 0) {
            tdx_error_set(err, "the server returned an empty chunk at offset %zu", offset);
            goto done;
        }
        if (payload_size > wanted) {
            tdx_error_set(err, "chunk at offset %zu delivered %zu of %zu bytes", offset,
                          payload_size, wanted);
            goto done;
        }
        if (tdx_buf_append(out, payload, payload_size, err) != TDX_OK)
            goto done;
    }

    if (verify_md5 && info->has_md5) {
        char actual[TDX_MD5_HEX_SIZE];
        tdx_md5_hex_of(out->data, out->len, actual);
        if (strcmp(actual, info->md5) != 0) {
            tdx_error_set(err, "resource digest mismatch: announced %s, received %s", info->md5,
                          actual);
            goto done;
        }
    }
    result = TDX_OK;

done:
    tdx_buf_free(&request);
    tdx_buf_free(&response);
    if (result != TDX_OK)
        tdx_buf_clear(out);
    return result;
}

int tdx_download_resource(const tdx_endpoint_pool *pool, int timeout_ms,
                          const char *remote_path, int verify_md5, tdx_buf *out,
                          tdx_file_info *info_out, char *endpoint_used,
                          size_t endpoint_used_size, tdx_error *err) {
    tdx_connection connection;
    size_t attempt;
    int result = TDX_ERR;

    if (!pool || !pool->count || !out) {
        tdx_error_set(err, "resource transfer needs an endpoint pool and a buffer");
        return TDX_ERR;
    }
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;
    for (attempt = 0; attempt < pool->count; ++attempt) {
        char address[80];
        tdx_file_info info;
        int ok;
        tdx_endpoint_address(&pool->items[attempt], address, sizeof(address));
        if (tdx_connection_open(&connection, &pool->items[attempt], timeout_ms, err) != TDX_OK)
            continue;
        ok = tdx_download_info(&connection, remote_path, &info, err) == TDX_OK &&
             tdx_download_fetch(&connection, remote_path, &info, verify_md5, out, err) == TDX_OK;
        tdx_connection_close(&connection);
        if (!ok)
            continue;
        if (info_out)
            *info_out = info;
        if (endpoint_used && endpoint_used_size)
            snprintf(endpoint_used, endpoint_used_size, "%s", address);
        result = TDX_OK;
        break;
    }
    if (result != TDX_OK && err && !err->message[0])
        tdx_error_set(err, "every endpoint failed for %s", remote_path);
    return result;
}
