/* tdx_limits.h - the special-treatment price-limit list over 0x0452.
 *
 * Ported from the verified C++ implementation (native/src/corporate/).
 *
 * This is the list of securities whose price limits are NOT the standard ten
 * percent: the ST and *ST names, whose band is five percent.  It matters because
 * the cached daily image carries limit-up and limit-down prices too, and the
 * relationship those follow - roughly previous close times 1.1 - does not hold for
 * anything on this list.  So this command is both data and the explanation for why
 * a limit field computed from the usual rule disagrees with the cached one.
 *
 * Request, 14 bytes:
 *   u16 start_index, then twelve zero bytes
 *
 * Reply:
 *   u16 count, then count records of 13 bytes:
 *     [0]     u8 market
 *     [1..4]  u32 code NUMBER, zero-padded to six digits for display
 *     [5..8]  f32 limit up price
 *     [9..12] f32 limit down price
 *
 * The list is paged by index and the server answers ONE row per request: a request
 * from start_index 0 comes back with a single record, and the walk advances by an
 * explicit index until a page is empty.  So a one-record page is the normal case
 * and only an empty page ends the list - treating "a short page" as the end stops
 * the walk after the first row, which is a mistake this port made and fixed.
 * The length is checked as exactly 2 + count * 13 on every page. */
#ifndef TDX_LIMITS_H
#define TDX_LIMITS_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bytes.h"
#include "tdx_endpoint.h"
#include "tdx_error.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_CMD_SPECIAL_LIMITS 0x0452
#define TDX_LIMITS_RECORD_SIZE 13
#define TDX_LIMITS_REQUEST_SIZE 14
#define TDX_LIMITS_START_INDEX_MAX 65535u
/* A page bound.  The server's own page size is small; this is the limit the
 * decoder and the CLI will hold, and a page larger than it is refused rather than
 * silently truncated. */
#define TDX_LIMITS_PAGE_MAX 4096
/* A cap for one walk, so a server that never returns an empty page cannot make
 * the caller allocate forever.  The list was 13 rows when it was probed. */
#define TDX_LIMITS_MAX_RECORDS 4096
#define TDX_LIMITS_MAX_PAGES 8192

typedef struct tdx_limit_record {
    tdx_code security;
    uint32_t code_number;
    double limit_up;
    double limit_down;
} tdx_limit_record;

/* --- framing ---------------------------------------------------------- */

int tdx_limits_build_request(unsigned start_index, tdx_buf *out, tdx_error *err);

/* index_of_first is the page's start index, so each record can be numbered. */
int tdx_limits_parse(const uint8_t *payload, size_t size, unsigned index_of_first,
                     tdx_limit_record *out, size_t capacity, size_t *out_count,
                     unsigned *indices, tdx_error *err);

int tdx_limits_parse_record(const uint8_t *record, size_t size, unsigned index,
                            tdx_limit_record *out, tdx_error *err);

/* --- session-bound ---------------------------------------------------- */

/* One page. */
int tdx_limits_fetch(tdx_connection *connection, unsigned start_index, tdx_limit_record *out,
                     size_t capacity, size_t *out_count, unsigned *indices, tdx_error *err);

/* Walks the pages from start_index until a page comes back empty, the caller's
 * cap is reached, or max_pages is hit.  Returns how many records were stored and
 * reports the index the walk stopped at, so a caller can resume. */
int tdx_limits_fetch_all(tdx_connection *connection, unsigned start_index, size_t max_records,
                         unsigned max_pages, tdx_limit_record *out, size_t capacity,
                         size_t *out_count, unsigned *next_index, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_LIMITS_H */
