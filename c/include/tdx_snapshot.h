/* tdx_snapshot.h - whole-universe L1 snapshot over 0x054C.
 *
 * Ported from the verified C++ implementation (native/src/market/market_protocol.cpp).
 *
 * This is the command that makes a whole-market sweep affordable: 0x0547 takes a
 * code list and returns the book as well, while 0x054C returns the same L1 record
 * without the ladder, so a batch covers more securities per round trip.  The
 * record body is byte-identical to the 0x0547 record minus the five levels, which
 * is why this module reuses the project's varint, wire-number and price-divisor
 * helpers instead of restating them.
 *
 * Request, 10 + 7 * count bytes:
 *   [0]     5          fixed marker
 *   [1..7]  zero
 *   [8..9]  u16 count
 *   then per security: u8 market + code[6]
 *
 * Reply:
 *   [0..1]  not interpreted by this build (kept raw as header_raw)
 *   [2..3]  u16 count
 *   [4..]   the records, back to back and with no length prefix
 *
 * The records carry no lengths, so the boundaries are recovered by scanning for a
 * position where the byte is a market id (0..2) and the next six bytes are ASCII
 * digits.  That scan can in principle hit a false boundary inside a record's
 * payload, so it is only accepted when it yields exactly the declared count and
 * the first start is at offset 0; anything else is an error rather than a
 * silently mis-split series.
 *
 * Record:
 *   [0]     u8 market
 *   [1..6]  code[6]
 *   [7..8]  u16 active
 *   five varints: current, previous, open, high and low price DELTAS
 *   varint time_raw
 *   varint auxiliary_price_delta_raw
 *   varint total_hand
 *   varint current_hand
 *   u32    amount, wire encoded
 *   varint inside
 *   varint outside
 *   varint auction_imbalance_hand
 *   varint open_amount_raw, scaled by 100 on the way out
 *
 * The four non-current prices are stored as deltas against the current price, so
 * previous = (previous_delta + current_delta) * 10 and the rest follow the same
 * shape; the scale on top is the per-security divisor times 1000. */
#ifndef TDX_SNAPSHOT_H
#define TDX_SNAPSHOT_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bytes.h"
#include "tdx_endpoint.h"
#include "tdx_error.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_CMD_SNAPSHOT 0x054C

#define TDX_SNAPSHOT_REQUEST_FIXED 10
#define TDX_SNAPSHOT_CODE_STRIDE 7
/* The server's own cap for this command.  Asking for 100 on a live node returns
 * exactly 80 records, so 80 is the batch size the decoder is sized for and the
 * one the command clamps to.  The C++ service defaults to the same 80 even
 * though its session option allows more. */
#define TDX_SNAPSHOT_BATCH_MAX 80

typedef struct tdx_snapshot {
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
    /* Only set for the fund codes whose net value the client derives from the
     * auxiliary price; see tdx_snapshot_is_fund_iopv. */
    int has_fund_iopv;
    double fund_iopv;
    /* Bytes of the record the field list above does not consume.  Reported
     * rather than assumed empty. */
    size_t tail_size;
} tdx_snapshot;

/* TdxW derives an indicative net value for exchange funds (and nothing else), so
 * this predicate decides whether fund_iopv is meaningful. */
int tdx_snapshot_is_fund_iopv(const tdx_code *security);

/* --- framing ---------------------------------------------------------- */

int tdx_snapshot_build_request(const tdx_code *codes, size_t count, tdx_buf *out,
                               tdx_error *err);

/* Recovers the record boundaries.  starts must hold at least count entries;
 * *out_count receives how many were found, which the caller compares with the
 * declared count. */
int tdx_snapshot_split_records(const uint8_t *data, size_t size, size_t count, size_t *starts,
                               size_t starts_capacity, size_t *out_count, tdx_error *err);

int tdx_snapshot_parse(const uint8_t *payload, size_t size, size_t requested,
                       tdx_snapshot *out, size_t out_capacity, size_t *out_count,
                       uint16_t *header_raw, tdx_error *err);

/* Decodes one already-delimited record. */
int tdx_snapshot_parse_record(const uint8_t *record, size_t size, tdx_snapshot *out,
                              tdx_error *err);

/* --- session-bound ---------------------------------------------------- */

int tdx_snapshot_fetch(tdx_connection *connection, const tdx_code *codes, size_t count,
                       tdx_snapshot *out, size_t out_capacity, size_t *out_count,
                       tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_SNAPSHOT_H */
