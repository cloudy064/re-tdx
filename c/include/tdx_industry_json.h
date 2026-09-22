/* tdx_industry_json.h - JSONL rendering for the industry resource. */
#ifndef TDX_INDUSTRY_JSON_H
#define TDX_INDUSTRY_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_industry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One industry, without a trailing newline.  Both membership counts travel with it and so
 * does whether they agree, because that comparison is the resource checking itself. */
int tdx_industry_format(tdx_buf *out, const tdx_industry *industry, size_t index,
                        tdx_error *err);

/* One stock-to-industry row, without a trailing newline. */
int tdx_industry_format_row(tdx_buf *out, const tdx_industry_row *row, size_t index,
                            tdx_error *err);

/* The file-level summary, without a trailing newline. */
int tdx_industry_format_summary(tdx_buf *out, const char *resource, size_t rows,
                                size_t skipped, size_t industries, size_t agreeing,
                                size_t inconsistent, const char *endpoint, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_INDUSTRY_JSON_H */
