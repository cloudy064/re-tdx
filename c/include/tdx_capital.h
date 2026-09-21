/* tdx_capital.h - share-capital changes and ex-rights events over 0x000F.
 *
 * Ported from the verified C++ implementation (native/src/corporate/).
 *
 * Request and reply shapes
 * ------------------------
 * The request is the same one 0x0010 uses, and the CLI sends one security per
 * request because the reply's header echoes a single security:
 *
 *   request   2 + 7 * N bytes: u16 count, then per security u8 market + code[6]
 *   reply     11 bytes: u16 block_count, u8 market, code[6], u16 count
 *             then count records of 29 bytes
 *
 * Because the header identifies exactly one security, a multi-security request
 * would leave the parser unable to attribute the blocks, so this module takes one
 * security and verifies that the echoed market and code match the request.  The
 * length is checked as exactly 11 + count * 29.
 *
 * The record
 * ----------
 *   [0]      u8 market
 *   [1..6]   code[6]
 *   [7]      u8 reserved
 *   [8..11]  u32 date, YYYYMMDD
 *   [12]     u8 category
 *   [13..28] four u32 slots, read TWO ways
 *
 * The dual reading is the whole subtlety of this record and it is not a guess:
 * the C++ reference decodes each slot both as a float32 and as a wire number
 * (the same exponent-format encoding the quote records use for money), and the
 * category decides which view is meaningful.  Both views are kept, because
 * discarding one would silently pick a side.
 *
 * Categories, with the reference's own labels:
 *   1  ex-rights and ex-dividend      2  bonus/rights shares listed
 *   3  non-tradable shares listed     4  state share placement
 *   5  share capital change           6  seasoned new issue
 *   7  share buyback                  8  seasoned issue listed
 *   9  transferred placement listed  10  convertible bond listed
 *  11  share expansion or shrink     12  non-tradable share shrink
 *  13  subscription warrant grant    14  put warrant grant
 *  15  restructuring adjustment
 *
 * Which view each category uses:
 *   category 1        floats: dividend per 10 shares, rights price,
 *                     bonus+transfer per 10 shares, rights per 10 shares
 *   category 11 or 12 float[2] is the shrink ratio
 *   category 13 or 14 float[0] is the exercise price, float[2] the warrant units
 *   category 15       all four floats, unlabelled
 *   everything else   the wire view, scaled by 10000, is the share count:
 *                     circulating before, total before, circulating after,
 *                     total after
 *
 * The per-10-share basis of category 1 is a live-verified fact, not an
 * assumption: the reference records Ping An Bank's 2024 "10 for 7.19" dividend as
 * 7.19 in that slot.  A caller that treats it as per-share inflates a cash
 * dividend tenfold, so this module exposes both bases explicitly. */
#ifndef TDX_CAPITAL_H
#define TDX_CAPITAL_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bytes.h"
#include "tdx_endpoint.h"
#include "tdx_error.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_CMD_CAPITAL_CHANGES 0x000F
#define TDX_CAPITAL_CHANGES_SIZE 29
#define TDX_CAPITAL_CHANGES_HEADER 11
#define TDX_CAPITAL_REQUEST_FIXED 2
#define TDX_CAPITAL_CODE_STRIDE 7

/* The four dual-read slots. */
#define TDX_CAPITAL_SLOTS 4
/* Bound on the records a caller has to hold for one security.  Ping An Bank's
 * whole history is 81 records and it is one of the longest, so this is generous. */
#define TDX_CAPITAL_RECORDS_MAX 4096

typedef struct tdx_capital_record {
    tdx_code security;
    uint8_t reserved;
    int date; /* YYYYMMDD, 0 when the field carried no date */
    uint8_t category;
    /* The float32 reading of the four slots. */
    double float_values[TDX_CAPITAL_SLOTS];
    /* The wire-number reading, scaled by 10000 into shares. */
    double share_values[TDX_CAPITAL_SLOTS];
} tdx_capital_record;

/* A short machine-readable name for a category, or "unknown".  The reference's
 * Chinese labels are in the header comment and in c/README.md. */
const char *tdx_capital_category_key(unsigned category);

/* --- framing ---------------------------------------------------------- */

int tdx_capital_build_request(const tdx_code *security, tdx_buf *out, tdx_error *err);

/* Verifies that the header echoes the requested security, then decodes. */
int tdx_capital_parse(const uint8_t *payload, size_t size, const tdx_code *expected,
                      tdx_capital_record *out, size_t capacity, size_t *out_count,
                      size_t *block_count, tdx_error *err);

int tdx_capital_parse_record(const uint8_t *record, size_t size, tdx_capital_record *out,
                             tdx_error *err);

/* --- session-bound ---------------------------------------------------- */

int tdx_capital_fetch(tdx_connection *connection, const tdx_code *security,
                      tdx_capital_record *out, size_t capacity, size_t *out_count,
                      size_t *block_count, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_CAPITAL_H */
