#include "cli_commands.h"
#include <stdio.h>
#include <stdlib.h>
#include "tdx_pool.h"
#include "tdx_format.h"
#include "tdx_thread.h"

int cli_command_watch(const cli_watch_options *options, tdx_error *err) {
    tdx_code *codes = NULL;
    size_t count = 0;
    tdx_depth *records = NULL;
    tdx_state state;
    tdx_pool *pool = NULL;
    tdx_sweep_options sweep;
    tdx_buf line = {0};
    FILE *stream = NULL;
    size_t round = 0;
    int result = TDX_ERR;
    int state_ready = 0;
    uint64_t total_events = 0;

    if (cli_build_universe(&options->common, &codes, &count, err) != TDX_OK)
        return TDX_ERR;
    records = (tdx_depth *)calloc(count, sizeof(*records));
    if (!records) {
        tdx_error_set(err, "out of memory for %zu records", count);
        free(codes);
        return TDX_ERR;
    }
    if (tdx_state_init(&state, count, err) != TDX_OK)
        goto done;
    state_ready = 1;
    sweep.connections = options->connections;
    sweep.batch_size = options->batch_size;
    sweep.timeout_ms = options->common.timeout_ms;
    sweep.max_attempts = 3;
    if (tdx_pool_create(&pool, &options->common.pool, &sweep, err) != TDX_OK)
        goto done;

    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s", options->common.output);
        goto done;
    }

    for (round = 0;; ++round) {
        tdx_sweep_stats stats;
        size_t index;
        size_t changed = 0;

        if (tdx_pool_run(pool, codes, count, records, count, &stats,
                      err) != TDX_OK) {
            fprintf(stderr, "round %zu failed: %s (%zu/%zu records, %lld ms)\n",
                    round + 1, err->message, stats.records, stats.securities,
                    (long long)stats.elapsed_ms);
            goto done;
        }
        for (index = 0; index < count; ++index) {
            tdx_diff_mask mask = 0;
            const tdx_state_entry *entry = NULL;
            if (tdx_state_apply(&state, &records[index], &mask, &entry, err) != TDX_OK)
                goto done;
            if (mask == 0)
                continue;
            changed++;
            total_events++;
            tdx_buf_clear(&line);
            if (tdx_format_depth_round_event(&line, (mask & TDX_DIFF_NEW) ? "snapshot" : "change",
                                              round + 1, &records[index], mask,
                                              entry ? entry->updates : 0, err) != TDX_OK ||
                cli_write_json_line(stream, &line, err) != TDX_OK)
                goto done;
        }
        if (changed == 0) {
            tdx_buf_clear(&line);
            if (tdx_format_heartbeat_round_event(&line, round + 1, count, total_events, err) != TDX_OK ||
                cli_write_json_line(stream, &line, err) != TDX_OK)
                goto done;
        }
        if (fflush(stream) != 0) {
            tdx_error_set(err, "cannot flush watch JSON output");
            goto done;
        }
        fprintf(stderr,
                "round %zu: %zu/%zu records, %zu changed (%.0f%%), %zu batches, "
                "%lld ms, %zu sessions\n",
                round + 1, stats.records, stats.securities, changed,
                count ? (double)changed * 100.0 / (double)count : 0.0, stats.batches,
                (long long)stats.elapsed_ms, stats.connections_opened);

        if (options->iterations != 0 && round + 1 >= options->iterations)
            break;
        tdx_sleep_ms(options->interval_ms);
    }
    result = TDX_OK;

done:
    if (pool)
        tdx_pool_destroy(pool);
    if (state_ready)
        tdx_state_free(&state);
    result = cli_finish_output(stream, result, err);
    tdx_buf_free(&line);
    free(records);
    free(codes);
    return result;
}
