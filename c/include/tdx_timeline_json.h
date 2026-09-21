/* tdx_timeline_json.h - JSONL rendering for the time-share series. */
#ifndef TDX_TIMELINE_JSON_H
#define TDX_TIMELINE_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_timeline.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One point, without a trailing newline. */
int tdx_timeline_format_point(tdx_buf *out, const tdx_timeline *timeline,
                              const tdx_timeline_point *point, int market_id, const char *code,
                              tdx_error *err);

/* The trailing summary event, without a trailing newline. */
int tdx_timeline_format_summary(tdx_buf *out, const tdx_timeline *timeline, int market_id,
                                const char *code, const char *endpoint, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_TIMELINE_JSON_H */
