/* ranking: command orchestration. */
#include "cli_commands.h"
#include "tdx_bytes.h"
#include "tdx_endpoint.h"
#include "tdx_error.h"
#include "tdx_quote.h"
#include "tdx_ranking.h"
#include "tdx_ranking_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



int cli_command_ranking(const cli_ranking_options *options, tdx_error *err) {
    static tdx_buf line = {0};
    tdx_ranking_record *records = NULL;
    tdx_connection connection;
    FILE *stream = NULL;
    char endpoint[80];
    uint16_t category = TDX_RANKING_CATEGORY_A_SHARES;
    uint16_t sort = 0x000E; /* change-pct: a ranking without a key is a list, not a ranking */
    size_t want = options->common.limit > 0 ? (size_t)options->common.limit : 80;
    size_t start = options->start > 0 ? (size_t)options->start : 0;
    size_t stored = 0;
    size_t pages = 0;
    int status = TDX_ERR;

    if (*options->common.category &&
        !tdx_ranking_category_id(options->common.category, &category)) {
        tdx_error_set(err, "--category must be a name like a-shares or a number");
        return TDX_ERR;
    }
    if (options->sort && *options->sort && !tdx_ranking_sort_id(options->sort, &sort)) {
        tdx_error_set(err, "--sort must be a key like change-pct, or a number");
        return TDX_ERR;
    }
    if (start > 0xFFFF || want > TDX_RANKING_RECORDS_MAX) {
        tdx_error_set(err, "--start must be up to 65535 and --limit up to %d",
                      TDX_RANKING_RECORDS_MAX);
        return TDX_ERR;
    }
    records = (tdx_ranking_record *)calloc(TDX_RANKING_RECORDS_MAX, sizeof(*records));
    if (!records) {
        tdx_error_set(err, "out of memory for the ranking records");
        return TDX_ERR;
    }
    tdx_buf_init(&line);
    memset(endpoint, 0, sizeof(endpoint));
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;
    if (tdx_connection_open(&connection, &options->common.pool.items[0], options->common.timeout_ms, err) !=
        TDX_OK)
        goto done;
    tdx_endpoint_address(&options->common.pool.items[0], endpoint, sizeof(endpoint));

    while (stored < want) {
        tdx_buf request = {0};
        tdx_buf response = {0};
        tdx_ranking_page page;
        size_t take = want - stored;
        if (take > TDX_RANKING_PAGE_MAX)
            take = TDX_RANKING_PAGE_MAX;
        tdx_buf_init(&request);
        tdx_buf_init(&response);
        if (tdx_ranking_build_request(category, sort, (uint16_t)(start + stored), (uint16_t)take,
                                      options->ascending, (uint16_t)options->field_id, &request,
                                      err) != TDX_OK) {
            tdx_buf_free(&request);
            tdx_buf_free(&response);
            goto close_connection;
        }
        if (tdx_connection_call(&connection, TDX_CMD_CATEGORY_QUOTES, request.data, request.len,
                                &response, err) != TDX_OK) {
            tdx_buf_free(&request);
            tdx_buf_free(&response);
            goto close_connection;
        }
        if (tdx_ranking_parse(response.data, response.len, records + stored,
                              TDX_RANKING_RECORDS_MAX - stored, &page, err) != TDX_OK) {
            tdx_buf_free(&request);
            tdx_buf_free(&response);
            goto close_connection;
        }
        pages++;
        stored += page.records;
        /* A page shorter than asked for means the list ended, and asking again would only
         * repeat it. */
        if (page.records < take) {
            tdx_buf_free(&request);
            tdx_buf_free(&response);
            break;
        }
        tdx_buf_free(&request);
        tdx_buf_free(&response);
    }

    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        goto close_connection;
    }
    {
        size_t index;
        for (index = 0; index < stored; ++index) {
            tdx_buf_clear(&line);
            if (tdx_ranking_format(&line, &records[index], start + index, category, sort,
                                   err) != TDX_OK)
                goto close_output;
            if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
                fwrite(line.data, 1, line.len, stream) != line.len) {
                tdx_error_set(err, "cannot write the ranking stream");
                goto close_output;
            }
        }
    }
    tdx_buf_clear(&line);
    if (tdx_ranking_format_summary(&line, category, sort, options->ascending, stored, pages,
                                   endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
        fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the ranking summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    status = cli_finish_output(stream, status, err);
    stream = NULL;
close_connection:
    tdx_connection_close(&connection);
    if (status == TDX_OK && !options->common.quiet)
        fprintf(stderr, "ranking category %u sort %u: %zu records over %zu pages\n", category,
                sort, stored, pages);
done:
    free(records);
    tdx_buf_free(&line);
    return status;
}
