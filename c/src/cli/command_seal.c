/* seal: command orchestration. */
#include "cli_commands.h"
#include "tdx_bytes.h"
#include "tdx_endpoint.h"
#include "tdx_error.h"
#include "tdx_limit.h"
#include "tdx_quote.h"
#include "tdx_seal.h"
#include "tdx_seal_json.h"
#include "cli_date.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



int cli_command_seal(const cli_seal_options *options, tdx_error *err) {
    static tdx_buf line = {0};
    static tdx_buf request = {0};
    static tdx_buf response = {0};
    tdx_depth depth;
    tdx_limit_rules rules;
    tdx_limit_prices limits;
    tdx_seal_input input;
    tdx_seal_result seal = {0};
    tdx_connection connection;
    FILE *stream = NULL;
    char endpoint[80];
    char as_of[16];
    int status = TDX_ERR;

    if (options->common.security_count != 1) {
        tdx_error_set(err, "seal needs exactly one --security CODE");
        return TDX_ERR;
    }
    uint32_t date;
    if (cli_resolve_date(options->date, as_of, &date, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_limit_rules_load(options->common.root, &rules, err) != TDX_OK)
        return TDX_ERR;

    tdx_buf_init(&line);
    tdx_buf_init(&request);
    tdx_buf_init(&response);
    memset(&depth, 0, sizeof(depth));
    memset(endpoint, 0, sizeof(endpoint));
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;
    if (tdx_connection_open(&connection, &options->common.pool.items[0], options->common.timeout_ms, err) !=
        TDX_OK)
        goto done;
    tdx_endpoint_address(&options->common.pool.items[0], endpoint, sizeof(endpoint));
    if (tdx_quote_build_depth_request(options->common.securities, 1, &request, err) != TDX_OK)
        goto close_connection;
    if (tdx_connection_call(&connection, TDX_CMD_DEPTH, request.data, request.len, &response,
                            err) != TDX_OK)
        goto close_connection;
    {
        size_t parsed = 0;
        if (tdx_quote_parse_depth_response(response.data, response.len, options->common.securities, 1,
                                           &depth, 1, &parsed, err) != TDX_OK)
            goto close_connection;
        if (parsed != 1) {
            tdx_error_set(err, "the depth reply holds %zu records for one request", parsed);
            goto close_connection;
        }
    }

    /* The security name is not needed: the limit rules only use a name to recognise special
     * treatment, and the depth record does not carry one. */
    if (!tdx_limit_calculate(options->common.securities[0].market_id, options->common.securities[0].code, "",
                             depth.previous, (int)date, &rules, &limits)) {
        tdx_error_set(err, "%s is not price limited", options->common.securities[0].code);
        goto close_connection;
    }
    tdx_seal_input_from_depth(&depth, &input);
    if (!tdx_seal_calculate(&input, &limits, &seal)) {
        tdx_error_set(err, "the seal is not available for %s", options->common.securities[0].code);
        goto close_connection;
    }

    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        goto close_connection;
    }
    if (tdx_seal_format(&line, &options->common.securities[0], &limits, &seal, &input, as_of,
                        err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
        fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the seal");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    status = cli_finish_output(stream, status, err);
    stream = NULL;
close_connection:
    tdx_connection_close(&connection);
    if (status == TDX_OK && !options->common.quiet)
        fprintf(stderr, "seal %s: direction %d mode %s amount %.2f\n",
                options->common.securities[0].code, seal.direction,
                seal.mode ? seal.mode : "not-sealed", seal.amount_yuan);
done:
    tdx_buf_free(&line);
    tdx_buf_free(&request);
    tdx_buf_free(&response);
    return status;
}
