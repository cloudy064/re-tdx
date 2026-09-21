/* tdx_zst_json.h - JSON rendering of a replayed zst snapshot.
 *
 * This sits beside tdx_format.c rather than inside it: the L1 formatter renders a
 * 0x0547 depth record whose semantics are the live wire's, while a zst snapshot
 * has no current-hand, no inside/outside dishes, and carries aggregates the live
 * record does not.  Sharing one function would force both to speak the weaker
 * schema.
 *
 * Units, because the two files disagree and the field names must not hide it:
 *   volume / *_volume  shares (10, 1H, 1J and the ladder volumes)
 *   amount             yuan (1A)
 *   trade_count        trades (09)
 * Every key is always present; a value the stream never delivered is null, so a
 * consumer never has to guess whether a 0 means "zero" or "unknown". */
#ifndef TDX_ZST_JSON_H
#define TDX_ZST_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_zst_replay.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Appends one JSON object without a trailing newline.  market_id is the market
 * of the security the caller actually asked for, in the 0x0547 convention
 * (0 Shenzhen, 1 Shanghai, 2 Beijing); it is a parameter rather than a snapshot
 * member because the key's own digits do not use that numbering.
 * include_raw_tags adds the merged tag map as a "fields" object. */
int tdx_zst_format_snapshot(tdx_buf *out, const tdx_zst_snapshot *snapshot, int market_id,
                            tdx_zst_diff_mask changed, int include_raw_tags, tdx_error *err);

/* Appends the changed-group names as a JSON array, tdx_format.c style. */
int tdx_zst_format_diff_names(tdx_buf *out, tdx_zst_diff_mask mask, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_ZST_JSON_H */
