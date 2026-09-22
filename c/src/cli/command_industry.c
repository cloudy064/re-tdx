/* industry: command orchestration. */
#include "cli_commands.h"
#include "tdx_bytes.h"
#include "tdx_download.h"
#include "tdx_error.h"
#include "tdx_industry.h"
#include "tdx_industry_json.h"
#include "tdx_jsn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



int cli_command_industry(const cli_industry_options *options, tdx_error *err) {
    static tdx_jsn_document document = {0};
    static tdx_buf raw = {0};
    static tdx_buf utf8 = {0};
    static tdx_buf line = {0};
    tdx_industry_row *rows = NULL;
    tdx_industry *industries = NULL;
    tdx_file_info info;
    char remote[128];
    char endpoint[80];
    FILE *stream = NULL;
    size_t row_count = 0;
    size_t skipped = 0;
    size_t industry_count = 0;
    size_t agreeing = 0;
    size_t inconsistent = 0;
    size_t emitted = 0;
    size_t index;
    int status = TDX_ERR;

    tdx_buf_init(&raw);
    tdx_buf_init(&utf8);
    tdx_buf_init(&line);
    tdx_jsn_document_init(&document);
    memset(&info, 0, sizeof(info));
    memset(endpoint, 0, sizeof(endpoint));

    if (tdx_jsn_remote_path(TDX_INDUSTRY_RESOURCE, "bi", remote, sizeof(remote), err) != TDX_OK)
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

    rows = (tdx_industry_row *)calloc(TDX_INDUSTRY_ROWS_MAX, sizeof(*rows));
    industries = (tdx_industry *)calloc(TDX_INDUSTRY_CODES_MAX, sizeof(*industries));
    if (!rows || !industries) {
        tdx_error_set(err, "out of memory for the industry buffers");
        goto done;
    }
    if (tdx_industry_parse(&document, &document.groups[0], rows, TDX_INDUSTRY_ROWS_MAX,
                           &row_count, &skipped, err) != TDX_OK)
        goto done;
    if (tdx_industry_catalog(rows, row_count, industries, TDX_INDUSTRY_CODES_MAX,
                             &industry_count, err) != TDX_OK)
        goto done;
    for (index = 0; index < industry_count; ++index) {
        if (industries[index].counts_agree)
            agreeing++;
        if (industries[index].inconsistent)
            inconsistent++;
    }
    if (!options->common.quiet)
        fprintf(stderr, "industry %s: %zu bytes, md5=%s, %zu rows, %zu industries\n", remote,
                raw.len, info.md5, row_count, industry_count);

    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        goto done;
    }
    for (index = 0; index < industry_count; ++index) {
        if (options->max_records && emitted >= options->max_records)
            break;
        tdx_buf_clear(&line);
        if (tdx_industry_format(&line, &industries[index], index, err) != TDX_OK)
            goto done;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
            fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the industry stream");
            goto done;
        }
        emitted++;
    }
    /* The per-stock rows are a second, much longer view of the same file, so they are opt
     * in rather than the default. */
    if (options->bonds) {
        for (index = 0; index < row_count; ++index) {
            if (options->max_records && emitted >= options->max_records)
                break;
            tdx_buf_clear(&line);
            if (tdx_industry_format_row(&line, &rows[index], index, err) != TDX_OK)
                goto done;
            if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
                fwrite(line.data, 1, line.len, stream) != line.len) {
                tdx_error_set(err, "cannot write the stock-to-industry stream");
                goto done;
            }
            emitted++;
        }
    }
    tdx_buf_clear(&line);
    if (tdx_industry_format_summary(&line, TDX_INDUSTRY_RESOURCE, row_count, skipped,
                                    industry_count, agreeing, inconsistent, endpoint,
                                    err) != TDX_OK)
        goto done;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
        fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the industry summary");
        goto done;
    }
    status = TDX_OK;
    if (!options->common.quiet)
        fprintf(stderr,
                "industry: %zu industries, %zu whose two counts agree, %zu inconsistent, "
                "%zu emitted\n",
                industry_count, agreeing, inconsistent, emitted);

done:
    status = cli_finish_output(stream, status, err);
    stream = NULL;
    free(rows);
    free(industries);
    tdx_jsn_document_free(&document);
    tdx_buf_free(&raw);
    tdx_buf_free(&utf8);
    tdx_buf_free(&line);
    return status;
}
