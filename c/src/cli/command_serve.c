/* serve: command orchestration. */
#include "cli_commands.h"
#include "tdx_error.h"
#include "tdx_hub.h"
#include "tdx_pool.h"
#include "tdx_quote.h"
#include "tdx_serve.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



typedef struct serve_fetch_context {
    const tdx_code *codes;
    size_t code_count;
    tdx_code *batch; /* scratch, one entry per code */
    tdx_pool *pool;
    tdx_sweep_stats last;
} serve_fetch_context;

static int serve_fetch(void *context, const size_t *indices, size_t count,
                       tdx_depth *out, size_t *batches_out, tdx_error *err) {
    serve_fetch_context *state = (serve_fetch_context *)context;
    tdx_sweep_stats stats;
    size_t index;

    if (count > state->code_count) {
        tdx_error_set(err, "the hub asked for %zu of %zu securities", count,
                      state->code_count);
        return TDX_ERR;
    }
    /* The pool works on securities, the hub schedules by universe index. */
    for (index = 0; index < count; ++index)
        state->batch[index] = state->codes[indices[index]];

    memset(&stats, 0, sizeof(stats));
    if (tdx_pool_run(state->pool, state->batch, count, out, count, &stats, err) !=
        TDX_OK) {
        /* The batches a failed round managed to send still cost upstream, so they are
         * reported rather than discarded. */
        if (batches_out)
            *batches_out = stats.batches;
        state->last = stats;
        return TDX_ERR;
    }
    if (batches_out)
        *batches_out = stats.batches;
    state->last = stats;
    return TDX_OK;
}


int cli_command_serve(const cli_serve_options *options, tdx_error *err) {
    tdx_code *codes = NULL;
    size_t count = 0;
    tdx_hub *hub = NULL;
    tdx_pool *pool = NULL;
    tdx_hub_options hub_options;
    tdx_serve_options serve_options;
    tdx_sweep_options sweep;
    serve_fetch_context fetch;
    int result = TDX_ERR;

    if (cli_build_universe(&options->common, &codes, &count, err) != TDX_OK)
        return TDX_ERR;
    sweep.connections = options->connections;
    sweep.batch_size = options->batch_size;
    sweep.timeout_ms = options->common.timeout_ms;
    sweep.max_attempts = 3;
    memset(&fetch, 0, sizeof(fetch));
    fetch.codes = codes;
    fetch.code_count = count;
    fetch.batch = (tdx_code *)calloc(count, sizeof(*fetch.batch));
    if (!fetch.batch) {
        tdx_error_set(err, "out of memory for the poll scratch list");
        goto done;
    }

    if (tdx_pool_create(&pool, &options->common.pool, &sweep, err) != TDX_OK)
        goto done;
    fetch.pool = pool;
    tdx_hub_options_default(&hub_options);
    hub_options.max_subscribers = options->max_subscribers;
    hub_options.subscriber_queue_limit = TDX_HUB_DEFAULT_QUEUE_LIMIT;
    hub_options.interval_ms = options->interval_ms;
    hub_options.heartbeat_ms = options->heartbeat_ms;
    hub_options.tier_warm_ms = options->tier_warm_ms;
    hub_options.idle_interval_ms = options->idle_interval_ms;
    hub_options.idle_rounds = options->idle_rounds;
    if (tdx_hub_create(&hub, codes, count, &hub_options, serve_fetch, &fetch,
                       err) != TDX_OK)
        goto done;
    if (tdx_hub_start(hub, err) != TDX_OK)
        goto done;
    tdx_serve_options_default(&serve_options);
    serve_options.port = options->port;
    fprintf(stderr, "polling %zu securities every %d ms over %zu sessions\n", count,
            options->interval_ms, options->connections);
    if (tdx_serve_run(hub, codes, count, &serve_options, err) != TDX_OK)
        goto done;
    result = TDX_OK;

done:
    /* Stop consumers before releasing the fetch context and pool. */
    if (hub)
        tdx_hub_destroy(hub);
    if (pool)
        tdx_pool_destroy(pool);
    free(fetch.batch);
    free(codes);
    return result;
}
