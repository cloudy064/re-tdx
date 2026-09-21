/* tdx_zst_day.c - historical L1 retrieval and replay for one security and date. */
#include "tdx_zst_day.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tdx_bytes.h"
#include "tdx_download.h"
#include "tdx_zst.h"
#include "tdx_zst_json.h"
#include "tdx_zst_replay.h"

static const char *market_prefix(int market_id) {
    switch (market_id) {
    case 1:
        return "sh";
    case 2:
        return "bj";
    default:
        return "sz";
    }
}

static int date_valid(const char *date, tdx_error *err) {
    int year;
    int month;
    int day;
    size_t index;
    if (!date || strlen(date) != 8) {
        tdx_error_set(err, "--date must be eight digits, YYYYMMDD");
        return TDX_ERR;
    }
    for (index = 0; index < 8; ++index)
        if (date[index] < '0' || date[index] > '9') {
            tdx_error_set(err, "--date must be eight digits, YYYYMMDD");
            return TDX_ERR;
        }
    year = (date[0] - '0') * 1000 + (date[1] - '0') * 100 + (date[2] - '0') * 10 +
           (date[3] - '0');
    month = (date[4] - '0') * 10 + (date[5] - '0');
    day = (date[6] - '0') * 10 + (date[7] - '0');
    if (year < 1990 || year > 2100 || month < 1 || month > 12 || day < 1 || day > 31) {
        tdx_error_set(err, "--date %s is not a usable calendar date", date);
        return TDX_ERR;
    }
    return TDX_OK;
}

int tdx_zst_day_build_paths(const tdx_code *security, const char *date,
                            const char *cache_dir, char *remote, size_t remote_size,
                            char *local, size_t local_size, tdx_error *err) {
    const char *prefix;
    if (!security) {
        tdx_error_set(err, "day needs a security");
        return TDX_ERR;
    }
    if (date_valid(date, err) != TDX_OK)
        return TDX_ERR;
    if (strlen(security->code) != 6) {
        tdx_error_set(err, "day needs a six-digit code, got %s", security->code);
        return TDX_ERR;
    }
    prefix = market_prefix(security->market_id);
    if (remote) {
        if (snprintf(remote, remote_size, "hishf/date/%s/%s%s.img", date, prefix,
                     security->code) >= (int)remote_size) {
            tdx_error_set(err, "remote resource path does not fit %zu bytes", remote_size);
            return TDX_ERR;
        }
    }
    if (local) {
        if (!cache_dir || !*cache_dir) {
            tdx_error_set(err, "no cache directory is configured");
            return TDX_ERR;
        }
        if (snprintf(local, local_size, "%s/%s%s_%s.img", cache_dir, prefix, security->code,
                     date) >= (int)local_size) {
            tdx_error_set(err, "cache path does not fit %zu bytes", local_size);
            return TDX_ERR;
        }
    }
    return TDX_OK;
}

/* Reads a whole file into a buffer. */
static int read_whole_file(const char *path, tdx_buf *out, tdx_error *err) {
    FILE *file = fopen(path, "rb");
    long size;
    if (!file) {
        tdx_error_set(err, "cannot open %s", path);
        return TDX_ERR;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) <= 0) {
        fclose(file);
        tdx_error_set(err, "%s is not a usable cache entry", path);
        return TDX_ERR;
    }
    rewind(file);
    tdx_buf_clear(out);
    if (tdx_buf_reserve(out, (size_t)size, err) != TDX_OK) {
        fclose(file);
        return TDX_ERR;
    }
    if (fread(out->data, 1, (size_t)size, file) != (size_t)size) {
        fclose(file);
        tdx_error_set(err, "short read on %s", path);
        return TDX_ERR;
    }
    fclose(file);
    out->len = (size_t)size;
    return TDX_OK;
}

/* Best effort: a cache that cannot be written is a warning, never a failure,
 * because the bytes are already in hand. */
static void write_cache(const char *path, const tdx_buf *data, int quiet) {
    char temporary[600];
    FILE *file;
    if (snprintf(temporary, sizeof(temporary), "%s.part", path) >= (int)sizeof(temporary))
        return;
    file = fopen(temporary, "wb");
    if (!file) {
        if (!quiet)
            fprintf(stderr, "warning: cannot cache to %s\n", temporary);
        return;
    }
    if (fwrite(data->data, 1, data->len, file) != data->len) {
        fclose(file);
        remove(temporary);
        if (!quiet)
            fprintf(stderr, "warning: short write caching %s\n", temporary);
        return;
    }
    fclose(file);
    remove(path);
    if (rename(temporary, path) != 0) {
        remove(temporary);
        if (!quiet)
            fprintf(stderr, "warning: cannot move the cache entry into %s\n", path);
    }
}

typedef struct day_context {
    const tdx_zst_day_options *options;
    tdx_zst_day_result *result;
    tdx_buf line;
    int limit_reached;
} day_context;

