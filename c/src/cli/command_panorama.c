/* panorama: command orchestration. */
#include "cli_commands.h"
#include "tdx_bytes.h"
#include "tdx_download.h"
#include "tdx_error.h"
#include "tdx_jsn.h"
#include "tdx_panorama.h"
#include "tdx_panorama_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



int cli_command_panorama(const cli_panorama_options *options, tdx_error *err) {
    static tdx_jsn_document document = {0};
    static tdx_buf raw = {0};
    static tdx_buf utf8 = {0};
    static tdx_buf line = {0};
    const tdx_panorama_view *view = NULL;
    tdx_panorama_row *rows = NULL;
    tdx_file_info info;
    char remote[192];
    char endpoint[80];
    FILE *stream = NULL;
    size_t row_count = 0;
    size_t skipped = 0;
    size_t emitted = 0;
    size_t index;
    int status = TDX_ERR;

    tdx_buf_init(&raw);
    tdx_buf_init(&utf8);
    tdx_buf_init(&line);
    tdx_jsn_document_init(&document);
    memset(&info, 0, sizeof(info));
    memset(endpoint, 0, sizeof(endpoint));

    /* The catalog view is the default, and it is not a resource fetch. */
    if (!options->view || !*options->view || strcmp(options->view, "catalog") == 0) {
        stream = cli_open_output(&options->common);
        if (!stream) {
            tdx_error_set(err, "cannot open output %s",
                          options->common.output ? options->common.output : "<stdout>");
            goto done;
        }
        for (index = 0; index < tdx_panorama_view_count; ++index) {
            if (options->max_records && emitted >= options->max_records)
                break;
            tdx_buf_clear(&line);
            if (tdx_panorama_format_view(&line, &tdx_panorama_views[index], index, err) !=
                TDX_OK)
                goto close_output;
            if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
                fwrite(line.data, 1, line.len, stream) != line.len) {
                tdx_error_set(err, "cannot write the catalog");
                goto close_output;
            }
            emitted++;
        }
        tdx_buf_clear(&line);
        if (tdx_panorama_format_summary(&line, "catalog", NULL, emitted, 0, NULL, err) !=
            TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
            fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the summary");
            goto close_output;
        }
        status = TDX_OK;
        if (!options->common.quiet)
            fprintf(stderr, "panorama: catalog of %zu views\n", tdx_panorama_view_count);
        goto close_output;
    }

    view = tdx_panorama_find(options->view);
    if (!view) {
        tdx_error_set(err, "there is no view called %s; --view catalog lists them",
                      options->view);
        goto done;
    }
    if (tdx_jsn_remote_path(view->resource, "bi", remote, sizeof(remote), err) != TDX_OK)
        goto done;
    if (tdx_download_resource(&options->common.pool, options->common.timeout_ms, remote, 1, &raw, &info,
                              endpoint, sizeof(endpoint), err) != TDX_OK)
        goto done;
    if (tdx_jsn_gbk_to_utf8(raw.data, raw.len, &utf8, err) != TDX_OK)
        goto done;
    if (tdx_jsn_parse(utf8.data, utf8.len, &document, err) != TDX_OK)
        goto done;
    if (document.group_count != 1) {
        tdx_error_set(err, "%s holds %zu groups, expected one", remote, document.group_count);
        goto done;
    }
    rows = (tdx_panorama_row *)calloc(TDX_PANORAMA_ROWS_MAX, sizeof(*rows));
    if (!rows) {
        tdx_error_set(err, "out of memory for the panorama rows");
        goto done;
    }
    if (tdx_panorama_project(view, &document, &document.groups[0], rows,
                             TDX_PANORAMA_ROWS_MAX, &row_count, &skipped, err) != TDX_OK)
        goto done;
    if (!options->common.quiet)
        fprintf(stderr, "panorama %s: %s, %zu bytes, %zu rows, %zu skipped\n", view->id,
                view->resource, raw.len, row_count, skipped);

    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        goto done;
    }
    for (index = 0; index < row_count; ++index) {
        if (options->max_records && emitted >= options->max_records)
            break;
        tdx_buf_clear(&line);
        if (tdx_panorama_format_row(&line, view, &rows[index], index, err) != TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
            fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the panorama stream");
            goto close_output;
        }
        emitted++;
    }
    tdx_buf_clear(&line);
    if (tdx_panorama_format_summary(&line, view->id, view->resource, row_count, skipped,
                                    endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
        fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    status = cli_finish_output(stream, status, err);
    stream = NULL;
done:
    free(rows);
    tdx_jsn_document_free(&document);
    tdx_buf_free(&raw);
    tdx_buf_free(&utf8);
    tdx_buf_free(&line);
    return status;
}
