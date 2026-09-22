#include "cli_commands.h"
#include <stdio.h>
#include <stdlib.h>
#include "tdx_pool.h"

int cli_command_sweep(const cli_sweep_options *options, tdx_error *err) {
    tdx_code *codes = NULL;
    size_t count = 0;
    tdx_depth *records = NULL;
    tdx_sweep_options sweep;
    FILE *stream = NULL;
    size_t round;
    int result = TDX_ERR;

    if (cli_build_universe(&options->common, &codes, &count, err) != TDX_OK)
        return TDX_ERR;
    records = (tdx_depth *)calloc(count, sizeof(*records));
    if (!records) {
        tdx_error_set(err, "out of memory for %zu records", count);
        free(codes);
        return TDX_ERR;
    }
    sweep.connections = options->connections;
    sweep.batch_size = options->batch_size;
    sweep.timeout_ms = options->common.timeout_ms;
    sweep.max_attempts = 3;

    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s", options->common.output);
        goto done;
    }
    if (stream != stdout && fputs("[\n", stream) == EOF) {
        tdx_error_set(err, "cannot start the sweep JSON array");
        goto done;
    }

    for (round = 0; round < options->iterations; ++round) {
        tdx_sweep_stats stats;
        size_t index;
        if (tdx_sweep(codes, count, &options->common.pool, &sweep, records, count, &stats,
                      err) != TDX_OK) {
            fprintf(stderr,
                    "round %zu failed: %s (records %zu/%zu, failed batches %zu, "
                    "%lld ms)\n",
                    round + 1, err->message, stats.records, stats.securities,
                    stats.failed_batches, (long long)stats.elapsed_ms);
            goto done;
        }
        fprintf(stderr,
                "round %zu: %zu/%zu records, %zu batches, %zu sessions, %zu requests, "
                "%zu retries, %lld ms, %.0f records/s\n",
                round + 1, stats.records, stats.securities, stats.batches,
                stats.connections_opened, stats.upstream_requests, stats.retries,
                (long long)stats.elapsed_ms,
                stats.elapsed_ms > 0
                    ? (double)stats.records * 1000.0 / (double)stats.elapsed_ms
                    : 0.0);
        if (round + 1 != options->iterations)
            continue;
        for (index = 0; index < count; ++index) {
            if (stream == stdout) {
                if (cli_depth_json(stream, &records[index], err) != TDX_OK)
                    goto done;
                if (fputc('\n', stream) == EOF) {
                    tdx_error_set(err, "cannot finish the sweep JSON line");
                    goto done;
                }
            } else {
                if (fputs("  ", stream) == EOF) {
                    tdx_error_set(err, "cannot write the sweep JSON array");
                    goto done;
                }
                if (cli_depth_json(stream, &records[index], err) != TDX_OK)
                    goto done;
                if (fputs(index + 1 < count ? ",\n" : "\n", stream) == EOF) {
                    tdx_error_set(err, "cannot finish the sweep JSON element");
                    goto done;
                }
            }
        }
    }
    if (stream != stdout) {
        if (fputs("]\n", stream) == EOF) {
            tdx_error_set(err, "cannot finish the sweep JSON array");
            goto done;
        }
        fprintf(stderr, "wrote %zu records to %s\n", count, options->common.output);
    }
    result = TDX_OK;

done:
    result = cli_finish_output(stream, result, err);
    free(records);
    free(codes);
    return result;
}
