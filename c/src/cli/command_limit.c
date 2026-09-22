/* limit: command orchestration. */
#include "cli_commands.h"
#include "cli_date.h"
#include "tdx_limit_json.h"
#include <stdio.h>

int cli_command_limit(const cli_limit_options *options, tdx_error *err) {
    tdx_buf line = {0};
    tdx_limit_rules rules;
    tdx_limit_prices prices;
    FILE *stream = NULL;
    int status = TDX_ERR;
    int as_of = 0;

    if (tdx_limit_rules_load(options->common.root, &rules, err) != TDX_OK)
        return TDX_ERR;
    if (options->date && *options->date) {
        uint32_t date;
        char canonical[9];
        if (cli_resolve_date(options->date, canonical, &date, err) != TDX_OK) return TDX_ERR;
        as_of = (int)date;
    }
    tdx_buf_init(&line);
    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        goto done;
    }
    tdx_buf_clear(&line);
    if (tdx_limit_format_rules(&line, &rules, err) != TDX_OK ||
        cli_write_json_line(stream, &line, err) != TDX_OK)
        goto done;
    if (options->common.security_count == 1) {
        const tdx_code *security = &options->common.securities[0];
        /* --prev is the previous close, which the rules need and a caller has to supply:
         * nothing here can know it. */
        if (!tdx_limit_calculate(security->market_id, security->code, options->name,
                                 (double)options->previous_close, as_of, &rules, &prices)) {
            tdx_error_set(err, "%s is not price limited", security->code);
            goto done;
        }
        tdx_buf_clear(&line);
        if (tdx_limit_format_prices(&line, security, (double)options->previous_close,
                                     &prices, err) != TDX_OK ||
            cli_write_json_line(stream, &line, err) != TDX_OK)
            goto done;
    }
    status = TDX_OK;

done:
    status = cli_finish_output(stream, status, err);
    tdx_buf_free(&line);
    return status;
}
