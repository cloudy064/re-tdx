/* tdx_quote.h - 7709 quote session, security identity and 0x0547 decoding.
 *
 * 0x0547 returns, for every requested security, one variable-length record
 * carrying both the L1 snapshot and the five-level order book.  The record is
 * a varint stream; price fields are stored as deltas against the current
 * price, which is itself stored as a scaled integer.  Everything below is a
 * byte-for-byte port of the verified C++ decoder. */
#ifndef TDX_QUOTE_H
#define TDX_QUOTE_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bytes.h"
#include "tdx_endpoint.h"
#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_CMD_DEPTH 0x0547
#define TDX_DEPTH_LEVELS 5
#define TDX_DEPTH_BATCH_MAX 100 /* server-side cap observed on live nodes */
#define TDX_QUOTE_TAIL_MAX 64
/* 0x054C and its batch bounds live in tdx_snapshot.h, next to the code that
 * actually speaks that command. */

typedef struct tdx_code {
    int market_id; /* 0 Shenzhen, 1 Shanghai, 2 Beijing */
    char code[8];  /* six ASCII digits plus terminator */
} tdx_code;

/* Accepts "sz000001", "SZ000001", "sz:000001" or a bare "000001". */
int tdx_code_parse(const char *text, tdx_code *out, tdx_error *err);
/* Renders the canonical "SZ000001" form. */
void tdx_code_id(const tdx_code *code, char *out, size_t out_size);
/* Wire decimal policy: bonds and repos carry two extra digits, exchange
 * funds one.  Returns the divisor applied on top of the 1000 base scale. */
int tdx_price_divisor(const char *code);

typedef struct tdx_level {
    double price;
    int64_t volume_hand;
} tdx_level;

typedef struct tdx_depth {
    tdx_code security;
    uint16_t active;
    double last;
    double previous;
    double open;
    double high;
    double low;
    int64_t time_raw;
    int64_t auxiliary_price_delta_raw;
    int64_t total_hand;
    int64_t current_hand;
    double amount;
    int64_t inside;
    int64_t outside;
    int64_t auction_imbalance_hand;
    double open_amount;
    uint32_t update_time;
    int64_t status;
    int64_t unknown_after_outer;
    tdx_level buys[TDX_DEPTH_LEVELS];
    tdx_level sells[TDX_DEPTH_LEVELS];
    /* Trailing bytes the current client build does not consume.  Kept as a
     * count only: the C port must not invent semantics for them. */
    size_t tail_size;
} tdx_depth;

/* --- primitive codecs, exposed for direct unit testing ------------- */

/* Variable-length signed integer: 6 magnitude bits in the first byte, sign in
 * bit 6, continuation in bit 7. */
int tdx_varint_decode(const uint8_t *data, size_t size, size_t *offset,
                      int64_t *out, tdx_error *err);

/* Decodes the 32-bit float-ish wire encoding used by turnover fields. */
double tdx_wire_number(uint32_t value);

/* --- session ------------------------------------------------------ */

typedef struct tdx_connection {
    tdx_endpoint endpoint;
    intptr_t socket_handle; /* -1 when closed */
    uint32_t next_message_id;
    char server_name[TDX_ENDPOINT_NAME_MAX];
    int timeout_ms;
} tdx_connection;

/* Connects and performs the 0x000D handshake, capturing the server name. */
int tdx_connection_open(tdx_connection *connection, const tdx_endpoint *endpoint,
                        int timeout_ms, tdx_error *err);
void tdx_connection_close(tdx_connection *connection);

/* Sends one command and returns the decoded body. out must be initialized;
 * its allocation is reused, and its length is zero on failure. */
int tdx_connection_call(tdx_connection *connection, uint16_t message_type,
                        const void *body, size_t body_size, tdx_buf *out,
                        tdx_error *err);

/* --- 0x0547 ------------------------------------------------------- */

/* out must be initialized; rebuilds reuse its allocation. Failure leaves an
 * empty buffer that remains owned by the caller. codes must not alias out. */
int tdx_quote_build_depth_request(const tdx_code *codes, size_t count,
                                  tdx_buf *out, tdx_error *err);

/* payload is the decoded 0x0547 body.  It is XOR-obfuscated with 0x93 and is
 * mutated in place by this call, matching the peer behaviour. */
int tdx_quote_parse_depth_response(uint8_t *payload, size_t size,
                                   const tdx_code *requested,
                                   size_t requested_count, tdx_depth *out,
                                   size_t out_capacity, size_t *out_count,
                                   tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_QUOTE_H */
