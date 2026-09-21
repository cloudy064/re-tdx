/* tdx_pool.h - parallel batch polling over a pool of persistent sessions.
 *
 * Two entry points share one implementation:
 *
 *   tdx_pool_*   a long-lived pool.  Worker threads and their 7709 sessions are
 *                created once and reused for every round, so a resident service
 *                does not pay a TCP + 0x000D handshake per round.
 *   tdx_sweep    the one-shot convenience wrapper used by the CLI commands.
 */
#ifndef TDX_POOL_H
#define TDX_POOL_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_endpoint.h"
#include "tdx_error.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_POOL_MAX_WORKERS 32

typedef struct tdx_sweep_options {
    /* Parallel 7709 sessions. 1..TDX_POOL_MAX_WORKERS. */
    size_t connections;
    /* Securities per 0x0547 request. 1..TDX_DEPTH_BATCH_MAX. */
    size_t batch_size;
    int timeout_ms;
    /* Extra attempts (with endpoint failover) when a batch fails. */
    int max_attempts;
} tdx_sweep_options;

typedef struct tdx_sweep_stats {
    size_t securities;
    size_t batches;
    size_t records;
    size_t failed_batches;
    size_t connections_opened;
    size_t upstream_requests;
    size_t retries;
    int64_t elapsed_ms;
    char first_error[256];
} tdx_sweep_stats;

typedef struct tdx_pool tdx_pool;

void tdx_sweep_options_default(tdx_sweep_options *options);

int tdx_pool_create(tdx_pool **out, const tdx_endpoint_pool *endpoints,
                    const tdx_sweep_options *options, tdx_error *err);
void tdx_pool_destroy(tdx_pool *pool);

/* Polls every security once.  out must hold at least count records, which keep
 * the order of the input regardless of which worker fetched them.
 *
 * The call fails when any batch could not be fetched or when a response carried
 * fewer records than requested, so a silently truncated round can never be
 * reported as complete.  Stats are filled in either way. */
int tdx_pool_run(tdx_pool *pool, const tdx_code *codes, size_t count, tdx_depth *out,
                 size_t out_capacity, tdx_sweep_stats *stats, tdx_error *err);

/* Creates a pool, runs exactly one round and tears it down. */
int tdx_sweep(const tdx_code *codes, size_t count, const tdx_endpoint_pool *endpoints,
              const tdx_sweep_options *options, tdx_depth *out, size_t out_capacity,
              tdx_sweep_stats *stats, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_POOL_H */
