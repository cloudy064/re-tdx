/* tdx_snapshot_json.h - JSONL rendering for the 0x054C snapshot feed. */
#ifndef TDX_SNAPSHOT_JSON_H
#define TDX_SNAPSHOT_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One snapshot record, without a trailing newline. */
int tdx_snapshot_format(tdx_buf *out, const tdx_snapshot *snapshot, tdx_error *err);

/* Running totals for a command that walks many batches.  The caller cannot keep
 * every record around, so the summary is fed an accumulator instead of an array. */
typedef struct tdx_snapshot_tally {
    size_t record_count;
    size_t records_with_fund_iopv;
    size_t records_with_tail;
    double amount_sum;
    double total_hand_sum;
} tdx_snapshot_tally;

void tdx_snapshot_tally_init(tdx_snapshot_tally *tally);
void tdx_snapshot_tally_add(tdx_snapshot_tally *tally, const tdx_snapshot *records,
                            size_t count);

/* The trailing summary event, without a trailing newline. */
int tdx_snapshot_format_summary(tdx_buf *out, const tdx_snapshot_tally *tally, size_t requested,
                                const char *endpoint, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_SNAPSHOT_JSON_H */
