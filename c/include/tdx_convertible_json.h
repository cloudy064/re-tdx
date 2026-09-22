/* tdx_convertible_json.h - JSONL rendering for a convertible-bond overview row. */
#ifndef TDX_CONVERTIBLE_JSON_H
#define TDX_CONVERTIBLE_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_convertible.h"
#include "tdx_convertible_join.h"
#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One overview row, without a trailing newline. */
int tdx_convertible_format(tdx_buf *out, const tdx_convertible_row *row, const char *resource,
                           size_t group, size_t row_index, tdx_error *err);

/* One JOINED row: the overview group plus the progress, coupon and three trigger
 * groups the other five documents carry, and a `sources` block naming which of them
 * actually held this bond.  A row that appears in only some of the six is normal -
 * the reference unions the keys - so `sources` is how a caller tells a genuine zero
 * from a field that simply was not carried. */
int tdx_convertible_format_joined(tdx_buf *out, const tdx_convertible_row *row,
                                  const tdx_convertible_extra *extra,
                                  const tdx_convertible_join_flags *flags,
                                  const char *overview_resource, tdx_error *err);

/* The trailing summary, without a trailing newline.  The reference's own
 * completeness test is counted here, because a view of convertible bonds that are
 * missing their core terms is the thing a caller most needs to know about. */
int tdx_convertible_format_summary(tdx_buf *out, size_t rows, size_t rows_complete,
                                   size_t rows_exchangeable, size_t rows_with_underlying,
                                   const char *resource, const char *endpoint, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_CONVERTIBLE_JSON_H */
