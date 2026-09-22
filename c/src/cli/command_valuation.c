/* valuation: command orchestration. */
#include "cli_commands.h"
#include "tdx_bytes.h"
#include "tdx_download.h"
#include "tdx_error.h"
#include "tdx_jsn.h"
#include "tdx_quote.h"
#include "tdx_valuation.h"
#include "tdx_valuation_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



int cli_command_valuation(const cli_valuation_options *options, tdx_error *err) {
    static tdx_jsn_document master = {0};
    static tdx_buf raw = {0};
    static tdx_buf utf8 = {0};
    static tdx_buf line = {0};
    tdx_valuation_index *indices = NULL;
    tdx_valuation_point *points = NULL;
    tdx_valuation_fund *funds = NULL;
    tdx_jsn_document pe = {0};
    tdx_jsn_document pb = {0};
    tdx_jsn_document fund_doc = {0};
    tdx_file_info info;
    char remote[192];
    char endpoint[80];
    FILE *stream = NULL;
    size_t index_count = 0;
    size_t skipped = 0;
    size_t point_count = 0;
    size_t fund_count = 0;
    size_t emitted = 0;
    size_t index;
    int status = TDX_ERR;

    tdx_buf_init(&raw);
    tdx_buf_init(&utf8);
    tdx_buf_init(&line);
    tdx_jsn_document_init(&master);
    tdx_jsn_document_init(&pe);
    tdx_jsn_document_init(&pb);
    tdx_jsn_document_init(&fund_doc);
    memset(&info, 0, sizeof(info));
    memset(endpoint, 0, sizeof(endpoint));

    if (tdx_jsn_remote_path(TDX_VALUATION_MASTER_RESOURCE, "bi", remote, sizeof(remote),
                            err) != TDX_OK)
        goto done;
    if (tdx_download_resource(&options->common.pool, options->common.timeout_ms, remote, 1, &raw, &info,
                              endpoint, sizeof(endpoint), err) != TDX_OK)
        goto done;
    if (tdx_jsn_gbk_to_utf8(raw.data, raw.len, &utf8, err) != TDX_OK)
        goto done;
    if (tdx_jsn_parse(utf8.data, utf8.len, &master, err) != TDX_OK)
        goto done;
    if (master.group_count != 1) {
        tdx_error_set(err, "%s holds %zu groups, expected one", remote, master.group_count);
        goto done;
    }
    indices = (tdx_valuation_index *)calloc(TDX_VALUATION_INDICES_MAX, sizeof(*indices));
    points = (tdx_valuation_point *)calloc(TDX_VALUATION_HISTORY_MAX, sizeof(*points));
    funds = (tdx_valuation_fund *)calloc(TDX_VALUATION_FUNDS_MAX, sizeof(*funds));
    if (!indices || !points || !funds) {
        tdx_error_set(err, "out of memory for the valuation buffers");
        goto done;
    }
    if (tdx_valuation_parse_master(&master, &master.groups[0], indices,
                                   TDX_VALUATION_INDICES_MAX, &index_count, &skipped,
                                   err) != TDX_OK)
        goto done;
    if (!options->common.quiet)
        fprintf(stderr, "valuation %s: %zu bytes, md5=%s, %zu indices\n", remote, raw.len,
                info.md5, index_count);

    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        goto done;
    }
    for (index = 0; index < index_count; ++index) {
        if (options->max_records && emitted >= options->max_records)
            break;
        tdx_buf_clear(&line);
        if (tdx_valuation_format_index(&line, &indices[index], index, err) != TDX_OK)
            goto done;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
            fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the valuation index");
            goto done;
        }
        emitted++;
    }

    /* One index's history, when the caller named one. */
    if (options->common.security_count == 1) {
        const tdx_code *security = &options->common.securities[0];
        const char *detail_id = NULL;
        size_t found;
        for (found = 0; found < index_count; ++found)
            if (indices[found].market_id == security->market_id &&
                strcmp(indices[found].code, security->code) == 0) {
                detail_id = indices[found].detail_id;
                break;
            }
        if (!detail_id) {
            tdx_error_set(err, "%s is not one of the %zu valued indices", security->code,
                          index_count);
            goto done;
        }
        {
            /* The three detail resources sit under the same root with no "list/" in
             * between: the detail id names them directly. */
            static const char *const suffixes[3] = {"zsgz1", "zsgz3", "zsgz4"};
            tdx_jsn_document *targets[3];
            char detail_endpoint[80];
            targets[0] = &fund_doc;
            targets[1] = &pe;
            targets[2] = &pb;
            memset(detail_endpoint, 0, sizeof(detail_endpoint));
            for (index = 0; index < 3; ++index) {
                snprintf(remote, sizeof(remote), "%s/%s.jsn", suffixes[index], detail_id);
                /* The downloader wants the fully prefixed path, as every other command
                 * here builds it. */
                {
                    char prefixed[224];
                    if (tdx_jsn_remote_path(remote, "bi", prefixed, sizeof(prefixed), err) !=
                        TDX_OK)
                        goto done;
                    if (strlen(prefixed) >= sizeof(remote)) {
                        tdx_error_set(err, "the valuation resource path is too long");
                        goto done;
                    }
                    memcpy(remote, prefixed, strlen(prefixed) + 1);
                }
                tdx_buf_clear(&raw);
                tdx_buf_clear(&utf8);
                memset(&info, 0, sizeof(info));
                if (tdx_download_resource(&options->common.pool, options->common.timeout_ms, remote, 1, &raw,
                                          &info, detail_endpoint, sizeof(detail_endpoint),
                                          err) != TDX_OK)
                    goto done;
                if (tdx_jsn_gbk_to_utf8(raw.data, raw.len, &utf8, err) != TDX_OK)
                    goto done;
                if (tdx_jsn_parse(utf8.data, utf8.len, targets[index], err) != TDX_OK)
                    goto done;
                if (!options->common.quiet)
                    fprintf(stderr, "valuation %s: %zu bytes, %zu rows\n", remote, raw.len,
                            targets[index]->group_count ? targets[index]->groups[0].row_count
                                                        : 0);
            }
        }
        {
            tdx_valuation_merge merge;
            char pe_resource[64];
            char pb_resource[64];
            snprintf(pe_resource, sizeof(pe_resource), "zsgz3/%s.jsn", detail_id);
            snprintf(pb_resource, sizeof(pb_resource), "zsgz4/%s.jsn", detail_id);
            if (tdx_valuation_merge_history(&pe, pe.group_count ? &pe.groups[0] : NULL,
                                            &pb, pb.group_count ? &pb.groups[0] : NULL,
                                            points, TDX_VALUATION_HISTORY_MAX, &merge,
                                            err) != TDX_OK)
                goto done;
            point_count = merge.points;
            tdx_buf_clear(&line);
            if (tdx_valuation_format_merge(&line, &merge, security->code, pe_resource,
                                           pb_resource, err) != TDX_OK)
                goto done;
            if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
                fwrite(line.data, 1, line.len, stream) != line.len) {
                tdx_error_set(err, "cannot write the merge report");
                goto done;
            }
            for (index = 0; index < point_count; ++index) {
                if (options->max_records && emitted >= options->max_records)
                    break;
                tdx_buf_clear(&line);
                if (tdx_valuation_format_point(&line, &points[index], indices[0].security_id,
                                               index, err) != TDX_OK)
                    goto done;
                if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
                    fwrite(line.data, 1, line.len, stream) != line.len) {
                    tdx_error_set(err, "cannot write the history");
                    goto done;
                }
                emitted++;
            }
        }
        {
            size_t fund_skipped = 0;
            if (tdx_valuation_parse_funds(&fund_doc,
                                          fund_doc.group_count ? &fund_doc.groups[0] : NULL,
                                          funds, TDX_VALUATION_FUNDS_MAX, &fund_count,
                                          &fund_skipped, err) != TDX_OK)
                goto done;
            for (index = 0; index < fund_count; ++index) {
                if (options->max_records && emitted >= options->max_records)
                    break;
                tdx_buf_clear(&line);
                if (tdx_valuation_format_fund(&line, &funds[index], index, err) != TDX_OK)
                    goto done;
                if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
                    fwrite(line.data, 1, line.len, stream) != line.len) {
                    tdx_error_set(err, "cannot write the funds");
                    goto done;
                }
                emitted++;
            }
        }
    }

    tdx_buf_clear(&line);
    if (tdx_valuation_format_summary(&line, TDX_VALUATION_MASTER_RESOURCE, index_count,
                                     skipped, endpoint, err) != TDX_OK)
        goto done;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
        fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the valuation summary");
        goto done;
    }
    status = TDX_OK;
    if (!options->common.quiet)
        fprintf(stderr, "valuation: %zu indices, %zu history points, %zu funds, %zu emitted\n",
                index_count, point_count, fund_count, emitted);

done:
    status = cli_finish_output(stream, status, err);
    stream = NULL;
    free(indices);
    free(points);
    free(funds);
    tdx_jsn_document_free(&master);
    tdx_jsn_document_free(&pe);
    tdx_jsn_document_free(&pb);
    tdx_jsn_document_free(&fund_doc);
    tdx_buf_free(&raw);
    tdx_buf_free(&utf8);
    tdx_buf_free(&line);
    return status;
}
