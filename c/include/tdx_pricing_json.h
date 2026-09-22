/* tdx_pricing_json.h - JSONL rendering for the pricing view. */
#ifndef TDX_PRICING_JSON_H
#define TDX_PRICING_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_pricing.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The name of a price source, for the quote block. */
const char *tdx_pricing_price_source_name(tdx_price_source source);

/* One pricing row, without a trailing newline. */
int tdx_pricing_format(tdx_buf *out, const tdx_pricing_row *row, const char *resource,
                       size_t row_index, tdx_error *err);

/* The trailing summary, without a trailing newline.  It counts how far the rows got,
 * because a view of convertible bonds where most rows are priced from a previous close
 * is a different claim from one where they are priced live. */
int tdx_pricing_format_summary(tdx_buf *out, size_t rows, size_t skipped, size_t complete,
                               size_t bond_only, size_t terms_only, size_t with_yield,
                               size_t with_pure_value, size_t live_priced, size_t pre_close_priced,
                               const char *resource, const char *as_of, const char *endpoint,
                               tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_PRICING_JSON_H */
