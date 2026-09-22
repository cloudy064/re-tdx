/* Process entry point: command selection, arguments and exit status. */
#include "cli/cli.h"
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const cli_command *command;
    tdx_error error = {{0}};
    void *options;
    int result;
    if (argc < 2) { cli_usage(); return 2; }
    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        cli_usage();
        return 0;
    }
    command = cli_find_command(argv[1]);
    if (!command) {
        fprintf(stderr, "unknown command: %s\n", argv[1]);
        return 2;
    }
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            cli_usage();
            return 0;
        }
    }
    options = calloc(1, command->options_size);
    if (!options) { fprintf(stderr, "error: cannot allocate command options\n"); return 1; }
    if (cli_parse_options(command, options, argc - 2, argv + 2, &error) != TDX_OK) {
        fprintf(stderr, "error: %s\n", error.message);
        free(options);
        return 2;
    }
    result = command->run(options, &error);
    free(options);
    if (result != TDX_OK) fprintf(stderr, "error: %s\n", error.message);
    return result == TDX_OK ? 0 : 1;
}
