/* tdx_newbond_json.h - JSONL rendering for the projection and its reconciliation. */
#ifndef TDX_NEWBOND_JSON_H
#define TDX_NEWBOND_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_newbond.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One projection row with its match, without a trailing newline.  The match travels
 * with the row, because a projection row on its own is not the useful object - what it
 * agrees or disagrees with the subscription list about is. */
int tdx_newbond_format_row(tdx_buf *out, const tdx_newbond_row *row,
                           const tdx_newbond_match *match, size_t row_index, tdx_error *err);

/* The reconciliation summary, without a trailing newline. */
int tdx_newbond_format_reconciliation(tdx_buf *out,
                                      const tdx_newbond_reconciliation *reconciliation,
                                      const char *primary_resource,
                                      const char *projection_resource, const char *endpoint,
                                      tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_NEWBOND_JSON_H */
