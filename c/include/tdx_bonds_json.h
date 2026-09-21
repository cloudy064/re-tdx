/* tdx_bonds_json.h - JSONL rendering for a normalized bond reference row. */
#ifndef TDX_BONDS_JSON_H
#define TDX_BONDS_JSON_H

#include <stddef.h>

#include "tdx_bonds.h"
#include "tdx_bytes.h"
#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One bond row, without a trailing newline.  The resource and the row number are
 * carried so a caller can trace a value back to its source row. */
int tdx_bonds_format(tdx_buf *out, const tdx_bond_row *row, const char *resource, size_t group,
                     size_t row_index, tdx_error *err);

/* Appends the coupon-schedule array of a row as a JSON array, given the two column
 * keys to pair.  Exposed separately because the reference defers this expansion:
 * the all-bond archive holds long historical sequences, and paying for them on
 * every row would dominate a scan that does not read them. */
int tdx_bonds_format_schedule(tdx_buf *out, const tdx_jsn_document *doc,
                              const tdx_jsn_group *group, size_t row_in_group,
                              const char *dates_key, const char *rates_key, size_t capacity,
                              tdx_error *err);

/* The trailing summary event, without a trailing newline.  It takes counters rather
 * than the rows, because a whole-market archive is tens of thousands of rows and
 *. keeping them all just to count three things would dominate the scan. */
int tdx_bonds_format_summary(tdx_buf *out, size_t rows, size_t rows_with_name,
                             size_t rows_with_underlying, size_t rows_with_size,
                             const char *resource, const char *scale_name, const char *endpoint,
                             tdx_error *err);

/* The reference's own name for a scale, for the summary. */
const char *tdx_bonds_scale_name(tdx_bond_scale scale);

#ifdef __cplusplus
}
#endif

#endif /* TDX_BONDS_JSON_H */
