/* daily: command orchestration. */
#include "cli_commands.h"
#include "tdx_bytes.h"
#include "tdx_daily.h"
#include "tdx_daily_json.h"
#include "tdx_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



int cli_command_daily(const cli_daily_options *options, tdx_error *err) {
    static tdx_buf raw = {0};
    static tdx_buf line = {0};
    tdx_daily_bar *bars = NULL;
    char path[TDX_DAILY_PATH_MAX];
    char security_id[24];
    FILE *input = NULL;
    FILE *stream = NULL;
    long length;
    size_t bar_count = 0;
    size_t suspicious = 0;
    size_t emitted = 0;
    size_t index;
    size_t first_date = 0;
    size_t last_date = 0;
    const char *code = NULL;
    int market_id = -1;
    int scale;
    int status = TDX_ERR;

    if (options->common.security_count == 1) {
        code = options->common.securities[0].code;
        market_id = options->common.securities[0].market_id;
    } else if (!options->input || !*options->input) {
        tdx_error_set(err, "daily needs one --security CODE or an --input PATH");
        return TDX_ERR;
    }
    if (options->input && *options->input) {
        snprintf(path, sizeof(path), "%s", options->input);
        /* With an explicit file the scale has to come from the code, so a caller who knows
         * it should still pass --security; otherwise the stock scale is assumed and the
         * output says which divisor was used. */
    } else if (tdx_daily_locate(options->common.root, market_id, code, path, sizeof(path), err) !=
               TDX_OK) {
        return TDX_ERR;
    }
    if (code && market_id >= 0) {
        const char *prefix = market_id == 0 ? "SZ" : market_id == 1 ? "SH" : "BJ";
        snprintf(security_id, sizeof(security_id), "%s%s", prefix, code);
    } else {
        snprintf(security_id, sizeof(security_id), "%s", "");
    }
    scale = tdx_daily_scale_divisor(code);

    tdx_buf_init(&raw);
    tdx_buf_init(&line);
    input = fopen(path, "rb");
    if (!input) {
        tdx_error_set(err, "cannot open %s", path);
        goto done;
    }
    if (fseek(input, 0, SEEK_END) != 0 || (length = ftell(input)) < 0 ||
        fseek(input, 0, SEEK_SET) != 0) {
        tdx_error_set(err, "cannot size %s", path);
        goto done;
    }
    if (length > 0) {
        if (tdx_buf_reserve(&raw, (size_t)length, err) != TDX_OK)
            goto done;
        if (fread(raw.data, 1, (size_t)length, input) != (size_t)length) {
            tdx_error_set(err, "cannot read %s", path);
            goto done;
        }
        raw.len = (size_t)length;
    }
    fclose(input);
    input = NULL;

    bars = (tdx_daily_bar *)calloc(TDX_DAILY_BARS_MAX, sizeof(*bars));
    if (!bars) {
        tdx_error_set(err, "out of memory for the daily bars");
        goto done;
    }
    if (tdx_daily_parse(raw.data, raw.len, code, bars, TDX_DAILY_BARS_MAX, &bar_count,
                        &suspicious, err) != TDX_OK)
        goto done;
    for (index = 0; index < bar_count; ++index) {
        if (!first_date || bars[index].date < first_date)
            first_date = bars[index].date;
        if (bars[index].date > last_date)
            last_date = bars[index].date;
    }

    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        goto done;
    }
    tdx_buf_clear(&line);
    if (tdx_daily_format_summary(&line, path, security_id, scale, raw.len, bar_count, suspicious,
                                 first_date, last_date, err) != TDX_OK)
        goto done;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
        fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the daily summary");
        goto done;
    }
    for (index = 0; index < bar_count; ++index) {
        if (options->max_records && emitted >= options->max_records)
            break;
        tdx_buf_clear(&line);
        if (tdx_daily_format_bar(&line, &bars[index], security_id, scale, index, err) != TDX_OK)
            goto done;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
            fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the daily bars");
            goto done;
        }
        emitted++;
    }
    status = TDX_OK;
    if (!options->common.quiet)
        fprintf(stderr, "daily %s: %zu bytes, %zu bars, scale divisor %d, %zu emitted\n", path,
                raw.len, bar_count, scale, emitted);

done:
    if (input)
        fclose(input);
    status = cli_finish_output(stream, status, err);
    stream = NULL;
    free(bars);
    tdx_buf_free(&raw);
    tdx_buf_free(&line);
    return status;
}
