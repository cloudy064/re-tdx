/* subscription: command orchestration. */
#include "cli_commands.h"
#include "tdx_bytes.h"
#include "tdx_download.h"
#include "tdx_error.h"
#include "tdx_jsn.h"
#include "tdx_subscription.h"
#include "tdx_subscription_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



int cli_command_subscription(const cli_subscription_options *options, tdx_error *err) {
    static tdx_jsn_document document = {0};
    static tdx_subscription_row rows[TDX_SUBSCRIPTION_ROWS_MAX];
    static tdx_buf raw = {0};
    static tdx_buf utf8 = {0};
    static tdx_buf line = {0};
    tdx_file_info info;
    char remote[128];
    char endpoint[80];
    FILE *stream = NULL;
    size_t count = 0;
    size_t skipped = 0;
    size_t index;
    size_t listed = 0;
    size_t with_value = 0;
    size_t with_premium = 0;
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
    tdx_jsn_document_init(&document);
    memset(&info, 0, sizeof(info));
    memset(endpoint, 0, sizeof(endpoint));

    if (tdx_jsn_remote_path(TDX_SUBSCRIPTION_RESOURCE, "bi", remote, sizeof(remote), err) !=
        TDX_OK)
        goto close_output;
    if (tdx_download_resource(&options->common.pool, options->common.timeout_ms, remote, 1, &raw, &info,
                              endpoint, sizeof(endpoint), err) != TDX_OK)
        goto close_output;
    if (tdx_jsn_gbk_to_utf8(raw.data, raw.len, &utf8, err) != TDX_OK)
        goto close_output;
    if (tdx_jsn_parse(utf8.data, utf8.len, &document, err) != TDX_OK)
        goto close_output;
    if (document.group_count != 1) {
        tdx_error_set(err, "%s holds %zu groups, expected one", remote, document.group_count);
        goto close_output;
    }
    if (tdx_subscription_normalize(&document, &document.groups[0], rows,
                                   TDX_SUBSCRIPTION_ROWS_MAX, &count, &skipped, err) != TDX_OK)
        goto close_output;
    if (!options->common.quiet)
        fprintf(stderr, "subscription %s: %zu bytes, md5=%s, %zu rows (%zu skipped)\n", remote,
                raw.len, info.md5, count, skipped);

    for (index = 0; index < count; ++index) {
        tdx_buf_clear(&line);
        if (tdx_subscription_format(&line, &rows[index], TDX_SUBSCRIPTION_RESOURCE, index, err) !=
            TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the subscription stream");
            goto close_output;
        }
        if (rows[index].listed)
            listed++;
        if (rows[index].has_conversion_value_yuan)
            with_value++;
        if (rows[index].has_conversion_premium_pct)
            with_premium++;
    }

    tdx_buf_clear(&line);
    if (tdx_subscription_format_summary(&line, count, skipped, listed, with_value, with_premium,
                                        TDX_SUBSCRIPTION_RESOURCE, endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the subscription summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    status = cli_finish_output(stream, status, err);
    stream = NULL;
    tdx_jsn_document_free(&document);
    tdx_buf_free(&raw);
    tdx_buf_free(&utf8);
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->common.quiet)
        fprintf(stderr,
                "subscription: %zu events, %zu listed, %zu with a conversion value, %zu with a "
                "premium\n",
                count, listed, with_value, with_premium);
    return status;
}
