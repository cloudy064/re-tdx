/* pricing: command orchestration. */
#include "cli_commands.h"
#include "tdx_bytes.h"
#include "tdx_download.h"
#include "tdx_endpoint.h"
#include "tdx_error.h"
#include "tdx_jsn.h"
#include "tdx_pricing.h"
#include "tdx_pricing_json.h"
#include "tdx_quote.h"
#include "tdx_snapshot.h"
#include "cli_date.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



int cli_command_pricing(const cli_pricing_options *options, tdx_error *err) {
    static tdx_jsn_document document = {0};
    static tdx_buf raw = {0};
    static tdx_buf utf8 = {0};
    static tdx_buf line = {0};
    static tdx_code codes[TDX_PRICING_ROWS_MAX * 2];
    tdx_snapshot *quotes = NULL;
    tdx_pricing_row *rows = NULL;
    tdx_file_info info;
    char remote[128];
    char endpoint[80];
    char as_of[16];
    FILE *stream = NULL;
    size_t code_count = 0;
    size_t collected_skipped = 0;
    size_t quote_count = 0;
    size_t count = 0;
    size_t skipped = 0;
    size_t index;
    size_t complete = 0;
    size_t bond_only = 0;
    size_t terms_only = 0;
    size_t with_yield = 0;
    size_t with_pure = 0;
    size_t live_priced = 0;
    size_t pre_close_priced = 0;
    int status = TDX_ERR;

    if (cli_resolve_date(options->date, as_of, NULL, err) != TDX_OK)
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
    memset(&info, 0, sizeof(info));
    memset(endpoint, 0, sizeof(endpoint));

    if (tdx_jsn_remote_path(TDX_PRICING_RESOURCE, "bi", remote, sizeof(remote), err) != TDX_OK)
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
    if (tdx_pricing_collect_codes(&document, &document.groups[0], codes,
                                  TDX_PRICING_ROWS_MAX * 2, &code_count, &collected_skipped,
                                  err) != TDX_OK)
        goto close_output;
    if (!options->common.quiet)
        fprintf(stderr, "pricing %s: %zu bytes, md5=%s, %zu rows, %zu securities to quote\n",
                remote, raw.len, info.md5, document.groups[0].row_count, code_count);

    /* The quotes, in batches of the snapshot command's own cap. */
    if (code_count > 0) {
        quotes = (tdx_snapshot *)calloc(code_count, sizeof(*quotes));
        if (!quotes) {
            tdx_error_set(err, "out of memory for %zu quotes", code_count);
            goto close_output;
        }
        {
            tdx_connection connection;
            size_t offset;
            memset(&connection, 0, sizeof(connection));
            connection.socket_handle = (intptr_t)-1;
            if (tdx_connection_open(&connection, &options->common.pool.items[0], options->common.timeout_ms,
                                    err) != TDX_OK)
                goto close_output;
            tdx_endpoint_address(&options->common.pool.items[0], endpoint, sizeof(endpoint));
            for (offset = 0; offset < code_count; offset += TDX_SNAPSHOT_BATCH_MAX) {
                size_t want = code_count - offset;
                size_t got = 0;
                if (want > TDX_SNAPSHOT_BATCH_MAX)
                    want = TDX_SNAPSHOT_BATCH_MAX;
                if (tdx_snapshot_fetch(&connection, codes + offset, want, quotes + quote_count,
                                       code_count - quote_count, &got, err) != TDX_OK) {
                    tdx_connection_close(&connection);
                    goto close_output;
                }
                quote_count += got;
            }
            tdx_connection_close(&connection);
        }
        if (!options->common.quiet)
            fprintf(stderr, "pricing: %zu quotes for %zu securities, endpoint=%s\n", quote_count,
                    code_count, endpoint);
    }

    rows = (tdx_pricing_row *)calloc(TDX_PRICING_ROWS_MAX, sizeof(*rows));
    if (!rows) {
        tdx_error_set(err, "out of memory for the pricing rows");
        goto close_output;
    }
    if (tdx_pricing_normalize(&document, &document.groups[0], quotes, quote_count, as_of,
                              strlen(as_of), rows, TDX_PRICING_ROWS_MAX, &count, &skipped,
                              err) != TDX_OK)
        goto close_output;

    for (index = 0; index < count; ++index) {
        tdx_price_source source = TDX_PRICE_UNAVAILABLE;
        double price = 0.0;
        tdx_buf_clear(&line);
        if (tdx_pricing_format(&line, &rows[index], TDX_PRICING_RESOURCE, index, err) != TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the pricing stream");
            goto close_output;
        }
        if (rows[index].availability && strcmp(rows[index].availability, "complete") == 0)
            complete++;
        else if (rows[index].availability && strcmp(rows[index].availability, "bond-only") == 0)
            bond_only++;
        else
            terms_only++;
        if (rows[index].has_maturity_yield_pct)
            with_yield++;
        if (rows[index].has_pure_bond_value)
            with_pure++;
        if (tdx_pricing_quote_price(&rows[index].bond_quote, &price, &source)) {
            if (source == TDX_PRICE_LAST)
                live_priced++;
            else if (source == TDX_PRICE_PRE_CLOSE)
                pre_close_priced++;
        }
    }

    tdx_buf_clear(&line);
    if (tdx_pricing_format_summary(&line, count, skipped, complete, bond_only, terms_only,
                                   with_yield, with_pure, live_priced, pre_close_priced,
                                   TDX_PRICING_RESOURCE, as_of, endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the pricing summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    status = cli_finish_output(stream, status, err);
    stream = NULL;
    free(quotes);
    free(rows);
    tdx_jsn_document_free(&document);
    tdx_buf_free(&raw);
    tdx_buf_free(&utf8);
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->common.quiet)
        fprintf(stderr,
                "pricing: %zu rows as of %s, %zu complete, %zu bond-only, %zu terms-only, "
                "%zu with a yield (%zu live / %zu pre-close priced)\n",
                count, as_of, complete, bond_only, terms_only, with_yield, live_priced,
                pre_close_priced);
    return status;
}
