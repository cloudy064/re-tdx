/* tdx_finance_json.h - JSONL rendering for the 0x0010 finance feed.
 *
 * The record is emitted with the same grouping the C++ reference uses, so the two
 * implementations diff field for field.  Values whose meaning this port could NOT
 * verify are still emitted, under their raw names, because dropping them would
 * hide the gap rather than state it: see the note on national_shares in
 * tdx_finance.h. */
#ifndef TDX_FINANCE_JSON_H
#define TDX_FINANCE_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_finance.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One finance record, without a trailing newline. */
int tdx_finance_format(tdx_buf *out, const tdx_finance_record *record, tdx_error *err);

/* Running totals for a command that walks many batches, for the same reason the
 * snapshot command needs one: the caller cannot keep every record around. */
typedef struct tdx_finance_tally {
    size_t record_count;
    size_t records_with_listing_date;
    double total_shares_sum;
    double total_assets_sum;
} tdx_finance_tally;

void tdx_finance_tally_init(tdx_finance_tally *tally);
void tdx_finance_tally_add(tdx_finance_tally *tally, const tdx_finance_record *records,
                           size_t count);

/* The trailing summary event, without a trailing newline. */
int tdx_finance_format_summary(tdx_buf *out, const tdx_finance_tally *tally,
                               const char *endpoint, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_FINANCE_JSON_H */
