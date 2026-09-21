/* tdx_download.h - 0x02C5 / 0x06B9 resource transfer over an L1 session.
 *
 * The same pair of commands that carries the JSN lists also carries the
 * historical image resources, so no new protocol is involved:
 *
 *   0x02C5 (709)  body = the remote path, ASCII, NUL padded to 40 bytes.
 *                 reply = u32 size, byte[4] has_md5, then up to 32 hex digits
 *                 of MD5; the reply is at least 38 bytes long.
 *   0x06B9 (1721) body = u32 offset, u32 requested, then the same path NUL
 *                 padded into 100 bytes, all inside a 308 byte body.
 *                 reply = u32 actual_size followed by that many bytes, and the
 *                 server never returns more than was requested.
 *
 * Chunks are capped at 30000 bytes.  That is not arbitrary: the response frame
 * carries a uint16 length, so a 30004 byte reply is already close to the ceiling
 * the framing can express.
 *
 * Paths are validated before they reach the wire because the peer resolves them
 * against its own resource root. */
#ifndef TDX_DOWNLOAD_H
#define TDX_DOWNLOAD_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bytes.h"
#include "tdx_endpoint.h"
#include "tdx_error.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_CMD_FILE_INFO 0x02C5
#define TDX_CMD_FILE_CHUNK 0x06B9

#define TDX_DOWNLOAD_INFO_PATH_SIZE 40
#define TDX_DOWNLOAD_CHUNK_PATH_SIZE 100
#define TDX_DOWNLOAD_CHUNK_REQUEST_SIZE 308
#define TDX_DOWNLOAD_CHUNK_MAX 30000u
#define TDX_DOWNLOAD_RESOURCE_MAX (512u * 1024u * 1024u)

typedef struct tdx_file_info {
    uint32_t size;
    int has_md5;
    char md5[33]; /* lower-case hex, empty when the server did not send one */
} tdx_file_info;

/* Rejects a path the peer must not resolve: empty, too long for the fixed-width
 * field, non-ASCII, absolute, or carrying a "." / ".." / ':' segment. */
int tdx_download_path_valid(const char *remote_path, size_t field_size, tdx_error *err);

/* Builds one 0x02C5 body. */
int tdx_download_info_request(const char *remote_path, tdx_buf *out, tdx_error *err);

/* Builds one 0x06B9 body.  length must be in 1..TDX_DOWNLOAD_CHUNK_MAX. */
int tdx_download_chunk_request(const char *remote_path, uint32_t offset, uint32_t length,
                               tdx_buf *out, tdx_error *err);

/* Validates a 0x02C5 reply body. */
int tdx_download_parse_info(const uint8_t *body, size_t size, tdx_file_info *out,
                            tdx_error *err);

/* Validates a 0x06B9 reply body and points *data at its payload. */
int tdx_download_parse_chunk(const uint8_t *body, size_t size, uint32_t requested,
                             const uint8_t **data, size_t *length, tdx_error *err);

/* --- session-bound transfers ------------------------------------------ */

int tdx_download_info(tdx_connection *connection, const char *remote_path,
                      tdx_file_info *out, tdx_error *err);

/* Streams the whole resource into out, which is cleared first.  When
 * verify_md5 is set and the server announced a digest, a mismatch fails the
 * transfer: a short or reordered chunk stream must never look complete. */
int tdx_download_fetch(tdx_connection *connection, const char *remote_path,
                       const tdx_file_info *info, int verify_md5, tdx_buf *out,
                       tdx_error *err);

/* --- endpoint pool convenience ---------------------------------------- */

/* Opens one of the pool's endpoints (trying each in turn), fetches the resource
 * and closes the session again.  endpoint_used may be NULL. */
int tdx_download_resource(const tdx_endpoint_pool *pool, int timeout_ms,
                          const char *remote_path, int verify_md5, tdx_buf *out,
                          tdx_file_info *info_out, char *endpoint_used,
                          size_t endpoint_used_size, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_DOWNLOAD_H */
