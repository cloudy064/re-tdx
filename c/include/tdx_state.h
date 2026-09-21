/* tdx_state.h - last-known-value table and change detection.
 *
 * The wire protocol has no delta: every 0x0547 reply repeats the full record.
 * Incrementality therefore has to be produced here, by comparing each decoded
 * record against what we already stored and reporting only what differs. */
#ifndef TDX_STATE_H
#define TDX_STATE_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_error.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Field groups that can differ between two records.  A mask of zero means the
 * record is byte-for-byte equivalent for every field we track. */
typedef unsigned tdx_diff_mask;

#define TDX_DIFF_NEW 0x0001u    /* first observation of this security */
#define TDX_DIFF_LAST 0x0002u   /* last price */
#define TDX_DIFF_OHLC 0x0004u   /* open / high / low / previous close */
#define TDX_DIFF_AMOUNT 0x0008u /* turnover */
#define TDX_DIFF_VOLUME 0x0010u /* total and current hand */
#define TDX_DIFF_DISHES 0x0020u /* inside / outside / auction imbalance */
#define TDX_DIFF_BOOK 0x0040u   /* five level order book */
#define TDX_DIFF_STATUS 0x0080u /* status, update time, trailing counters */

typedef struct tdx_state_entry {
    tdx_code security;
    tdx_depth depth;
    uint64_t updates; /* how many times this security changed */
    uint64_t observations;
    int used;
} tdx_state_entry;

typedef struct tdx_state {
    tdx_state_entry *entries;
    size_t bucket_count; /* always a power of two, zero when uninitialised */
    size_t count;
} tdx_state;

/* Compares two decoded records.  Because the decoder is deterministic an exact
 * comparison is meaningful: equal numbers mean an equal wire record. */
tdx_diff_mask tdx_depth_diff(const tdx_depth *previous, const tdx_depth *current);

/* Renders the mask into field names, newest-value ordering.  Returns the number
 * of names written, never more than capacity. */
size_t tdx_diff_names(tdx_diff_mask mask, const char **names, size_t capacity);

int tdx_state_init(tdx_state *state, size_t expected, tdx_error *err);
void tdx_state_free(tdx_state *state);
size_t tdx_state_count(const tdx_state *state);

/* Inserts or updates one record.  *mask receives the change mask (TDX_DIFF_NEW
 * when the security was not present before), and *entry the stored row after
 * the update.  Both out parameters may be NULL. */
int tdx_state_apply(tdx_state *state, const tdx_depth *depth, tdx_diff_mask *mask,
                    const tdx_state_entry **entry, tdx_error *err);

const tdx_state_entry *tdx_state_find(const tdx_state *state, const tdx_code *code);

#ifdef __cplusplus
}
#endif

#endif /* TDX_STATE_H */
