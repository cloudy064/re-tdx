/* tdx_format.h - canonical JSON rendering for L1 records and stream events.
 *
 * Both the CLI and the resident hub must emit byte-identical payloads, so the
 * wire-facing JSON lives here exactly once. */
#ifndef TDX_FORMAT_H
#define TDX_FORMAT_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_quote.h"
#include "tdx_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Appends a JSON string literal, escaping what the grammar cannot carry. */
int tdx_format_json_string(tdx_buf *out, const char *text, tdx_error *err);

/* Appends one decoded depth record. */
int tdx_format_depth(tdx_buf *out, const tdx_depth *depth, tdx_error *err);

/* Appends the "changed" array for a diff mask. */
int tdx_format_diff_names(tdx_buf *out, tdx_diff_mask mask, tdx_error *err);

/* Appends a snapshot/change event. */
int tdx_format_depth_event(tdx_buf *out, const char *type, int64_t sequence,
                           const tdx_depth *depth, tdx_diff_mask mask,
                           uint64_t updates, tdx_error *err);

/* Appends an idle-connection heartbeat event. */
int tdx_format_heartbeat_event(tdx_buf *out, int64_t sequence, size_t subscribed,
                               uint64_t total_events, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_FORMAT_H */
