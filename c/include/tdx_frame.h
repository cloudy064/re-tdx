/* tdx_frame.h - 7709 request/response framing.
 *
 * Wire layout, both directions, as recovered from the client and verified
 * against live servers:
 *
 *   request  [0]      prefix      0x0C standard, 0x01 expansion
 *            [1..4]   message_id  uint32 LE
 *            [5]      control     always 1
 *            [6..7]   length      uint16 LE = body size + 2
 *            [8..9]   length      uint16 LE, repeated
 *            [10..11] message_type uint16 LE (command number)
 *            [12..]   body
 *
 *   response [0..3]   prefix      B1 CB 74 00
 *            [4]      control
 *            [5..8]   message_id  uint32 LE
 *            [9]      (unused by current peers)
 *            [10..11] message_type uint16 LE
 *            [12..13] wire_size   uint16 LE
 *            [14..15] decoded_size uint16 LE
 *            [16..]   body, zlib-compressed when wire_size != decoded_size
 */
#ifndef TDX_FRAME_H
#define TDX_FRAME_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bytes.h"
#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_REQUEST_PREFIX 0x0C
#define TDX_EXPANSION_REQUEST_PREFIX 0x01
#define TDX_RESPONSE_HEADER_SIZE 16
#define TDX_HANDSHAKE_TYPE 0x000D
#define TDX_MAX_BODY_BYTES 0xFFFD /* uint16 length field minus the 2-byte tail */

typedef struct tdx_response_header {
    uint8_t control;
    uint32_t message_id;
    uint16_t message_type;
    uint16_t wire_size;
    uint16_t decoded_size;
} tdx_response_header;

/* Encodes one request frame with the given message id.  out must be initialized;
 * its allocation is reused.  On failure out is empty.  body must not alias out. */
int tdx_frame_build_request(uint32_t message_id, uint16_t message_type,
                            const void *body, size_t body_size, uint8_t prefix,
                            tdx_buf *out, tdx_error *err);

/* Validates and decodes the fixed 16-byte response header. */
int tdx_frame_decode_header(const uint8_t header[TDX_RESPONSE_HEADER_SIZE],
                            tdx_response_header *out, tdx_error *err);

/* Expands a response body.  When wire_size == decoded_size the bytes are
 * copied verbatim; otherwise they are inflated with zlib.  out must be initialized;
 * its allocation is reused and is empty on failure.  wire must not alias out. */
int tdx_frame_decode_body(const uint8_t *wire, size_t wire_size,
                          size_t decoded_size, tdx_buf *out, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_FRAME_H */
