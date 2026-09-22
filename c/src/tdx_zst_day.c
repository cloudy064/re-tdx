/* tdx_zst_day.c - historical L1 retrieval and replay for one security and date. */
#include "tdx_zst_day.h"
#include "tdx_zst_day_internal.h"
#include "tdx_date.h"
#include "tdx_cache.h"

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
    uint32_t value;
    if (!date || !tdx_date_parse(date, strlen(date), 0, &value) ||
        value / 10000 < 1990 || value / 10000 > 2100) {
        tdx_error_set(err, "--date must be a real date in 1990..2100, YYYYMMDD");
        return TDX_ERR;
    }
    return TDX_OK;
}

int tdx_zst_day_build_paths(const tdx_code *security, const char *date,
                            const char *cache_dir, char *remote, size_t remote_size,
                            char *local, size_t local_size, tdx_error *err) {
    const char *prefix;
    size_t index;
    if (!security) {
        tdx_error_set(err, "day needs a security");
        return TDX_ERR;
    }
    if (date_valid(date, err) != TDX_OK)
        return TDX_ERR;
    if (security->market_id < 0 || security->market_id > 2 || security->code[6] != '\0') {
        tdx_error_set(err, "day needs market 0..2 and a six-digit code");
        return TDX_ERR;
    }
    for (index = 0; index < 6; ++index) {
        if (security->code[index] < '0' || security->code[index] > '9') {
            tdx_error_set(err, "day needs a six-digit code");
            return TDX_ERR;
        }
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

/* Cache entries are committed only after format and identity validation. */
static int decode_payload(const tdx_buf *payload, const tdx_code *security,
                          tdx_zst_document *document, tdx_error *err) {
    size_t index;
    if (tdx_zst_decode(payload->data, payload->len, document, err) != TDX_OK)
        return TDX_ERR;
    for (index = 0; index < document->count; ++index) {
        if (strcmp(document->records[index].code, security->code) != 0) {
            tdx_error_set(err, "historical resource holds code %s, expected %s",
                          document->records[index].code, security->code);
            return TDX_ERR;
        }
    }
    return TDX_OK;
}

static int default_fetch(void *context, const tdx_endpoint_pool *pool, int timeout_ms,
    const char *path, tdx_buf *payload, tdx_file_info *info, char *endpoint,
    size_t endpoint_size, tdx_error *err) {
    (void)context;
    if (!pool || !pool->count) {
        tdx_error_set(err, "no usable cached copy of %s and no endpoint to transfer it from", path);
        return TDX_ERR;
    }
    return tdx_download_resource(pool, timeout_ms, path, 1, payload, info,
                                  endpoint, endpoint_size, err);
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

int tdx_zst_day_run_with_fetch(const tdx_endpoint_pool *pool, int timeout_ms,
                    const tdx_zst_day_options *options, tdx_zst_day_result *result,
                    tdx_zst_day_fetch fetch, void *context, tdx_error *err) {
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

    if (!fetch) fetch = default_fetch;
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
        tdx_error cache_error = {{0}};
        tdx_cache_read_status read = tdx_cache_read(local, &payload, &cache_error);
        if (read == TDX_CACHE_ERROR) {
            tdx_error_set(err, "%s", cache_error.message);
            goto done;
        }
        if (read == TDX_CACHE_FOUND) {
            if (decode_payload(&payload, &options->security, &document, &cache_error) == TDX_OK) {
                from_cache = 1;
                summary.bytes = payload.len;
                snprintf(summary.source, sizeof(summary.source), "%s", local);
                if (!options->quiet)
                    fprintf(stderr, "cache hit %s (%zu bytes)\n", local, payload.len);
            } else {
                if (!options->quiet)
                    fprintf(stderr, "invalid cache %s: %s; fetching again\n", local, cache_error.message);
                tdx_zst_document_free(&document);
                tdx_zst_document_init(&document);
                tdx_buf_clear(&payload);
            }
        }
    }

    if (!from_cache) {
        tdx_error transfer = {{0}};
        if (!options->quiet)
            fprintf(stderr, "transferring %s\n", remote);
        if (fetch(context, pool, timeout_ms, remote, &payload, &info,
                  summary.endpoint, sizeof(summary.endpoint), &transfer) != TDX_OK) {
            tdx_error_set(err, "%s (remote path %s)", transfer.message, remote);
            goto done;
        }
        if (decode_payload(&payload, &options->security, &document, err) != TDX_OK)
            goto done;
        summary.downloaded = 1;
        summary.bytes = payload.len;
        snprintf(summary.source, sizeof(summary.source), "%s", remote);
        if (info.has_md5)
            snprintf(summary.md5, sizeof(summary.md5), "%s", info.md5);
        if (use_cache) {
            tdx_error cache_error = {{0}};
            if (tdx_cache_write(local, &payload, &cache_error) != TDX_OK && !options->quiet)
                fprintf(stderr, "warning: %s\n", cache_error.message);
        }
    }
    summary.records = document.count;

    tdx_buf_init(&day.line);
    day.options = options;
    day.result = &summary;
    walk = tdx_zst_replay_document(&document, day_visit, &day, &visited, err);
    tdx_buf_free(&day.line);
    if (walk != TDX_OK && !day.limit_reached)
        goto done;
    if (walk != TDX_OK && err)
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

int tdx_zst_day_run(const tdx_endpoint_pool *pool, int timeout_ms,
                    const tdx_zst_day_options *options, tdx_zst_day_result *result,
                    tdx_error *err) {
    return tdx_zst_day_run_with_fetch(pool, timeout_ms, options, result, NULL, NULL, err);
}
