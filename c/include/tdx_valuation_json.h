/* tdx_valuation_json.h - JSONL rendering for the index valuation family. */
#ifndef TDX_VALUATION_JSON_H
#define TDX_VALUATION_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_valuation.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One index, without a trailing newline.  The detail id travels with it because it is what
 * names the three detail resources. */
int tdx_valuation_format_index(tdx_buf *out, const tdx_valuation_index *index, size_t row,
                               tdx_error *err);

/* One merged history point, without a trailing newline. */
int tdx_valuation_format_point(tdx_buf *out, const tdx_valuation_point *point,
                               const char *security_id, size_t row, tdx_error *err);

/* The merge report, without a trailing newline.  It is emitted because "the two halves
 * describe one series" is a claim worth showing rather than assuming. */
int tdx_valuation_format_merge(tdx_buf *out, const tdx_valuation_merge *merge,
                               const char *security_id, const char *pe_resource,
                               const char *pb_resource, tdx_error *err);

/* One fund, without a trailing newline. */
int tdx_valuation_format_fund(tdx_buf *out, const tdx_valuation_fund *fund, size_t row,
                              tdx_error *err);

/* The file-level summary, without a trailing newline. */
int tdx_valuation_format_summary(tdx_buf *out, const char *resource, size_t indices,
                                 size_t skipped, const char *endpoint, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_VALUATION_JSON_H */
