/* tdx_capital_json.h - JSONL rendering for the 0x000F capital-change feed. */
#ifndef TDX_CAPITAL_JSON_H
#define TDX_CAPITAL_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_capital.h"
#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One capital-change record, without a trailing newline.
 *
 * The category decides which named fields are meaningful, and the object only
 * carries those, exactly as the C++ reference does.  The four raw float32 slots
 * and the wire reading are always included as well, so nothing the record holds is
 * hidden behind the interpretation. */
int tdx_capital_format(tdx_buf *out, const tdx_capital_record *record, tdx_error *err);

/* Running totals for a walk over many securities. */
typedef struct tdx_capital_tally {
    size_t record_count;
    size_t securities;
    size_t category_counts[16];
    int earliest_date;
    int latest_date;
} tdx_capital_tally;

void tdx_capital_tally_init(tdx_capital_tally *tally);
void tdx_capital_tally_add(tdx_capital_tally *tally, const tdx_capital_record *records,
                           size_t count);

/* The trailing summary event, without a trailing newline. */
int tdx_capital_format_summary(tdx_buf *out, const tdx_capital_tally *tally, size_t requested,
                               const char *endpoint, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_CAPITAL_JSON_H */
