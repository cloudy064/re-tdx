/* capital: command orchestration. */
#include "cli_commands.h"
#include "tdx_bytes.h"
#include "tdx_capital.h"
#include "tdx_capital_json.h"
#include "tdx_endpoint.h"
#include "tdx_error.h"
#include "tdx_finance.h"
#include "tdx_gbbq.h"
#include "tdx_quote.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



int cli_command_capital(const cli_capital_options *options, tdx_error *err) {
    static tdx_capital_record records[TDX_CAPITAL_RECORDS_MAX];
    tdx_capital_tally tally;
    tdx_code *codes = NULL;
    size_t code_count = 0;
    tdx_connection connection;
    tdx_buf line = {0};
    FILE *stream = NULL;
    char endpoint[80];
    size_t index;
    int status = TDX_ERR;
    int connected = 0;

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
    tdx_capital_tally_init(&tally);
    memset(endpoint, 0, sizeof(endpoint));
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;

    if (options->capital_local) {
        /* The local file holds the whole market, so every security is answered from
         * one read of one file rather than one request each. */
        if (options->gbbq_path)
            snprintf(endpoint, sizeof(endpoint), "local:%s", options->gbbq_path);
        else if (tdx_gbbq_default_path(options->common.root, endpoint + 6, sizeof(endpoint) - 6,
                                       err) != TDX_OK)
            goto close_output;
        if (options->gbbq_path == NULL)
            memmove(endpoint, "local:", 6);
        for (index = 0; index < code_count; ++index) {
            size_t got = 0;
            size_t source_count = 0;
            size_t record;
            if (tdx_gbbq_load(options->gbbq_path, options->common.root, &codes[index], records,
                              TDX_CAPITAL_RECORDS_MAX, &got, &source_count, err) != TDX_OK)
                goto close_output;
            for (record = 0; record < got; ++record) {
                tdx_buf_clear(&line);
                if (tdx_capital_format(&line, &records[record], err) != TDX_OK)
                    goto close_output;
                if (tdx_buf_push(&line, '\n', err) != TDX_OK)
                    goto close_output;
                if (fwrite(line.data, 1, line.len, stream) != line.len) {
                    tdx_error_set(err, "cannot write the capital stream");
                    goto close_output;
                }
            }
            tdx_capital_tally_add(&tally, records, got);
        }
    } else {
        if (tdx_connection_open(&connection, &options->common.pool.items[0], options->common.timeout_ms,
                                err) != TDX_OK)
            goto close_output;
        connected = 1;
        tdx_endpoint_address(&options->common.pool.items[0], endpoint, sizeof(endpoint));

        for (index = 0; index < code_count; ++index) {
            size_t got = 0;
            size_t blocks = 0;
            size_t record;
            tdx_error step;
            int done = 0;
            int attempt;

            /* One request per security means a long walk issues far more requests
             * than the batched commands, and it meets the same transient server
             * timeout.  Transport failures retry on a fresh connection; a decode
             * error does not, because that is a bug and retrying would hide it. */
            for (attempt = 0; attempt < TDX_FINANCE_ATTEMPTS && !done; ++attempt) {
                step.message[0] = '\0';
                if (attempt > 0) {
                    tdx_connection_close(&connection);
                    connected = 0;
                    connection.socket_handle = (intptr_t)-1;
                    if (tdx_connection_open(&connection, &options->common.pool.items[0],
                                            options->common.timeout_ms, &step) != TDX_OK) {
                        *err = step;
                        continue;
                    }
                    connected = 1;
                }
                if (tdx_capital_fetch(&connection, &codes[index], records,
                                      TDX_CAPITAL_RECORDS_MAX, &got, &blocks, &step) == TDX_OK) {
                    done = 1;
                    continue;
                }
                *err = step;
            }
            if (!done)
                goto close_output;
            for (record = 0; record < got; ++record) {
                tdx_buf_clear(&line);
                if (tdx_capital_format(&line, &records[record], err) != TDX_OK)
                    goto close_output;
                if (tdx_buf_push(&line, '\n', err) != TDX_OK)
                    goto close_output;
                if (fwrite(line.data, 1, line.len, stream) != line.len) {
                    tdx_error_set(err, "cannot write the capital stream");
                    goto close_output;
                }
            }
            tdx_capital_tally_add(&tally, records, got);
        }
    }

    tdx_buf_clear(&line);
    if (tdx_capital_format_summary(&line, &tally, code_count, endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the capital summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    if (connected)
        tdx_connection_close(&connection);
    status = cli_finish_output(stream, status, err);
    stream = NULL;
    free(codes);
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->common.quiet)
        fprintf(stderr, "capital: %zu securities, %zu records, source=%s\n", tally.securities,
                tally.record_count, endpoint);
    return status;
}
