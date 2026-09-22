/* Internal file boundary shared by local data readers and the historical cache. */
#ifndef TDX_CACHE_H
#define TDX_CACHE_H
#include <stdio.h>
#include "tdx_bytes.h"

typedef enum tdx_cache_read_status {
    TDX_CACHE_ERROR = -1, TDX_CACHE_MISSING = 0, TDX_CACHE_FOUND = 1
} tdx_cache_read_status;
tdx_cache_read_status tdx_cache_read(const char *path, tdx_buf *out, tdx_error *err);
int tdx_cache_write(const char *path, const tdx_buf *data, tdx_error *err);

/* Per-call operations for deterministic failure tests. NULL entries use real I/O.
 * close must close the FILE even when it reports failure; replace must leave the
 * original target intact on failure. Temporary creation always uses exclusive I/O. */
typedef struct tdx_cache_write_ops {
    void *context;
    size_t (*write)(void *context, const void *data, size_t size, FILE *file);
    int (*close)(void *context, FILE *file);
    int (*replace)(void *context, const char *temporary, const char *path);
} tdx_cache_write_ops;
int tdx_cache_write_with_ops(const char *path, const tdx_buf *data,
                             const tdx_cache_write_ops *ops, tdx_error *err);
#endif
