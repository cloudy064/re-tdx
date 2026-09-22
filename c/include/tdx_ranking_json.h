/* tdx_ranking_json.h - JSONL rendering for the 0x054B category ranking. */
#ifndef TDX_RANKING_JSON_H
#define TDX_RANKING_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_ranking.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One ranked security, without a trailing newline.  The two unmodelled byte runs travel with
 * it as hex, and so does the sort key the server echoed. */
int tdx_ranking_format(tdx_buf *out, const tdx_ranking_record *record, size_t index,
                       uint16_t category, uint16_t sort, tdx_error *err);

/* The trailing summary, without a trailing newline. */
int tdx_ranking_format_summary(tdx_buf *out, uint16_t category, uint16_t sort, int ascending,
                               size_t records, size_t pages, const char *endpoint,
                               tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_RANKING_JSON_H */
