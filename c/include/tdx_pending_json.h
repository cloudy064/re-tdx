/* tdx_pending_json.h - JSONL rendering for the pending issue list. */
#ifndef TDX_PENDING_JSON_H
#define TDX_PENDING_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_pending.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One plan row, without a trailing newline. */
int tdx_pending_format(tdx_buf *out, const tdx_pending_row *row, const char *resource,
                       size_t row_index, tdx_error *err);

/* One reconciliation report, without a trailing newline.  Emitted once per projection,
 * because that is what the reference reports: the two views are compared on WHO they
 * describe, not on what values they carry. */
int tdx_pending_format_reconciliation(tdx_buf *out,
                                      const tdx_pending_reconciliation *reconciliation,
                                      const char *primary_resource,
                                      const char *projection_resource, const char *endpoint,
                                      tdx_error *err);

/* The trailing summary, without a trailing newline. */
int tdx_pending_format_summary(tdx_buf *out, size_t rows, size_t skipped, size_t projections,
                               size_t rows_common_with_every_projection,
                               const char *primary_resource, const char *endpoint,
                               tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_PENDING_JSON_H */
