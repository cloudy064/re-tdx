/* finance: command orchestration. */
#include "cli_commands.h"
#include "tdx_bytes.h"
#include "tdx_endpoint.h"
#include "tdx_error.h"
#include "tdx_finance.h"
#include "tdx_finance_json.h"
#include "tdx_quote.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



int cli_command_finance(const cli_finance_options *options, tdx_error *err) {
    tdx_connection connection;
    tdx_finance_record records[TDX_FINANCE_BATCH_MAX];
    tdx_finance_tally tally;
    tdx_code *codes = NULL;
    size_t code_count = 0;
    tdx_buf line = {0};
    FILE *stream = NULL;
    char endpoint[80];
    size_t batch = options->batch_size;
    size_t offset;
    int status = TDX_ERR;

    if (batch < 1)
        batch = 100;
    if (batch > TDX_FINANCE_BATCH_MAX)
        batch = TDX_FINANCE_BATCH_MAX;
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
    tdx_finance_tally_init(&tally);
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
        /* A long walk of this command does get a transient timeout from the
         * server - a whole-market run died after forty-eight clean batches - so
         * each batch is retried on a fresh connection before the command gives
         * up.  Only transport failures are retried: a decode error is a bug and
         * retrying it would just hide the message. */
        {
            int attempt;
            int done = 0;
            for (attempt = 0; attempt < TDX_FINANCE_ATTEMPTS && !done; ++attempt) {
                step.message[0] = '\0';
                if (tdx_connection_open(&connection, &options->common.pool.items[0],
                                        options->common.timeout_ms, &step) != TDX_OK) {
                    *err = step;
                    continue;
                }
                if (tdx_finance_fetch(&connection, codes + offset, want, records,
                                      TDX_FINANCE_BATCH_MAX, &got, &step) != TDX_OK) {
                    tdx_connection_close(&connection);
                    *err = step;
                    continue;
                }
                tdx_endpoint_address(&options->common.pool.items[0], endpoint, sizeof(endpoint));
                tdx_connection_close(&connection);
                done = 1;
            }
            if (!done)
                goto close_output;
        }
        /* The server does drop securities it holds nothing for; that is a short
         * answer rather than a failure, so it is reported, not fatal. */
        for (index = 0; index < got; ++index) {
            tdx_buf_clear(&line);
            if (tdx_finance_format(&line, &records[index], err) != TDX_OK)
                goto close_output;
            if (tdx_buf_push(&line, '\n', err) != TDX_OK)
                goto close_output;
            if (fwrite(line.data, 1, line.len, stream) != line.len) {
                tdx_error_set(err, "cannot write the finance stream");
                goto close_output;
            }
        }
        tdx_finance_tally_add(&tally, records, got);
    }
    tdx_buf_clear(&line);
    if (tdx_finance_format_summary(&line, &tally, endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the finance summary");
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
        fprintf(stderr, "finance: %zu of %zu securities in batches of %zu, endpoint=%s\n",
                tally.record_count, code_count, batch, endpoint);
    return status;
}
