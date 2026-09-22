/* pending: command orchestration. */
#include "cli_commands.h"
#include "tdx_bytes.h"
#include "tdx_download.h"
#include "tdx_error.h"
#include "tdx_jsn.h"
#include "tdx_pending.h"
#include "tdx_pending_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



int cli_command_pending(const cli_pending_options *options, tdx_error *err) {
    static const char *const resources[3] = {
        TDX_PENDING_PRIMARY_RESOURCE,
        TDX_PENDING_PROJECTION_A_RESOURCE,
        TDX_PENDING_PROJECTION_B_RESOURCE,
    };
    static tdx_jsn_document documents[3];
    static tdx_pending_row rows[3][TDX_PENDING_ROWS_MAX];
    static size_t counts[3];
    static size_t skipped[3];
    static tdx_buf raw = {0};
    static tdx_buf utf8 = {0};
    static tdx_buf line = {0};
    tdx_file_info info;
    char remote[128];
    char endpoint[80];
    FILE *stream = NULL;
    size_t index;
    size_t agreeing = 0;
    int status = TDX_ERR;

    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        return TDX_ERR;
    }
    tdx_buf_init(&raw);
    tdx_buf_init(&utf8);
    tdx_buf_init(&line);
    memset(endpoint, 0, sizeof(endpoint));
    for (index = 0; index < 3; ++index) {
        tdx_jsn_document_init(&documents[index]);
        counts[index] = 0;
        skipped[index] = 0;
    }

    for (index = 0; index < 3; ++index) {
        memset(&info, 0, sizeof(info));
        tdx_buf_clear(&raw);
        tdx_buf_clear(&utf8);
        if (tdx_jsn_remote_path(resources[index], "bi", remote, sizeof(remote), err) != TDX_OK)
            goto close_output;
        if (tdx_download_resource(&options->common.pool, options->common.timeout_ms, remote, 1, &raw, &info,
                                  endpoint, sizeof(endpoint), err) != TDX_OK)
            goto close_output;
        if (tdx_jsn_gbk_to_utf8(raw.data, raw.len, &utf8, err) != TDX_OK)
            goto close_output;
        if (tdx_jsn_parse(utf8.data, utf8.len, &documents[index], err) != TDX_OK)
            goto close_output;
        if (documents[index].group_count != 1) {
            tdx_error_set(err, "%s holds %zu groups, expected one", remote,
                          documents[index].group_count);
            goto close_output;
        }
        if (tdx_pending_normalize(&documents[index], &documents[index].groups[0], rows[index],
                                  TDX_PENDING_ROWS_MAX, &counts[index], &skipped[index],
                                  err) != TDX_OK)
            goto close_output;
        if (!options->common.quiet)
            fprintf(stderr, "pending %s: %zu bytes, md5=%s, %zu rows (%zu skipped)\n", remote,
                    raw.len, info.md5, counts[index], skipped[index]);
    }

    for (index = 0; index < counts[0]; ++index) {
        tdx_buf_clear(&line);
        if (tdx_pending_format(&line, &rows[0][index], resources[0], index, err) != TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the pending stream");
            goto close_output;
        }
    }

    /* One report per projection, which is what the reference produces. */
    for (index = 1; index < 3; ++index) {
        tdx_pending_reconciliation reconciliation;
        if (tdx_pending_reconcile(rows[0], counts[0], rows[index], counts[index],
                                  &reconciliation, err) != TDX_OK)
            goto close_output;
        tdx_buf_clear(&line);
        if (tdx_pending_format_reconciliation(&line, &reconciliation, resources[0],
                                              resources[index], endpoint, err) != TDX_OK) {
            tdx_pending_reconciliation_free(&reconciliation);
            goto close_output;
        }
        if (reconciliation.exact_security_set)
            agreeing++;
        tdx_pending_reconciliation_free(&reconciliation);
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the pending reconciliation");
            goto close_output;
        }
    }

    tdx_buf_clear(&line);
    if (tdx_pending_format_summary(&line, counts[0], skipped[0], 2, agreeing, resources[0],
                                   endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the pending summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    status = cli_finish_output(stream, status, err);
    stream = NULL;
    for (index = 0; index < 3; ++index)
        tdx_jsn_document_free(&documents[index]);
    tdx_buf_free(&raw);
    tdx_buf_free(&utf8);
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->common.quiet)
        fprintf(stderr, "pending: %zu plans, %zu skipped, %zu of 2 projections agree exactly\n",
                counts[0], skipped[0], agreeing);
    return status;
}
