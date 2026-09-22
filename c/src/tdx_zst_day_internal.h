#ifndef TDX_ZST_DAY_INTERNAL_H
#define TDX_ZST_DAY_INTERNAL_H
#include "tdx_zst_day.h"
#include "tdx_download.h"
typedef int (*tdx_zst_day_fetch)(void *context, const tdx_endpoint_pool *pool,
    int timeout_ms, const char *path, tdx_buf *payload, tdx_file_info *info,
    char *endpoint, size_t endpoint_size, tdx_error *err);
/* A per-call fetch boundary; no process-wide mutable test hook. */
int tdx_zst_day_run_with_fetch(const tdx_endpoint_pool *pool, int timeout_ms,
    const tdx_zst_day_options *options, tdx_zst_day_result *result,
    tdx_zst_day_fetch fetch, void *context, tdx_error *err);
#endif
