/* snapshot: command orchestration. */
#include "cli_commands.h"
#include "tdx_bytes.h"
#include "tdx_endpoint.h"
#include "tdx_error.h"
#include "tdx_quote.h"
#include "tdx_snapshot.h"
#include "tdx_snapshot_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



int cli_command_snapshot(const cli_snapshot_options *options, tdx_error *err) {
    tdx_connection connection;
    tdx_snapshot records[TDX_SNAPSHOT_BATCH_MAX];
    tdx_snapshot_tally tally;
    tdx_code *codes = NULL;
    size_t code_count = 0;
    tdx_buf line = {0};
    FILE *stream = NULL;
    char endpoint[80];
    size_t batch = options->batch_size;
    size_t offset;
    int status = TDX_ERR;

    if (batch < 1) {
        tdx_error_set(err, "--batch-size must be positive");
        return TDX_ERR;
    }
    /* --batch-size is shared with the depth commands, whose cap is higher, so a
     * request above this command's server cap is reduced rather than refused -
     * but it is said out loud instead of silently. */
    if (batch > TDX_SNAPSHOT_BATCH_MAX) {
        if (!options->common.quiet)
            fprintf(stderr,
                    "note: 0x054C is capped at %u records per request; using %u instead of %zu\n",
                    (unsigned)TDX_SNAPSHOT_BATCH_MAX, (unsigned)TDX_SNAPSHOT_BATCH_MAX, batch);
        batch = TDX_SNAPSHOT_BATCH_MAX;
    }
    if (cli_build_universe(&options->common, &codes, &code_count, err) != TDX_OK)
        return TDX_ERR;
    if (code_count == 0) {
        tdx_error_set(err, "the universe is empty");
        free(codes);
        return TDX_ERR;
    }
    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        free(codes);
        return TDX_ERR;
    }
    tdx_buf_init(&line);
    tdx_snapshot_tally_init(&tally);
    memset(endpoint, 0, sizeof(endpoint));
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;

    for (offset = 0; offset < code_count; offset += batch) {
        size_t want = code_count - offset;
        size_t got = 0;
        size_t index;
        tdx_error step;

        if (want > batch)
            want = batch;
        step.message[0] = '\0';
        if (tdx_connection_open(&connection, &options->common.pool.items[0], options->common.timeout_ms,
                                &step) != TDX_OK) {
            *err = step;
            goto close_output;
        }
        if (tdx_snapshot_fetch(&connection, codes + offset, want, records,
                               TDX_SNAPSHOT_BATCH_MAX, &got, &step) != TDX_OK) {
            tdx_connection_close(&connection);
            *err = step;
            goto close_output;
        }
        tdx_endpoint_address(&options->common.pool.items[0], endpoint, sizeof(endpoint));
        tdx_connection_close(&connection);
        /* A truncated batch is a failure, not a short answer. */
        if (got != want) {
            tdx_error_set(err, "snapshot batch returned %zu of %zu records", got, want);
            goto close_output;
        }
        for (index = 0; index < got; ++index) {
            tdx_buf_clear(&line);
            if (tdx_snapshot_format(&line, &records[index], err) != TDX_OK)
                goto close_output;
            if (tdx_buf_push(&line, '\n', err) != TDX_OK)
                goto close_output;
            if (fwrite(line.data, 1, line.len, stream) != line.len) {
                tdx_error_set(err, "cannot write the snapshot stream");
                goto close_output;
            }
        }
        tdx_snapshot_tally_add(&tally, records, got);
    }
    tdx_buf_clear(&line);
    if (tdx_snapshot_format_summary(&line, &tally, code_count, endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the snapshot summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    status = cli_finish_output(stream, status, err);
    stream = NULL;
    free(codes);
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->common.quiet)
        fprintf(stderr, "snapshot: %zu securities in batches of %zu, endpoint=%s\n",
                tally.record_count, batch, endpoint);
    return status;
}
