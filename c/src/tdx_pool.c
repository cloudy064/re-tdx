/* tdx_pool.c - persistent worker pool over reused 7709 sessions.
 *
 * Workers are started once and parked on a generation counter.  A round is a
 * generation bump followed by a wait for every worker to report completion;
 * results land in disjoint slices of the caller's array, so the hot path needs
 * no locking beyond the generation handshake. */
#include "tdx_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tdx_frame.h"
#include "tdx_thread.h"

typedef struct pool_worker {
    struct tdx_pool *pool;
    size_t index;
    tdx_connection connection;
    int connected;
    size_t endpoint_cursor;
    /* Per-round counters, reset before every generation. */
    size_t connections_opened;
    size_t upstream_requests;
    size_t retries;
    size_t failed_batches;
    char first_error[256];
    tdx_thread thread;
} pool_worker;

struct tdx_pool {
    tdx_endpoint_pool endpoints;
    tdx_sweep_options options;
    pool_worker *workers;
    size_t worker_count;

    tdx_mutex lock;
    tdx_cond cond;

    /* Current generation description, read by workers under the lock. */
    uint64_t generation;
    size_t finished;
    const tdx_code *codes;
    size_t code_count;
    size_t batch_count;
    tdx_depth *records;
    size_t *batch_records;
    int stopping;
    int running;
};

void tdx_sweep_options_default(tdx_sweep_options *options) {
    if (!options)
        return;
    options->connections = 6;
    options->batch_size = TDX_DEPTH_BATCH_MAX;
    options->timeout_ms = 10000;
    options->max_attempts = 3;
}

static void worker_note(pool_worker *worker, const char *message) {
    if (worker->first_error[0] == '\0')
        snprintf(worker->first_error, sizeof(worker->first_error), "%s", message);
}

static void worker_reset(pool_worker *worker) {
    worker->connections_opened = 0;
    worker->upstream_requests = 0;
    worker->retries = 0;
    worker->failed_batches = 0;
    worker->first_error[0] = '\0';
}

/* Polls one contiguous batch range with this worker's own session. */
static void worker_run(pool_worker *worker, const tdx_code *codes, size_t code_count,
                       size_t batch_count, tdx_depth *records, size_t *batch_records,
                       size_t *batch_size_out) {
    tdx_pool *pool = worker->pool;
    const size_t batch_size = pool->options.batch_size;
    const size_t first = (worker->index * batch_count) / pool->worker_count;
    const size_t last = ((worker->index + 1) * batch_count) / pool->worker_count;
    tdx_buf request;
    tdx_buf response;
    tdx_error error;
    size_t batch;

    *batch_size_out = 0;
    tdx_buf_init(&request);
    tdx_buf_init(&response);
    error.message[0] = '\0';

    for (batch = first; batch < last; ++batch) {
        const size_t begin = batch * batch_size;
        size_t take = code_count - begin;
        int attempt;
        int done = 0;

        if (take > batch_size)
            take = batch_size;
        if (tdx_quote_build_depth_request(codes + begin, take, &request, &error) != TDX_OK) {
            worker_note(worker, error.message);
            worker->failed_batches++;
            continue;
        }
        for (attempt = 0; attempt < pool->options.max_attempts && !done; ++attempt) {
            size_t parsed = 0;
            if (!worker->connected) {
                const tdx_endpoint *endpoint =
                    &pool->endpoints.items[worker->endpoint_cursor %
                                           pool->endpoints.count];
                worker->endpoint_cursor++;
                if (attempt > 0)
                    worker->retries++;
                if (tdx_connection_open(&worker->connection, endpoint,
                                        pool->options.timeout_ms, &error) != TDX_OK) {
                    worker_note(worker, error.message);
                    continue;
                }
                worker->connected = 1;
                worker->connections_opened++;
            }
            worker->upstream_requests++;
            if (tdx_connection_call(&worker->connection, TDX_CMD_DEPTH, request.data,
                                    request.len, &response, &error) == TDX_OK &&
                tdx_quote_parse_depth_response(response.data, response.len,
                                               codes + begin, take, records + begin,
                                               batch_size, &parsed, &error) == TDX_OK) {
                if (parsed != take) {
                    /* Fewer records than requested is a silent truncation and is
                     * treated exactly like a failed batch. */
                    tdx_error_set(&error, "depth batch returned %zu of %zu records",
                                  parsed, take);
                } else {
                    batch_records[batch] = parsed;
                    done = 1;
                }
            }
            if (!done)
                worker_note(worker, error.message);
            tdx_buf_free(&response);
            if (!done) {
                /* A stale or desynchronised session is retired, not reused. */
                tdx_connection_close(&worker->connection);
                worker->connected = 0;
            }
        }
        if (!done) {
            worker->failed_batches++;
            batch_records[batch] = 0;
        }
    }
    tdx_buf_free(&request);
    tdx_buf_free(&response);
}

