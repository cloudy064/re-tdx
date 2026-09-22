/* tdx_panorama_json.h - JSONL rendering for the market panorama. */
#ifndef TDX_PANORAMA_JSON_H
#define TDX_PANORAMA_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_panorama.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One registry entry, without a trailing newline.  The field list travels with it, so a caller
 * reading only the catalog knows what a view will produce. */
int tdx_panorama_format_view(tdx_buf *out, const tdx_panorama_view *view, size_t index,
                             tdx_error *err);

/* One projected row, without a trailing newline.  The fields are named by the registry, so the
 * output reads as data rather than as a bag of column codes. */
int tdx_panorama_format_row(tdx_buf *out, const tdx_panorama_view *view,
                            const tdx_panorama_row *row, size_t index, tdx_error *err);

/* The trailing summary, without a trailing newline. */
int tdx_panorama_format_summary(tdx_buf *out, const char *view_id, const char *resource,
                                size_t rows, size_t skipped, const char *endpoint,
                                tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_PANORAMA_JSON_H */
