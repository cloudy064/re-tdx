/* Strict scalar parsing and command-local option validation. */
#include "cli.h"
#include "tdx_date.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

void cli_common_init(cli_common_options *options) {
    memset(options, 0, sizeof(*options));
    options->timeout_ms = 10000;
    options->endpoint_limit = 3;
    options->markets[0] = 0;
    options->markets[1] = 1;
    options->markets[2] = 2;
    options->market_count = 3;
    snprintf(options->category, sizeof(options->category), "a_share");
}

static int parse_markets(const char *text, cli_common_options *options, tdx_error *err) {
    size_t count = 0;
    const char *cursor = text;
    int used[3] = {0};
    while (*cursor) {
        const char *end = strchr(cursor, ',');
        size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
        int market = -1;
        while (length && (*cursor == ' ' || *cursor == '\t')) { ++cursor; --length; }
        while (length && (cursor[length - 1] == ' ' || cursor[length - 1] == '\t')) --length;
        if (length == 2) {
            if (memcmp(cursor, "sz", 2) == 0) market = 0;
            if (memcmp(cursor, "sh", 2) == 0) market = 1;
            if (memcmp(cursor, "bj", 2) == 0) market = 2;
        }
        if (market < 0 || used[market] || count == 3) {
            tdx_error_set(err, "--market needs distinct sz,sh,bj names");
            return TDX_ERR;
        }
        options->markets[count++] = market;
        used[market] = 1;
        if (!end) break;
        cursor = end + 1;
        if (!*cursor) {
            tdx_error_set(err, "--market has an empty trailing market");
            return TDX_ERR;
        }
    }
    if (!count) { tdx_error_set(err, "--market needs at least one market"); return TDX_ERR; }
    options->market_count = count;
    return TDX_OK;
}

static const cli_option_spec *find_option(const cli_command *command, const char *name) {
    for (size_t i = 0; i < command->option_count; ++i)
        if (strcmp(command->options[i].name, name) == 0) return &command->options[i];
    return NULL;
}

static int integer_value(const char *value, const cli_option_spec *spec, long long *out, tdx_error *err) {
    char *end;
    long long result;
    if (!*value || (*value != '-' && (*value < '0' || *value > '9'))) goto invalid;
    errno = 0;
    result = strtoll(value, &end, 10);
    if (errno == ERANGE || *end || result < spec->minimum || result > spec->maximum) goto invalid;
    *out = result;
    return TDX_OK;
invalid:
    tdx_error_set(err, "%s needs an integer in %lld..%lld", spec->name, spec->minimum, spec->maximum);
    return TDX_ERR;
}

int cli_parse_options(const cli_command *command, void *raw, int argc, char **argv, tdx_error *err) {
    cli_common_options *common = raw; /* First member of every command-specific struct. */
    tdx_endpoint hosts[TDX_ENDPOINT_POOL_MAX];
    size_t host_count = 0;
    command->initialize(raw);
    for (int index = 0; index < argc; ++index) {
        const cli_option_spec *spec = find_option(command, argv[index]);
        const char *value;
        unsigned char *field;
        long long number = 0;
        if (!spec) {
            tdx_error_set(err, "%s does not accept %s", command->name, argv[index]);
            return TDX_ERR;
        }
        field = (unsigned char *)raw + spec->offset;
        if (spec->kind == CLI_FLAG) { *(int *)field = spec->flag_value; continue; }
        if (++index >= argc) { tdx_error_set(err, "%s needs a value", spec->name); return TDX_ERR; }
        value = argv[index];
        switch (spec->kind) {
        case CLI_SECURITY:
            for (;;) {
                if (common->security_count == TDX_CLI_SECURITY_MAX) {
                    tdx_error_set(err, "too many --security entries"); return TDX_ERR;
                }
                if (tdx_code_parse(argv[index], &common->securities[common->security_count], err) != TDX_OK)
                    return TDX_ERR;
                ++common->security_count;
                if (index + 1 >= argc || argv[index + 1][0] == '-') break;
                ++index;
            }
            break;
        case CLI_HOST:
            if (host_count == TDX_ENDPOINT_POOL_MAX) {
                tdx_error_set(err, "too many --host entries"); return TDX_ERR;
            }
            if (tdx_endpoint_parse(value, &hosts[host_count++], err) != TDX_OK) return TDX_ERR;
            break;
        case CLI_ROOT:
        case CLI_CATEGORY: {
            size_t capacity = spec->kind == CLI_ROOT ? sizeof(common->root) : sizeof(common->category);
            if (!*value || strlen(value) >= capacity) {
                tdx_error_set(err, "%s is empty or too long", spec->name); return TDX_ERR;
            }
            memcpy(field, value, strlen(value) + 1);
            break;
        }
        case CLI_MARKETS:
            if (parse_markets(value, common, err) != TDX_OK) return TDX_ERR;
            break;
        case CLI_STRING:
            if (!*value) { tdx_error_set(err, "%s must not be empty", spec->name); return TDX_ERR; }
            if (strcmp(spec->name, "--date") == 0) {
                uint32_t date;
                if (!tdx_date_parse(value, strlen(value), 1, &date)) {
                    tdx_error_set(err, "--date needs a real calendar date"); return TDX_ERR;
                }
            }
            *(const char **)field = value;
            break;
        case CLI_REAL: {
            char *end;
            double real;
            errno = 0;
            real = strtod(value, &end);
            if (!*value || value[0] == ' ' || value[0] == '\t' || end == value || *end || errno == ERANGE || !isfinite(real)) {
                tdx_error_set(err, "%s needs a finite number", spec->name); return TDX_ERR;
            }
            *(double *)field = real;
            if (spec->presence_offset) *(int *)((unsigned char *)raw + spec->presence_offset) = 1;
            break;
        }
        case CLI_DATE: {
            uint32_t date;
            if (!tdx_date_parse(value, strlen(value), 0, &date)) {
                tdx_error_set(err, "%s needs a real YYYYMMDD date", spec->name); return TDX_ERR;
            }
            *(int *)field = (int)date;
            break;
        }
        case CLI_INT:
        case CLI_SIZE:
        case CLI_UNSIGNED:
            if (integer_value(value, spec, &number, err) != TDX_OK) return TDX_ERR;
            if (spec->kind == CLI_INT) *(int *)field = (int)number;
            else if (spec->kind == CLI_SIZE) *(size_t *)field = (size_t)number;
            else *(unsigned *)field = (unsigned)number;
            break;
        default:
            tdx_error_set(err, "invalid option registry for %s", spec->name); return TDX_ERR;
        }
    }
    if (host_count) {
        size_t count = host_count < common->endpoint_limit ? host_count : common->endpoint_limit;
        memcpy(common->pool.items, hosts, count * sizeof(*hosts));
        common->pool.count = count;
        common->pool.configured_count = host_count;
        snprintf(common->pool.source, sizeof(common->pool.source), "explicit-host");
    } else if (command->uses_network && tdx_endpoint_pool_load(common->root[0] ? common->root : NULL,
                    common->endpoint_limit, &common->pool, err) != TDX_OK) return TDX_ERR;
    return TDX_OK;
}
