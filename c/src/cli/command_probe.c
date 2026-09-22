#include "cli_commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cli_command_probe(const cli_probe_options *options, tdx_error *err) {
    tdx_connection connection;
    tdx_buf request = {0};
    tdx_buf response = {0};
    tdx_depth *depths;
    size_t depth_count = 0;
    size_t attempt;
    FILE *stream = NULL;
    int result = TDX_ERR;

    if (options->common.security_count == 0) {
        tdx_error_set(err, "probe needs at least one --security");
        return TDX_ERR;
    }
    depths = (tdx_depth *)calloc(options->common.security_count, sizeof(*depths));
    if (!depths) {
        tdx_error_set(err, "out of memory for %zu records", options->common.security_count);
        return TDX_ERR;
    }
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;
    tdx_buf_init(&request);
    tdx_buf_init(&response);
    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s", options->common.output);
        goto done;
    }
    if (tdx_quote_build_depth_request(options->common.securities, options->common.security_count,
                                      &request, err) != TDX_OK)
        goto done;

    for (attempt = 0; attempt < options->common.pool.count; ++attempt) {
        char address[80];
        size_t index;
        tdx_endpoint_address(&options->common.pool.items[attempt], address, sizeof(address));
        if (tdx_connection_open(&connection, &options->common.pool.items[attempt],
                                options->common.timeout_ms, err) != TDX_OK) {
            fprintf(stderr, "warning: %s: %s\n", address, err->message);
            continue;
        }
        fprintf(stderr, "endpoint=%s server_name=%s requested=%zu\n", address,
                connection.server_name[0] ? connection.server_name : "(unknown)",
                options->common.security_count);
        if (tdx_connection_call(&connection, TDX_CMD_DEPTH, request.data, request.len,
                                &response, err) != TDX_OK) {
            fprintf(stderr, "warning: %s: %s\n", address, err->message);
            tdx_connection_close(&connection);
            continue;
        }
        tdx_connection_close(&connection);
        if (tdx_quote_parse_depth_response(response.data, response.len,
                                           options->common.securities, options->common.security_count,
                                           depths, options->common.security_count, &depth_count,
                                           err) != TDX_OK)
            goto done;
        for (index = 0; index < depth_count; ++index) {
            if (cli_depth_json(stream, &depths[index], err) != TDX_OK)
                goto done;
            if (fputc('\n', stream) == EOF) {
                tdx_error_set(err, "cannot finish the depth JSON line");
                goto done;
            }
        }
        fprintf(stderr, "received %zu/%zu depth records\n", depth_count,
                options->common.security_count);
        result = TDX_OK;
        goto done;
    }
    tdx_error_set(err, "every candidate endpoint failed");

done:
    tdx_buf_free(&request);
    tdx_buf_free(&response);
    free(depths);
    result = cli_finish_output(stream, result, err);
    return result;
}
