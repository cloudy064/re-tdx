/* kline: command orchestration. */
#include "cli_commands.h"
#include "cli_date.h"
#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_kline.h"
#include "tdx_kline_json.h"
#include "tdx_minute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



static size_t kline_select_date(tdx_kline_bar *bars, size_t count, int wanted) {
    size_t read_index;
    size_t write_index = 0;
    for (read_index = 0; read_index < count; ++read_index)
        if (bars[read_index].date == wanted)
            bars[write_index++] = bars[read_index];
    return write_index;
}


int cli_command_kline(const cli_kline_options *options, tdx_error *err) {
    tdx_kline_series series;
    tdx_kline_period period;
    tdx_buf line = {0};
    FILE *stream = NULL;
    char endpoint[80];
    char day_filter[9];
    uint32_t wanted_date = 0;
    const char *code;
    int market_id;
    int index_mode;
    int status = TDX_ERR;
    size_t index;
    size_t emitted = 0;

    if (options->common.security_count != 1) {
        tdx_error_set(err, "kline needs exactly one --security");
        return TDX_ERR;
    }
    if (!options->period) {
        tdx_error_set(err, "kline needs --period time, 1m, 5m, 15m, 30m, 60m, day, week or month");
        return TDX_ERR;
    }
    if (tdx_kline_period_parse(options->period, &period, err) != TDX_OK)
        return TDX_ERR;
    market_id = options->common.securities[0].market_id;
    code = options->common.securities[0].code;
    if (options->index_mode > 0)
        index_mode = 1;
    else if (options->index_mode < 0)
        index_mode = 0;
    else
        index_mode = market_id == 1 && tdx_kline_is_block_index_code(code);
    day_filter[0] = '\0';
    if (options->date) {
        if (cli_resolve_date(options->date, day_filter, &wanted_date, err) != TDX_OK)
            return TDX_ERR;
        if (!period.intraday) {
            tdx_error_set(err, "--date only filters intraday periods; use --start for daily bars");
            return TDX_ERR;
        }
    }

    tdx_buf_init(&line);
    tdx_kline_series_init(&series);
    memset(endpoint, 0, sizeof(endpoint));
    if (tdx_kline_fetch_from_pool(&options->common.pool, options->common.timeout_ms, market_id, code, &period,
                                  index_mode, (uint16_t)options->start,
                                  (uint16_t)(options->page_size ? options->page_size
                                                                : TDX_KLINE_PAGE_SIZE_MAX),
                                  (size_t)(options->max_pages ? options->max_pages
                                                              : TDX_KLINE_PAGES_MAX),
                                  &series, endpoint, sizeof(endpoint), err) != TDX_OK)
        goto done;

    if (day_filter[0])
        series.count = kline_select_date(series.bars, series.count, (int)wanted_date);

    /* WRITE THE SAME BARS AS .lc1, which is the format the terminal itself reads for local
     * minute data.  The conversion happens first and in full, so a bar the file cannot hold
     * leaves no partial file behind. */
    if (options->lc1_output && *options->lc1_output) {
        static tdx_lc1_bar packed[TDX_LC1_BARS_MAX];
        tdx_buf records = {0};
        FILE *file;
        size_t converted;
        int refused = 0;
        if (series.count > TDX_LC1_BARS_MAX) {
            tdx_error_set(err, "%zu bars exceed the %d this writer holds", series.count,
                          TDX_LC1_BARS_MAX);
            goto done;
        }
        for (converted = 0; converted < series.count; ++converted) {
            if (tdx_lc1_from_kline(&series.bars[converted], &packed[converted], err) != TDX_OK) {
                refused = 1;
                break;
            }
        }
        if (refused)
            goto done;
        tdx_buf_init(&records);
        if (tdx_lc1_pack(packed, series.count, &records, err) != TDX_OK) {
            tdx_buf_free(&records);
            goto done;
        }
        file = fopen(options->lc1_output, "wb");
        if (!file) {
            tdx_error_set(err, "cannot open %s for writing", options->lc1_output);
            tdx_buf_free(&records);
            goto done;
        }
        if (records.len && fwrite(records.data, 1, records.len, file) != records.len) {
            fclose(file);
            tdx_error_set(err, "cannot write the .lc1 file");
            tdx_buf_free(&records);
            goto done;
        }
        if (cli_finish_output(file, TDX_OK, err) != TDX_OK) {
            tdx_buf_free(&records);
            goto done;
        }
        if (!options->common.quiet)
            fprintf(stderr, "wrote %zu bars (%zu bytes) to %s\n", series.count, records.len,
                    options->lc1_output);
        tdx_buf_free(&records);
    }

    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        goto done;
    }
    for (index = 0; index < series.count; ++index) {
        tdx_buf_clear(&line);
        if (tdx_kline_format_bar(&line, &series.bars[index], market_id, code, period.name,
                                 err) != TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the K-line stream");
            goto close_output;
        }
        emitted++;
    }
    tdx_buf_clear(&line);
    if (tdx_kline_format_summary(&line, &series, market_id, code, period.name,
                                 (uint16_t)options->start, endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the K-line summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    status = cli_finish_output(stream, status, err);
    stream = NULL;
    if (status != TDX_OK)
        goto done;
    if (!options->common.quiet) {
        char first[32];
        char last[32];
        if (emitted) {
            snprintf(first, sizeof(first), "%d %02d:%02d", series.bars[0].date,
                     series.bars[0].hour, series.bars[0].minute);
            snprintf(last, sizeof(last), "%d %02d:%02d", series.bars[series.count - 1].date,
                     series.bars[series.count - 1].hour,
                     series.bars[series.count - 1].minute);
        } else {
            snprintf(first, sizeof(first), "-");
            snprintf(last, sizeof(last), "-");
        }
        fprintf(stderr,
                "kline %s %s: period=%s(id=%u) index=%d pages=%zu bars=%zu %s..%s "
                "endpoint=%s reached_end=%d\n",
                code, options->period, period.name, (unsigned)period.id, index_mode,
                series.pages, emitted, first, last, endpoint, series.reached_end);
    }

done:
    tdx_buf_free(&line);
    tdx_kline_series_free(&series);
    return status;
}
