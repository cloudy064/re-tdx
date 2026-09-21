/* tdx_limits_json.h - JSONL rendering for the 0x0452 special-limit list. */
#ifndef TDX_LIMITS_JSON_H
#define TDX_LIMITS_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_limits.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One special-limit record, without a trailing newline.  index is the row number
 * the walk reached, which is what a caller needs to resume from a given point. */
int tdx_limits_format(tdx_buf *out, const tdx_limit_record *record, unsigned index,
                      tdx_error *err);

/* The trailing summary event, without a trailing newline. */
int tdx_limits_format_summary(tdx_buf *out, const tdx_limit_record *records, size_t count,
                              unsigned start_index, unsigned next_index, const char *endpoint,
                              tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_LIMITS_JSON_H */