static int day_visit(void *context, const tdx_zst_snapshot *snapshot,
                     const tdx_zst_record *record, tdx_zst_diff_mask changed, tdx_error *err) {
    day_context *day = (day_context *)context;
    (void)record;
    if (changed == 0)
        day->result->silent++;
    if (day->options->changed_only && changed == 0)
        return TDX_OK;
    if (day->options->limit && day->result->emitted >= day->options->limit) {
        day->limit_reached = 1;
        return TDX_ERR;
    }
    if (snapshot->dropped_fields > day->result->dropped_tags)
        day->result->dropped_tags = snapshot->dropped_fields;
    tdx_buf_clear(&day->line);
    if (tdx_zst_format_snapshot(&day->line, snapshot, day->options->security.market_id, changed,
                                day->options->raw_tags, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_push(&day->line, '\n', err) != TDX_OK)
        return TDX_ERR;
    if (fwrite(day->line.data, 1, day->line.len, day->options->out) != day->line.len) {
        tdx_error_set(err, "cannot write the snapshot stream");
        return TDX_ERR;
    }
    day->result->emitted++;
    return TDX_OK;
}

int tdx_zst_day_run(const tdx_endpoint_pool *pool, int timeout_ms,
                    const tdx_zst_day_options *options, tdx_zst_day_result *result,
                    tdx_error *err) {
    tdx_zst_document document;
    tdx_zst_day_result summary;
    tdx_file_info info;
    tdx_buf payload;
    day_context day;
    char remote[128];
    char local[512];
    const char *cache_dir;
    int use_cache;
    size_t visited = 0;
    int result_code = TDX_ERR;
    int from_cache = 0;
    int walk;

    if (!options) {
        tdx_error_set(err, "day needs options");
        return TDX_ERR;
    }
    if (!options->out) {
        tdx_error_set(err, "day needs an output stream");
        return TDX_ERR;
    }
    memset(&summary, 0, sizeof(summary));
    memset(&info, 0, sizeof(info));
    memset(&day, 0, sizeof(day));
    tdx_buf_init(&payload);
    tdx_zst_document_init(&document);
    cache_dir = options->cache_dir;
    use_cache = cache_dir && *cache_dir;
    if (tdx_zst_day_build_paths(&options->security, options->date, cache_dir, remote,
                                sizeof(remote), use_cache ? local : NULL,
                                use_cache ? sizeof(local) : 0, err) != TDX_OK)
        goto done;
    snprintf(summary.remote_path, sizeof(summary.remote_path), "%s", remote);

    if (use_cache && !options->refresh) {
        tdx_error cache_error;
        cache_error.message[0] = '\0';
        if (read_whole_file(local, &payload, &cache_error) == TDX_OK) {
            from_cache = 1;
            summary.bytes = payload.len;
            snprintf(summary.source, sizeof(summary.source), "%s", local);
            if (!options->quiet)
                fprintf(stderr, "cache hit %s (%zu bytes)\n", local, payload.len);
        } else if (!options->quiet) {
            fprintf(stderr, "cache miss %s: %s\n", local, cache_error.message);
        }
    }

    if (!from_cache) {
        tdx_error transfer;
        if (!pool || !pool->count) {
            tdx_error_set(err, "no cached copy of %s and no endpoint to transfer it from",
                          remote);
            goto done;
        }
        if (!options->quiet)
            fprintf(stderr, "transferring %s\n", remote);
        transfer.message[0] = '\0';
        if (tdx_download_resource(pool, timeout_ms, remote, 1, &payload, &info,
                                  summary.endpoint, sizeof(summary.endpoint),
                                  &transfer) != TDX_OK) {
            tdx_error_set(err, "%s (remote path %s)", transfer.message, remote);
            goto done;
        }
        summary.downloaded = 1;
        summary.bytes = payload.len;
        snprintf(summary.source, sizeof(summary.source), "%s", remote);
        if (info.has_md5)
            snprintf(summary.md5, sizeof(summary.md5), "%s", info.md5);
        if (use_cache)
            write_cache(local, &payload, options->quiet);
    }

    if (tdx_zst_decode(payload.data, payload.len, &document, err) != TDX_OK)
        goto done;
    summary.records = document.count;
    /* The key carries the code but its market digits use a numbering this build
     * has not calibrated, so only the code is cross-checked; the market comes
     * from the resource path the caller asked for. */
    if (summary.records &&
        strcmp(document.records[0].code, options->security.code) != 0 && !options->quiet)
        fprintf(stderr, "warning: %s holds code %s, not the requested %s\n", summary.source,
                document.records[0].code, options->security.code);

    tdx_buf_init(&day.line);
    day.options = options;
    day.result = &summary;
    walk = tdx_zst_replay_document(&document, day_visit, &day, &visited, err);
    tdx_buf_free(&day.line);
    if (walk != TDX_OK && !day.limit_reached)
        goto done;
    if (walk != TDX_OK)
        err->message[0] = '\0';

    if (summary.dropped_tags && !options->quiet)
        fprintf(stderr, "warning: %zu tags did not fit the replay map\n", summary.dropped_tags);
    if (result)
        *result = summary;
    result_code = TDX_OK;

done:
    tdx_zst_document_free(&document);
    tdx_buf_free(&payload);
    return result_code;
}