static void pool_worker_main(void *context) {
    pool_worker *worker = (pool_worker *)context;
    tdx_pool *pool = worker->pool;
    uint64_t seen = 0;

    for (;;) {
        const tdx_code *codes;
        size_t code_count;
        size_t batch_count;
        tdx_depth *records;
        size_t *batch_records;
        size_t counted = 0;

        tdx_mutex_lock(&pool->lock);
        while (!pool->stopping && pool->generation == seen)
            tdx_cond_wait(&pool->cond, &pool->lock, 1000);
        if (pool->stopping) {
            tdx_mutex_unlock(&pool->lock);
            break;
        }
        seen = pool->generation;
        codes = pool->codes;
        code_count = pool->code_count;
        batch_count = pool->batch_count;
        records = pool->records;
        batch_records = pool->batch_records;
        tdx_mutex_unlock(&pool->lock);

        worker_reset(worker);
        worker_run(worker, codes, code_count, batch_count, records, batch_records,
                   &counted);

        tdx_mutex_lock(&pool->lock);
        pool->finished++;
        if (pool->finished >= pool->worker_count)
            tdx_cond_broadcast(&pool->cond);
        tdx_mutex_unlock(&pool->lock);
    }

    if (worker->connected) {
        tdx_connection_close(&worker->connection);
        worker->connected = 0;
    }
}

int tdx_pool_create(tdx_pool **out, const tdx_endpoint_pool *endpoints,
                    const tdx_sweep_options *options, tdx_error *err) {
    tdx_sweep_options defaults;
    tdx_pool *pool;
    size_t index;

    if (!out || !endpoints) {
        tdx_error_set(err, "pool needs an output slot and an endpoint pool");
        return TDX_ERR;
    }
    tdx_sweep_options_default(&defaults);
    if (!options)
        options = &defaults;
    if (options->connections < 1 || options->connections > TDX_POOL_MAX_WORKERS) {
        tdx_error_set(err, "pool connections must be in 1..%d", TDX_POOL_MAX_WORKERS);
        return TDX_ERR;
    }
    if (options->batch_size < 1 || options->batch_size > TDX_DEPTH_BATCH_MAX) {
        tdx_error_set(err, "pool batch size must be in 1..%d", TDX_DEPTH_BATCH_MAX);
        return TDX_ERR;
    }
    if (options->max_attempts < 1 || options->max_attempts > 10) {
        tdx_error_set(err, "pool max attempts must be in 1..10");
        return TDX_ERR;
    }
    if (endpoints->count == 0) {
        tdx_error_set(err, "pool needs at least one endpoint");
        return TDX_ERR;
    }

    pool = (tdx_pool *)calloc(1, sizeof(*pool));
    if (!pool) {
        tdx_error_set(err, "out of memory for the poll pool");
        return TDX_ERR;
    }
    pool->endpoints = *endpoints;
    pool->options = *options;
    pool->worker_count = options->connections;
    pool->workers = (pool_worker *)calloc(pool->worker_count, sizeof(*pool->workers));
    if (!pool->workers) {
        tdx_error_set(err, "out of memory for %zu workers", pool->worker_count);
        free(pool);
        return TDX_ERR;
    }
    if (tdx_mutex_init(&pool->lock, err) != TDX_OK) {
        free(pool->workers);
        free(pool);
        return TDX_ERR;
    }
    if (tdx_cond_init(&pool->cond, err) != TDX_OK) {
        tdx_mutex_destroy(&pool->lock);
        free(pool->workers);
        free(pool);
        return TDX_ERR;
    }
    pool->worker_count = 0;
    for (index = 0; index < options->connections; ++index) {
        pool_worker *worker = &pool->workers[index];
        worker->pool = pool;
        worker->index = index;
        worker->endpoint_cursor = index;
        worker->connection.socket_handle = (intptr_t)-1;
        if (tdx_thread_start(&worker->thread, pool_worker_main, worker, err) != TDX_OK) {
            size_t joined;
            tdx_mutex_lock(&pool->lock);
            pool->stopping = 1;
            tdx_cond_broadcast(&pool->cond);
            tdx_mutex_unlock(&pool->lock);
            for (joined = 0; joined < pool->worker_count; ++joined)
                tdx_thread_join(&pool->workers[joined].thread);
            tdx_cond_destroy(&pool->cond);
            tdx_mutex_destroy(&pool->lock);
            free(pool->workers);
            free(pool);
            return TDX_ERR;
        }
        pool->worker_count++;
    }
    *out = pool;
    return TDX_OK;
}

