/* jsn: command orchestration. */
#include "cli_commands.h"
#include "tdx_bonds_json.h"
#include "tdx_convertible_json.h"
#include "tdx_download.h"
#include "tdx_jsn_json.h"
#include <stdio.h>
#include <string.h>

int cli_command_jsn(const cli_jsn_options *options, tdx_error *err) {
    char remote[TDX_JSN_RESOURCE_MAX + 16];
    char endpoint[80];
    tdx_buf raw = {0};
    tdx_buf utf8 = {0};
    tdx_buf line = {0};
    tdx_jsn_document document = {0};
    tdx_file_info info;
    FILE *stream;
    size_t group_index;
    size_t row;
    size_t group_count = 0;
    size_t row_count = 0;    /* How many columns had to be renamed because the resource names one like the
     * envelope does. */
    size_t columns_renamed = 0;

    size_t bonds_named = 0;
    size_t bonds_underlying = 0;
    size_t bonds_sized = 0;
    size_t convertible_complete = 0;
    size_t convertible_exchangeable = 0;
    size_t convertible_underlying = 0;
    int status = TDX_ERR;

    if (!options->resource || !*options->resource) {
        tdx_error_set(err, "jsn needs --resource");
        return TDX_ERR;
    }
    if (tdx_jsn_remote_path(options->resource, options->prefix, remote, sizeof(remote), err) !=
        TDX_OK)
        return TDX_ERR;
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
    memset(endpoint, 0, sizeof(endpoint));
    memset(&info, 0, sizeof(info));

    /* The transfer verifies the announced digest when there is one: a short or
     * reordered chunk stream must never look like a complete resource. */
    if (tdx_download_resource(&options->common.pool, options->common.timeout_ms, remote, 1, &raw, &info,
                              endpoint, sizeof(endpoint), err) != TDX_OK)
        goto close_output;
    if (!options->common.quiet)
        fprintf(stderr, "jsn %s: %zu bytes, md5=%s, endpoint=%s\n", remote, raw.len,
                info.md5[0] ? info.md5 : "(none)", endpoint);
    if (tdx_jsn_gbk_to_utf8(raw.data, raw.len, &utf8, err) != TDX_OK)
        goto close_output;
    if (tdx_jsn_parse(utf8.data, utf8.len, &document, err) != TDX_OK)
        goto close_output;

    for (group_index = 0; group_index < tdx_jsn_group_count(&document); ++group_index) {
        const tdx_jsn_group *group = &document.groups[group_index];

        for (row = 0; row < group->row_count; ++row) {
            tdx_buf_clear(&line);
            if (options->convertible) {
                tdx_convertible_row bond;
                tdx_error step;
                step.message[0] = '\0';
                if (tdx_convertible_normalize(&document, group, row, &bond, &step) != TDX_OK) {
                    if (err) *err = step;
                    goto close_output;
                }
                if (tdx_convertible_format(&line, &bond, remote, group_index, row, err) !=
                    TDX_OK)
                    goto close_output;
                if (bond.core_terms_complete)
                    convertible_complete++;
                if (bond.kind == TDX_CONVERTIBLE_EXCHANGEABLE)
                    convertible_exchangeable++;
                if (bond.has_underlying)
                    convertible_underlying++;
                if (cli_write_json_line(stream, &line, err) != TDX_OK)
                    goto close_output;
                continue;
            }
            if (options->bonds) {
                tdx_bond_row bond;
                tdx_error step;
                step.message[0] = '\0';
                if (tdx_bonds_normalize(&document, group, row, remote, &bond, &step) != TDX_OK) {
                    /* A row whose identity cannot be formed cannot be attributed to
                     * a security, so it is reported rather than skipped. */
                    if (err) *err = step;
                    goto close_output;
                }
                if (options->schedule) {
                    if (tdx_bonds_format_with_schedule(&line, &bond, remote, &document, group,
                                                        group_index, row, 512, err) != TDX_OK)
                        goto close_output;
                } else if (tdx_bonds_format(&line, &bond, remote, group_index, row, err) != TDX_OK) {
                    goto close_output;
                }
                if (bond.name_resolved)
                    bonds_named++;
                if (bond.has_underlying)
                    bonds_underlying++;
                if (bond.has_source_scale)
                    bonds_sized++;
                if (cli_write_json_line(stream, &line, err) != TDX_OK)
                    goto close_output;
                continue;
            }
            {
                size_t renamed = 0;
                if (tdx_jsn_format_row(&line, &document, group_index, row, remote,
                                        &renamed, err) != TDX_OK ||
                    cli_write_json_line(stream, &line, err) != TDX_OK)
                    goto close_output;
                columns_renamed += renamed;
            }
        }
    }
    /* Captured before the document is freed, because reading them afterwards
     * would be reading freed memory. */
    group_count = document.group_count;
    row_count = document.row_count;
    tdx_buf_clear(&line);
    if (options->convertible) {
        if (tdx_convertible_format_summary(&line, row_count, convertible_complete,
                                           convertible_exchangeable, convertible_underlying,
                                           remote, endpoint, err) != TDX_OK)
            goto close_output;
    } else if (options->bonds) {
        if (tdx_bonds_format_summary(&line, row_count, bonds_named, bonds_underlying,
                                     bonds_sized, remote,
                                     tdx_bonds_scale_name(tdx_bonds_profile(remote).scale),
                                     endpoint, err) != TDX_OK)
            goto close_output;
    } else {
        tdx_jsn_summary summary = {remote, raw.len, info.md5, group_count, row_count,
                                   columns_renamed, endpoint};
        if (tdx_jsn_format_summary(&line, &summary, err) != TDX_OK)
            goto close_output;
    }
    if (cli_write_json_line(stream, &line, err) != TDX_OK)
        goto close_output;
    status = TDX_OK;

close_output:
    status = cli_finish_output(stream, status, err);
    tdx_jsn_document_free(&document);
    tdx_buf_free(&raw);
    tdx_buf_free(&utf8);
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->common.quiet)
        fprintf(stderr, "jsn: %zu groups, %zu rows\n", group_count, row_count);
    return status;
}
