/* tdx_minute_json.h - JSONL rendering for the .lc1 minute bars. */
#ifndef TDX_MINUTE_JSON_H
#define TDX_MINUTE_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_minute.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One bar, without a trailing newline.  The two trailing words are rendered under their
 * raw names because their meaning is not established. */
int tdx_lc1_format_bar(tdx_buf *out, const tdx_lc1_bar *bar, const char *security_id,
                       size_t index, tdx_error *err);

/* The file-level summary, without a trailing newline.  ohlc_violations travels with it
 * because a caller reading these bars should know how many break the range the reference
 * would have refused. */
int tdx_lc1_format_summary(tdx_buf *out, const char *source, const char *security_id,
                           size_t bytes, size_t bars, size_t ohlc_violations,
                           size_t distinct_dates, size_t first_date, size_t last_date,
                           tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_MINUTE_JSON_H */