void tdx_pool_destroy(tdx_pool *pool) {
    size_t index;
    if (!pool)
        return;
    tdx_mutex_lock(&pool->lock);
    pool->stopping = 1;
    tdx_cond_broadcast(&pool->cond);
    tdx_mutex_unlock(&pool->lock);
    for (index = 0; index < pool->worker_count; ++index)
        tdx_thread_join(&pool->workers[index].thread);
    tdx_cond_destroy(&pool->cond);
    tdx_mutex_destroy(&pool->lock);
    free(pool->workers);
    free(pool);
}

int tdx_pool_run(tdx_pool *pool, const tdx_code *codes, size_t count, tdx_depth *out,
                 size_t out_capacity, tdx_sweep_stats *stats, tdx_error *err) {
    size_t batch_count;
    size_t *batch_records;
    size_t index;
    size_t total = 0;
    size_t failed = 0;
    int64_t started;

    if (!pool || !codes || !out || !stats) {
        tdx_error_set(err, "pool run arguments are null");
        return TDX_ERR;
    }
    memset(stats, 0, sizeof(*stats));
    stats->securities = count;
    if (count == 0)
        return TDX_OK;
    if (out_capacity < count) {
        tdx_error_set(err, "pool output capacity %zu is smaller than %zu securities",
                      out_capacity, count);
        return TDX_ERR;
    }

    batch_count = (count + pool->options.batch_size - 1) / pool->options.batch_size;
    batch_records = (size_t *)calloc(batch_count, sizeof(*batch_records));
    if (!batch_records) {
        tdx_error_set(err, "out of memory for %zu batch counters", batch_count);
        return TDX_ERR;
    }
    stats->batches = batch_count;

    tdx_mutex_lock(&pool->lock);
    /* Never start a round while the previous one is still in flight.  The
     * flag is round-scoped: a fresh pool is idle, not "unfinished". */
    while (pool->running && !pool->stopping)
        tdx_cond_wait(&pool->cond, &pool->lock, 1000);
    if (pool->stopping) {
        tdx_mutex_unlock(&pool->lock);
        free(batch_records);
        tdx_error_set(err, "the pool is shutting down");
        return TDX_ERR;
    }
    pool->codes = codes;
    pool->code_count = count;
    pool->batch_count = batch_count;
    pool->records = out;
    pool->batch_records = batch_records;
    pool->finished = 0;
    pool->generation++;
    pool->running = 1;
    started = tdx_monotonic_ms();
    tdx_cond_broadcast(&pool->cond);
    while (pool->finished < pool->worker_count)
        tdx_cond_wait(&pool->cond, &pool->lock, 1000);
    pool->running = 0;
    pool->codes = NULL;
    pool->records = NULL;
    pool->batch_records = NULL;

    for (index = 0; index < pool->worker_count; ++index) {
        const pool_worker *worker = &pool->workers[index];
        stats->connections_opened += worker->connections_opened;
        stats->upstream_requests += worker->upstream_requests;
        stats->retries += worker->retries;
        if (worker->first_error[0] != '\0' && stats->first_error[0] == '\0')
            snprintf(stats->first_error, sizeof(stats->first_error), "%s",
                     worker->first_error);
    }
    tdx_mutex_unlock(&pool->lock);
    stats->elapsed_ms = tdx_monotonic_ms() - started;

    for (index = 0; index < batch_count; ++index) {
        total += batch_records[index];
        if (batch_records[index] == 0)
            failed++;
    }
    stats->records = total;
    stats->failed_batches = failed;
    free(batch_records);

    if (failed != 0 || total != count) {
        tdx_error_set(err, "round incomplete: %zu/%zu records in %zu batches (%s)",
                      total, count, batch_count,
                      stats->first_error[0] ? stats->first_error : "no error detail");
        return TDX_ERR;
    }
    return TDX_OK;
}

int tdx_sweep(const tdx_code *codes, size_t count, const tdx_endpoint_pool *endpoints,
              const tdx_sweep_options *options, tdx_depth *out, size_t out_capacity,
              tdx_sweep_stats *stats, tdx_error *err) {
    tdx_pool *pool = NULL;
    int result;

    if (tdx_pool_create(&pool, endpoints, options, err) != TDX_OK)
        return TDX_ERR;
    result = tdx_pool_run(pool, codes, count, out, out_capacity, stats, err);
    tdx_pool_destroy(pool);
    return result;
}
