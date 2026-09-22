/* timeline: command orchestration. */
#include "cli_commands.h"
#include "cli_date.h"
#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_timeline.h"
#include "tdx_timeline_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



int cli_command_timeline(const cli_timeline_options *options, tdx_error *err) {
    tdx_timeline timeline;
    tdx_buf line = {0};
    FILE *stream = NULL;
    char endpoint[80];
    char requested_date[9];
    const char *requested = NULL;
    const char *code;
    int market_id;
    int history;
    int status = TDX_ERR;
    size_t index;
    long volume = 0;

    if (options->common.security_count != 1) {
        tdx_error_set(err, "timeline needs exactly one --security");
        return TDX_ERR;
    }
    if (options->date) {
        if (cli_resolve_date(options->date, requested_date, NULL, err) != TDX_OK)
            return TDX_ERR;
        requested = requested_date;
    }
    market_id = options->common.securities[0].market_id;
    code = options->common.securities[0].code;
    history = requested ? 1 : 0;

    tdx_buf_init(&line);
    tdx_timeline_init(&timeline);
    memset(endpoint, 0, sizeof(endpoint));
    if (tdx_timeline_fetch_from_pool(&options->common.pool, options->common.timeout_ms, market_id, code,
                                     history, requested, &timeline, endpoint,
                                     sizeof(endpoint), err) != TDX_OK)
        goto done;

    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        goto done;
    }
    for (index = 0; index < timeline.count; ++index) {
        tdx_buf_clear(&line);
        if (tdx_timeline_format_point(&line, &timeline, &timeline.points[index], market_id, code,
                                      err) != TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the time-share stream");
            goto close_output;
        }
        volume += (long)timeline.points[index].volume_hand;
    }
    tdx_buf_clear(&line);
    if (tdx_timeline_format_summary(&line, &timeline, market_id, code, endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the time-share summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    status = cli_finish_output(stream, status, err);
    stream = NULL;
    if (status != TDX_OK)
        goto done;
    if (!options->common.quiet)
        fprintf(stderr,
                "timeline %s %s: command=0x0537 points=%zu base=%.4f volume_hand=%ld "
                "endpoint=%s\n",
                code, requested ? requested : "today", timeline.count,
                timeline.base_price, volume, endpoint);

done:
    tdx_buf_free(&line);
    tdx_timeline_free(&timeline);
    return status;
}
