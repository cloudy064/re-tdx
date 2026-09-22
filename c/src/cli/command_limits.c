/* limits: command orchestration. */
#include "cli_commands.h"
#include "tdx_bytes.h"
#include "tdx_endpoint.h"
#include "tdx_error.h"
#include "tdx_limits.h"
#include "tdx_limits_json.h"
#include "tdx_quote.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



int cli_command_limits(const cli_limits_options *options, tdx_error *err) {
    static tdx_limit_record rows[TDX_LIMITS_MAX_RECORDS];
    tdx_connection connection;
    tdx_buf line = {0};
    FILE *stream = NULL;
    char endpoint[80];
    size_t count = 0;
    unsigned next_index = 0;
    size_t index;
    unsigned start = options->start > 0 ? (unsigned)options->start : 0;
    int status = TDX_ERR;

    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        return TDX_ERR;
    }
    tdx_buf_init(&line);
    memset(endpoint, 0, sizeof(endpoint));
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;
    if (tdx_connection_open(&connection, &options->common.pool.items[0], options->common.timeout_ms, err) !=
        TDX_OK)
        goto close_output;
    tdx_endpoint_address(&options->common.pool.items[0], endpoint, sizeof(endpoint));
    if (tdx_limits_fetch_all(&connection, start,
                             options->max_records > 0 ? (size_t)options->max_records
                                                      : (size_t)TDX_LIMITS_MAX_RECORDS,
                             TDX_LIMITS_MAX_PAGES, rows, TDX_LIMITS_MAX_RECORDS, &count,
                             &next_index, err) != TDX_OK)
        goto close_output;

    for (index = 0; index < count; ++index) {
        tdx_buf_clear(&line);
        if (tdx_limits_format(&line, &rows[index], start + (unsigned)index, err) != TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the limit stream");
            goto close_output;
        }
    }
    tdx_buf_clear(&line);
    if (tdx_limits_format_summary(&line, rows, count, start, next_index, endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the limit summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    tdx_connection_close(&connection);
    status = cli_finish_output(stream, status, err);
    stream = NULL;
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->common.quiet)
        fprintf(stderr, "limits: %zu rows from index %u, next=%u, endpoint=%s\n", count, start,
                next_index, endpoint);
    return status;
}
