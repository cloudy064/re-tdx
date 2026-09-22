/* tdx_finance.h - batch fundamental data over 0x0010.
 *
 * Ported from the verified C++ implementation (native/src/corporate/).
 *
 * This is not market data, and it is a different kind of command: one request
 * carries a whole list of securities and the reply is a dense array of fixed
 * 143-byte records, so there is no paging, no delta coding and no order book.
 * The strict length check is therefore the whole integrity story: 2 + count * 143
 * or nothing.
 *
 * Request, 2 + 7 * count bytes:
 *   u16 count
 *   per security: u8 market + code[6]
 *
 * Reply:
 *   u16 count, then count records of 143 bytes:
 *     [0]     u8 market
 *     [1..6]  code[6]
 *     [7..142] a 136-byte information block, 34 four-byte little-endian slots
 *
 * The block is a mix of widths, so the slot index is not the only thing that
 * matters:
 *   +0   f32 circulating shares      (unit 10k shares)
 *   +4   u16 province id, u16 industry id
 *   +8   u32 updated date, u32 listing date   (YYYYMMDD)
 *   +16  30 f32 values: the remaining share classes, per-share figures, then the
 *        balance sheet, the income statement and the cash-flow line, most of them
 *        scaled by 1000 into yuan.
 *
 * The two scales are the reason this module normalises on the way out rather than
 * handing back raw floats: shares arrive in ten-thousands and money in thousands,
 * so a caller that multiplies a raw share count by a price is off by 10^4.
 *
 * What is verified and what is not
 * --------------------------------
 * A whole-market pass over 5,574 A-shares backs the following, and the test
 * asserts exactly these:
 *
 *   * province_id and industry_id are non-zero on every record;
 *   * the six sampled listing dates match the real listings, and 5,564 of 5,574
 *     records carry one at all;
 *   * the share scale is pinned by Moutai's 1,250,081,562 total shares and ICBC's
 *     86,794,040,000 H shares, both public figures;
 *   * net_assets_per_share * total_shares reproduces net_assets within 2% for
 *     91.8% of the market - a wrong slot or scale could not land that often.
 *
 * NOT verified, and deliberately not asserted anywhere: national_shares,
 * promoter_legal_shares and legal_shares.  ICBC decodes to 4.27e12 legal-person
 * shares against 3.56e11 total, Moutai to 893,893,520,000 against 1,250,081,562,
 * and three large banks all report 19,660,000 "national" shares.  Those fields are
 * bound exactly as the C++ reference binds them and scaled the same way; the gap
 * is the reference's, and inventing a different binding here would hide it.  The
 * neighbouring circulating/total/b_share/h_share values in the same records are
 * all correct, so the record is not shifted - only these slots are uncalibrated.
 *
 * reserved_2_raw is likewise kept raw.  Across 5,574 securities it takes just two
 * values (6 and 9), so it is a marker and not a per-company quantity; the C++
 * reference left it unnamed for the same reason. */
#ifndef TDX_FINANCE_H
#define TDX_FINANCE_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bytes.h"
#include "tdx_endpoint.h"
#include "tdx_error.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_CMD_FINANCE 0x0010

#define TDX_FINANCE_RECORD_SIZE 143
#define TDX_FINANCE_INFO_SIZE 136
#define TDX_FINANCE_REQUEST_FIXED 2
#define TDX_FINANCE_CODE_STRIDE 7
/* The C++ takes the whole list in one request; the batch bound here is the same
 * 16-bit count field, and the CLI keeps it modest so a reply stays inside the
 * frame's uint16 length. */
#define TDX_FINANCE_BATCH_MAX 500
/* A whole-market walk of 0x0010 meets a transient server timeout after a few
 * dozen clean batches, so a caller walking many batches should retry one. */
#define TDX_FINANCE_ATTEMPTS 3

typedef struct tdx_finance_record {
    tdx_code security;

    /* Identifiers and dates, already in their final form. */
    uint16_t province_id;
    uint16_t industry_id;
    int updated_date; /* YYYYMMDD */
    int listing_date; /* YYYYMMDD */

    /* Share classes, in shares. */
    double circulating_shares;
    double total_shares;
    double national_shares;
    double promoter_legal_shares;
    double legal_shares;
    double b_shares;
    double h_shares;

    /* Per-share figures, unitless. */
    double eps;
    double net_assets_per_share;
    double shareholder_count;

    /* Balance sheet, in yuan. */
    double total_assets;
    double current_assets;
    double fixed_assets;
    double intangible_assets;
    double current_liabilities;
    double long_term_liabilities;
    double capital_reserve;
    double net_assets;
    double accounts_receivable;
    double inventory;

    /* Income statement and cash flow, in yuan. */
    double revenue;
    double main_profit;
    double operating_profit;
    double investment_income;
    double total_profit;
    double after_tax_profit;
    double net_profit;
    double undistributed_profit;
    double operating_cash_flow;
    double total_cash_flow;

    /* The last slot has no established meaning; kept raw rather than named. */
    double reserved_2_raw;
} tdx_finance_record;

/* --- framing ---------------------------------------------------------- */

int tdx_finance_build_request(const tdx_code *codes, size_t count, tdx_buf *out,
                              tdx_error *err);

/* Refuses a body whose length is not exactly 2 + count * 143, and a record whose
 * market or code is not a security. */
int tdx_finance_parse(const uint8_t *payload, size_t size, tdx_finance_record *out,
                      size_t capacity, size_t *out_count, tdx_error *err);

int tdx_finance_parse_record(const uint8_t *record, size_t size, tdx_finance_record *out,
                             tdx_error *err);

/* --- session-bound ---------------------------------------------------- */

int tdx_finance_fetch(tdx_connection *connection, const tdx_code *codes, size_t count,
                      tdx_finance_record *out, size_t capacity, size_t *out_count,
                      tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_FINANCE_H */
