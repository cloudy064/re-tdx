/* convertible: command orchestration. */
#include "cli_commands.h"
#include "tdx_convertible_json.h"
#include "tdx_download.h"
#include <stdio.h>
#include <string.h>

int cli_command_convertible(const cli_convertible_options *options, tdx_error *err) {
    /* Six core documents, then the exchangeable-bond substitute overview and the
     * projection that fills only the ten fields the reference allows it to fill. */
    static const char *const resources[8] = {
        TDX_CONVERTIBLE_OVERVIEW_RESOURCE,   TDX_CONVERTIBLE_PROGRESS_RESOURCE,
        TDX_CONVERTIBLE_COUPONS_RESOURCE,    TDX_CONVERTIBLE_SELLBACK_RESOURCE,
        TDX_CONVERTIBLE_REDEMPTION_RESOURCE, TDX_CONVERTIBLE_REVISION_RESOURCE,
        TDX_CONVERTIBLE_EXCHANGEABLE_RESOURCE,
        TDX_CONVERTIBLE_EXCHANGEABLE_PROJECTION_RESOURCE,
    };
    tdx_jsn_document documents[8] = {0};
    tdx_buf raw = {0};
    tdx_buf utf8 = {0};
    tdx_buf line = {0};
    static tdx_code keys[TDX_CONVERTIBLE_KEYS_MAX];
    tdx_convertible_documents set;
    tdx_convertible_row row;
    tdx_convertible_extra extra;
    tdx_convertible_join_flags flags;
    tdx_file_info info;
    char remote[128];
    char endpoint[80];
    FILE *stream;
    size_t index;
    size_t key_count = 0;
    size_t union_count = 0;
    size_t emitted = 0;
    size_t complete = 0;
    size_t from_overview = 0;
    size_t only_elsewhere = 0;
    size_t supplemented = 0;
    size_t projection_verified = 0;
    size_t projection_fields = 0;
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
    for (index = 0; index < 8; ++index)
        tdx_jsn_document_init(&documents[index]);

    for (index = 0; index < 8; ++index) {
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
        if (!options->common.quiet)
            fprintf(stderr, "convertible %s: %zu bytes, md5=%s, %zu rows\n", remote, raw.len,
                    info.md5, documents[index].row_count);
    }

    set.overview = &documents[0];
    set.progress = &documents[1];
    set.coupons = &documents[2];
    set.sellback = &documents[3];
    set.redemption = &documents[4];
    set.revision = &documents[5];
    set.exchangeable = &documents[6];
    set.projection = &documents[7];
    if (tdx_convertible_keys(&set, keys, TDX_CONVERTIBLE_KEYS_MAX, &key_count, &union_count,
                             err) != TDX_OK)
        goto close_output;
    if (!options->common.quiet)
        fprintf(stderr, "convertible: the eight documents name %zu distinct bonds\n",
                union_count);

    for (index = 0; index < key_count; ++index) {
        tdx_error step;
        step.message[0] = '\0';
        if (options->max_records > 0 && emitted >= (size_t)options->max_records)
            break;
        if (tdx_convertible_join(&set, &keys[index], &row, &extra, &flags, &step) != TDX_OK) {
            if (err) *err = step;
            goto close_output;
        }
        tdx_buf_clear(&line);
        if (tdx_convertible_format_joined(&line, &row, &extra, &flags, resources[0], err) !=
            TDX_OK)
            goto close_output;
        if (cli_write_json_line(stream, &line, err) != TDX_OK)
            goto close_output;
        emitted++;
        if (row.core_terms_complete)
            complete++;
        if (flags.from_overview)
            from_overview++;
        else
            only_elsewhere++;
        if (flags.exchangeable_supplemented)
            supplemented++;
        if (flags.exchangeable_projection_verified)
            projection_verified++;
        projection_fields += flags.projection_fields_used;
    }

    tdx_buf_clear(&line);
    {
        tdx_convertible_join_summary summary = {
            8, endpoint, union_count, emitted, complete, from_overview, only_elsewhere,
            supplemented, projection_verified, projection_fields
        };
        if (tdx_convertible_format_join_summary(&line, &summary, err) != TDX_OK)
            goto close_output;
    }
    if (cli_write_json_line(stream, &line, err) != TDX_OK)
        goto close_output;
    status = TDX_OK;

close_output:
    status = cli_finish_output(stream, status, err);
    for (index = 0; index < 8; ++index)
        tdx_jsn_document_free(&documents[index]);
    tdx_buf_free(&raw);
    tdx_buf_free(&utf8);
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->common.quiet)
        fprintf(stderr,
                "convertible: %zu of %zu bonds joined, %zu only in a non-overview document, "
                "%zu exchangeable, %zu projection fields used\n",
                emitted, union_count, only_elsewhere, supplemented, projection_fields);
    return status;
}
