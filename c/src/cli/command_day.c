/* day: command orchestration. */
#include "cli_commands.h"
#include "cli_date.h"
#include "tdx_error.h"
#include "tdx_zst_day.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



static int day_cache_dir(const cli_day_options *options, char *buffer, size_t buffer_size,
                         const char **out, tdx_error *err) {
    if (options->no_cache) {
        *out = NULL;
        return TDX_OK;
    }
    if (options->cache_dir) {
        *out = options->cache_dir;
        return TDX_OK;
    }
    if (!options->common.root[0]) {
        tdx_error_set(err, "day needs --cache-dir or --root, or --no-cache to always transfer");
        return TDX_ERR;
    }
    if (snprintf(buffer, buffer_size, "%s/T0002/zst_cache", options->common.root) >=
        (int)buffer_size) {
        tdx_error_set(err, "--root is too long to derive a cache path from");
        return TDX_ERR;
    }
    *out = buffer;
    return TDX_OK;
}


int cli_command_day(const cli_day_options *options, tdx_error *err) {
    tdx_zst_day_options day;
    tdx_zst_day_result result;
    char cache_buffer[TDX_CLI_ROOT_MAX + 32];
    const char *cache_dir = NULL;
    FILE *stream = NULL;
    int status;

    if (options->common.security_count != 1) {
        tdx_error_set(err, "day needs exactly one --security");
        return TDX_ERR;
    }
    if (!options->date) {
        tdx_error_set(err, "day needs --date YYYYMMDD");
        return TDX_ERR;
    }
    memset(&day, 0, sizeof(day));
    memset(&result, 0, sizeof(result));
    if (cli_resolve_date(options->date, day.date, NULL, err) != TDX_OK)
        return TDX_ERR;
    if (day_cache_dir(options, cache_buffer, sizeof(cache_buffer), &cache_dir, err) != TDX_OK)
        return TDX_ERR;
    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        return TDX_ERR;
    }

    day.security = options->common.securities[0];
    day.cache_dir = cache_dir;
    day.refresh = options->refresh;
    day.raw_tags = options->raw_tags;
    day.changed_only = options->changed_only;
    day.limit = options->max_records;
    day.quiet = options->common.quiet;
    day.out = stream;

    status = tdx_zst_day_run(&options->common.pool, options->common.timeout_ms, &day, &result, err);
    status = cli_finish_output(stream, status, err);
    stream = NULL;
    if (status != TDX_OK)
        return TDX_ERR;
    if (options->common.quiet)
        return TDX_OK;
    fprintf(stderr,
            "day %s %s: records=%zu emitted=%zu unchanged=%zu bytes=%zu source=%s "
            "endpoint=%s md5=%s\n",
            day.date, options->common.securities[0].code, result.records, result.emitted,
            result.silent, result.bytes, result.source,
            result.endpoint[0] ? result.endpoint : "-", result.md5[0] ? result.md5 : "-");
    if (result.dropped_tags)
        fprintf(stderr, "warning: %zu tags did not fit the replay map\n", result.dropped_tags);
    return TDX_OK;
}
