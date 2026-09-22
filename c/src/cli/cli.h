/* Internal CLI boundary; domain code does not depend on this header. */
#ifndef TDX_CLI_H
#define TDX_CLI_H
#include <stdio.h>
#include <stddef.h>
#include "tdx_l1.h"
#include "tdx_directory.h"
#include "tdx_state.h"
#define TDX_CLI_SECURITY_MAX 8192
#define TDX_CLI_ROOT_MAX 512
typedef struct cli_common_options {
    tdx_code securities[TDX_CLI_SECURITY_MAX];
    size_t security_count;
    tdx_endpoint_pool pool;
    int timeout_ms;
    size_t endpoint_limit;
    int markets[3];
    size_t market_count;
    char category[TDX_DIRECTORY_CATEGORY_MAX];
    const char *output;
    char root[TDX_CLI_ROOT_MAX];
    int quiet;
    size_t limit;
} cli_common_options;

typedef enum cli_value_kind { CLI_FLAG, CLI_INT, CLI_SIZE, CLI_UNSIGNED, CLI_STRING, CLI_REAL, CLI_DATE, CLI_SECURITY, CLI_HOST, CLI_ROOT, CLI_MARKETS, CLI_CATEGORY } cli_value_kind;
typedef struct cli_option_spec {
    const char *name;
    cli_value_kind kind;
    size_t offset;
    long long minimum, maximum;
    int flag_value;
    size_t presence_offset;
} cli_option_spec;
typedef struct cli_command {
    const char *name;
    size_t options_size;
    const cli_option_spec *options;
    size_t option_count;
    void (*initialize)(void *options);
    int (*run)(const void *options, tdx_error *err);
    int uses_network;
} cli_command;
extern const cli_command cli_commands[];
extern const size_t cli_command_count;
const cli_command *cli_find_command(const char *name);
void cli_common_init(cli_common_options *options);
int cli_parse_options(const cli_command *command, void *options, int argc, char **argv, tdx_error *err);
void cli_usage(void);
FILE *cli_open_output(const cli_common_options *options);
int cli_depth_json(FILE *stream, const tdx_depth *depth, tdx_error *err);
int cli_security_json(FILE *stream, const tdx_security *security, tdx_error *err);
/* Adds the JSONL newline, then writes the complete buffered object. */
int cli_write_json_line(FILE *stream, tdx_buf *line, tdx_error *err);
/* Flushes successful output, closes owned files, and propagates deferred I/O errors. */
int cli_finish_output(FILE *stream, int status, tdx_error *err);
int cli_build_universe(const cli_common_options *options, tdx_code **out, size_t *out_count, tdx_error *err);
#endif
