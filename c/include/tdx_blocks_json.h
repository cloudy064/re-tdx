/* tdx_blocks_json.h - JSONL rendering for the block families. */
#ifndef TDX_BLOCKS_JSON_H
#define TDX_BLOCKS_JSON_H

#include <stddef.h>

#include "tdx_blocks.h"
#include "tdx_bytes.h"
#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One block, without a trailing newline.  The hierarchy travels with it: the parent id, the
 * level and whether it is a leaf are the whole point of this family. */
int tdx_blocks_format_block(tdx_buf *out, const tdx_block *block, size_t index,
                            tdx_error *err);

/* One membership, without a trailing newline. */
int tdx_blocks_format_member(tdx_buf *out, const tdx_block_member *member, size_t index,
                             tdx_error *err);

/* One security's industry assignment, without a trailing newline. */
int tdx_blocks_format_assignment(tdx_buf *out, const tdx_block_assignment *assignment,
                                 size_t index, tdx_error *err);

/* The trailing summary.  The load report is part of it: which files were read, and how many
 * infoharbor blocks declared a member count that did not match what followed. */
int tdx_blocks_format_summary(tdx_buf *out, size_t blocks, size_t members, size_t assignments,
                              const tdx_blocks_load_report *report, const char *root,
                              tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_BLOCKS_JSON_H */
