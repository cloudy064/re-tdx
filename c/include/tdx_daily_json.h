/* tdx_daily_json.h - JSONL rendering for the .day daily bars. */
#ifndef TDX_DAILY_JSON_H
#define TDX_DAILY_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_daily.h"
#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One bar, without a trailing newline.  The scale divisor travels with the bar because
 * the prices cannot be read correctly without knowing it. */
int tdx_daily_format_bar(tdx_buf *out, const tdx_daily_bar *bar, const char *security_id,
                         int scale_divisor, size_t index, tdx_error *err);

/* The file-level summary, without a trailing newline. */
int tdx_daily_format_summary(tdx_buf *out, const char *source, const char *security_id,
                             int scale_divisor, size_t bytes, size_t bars,
                             size_t suspicious_prices, size_t first_date, size_t last_date,
                             tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_DAILY_JSON_H */
