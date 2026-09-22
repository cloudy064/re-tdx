#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tdx_format.h"
#include "tdx_directory_json.h"

FILE *cli_open_output(const cli_common_options *options) {
    if (!options->output)
        return stdout;
    return fopen(options->output, "wb");
}


static int write_buffer(FILE *stream, const tdx_buf *buffer, tdx_error *err) {
    if (!stream || !buffer) {
        tdx_error_set(err, "JSON output needs a stream and a buffer");
        return TDX_ERR;
    }
    if (fwrite(buffer->data, 1, buffer->len, stream) != buffer->len) {
        tdx_error_set(err, "cannot write JSON output");
        return TDX_ERR;
    }
    return TDX_OK;
}

int cli_write_json_line(FILE *stream, tdx_buf *line, tdx_error *err) {
    if (tdx_buf_push(line, '\n', err) != TDX_OK)
        return TDX_ERR;
    return write_buffer(stream, line, err);
}

int cli_finish_output(FILE *stream, int status, tdx_error *err) {
    if (!stream)
        return status;
    if (status == TDX_OK && fflush(stream) != 0) {
        tdx_error_set(err, "cannot flush JSON output");
        status = TDX_ERR;
    }
    if (stream != stdout && fclose(stream) != 0 && status == TDX_OK) {
        tdx_error_set(err, "cannot close JSON output");
        status = TDX_ERR;
    }
    return status;
}

int cli_depth_json(FILE *stream, const tdx_depth *depth, tdx_error *err) {
    tdx_buf buffer = {0};
    int result = tdx_format_depth(&buffer, depth, err);
    if (result == TDX_OK)
        result = write_buffer(stream, &buffer, err);
    tdx_buf_free(&buffer);
    return result;
}

int cli_security_json(FILE *stream, const tdx_security *security, tdx_error *err) {
    tdx_buf buffer = {0};
    int result = tdx_directory_format_security(&buffer, security, err);
    if (result == TDX_OK)
        result = write_buffer(stream, &buffer, err);
    tdx_buf_free(&buffer);
    return result;
}

int cli_build_universe(const cli_common_options *options, tdx_code **out,
                          size_t *out_count, tdx_error *err) {
    tdx_code *codes;
    tdx_security_list list;
    tdx_connection connection;
    size_t market_index;
    size_t index;
    size_t count = 0;

    if (options->security_count > 0) {
        codes = (tdx_code *)calloc(options->security_count, sizeof(*codes));
        if (!codes) {
            tdx_error_set(err, "out of memory for %zu securities",
                          options->security_count);
            return TDX_ERR;
        }
        memcpy(codes, options->securities, options->security_count * sizeof(*codes));
        *out = codes;
        *out_count = options->security_count;
        return TDX_OK;
    }

    tdx_security_list_init(&list);
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;
    if (tdx_connection_open(&connection, &options->pool.items[0], options->timeout_ms,
                            err) != TDX_OK) {
        tdx_security_list_free(&list);
        return TDX_ERR;
    }
    fprintf(stderr, "universe endpoint=%s server_name=%s\n",
            options->pool.items[0].host,
            connection.server_name[0] ? connection.server_name : "(unknown)");
    for (market_index = 0; market_index < options->market_count; ++market_index) {
        if (tdx_directory_fetch(&connection, options->markets[market_index],
                                TDX_DIRECTORY_PAGE_MAX, &list, err) != TDX_OK) {
            tdx_connection_close(&connection);
            tdx_security_list_free(&list);
            return TDX_ERR;
        }
    }
    tdx_connection_close(&connection);

    codes = (tdx_code *)calloc(list.count ? list.count : 1, sizeof(*codes));
    if (!codes) {
        tdx_security_list_free(&list);
        tdx_error_set(err, "out of memory for %zu securities", list.count);
        return TDX_ERR;
    }
    for (index = 0; index < list.count; ++index) {
        if (!tdx_security_matches_category(&list.items[index], options->category))
            continue;
        codes[count].market_id = list.items[index].market_id;
        memcpy(codes[count].code, list.items[index].code, sizeof(codes[count].code));
        count++;
        if (options->limit && count >= options->limit)
            break;
    }
    fprintf(stderr, "universe directory=%zu selected=%zu category=%s markets=%zu\n",
            list.count, count, options->category, options->market_count);
    tdx_security_list_free(&list);
    if (count == 0) {
        free(codes);
        tdx_error_set(err, "the universe is empty for category %s", options->category);
        return TDX_ERR;
    }
    *out = codes;
    *out_count = count;
    return TDX_OK;
}
