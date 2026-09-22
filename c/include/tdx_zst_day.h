/* tdx_zst_day.h - "one security, one date" historical L1 retrieval.
 *
 * Ties the pieces together: build the resource path, take the resource from the
 * local cache when it is already there, otherwise transfer it with 0x02C5/0x06B9,
 * inflate and parse the container, fold the change-only stream into full
 * snapshots and emit one JSON object per snapshot.
 *
 * Resource naming, confirmed against the capture in output/tap-tdxw.jsonl:
 *   remote  hishf/date/<YYYYMMDD>/<sz|sh|bj><code>.img
 *   cache   <root>/T0002/zst_cache/<sz|sh|bj><code>_<YYYYMMDD>.img
 * i.e. the client's own cache name is the remote name with the date moved from
 * the directory into the file name. */
#ifndef TDX_ZST_DAY_H
#define TDX_ZST_DAY_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "tdx_endpoint.h"
#include "tdx_error.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tdx_zst_day_options {
    tdx_code security;
    char date[9];      /* YYYYMMDD */
    const char *cache_dir; /* NULL or empty disables both cache reads and writes */
    int refresh;       /* ignore a cached copy and transfer again */
    int raw_tags;      /* add the merged tag map to every line */
    int changed_only;  /* skip snapshots whose change mask is zero */
    int quiet;
    size_t limit;      /* 0 = every record in the file */
    FILE *out;
} tdx_zst_day_options;

typedef struct tdx_zst_day_result {
    size_t records;
    size_t emitted;
    size_t silent;
    size_t dropped_tags;
    size_t bytes;
    int downloaded;
    char source[512];
    char remote_path[128];
    char endpoint[80];
    char md5[33];
} tdx_zst_day_result;

/* Validates the date and renders both names.  remote may be NULL, local may be
 * NULL, but at least one is expected. */
int tdx_zst_day_build_paths(const tdx_code *security, const char *date,
                            const char *cache_dir, char *remote, size_t remote_size,
                            char *local, size_t local_size, tdx_error *err);

/* Runs one retrieval.  pool may be NULL when the cache already holds the file;
 * a cache miss without a pool is an error. */
int tdx_zst_day_run(const tdx_endpoint_pool *pool, int timeout_ms,
                    const tdx_zst_day_options *options, tdx_zst_day_result *result,
                    tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_ZST_DAY_H */
