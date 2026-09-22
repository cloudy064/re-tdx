/* tdx_subscription_json.h - JSONL rendering for subscription events. */
#ifndef TDX_SUBSCRIPTION_JSON_H
#define TDX_SUBSCRIPTION_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_subscription.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One subscription event, without a trailing newline. */
int tdx_subscription_format(tdx_buf *out, const tdx_subscription_row *row, const char *resource,
                            size_t row_index, tdx_error *err);

/* The trailing summary, without a trailing newline.  It counts the events for which
 * each derived metric could be computed, because "how many of these have a premium" is
 * the first thing a caller needs to know about a derived field. */
int tdx_subscription_format_summary(tdx_buf *out, size_t rows, size_t skipped, size_t listed,
                                    size_t with_conversion_value, size_t with_premium,
                                    const char *resource, const char *endpoint, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_SUBSCRIPTION_JSON_H */
