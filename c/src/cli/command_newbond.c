/* newbond: command orchestration. */
#include "cli_commands.h"
#include "tdx_bytes.h"
#include "tdx_download.h"
#include "tdx_error.h"
#include "tdx_jsn.h"
#include "tdx_newbond.h"
#include "tdx_newbond_json.h"
#include "tdx_subscription.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



int cli_command_newbond(const cli_newbond_options *options, tdx_error *err) {
    static tdx_jsn_document subscription_document = {0};
    static tdx_jsn_document projection_document = {0};
    static tdx_subscription_row subscriptions[TDX_SUBSCRIPTION_ROWS_MAX];
    static tdx_newbond_row projections[TDX_NEWBOND_ROWS_MAX];
    static tdx_buf raw = {0};
    static tdx_buf utf8 = {0};
    static tdx_buf line = {0};
    tdx_file_info info;
    char remote[128];
    char endpoint[80];
    FILE *stream = NULL;
    size_t subscription_count = 0;
    size_t subscription_skipped = 0;
    size_t projection_count = 0;
    size_t projection_skipped = 0;
    size_t index;
    tdx_newbond_reconciliation reconciliation;
    int have_reconciliation = 0;
    int status = TDX_ERR;
    /* Kept for the closing message: the report is freed on the way out, so reading it
     * afterwards would print zeros - which is exactly what it did before. */
    size_t reported_projection_rows = 0;
    size_t reported_primary_rows = 0;
    size_t reported_by_code = 0;
    size_t reported_by_underlying = 0;
    size_t reported_unmatched = 0;
    size_t reported_date_mismatches = 0;
    size_t reported_size_mismatches = 0;
    int reported_exact = 0;

    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        return TDX_ERR;
    }
    tdx_buf_init(&raw);
    tdx_buf_init(&utf8);
    tdx_buf_init(&line);
    tdx_jsn_document_init(&subscription_document);
    tdx_jsn_document_init(&projection_document);
    memset(endpoint, 0, sizeof(endpoint));

    /* The projection, which is the smaller document, is read first so a failure costs
     * nothing.  It needs no quotes and no date. */
    memset(&info, 0, sizeof(info));
    if (tdx_jsn_remote_path(TDX_NEWBOND_PROJECTION_RESOURCE, "bi", remote, sizeof(remote),
                           err) != TDX_OK)
        goto close_output;
    if (tdx_download_resource(&options->common.pool, options->common.timeout_ms, remote, 1, &raw, &info,
                              endpoint, sizeof(endpoint), err) != TDX_OK)
        goto close_output;
    if (tdx_jsn_gbk_to_utf8(raw.data, raw.len, &utf8, err) != TDX_OK)
        goto close_output;
    if (tdx_jsn_parse(utf8.data, utf8.len, &projection_document, err) != TDX_OK)
        goto close_output;
    if (projection_document.group_count != 1) {
        tdx_error_set(err, "%s holds %zu groups, expected one", remote,
                      projection_document.group_count);
        goto close_output;
    }
    if (tdx_newbond_normalize(&projection_document, &projection_document.groups[0], projections,
                              TDX_NEWBOND_ROWS_MAX, &projection_count, &projection_skipped,
                              err) != TDX_OK)
        goto close_output;
    if (!options->common.quiet)
        fprintf(stderr, "newbond %s: %zu bytes, md5=%s, %zu rows (%zu skipped)\n", remote,
                raw.len, info.md5, projection_count, projection_skipped);

    /* The subscription list, which supplies the rows to compare against. */
    tdx_buf_clear(&raw);
    tdx_buf_clear(&utf8);
    memset(&info, 0, sizeof(info));
    if (tdx_jsn_remote_path(TDX_SUBSCRIPTION_RESOURCE, "bi", remote, sizeof(remote), err) !=
        TDX_OK)
        goto close_output;
    if (tdx_download_resource(&options->common.pool, options->common.timeout_ms, remote, 1, &raw, &info,
                              endpoint, sizeof(endpoint), err) != TDX_OK)
        goto close_output;
    if (tdx_jsn_gbk_to_utf8(raw.data, raw.len, &utf8, err) != TDX_OK)
        goto close_output;
    if (tdx_jsn_parse(utf8.data, utf8.len, &subscription_document, err) != TDX_OK)
        goto close_output;
    if (subscription_document.group_count != 1) {
        tdx_error_set(err, "%s holds %zu groups, expected one", remote,
                      subscription_document.group_count);
        goto close_output;
    }
    if (tdx_subscription_normalize(&subscription_document, &subscription_document.groups[0],
                                   subscriptions, TDX_SUBSCRIPTION_ROWS_MAX, &subscription_count,
                                   &subscription_skipped, err) != TDX_OK)
        goto close_output;
    if (!options->common.quiet)
        fprintf(stderr, "newbond %s: %zu bytes, md5=%s, %zu rows (%zu skipped)\n", remote,
                raw.len, info.md5, subscription_count, subscription_skipped);

    if (tdx_newbond_reconcile(subscriptions, subscription_count, projections, projection_count,
                              &reconciliation, err) != TDX_OK)
        goto close_output;
    have_reconciliation = 1;
    reported_projection_rows = reconciliation.projection_rows;
    reported_primary_rows = reconciliation.primary_rows;
    reported_by_code = reconciliation.exact_code_matches;
    reported_by_underlying = reconciliation.underlying_only_matches;
    reported_unmatched = reconciliation.unmatched_projection_rows;
    reported_date_mismatches = reconciliation.subscription_date_mismatch_count;
    reported_size_mismatches = reconciliation.issue_size_mismatch_count;
    reported_exact = reconciliation.exact_projection;

    for (index = 0; index < projection_count; ++index) {
        tdx_buf_clear(&line);
        if (tdx_newbond_format_row(&line, &projections[index], &reconciliation.matches[index],
                                   index, err) != TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the projection stream");
            goto close_output;
        }
    }
    tdx_buf_clear(&line);
    if (tdx_newbond_format_reconciliation(&line, &reconciliation, TDX_SUBSCRIPTION_RESOURCE,
                                          TDX_NEWBOND_PROJECTION_RESOURCE, endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the reconciliation");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    status = cli_finish_output(stream, status, err);
    stream = NULL;
    if (have_reconciliation)
        tdx_newbond_reconciliation_free(&reconciliation);
    tdx_jsn_document_free(&subscription_document);
    tdx_jsn_document_free(&projection_document);
    tdx_buf_free(&raw);
    tdx_buf_free(&utf8);
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->common.quiet)
        fprintf(stderr,
                "newbond: %zu projection rows against %zu subscriptions, %zu by code, %zu by "
                "underlying, %zu unmatched, %zu date mismatches, %zu size mismatches, "
                "exact=%d\n",
                reported_projection_rows, reported_primary_rows, reported_by_code,
                reported_by_underlying, reported_unmatched, reported_date_mismatches,
                reported_size_mismatches, reported_exact);
    return status;
}
