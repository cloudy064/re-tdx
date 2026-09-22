/* Public command behavior at the typed option boundary; no network needed. */
#include "cli_commands.h"
#include "cli_date.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static void *parse(const char *name, int argc, char **argv, int expected) {
    const cli_command *command = cli_find_command(name);
    tdx_error error = {{0}};
    void *options = calloc(1, command->options_size);
    int result = cli_parse_options(command, options, argc, argv, &error);
    CHECK(result == expected);
    if (result != expected) fprintf(stderr, "  %s: %s\n", name, error.message);
    return options;
}

static void test_explicit_dates(void) {
    static const struct {
        const char *command;
        const char *fetch_error;
    } cases[] = {
        {"day", "no usable cached copy of hishf/date/20240229/sz000001.img"},
        {"trades", "trade fetch needs an endpoint pool"},
        {"kline", "K-line fetch needs an endpoint pool"},
        {"timeline", "timeline fetch needs an endpoint pool"},
    };
    static const char *const dates[] = {"20240229", "2024-02-29"};
    static const char *const invalid[] = {"20260229", "2026-02-29", "2024-02-29junk"};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const cli_command *command = cli_find_command(cases[i].command);
        tdx_error baseline = {{0}};
        char *argv[] = {"--date", NULL, "--security", "sz000001", "--host",
                        "127.0.0.1:7709", "--quiet", NULL, NULL};
        int argc = 7;
        if (strcmp(command->name, "day") == 0) argv[argc++] = "--no-cache";
        if (strcmp(command->name, "kline") == 0) {
            argv[argc++] = "--period";
            argv[argc++] = "1m";
        }
        for (size_t j = 0; j < sizeof(dates) / sizeof(dates[0]); ++j) {
            tdx_error error = {{0}};
            char canonical[9];
            uint32_t value = 0;
            void *options;
            argv[1] = (char *)dates[j];
            CHECK(cli_resolve_date(dates[j], canonical, &value, &error) == TDX_OK);
            CHECK(strcmp(canonical, "20240229") == 0 && value == 20240229);
            options = parse(command->name, argc, argv, TDX_OK);
            /* A deliberately empty pool stops at the fetch boundary, without
             * any socket use. The handler must accept both date spellings;
             * day additionally exposes the canonical resource path here. */
            ((cli_common_options *)options)->pool.count = 0;
            CHECK(command->run(options, &error) == TDX_ERR);
            CHECK(strstr(error.message, cases[i].fetch_error) != NULL);
            if (j == 0) baseline = error;
            else CHECK(strcmp(error.message, baseline.message) == 0);
            free(options);
        }
        for (size_t j = 0; j < sizeof(invalid) / sizeof(invalid[0]); ++j) {
            argv[1] = (char *)invalid[j];
            free(parse(command->name, argc, argv, TDX_ERR));
        }
        {
            tdx_error error = {{0}};
            void *options = parse(command->name, argc - 2, argv + 2, TDX_OK);
            ((cli_common_options *)options)->pool.count = 0;
            CHECK(command->run(options, &error) == TDX_ERR);
            CHECK(strstr(error.message, strcmp(command->name, "day") == 0
                ? "day needs --date" : cases[i].fetch_error) != NULL);
            free(options);
        }
    }
}

static void test_poll_intervals(void) {
    char *argv[] = {"--interval-ms", "0"};
    free(parse("serve", 2, argv, TDX_ERR));
    cli_watch_options *watch = parse("watch", 2, argv, TDX_OK);
    CHECK(watch->interval_ms == 0);
    free(watch);
    argv[1] = "1";
    cli_serve_options *serve = parse("serve", 2, argv, TDX_OK);
    CHECK(serve->interval_ms == 1);
    free(serve);
}

int main(void) {
    CHECK(cli_find_command("unknown") == NULL);
    test_explicit_dates();
    test_poll_intervals();
    {
        char *argv[] = {"--security", "sz000001", "sh600000", "--host", "127.0.0.1:7709", "-j", "2", "--batch-size", "100"};
        cli_sweep_options *o = parse("sweep", 9, argv, TDX_OK);
        CHECK(o->common.security_count == 2 && o->connections == 2 && o->batch_size == 100);
        CHECK(o->common.pool.count == 1 && o->common.pool.items[0].port == 7709);
        free(o);
    }
    {
        char *argv[] = {"--port", "8790junk"};
        free(parse("serve", 2, argv, TDX_ERR));
        argv[1] = "999999999999999999999999999999999999";
        free(parse("serve", 2, argv, TDX_ERR));
        argv[1] = "-1";
        free(parse("serve", 2, argv, TDX_ERR));
    }
    {
        char *argv[] = {"--field", "nonsense"};
        free(parse("professional", 2, argv, TDX_ERR));
        argv[0] = "--from"; argv[1] = "20260230";
        free(parse("professional", 2, argv, TDX_ERR));
        argv[1] = "20240229";
        cli_professional_options *o = parse("professional", 2, argv, TDX_OK);
        CHECK(o->from_date == 20240229);
        free(o);
    }
    {
        char *argv[] = {"--prev", "nan"};
        free(parse("limit", 2, argv, TDX_ERR));
        argv[1] = "12.5garbage";
        free(parse("limit", 2, argv, TDX_ERR));
        argv[1] = "12.5";
        cli_limit_options *o = parse("limit", 2, argv, TDX_OK);
        CHECK(o->previous_close == 12.5 && o->has_previous_close);
        free(o);
    }
    {
        char *argv[] = {"--port", "8790"};
        free(parse("panorama", 2, argv, TDX_ERR));
        char *zip[] = {"--zip", "test.zip", "--member", "report.dat"};
        cli_professional_options *o = parse("professional", 4, zip, TDX_OK);
        CHECK(strcmp(o->zip_member, "report.dat") == 0 && o->kind == NULL);
        free(o);
    }
    {
        char *argv[] = {"--market", "sz,sz"};
        free(parse("sweep", 2, argv, TDX_ERR));
        argv[1] = "sz,";
        free(parse("sweep", 2, argv, TDX_ERR));
        argv[1] = "sh,bj";
        cli_sweep_options *o = parse("sweep", 2, argv, TDX_OK);
        CHECK(o->common.market_count == 2 && o->common.markets[0] == 1 && o->common.markets[1] == 2);
        free(o);
    }
    return failures ? 1 : 0;
}
