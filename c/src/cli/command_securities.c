#include "cli_commands.h"
#include <stdio.h>
#include <string.h>

int cli_command_securities(const cli_securities_options *options, tdx_error *err) {
    tdx_security_list list;
    tdx_connection connection;
    size_t market_index;
    size_t index;
    FILE *stream = NULL;
    size_t emitted = 0;
    int result = TDX_ERR;

    tdx_security_list_init(&list);
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;
    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s", options->common.output);
        return TDX_ERR;
    }
    if (tdx_connection_open(&connection, &options->common.pool.items[0], options->common.timeout_ms,
                            err) != TDX_OK)
        goto done;
    fprintf(stderr, "endpoint=%s server_name=%s\n", options->common.pool.items[0].host,
            connection.server_name[0] ? connection.server_name : "(unknown)");
    for (market_index = 0; market_index < options->common.market_count; ++market_index) {
        if (tdx_directory_fetch(&connection, options->common.markets[market_index],
                                TDX_DIRECTORY_PAGE_MAX, &list, err) != TDX_OK)
            goto done;
    }
    for (index = 0; index < list.count; ++index) {
        if (!tdx_security_matches_category(&list.items[index], options->common.category))
            continue;
        if (cli_security_json(stream, &list.items[index], err) != TDX_OK)
            goto done;
        if (fputc('\n', stream) == EOF) {
            tdx_error_set(err, "cannot finish the security JSON line");
            goto done;
        }
        emitted++;
        if (options->common.limit && emitted >= options->common.limit)
            break;
    }
    fprintf(stderr, "directory=%zu emitted=%zu category=%s\n", list.count, emitted,
            options->common.category);
    result = TDX_OK;

done:
    tdx_connection_close(&connection);
    tdx_security_list_free(&list);
    result = cli_finish_output(stream, result, err);
    return result;
}
