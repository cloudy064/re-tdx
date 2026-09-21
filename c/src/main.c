/* main.c - command line front end for the C L1 streamer.
 *
 * Commands:
 *   probe       one 0x0547 batch, decoded records as JSON
 *   securities  enumerate the server security directory
 *   sweep       poll a whole universe through a parallel connection pool
 *   watch       repeated sweeps with change detection, JSONL events on stdout
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tdx_directory.h"
#include "tdx_format.h"
#include "tdx_hub.h"
#include "tdx_serve.h"
#include "tdx_l1.h"
#include "tdx_pool.h"
#include "tdx_state.h"
#include "tdx_thread.h"

#define TDX_CLI_SECURITY_MAX 8192

static void usage(void) {
    printf("tdx-l1stream %s - standalone C client for TongDaXin L1 quotes\n\n",
           TDX_L1_VERSION);
    printf("Usage:\n");
    printf("  tdx-l1stream probe --security [MARKET:]CODE [options]\n");
    printf("  tdx-l1stream securities [--market sz,sh,bj] [options]\n");
    printf("  tdx-l1stream sweep [--market sz,sh,bj] [-j N] [options]\n");
    printf("  tdx-l1stream watch [--market sz,sh,bj] [-j N] [options]\n\n");
    printf("Universe:\n");
    printf("  --security CODE      repeatable, e.g. sz000001 or 600000\n");
    printf("  --market LIST        comma separated sz,sh,bj (default sz,sh,bj)\n");
    printf("  --category NAME      all|a_share|etf|index|... (default a_share)\n");
    printf("  --limit N            cap the universe size\n\n");
    printf("Transport:\n");
    printf("  --host HOST[:PORT]   repeatable; default connect.cfg HQHOST pool\n");
    printf("  --root PATH          TDX installation root, used for connect.cfg\n");
    printf("  --timeout-ms N       socket timeout, default 10000\n");
    printf("  --endpoints N        connect.cfg nodes to keep, default 3\n");
    printf("  -j, --connections N  parallel sessions, default 6\n");
    printf("  --batch-size N       securities per request, default 100\n\n");
    printf("serve:\n");
    printf("  --port N             listen port on 127.0.0.1, default 8790\n");
    printf("  --max-subscribers N  concurrent SSE readers, default 16\n");
    printf("  --heartbeat-ms N     idle event for a subscriber, default 15000\n\n");
    printf("  --idle-interval-ms N after N quiet rounds, slow down to this\n");
    printf("                       cadence; 0 disables, default 0\n");
    printf("  --idle-rounds N      quiet rounds before slowing, default 30\n\n");
    printf("watch:\n");
    printf("  --iterations N       rounds to run, 0 means forever (default 1)\n");
    printf("  --interval-ms N      delay between rounds, 0 for back-to-back,\n");
    printf("                       default 1000\n\n");
    printf("Output:\n");
    printf("  --output PATH        write the payload to a file instead of stdout\n");
    printf("  --help               show this message\n");
}

typedef struct cli_options {
    tdx_code securities[TDX_CLI_SECURITY_MAX];
    size_t security_count;
    tdx_endpoint_pool pool;
    int timeout_ms;
    size_t endpoint_limit;
    size_t connections;
    size_t batch_size;
    size_t iterations;
    int interval_ms;
    size_t max_subscribers;
    int heartbeat_ms;
    int idle_interval_ms;
    int idle_rounds;
    int port;
    size_t limit;
    int markets[3];
    size_t market_count;
    char category[TDX_DIRECTORY_CATEGORY_MAX];
    const char *output;
} cli_options;

static void options_init(cli_options *options) {
    memset(options, 0, sizeof(*options));
    options->timeout_ms = 10000;
    options->endpoint_limit = 3;
    options->connections = 6;
    options->batch_size = TDX_DEPTH_BATCH_MAX;
    options->iterations = 1;
    options->interval_ms = 1000;
    options->max_subscribers = 16;
    options->heartbeat_ms = TDX_HUB_DEFAULT_HEARTBEAT_MS;
    options->idle_interval_ms = 0;
    options->idle_rounds = 30;
    options->port = 8790;
    options->markets[0] = 0;
    options->markets[1] = 1;
    options->markets[2] = 2;
    options->market_count = 3;
    snprintf(options->category, sizeof(options->category), "a_share");
}

static int parse_markets(const char *text, cli_options *options, tdx_error *err) {
    char working[64];
    char *cursor;
    size_t count = 0;

    if (strlen(text) >= sizeof(working)) {
        tdx_error_set(err, "--market text is too long");
        return TDX_ERR;
    }
    snprintf(working, sizeof(working), "%s", text);
    cursor = working;
    for (;;) {
        char *comma = strchr(cursor, ',');
        char *piece;
        char *end;
        if (comma)
            *comma = '\0';
        piece = cursor;
        while (*piece == ' ' || *piece == '\t')
            piece++;
        end = piece + strlen(piece);
        while (end > piece && (end[-1] == ' ' || end[-1] == '\t'))
            *--end = '\0';
        if (*piece) {
            if (count >= 3) {
                tdx_error_set(err, "--market accepts at most three markets");
                return TDX_ERR;
            }
            if (strcmp(piece, "sz") == 0)
                options->markets[count] = 0;
            else if (strcmp(piece, "sh") == 0)
                options->markets[count] = 1;
            else if (strcmp(piece, "bj") == 0)
                options->markets[count] = 2;
            else {
                tdx_error_set(err, "unknown market: %s", piece);
                return TDX_ERR;
            }
            count++;
        }
        if (!comma)
            break;
        cursor = comma + 1;
    }
    if (count == 0) {
        tdx_error_set(err, "--market needs at least one market");
        return TDX_ERR;
    }
    options->market_count = count;
    return TDX_OK;
}

static int parse_options(int argc, char **argv, cli_options *options, tdx_error *err) {
    const char *root = NULL;
    tdx_endpoint explicit_hosts[8];
    size_t explicit_count = 0;
    int index;

    options_init(options);
    for (index = 1; index < argc; ++index) {
        const char *argument = argv[index];
        const char *value;
        if (argument[0] != '-')
            continue; /* the command word; validated by main */
        if (strcmp(argument, "--help") == 0 || strcmp(argument, "-h") == 0) {
            usage();
            exit(0);
        }
        if (index + 1 >= argc) {
            tdx_error_set(err, "%s needs a value", argument);
            return TDX_ERR;
        }
        value = argv[index + 1];

        if (strcmp(argument, "--security") == 0) {
            if (options->security_count >= TDX_CLI_SECURITY_MAX) {
                tdx_error_set(err, "--security accepts at most %d entries",
                              TDX_CLI_SECURITY_MAX);
                return TDX_ERR;
            }
            if (tdx_code_parse(value, &options->securities[options->security_count],
                               err) != TDX_OK)
                return TDX_ERR;
            options->security_count++;
        } else if (strcmp(argument, "--host") == 0) {
            if (explicit_count >= sizeof(explicit_hosts) / sizeof(explicit_hosts[0])) {
                tdx_error_set(err, "--host accepts at most %zu entries",
                              sizeof(explicit_hosts) / sizeof(explicit_hosts[0]));
                return TDX_ERR;
            }
            if (tdx_endpoint_parse(value, &explicit_hosts[explicit_count], err) != TDX_OK)
                return TDX_ERR;
            explicit_count++;
        } else if (strcmp(argument, "--root") == 0) {
            root = value;
        } else if (strcmp(argument, "--output") == 0) {
            options->output = value;
        } else if (strcmp(argument, "--category") == 0) {
            snprintf(options->category, sizeof(options->category), "%s", value);
        } else if (strcmp(argument, "--market") == 0) {
            if (parse_markets(value, options, err) != TDX_OK)
                return TDX_ERR;
        } else if (strcmp(argument, "--timeout-ms") == 0) {
            options->timeout_ms = atoi(value);
            if (options->timeout_ms < 100 || options->timeout_ms > 600000) {
                tdx_error_set(err, "--timeout-ms must be in 100..600000");
                return TDX_ERR;
            }
        } else if (strcmp(argument, "--endpoints") == 0) {
            int parsed = atoi(value);
            if (parsed < 1 || parsed > TDX_ENDPOINT_POOL_MAX) {
                tdx_error_set(err, "--endpoints must be in 1..%d",
                              TDX_ENDPOINT_POOL_MAX);
                return TDX_ERR;
            }
            options->endpoint_limit = (size_t)parsed;
        } else if (strcmp(argument, "-j") == 0 ||
                   strcmp(argument, "--connections") == 0) {
            int parsed = atoi(value);
            if (parsed < 1 || parsed > TDX_POOL_MAX_WORKERS) {
                tdx_error_set(err, "--connections must be in 1..%d",
                              TDX_POOL_MAX_WORKERS);
                return TDX_ERR;
            }
            options->connections = (size_t)parsed;
        } else if (strcmp(argument, "--batch-size") == 0) {
            int parsed = atoi(value);
            if (parsed < 1 || parsed > TDX_DEPTH_BATCH_MAX) {
                tdx_error_set(err, "--batch-size must be in 1..%d",
                              TDX_DEPTH_BATCH_MAX);
                return TDX_ERR;
            }
            options->batch_size = (size_t)parsed;
        } else if (strcmp(argument, "--iterations") == 0) {
            int parsed = atoi(value);
            if (parsed < 0 || parsed > 1000000) {
                tdx_error_set(err, "--iterations must be in 0..1000000");
                return TDX_ERR;
            }
            options->iterations = (size_t)parsed;
        } else if (strcmp(argument, "--interval-ms") == 0) {
            int parsed = atoi(value);
            if (parsed < 0 || parsed > 600000) {
                tdx_error_set(err, "--interval-ms must be in 0..600000");
                return TDX_ERR;
            }
            options->interval_ms = parsed;
        } else if (strcmp(argument, "--port") == 0) {
            int parsed = atoi(value);
            if (parsed < 1 || parsed > 65535) {
                tdx_error_set(err, "--port must be in 1..65535");
                return TDX_ERR;
            }
            options->port = parsed;
        } else if (strcmp(argument, "--max-subscribers") == 0) {
            int parsed = atoi(value);
            if (parsed < 1 || parsed > TDX_HUB_MAX_SUBSCRIBERS) {
                tdx_error_set(err, "--max-subscribers must be in 1..%d",
                              TDX_HUB_MAX_SUBSCRIBERS);
                return TDX_ERR;
            }
            options->max_subscribers = (size_t)parsed;
        } else if (strcmp(argument, "--idle-interval-ms") == 0) {
            int parsed = atoi(value);
            if (parsed < 0 || parsed > 600000) {
                tdx_error_set(err, "--idle-interval-ms must be in 0..600000");
                return TDX_ERR;
            }
            options->idle_interval_ms = parsed;
        } else if (strcmp(argument, "--idle-rounds") == 0) {
            int parsed = atoi(value);
            if (parsed < 0 || parsed > 1000000) {
                tdx_error_set(err, "--idle-rounds must be in 0..1000000");
                return TDX_ERR;
            }
            options->idle_rounds = parsed;
        } else if (strcmp(argument, "--heartbeat-ms") == 0) {
            int parsed = atoi(value);
            if (parsed < 0 || parsed > 600000) {
                tdx_error_set(err, "--heartbeat-ms must be in 0..600000");
                return TDX_ERR;
            }
            options->heartbeat_ms = parsed;
        } else if (strcmp(argument, "--limit") == 0) {
            int parsed = atoi(value);
            if (parsed < 1 || parsed > 100000) {
                tdx_error_set(err, "--limit must be in 1..100000");
                return TDX_ERR;
            }
            options->limit = (size_t)parsed;
        } else {
            tdx_error_set(err, "unknown option: %s", argument);
            return TDX_ERR;
        }
        index++;
    }

    if (explicit_count > 0) {
        size_t copy = explicit_count < options->endpoint_limit ? explicit_count
                                                              : options->endpoint_limit;
        memcpy(options->pool.items, explicit_hosts, copy * sizeof(explicit_hosts[0]));
        options->pool.count = copy;
        snprintf(options->pool.source, sizeof(options->pool.source), "explicit-host");
        options->pool.configured_count = explicit_count;
    } else if (tdx_endpoint_pool_load(root, options->endpoint_limit, &options->pool,
                                      err) != TDX_OK) {
        return TDX_ERR;
    }
    return TDX_OK;
}

/* ------------------------------------------------------------------ */
/* output helpers                                                      */
/* ------------------------------------------------------------------ */

static FILE *open_output(const cli_options *options) {
    if (!options->output)
        return stdout;
    return fopen(options->output, "wb");
}

/* Escapes the bytes JSON cannot carry literally.  Multi-byte UTF-8 passes
 * through untouched, which is what readers expect. */
static void json_string(FILE *stream, const char *text) {
    const unsigned char *cursor = (const unsigned char *)(text ? text : "");
    fputc('"', stream);
    for (; *cursor; ++cursor) {
        switch (*cursor) {
        case '"':
            fputs("\\\"", stream);
            break;
        case '\\':
            fputs("\\\\", stream);
            break;
        case '\n':
            fputs("\\n", stream);
            break;
        case '\r':
            fputs("\\r", stream);
            break;
        case '\t':
            fputs("\\t", stream);
            break;
        default:
            if (*cursor < 0x20)
                fprintf(stream, "\\u%04x", (unsigned)*cursor);
            else
                fputc((int)*cursor, stream);
            break;
        }
    }
    fputc('"', stream);
}

/* Delegates to the shared formatter so the CLI and the hub cannot drift. */
static void depth_json(FILE *stream, const tdx_depth *depth) {
    tdx_buf buffer;
    tdx_error ignored;
    ignored.message[0] = '\0';
    tdx_buf_init(&buffer);
    if (tdx_format_depth(&buffer, depth, &ignored) == TDX_OK && buffer.len > 0)
        (void)fwrite(buffer.data, 1, buffer.len, stream);
    tdx_buf_free(&buffer);
}

static void security_json(FILE *stream, const tdx_security *security) {
    static const char *upper[3] = {"SZ", "SH", "BJ"};
    static const char *lower[3] = {"sz", "sh", "bj"};
    int id = (security->market_id >= 0 && security->market_id <= 2)
                 ? security->market_id
                 : 0;
    char code[8];
    snprintf(code, sizeof(code), "%s", security->code);
    fprintf(stream, "{\"security_id\":\"%s%s\",\"market\":\"%s\",", upper[id], code,
            lower[id]);
    fputs("\"code\":", stream);
    json_string(stream, code);
    fputs(",\"name\":", stream);
    json_string(stream, security->name);
    fputs(",\"category\":", stream);
    json_string(stream, security->category);
    fputs(",\"board\":", stream);
    json_string(stream, security->board);
    fprintf(stream, ",\"multiple\":%u,\"decimal\":%u,\"previous_close_price\":%.6f}",
            (unsigned)security->multiple, (unsigned)security->decimal,
            security->previous_close_price);
}

static void diff_names_json(FILE *stream, tdx_diff_mask mask) {
    const char *names[8];
    const size_t count = tdx_diff_names(mask, names, 8);
    size_t index;
    fputc('[', stream);
    for (index = 0; index < count; ++index) {
        if (index)
            fputc(',', stream);
        json_string(stream, names[index]);
    }
    fputc(']', stream);
}

/* ------------------------------------------------------------------ */
/* probe                                                               */
/* ------------------------------------------------------------------ */

static int command_probe(const cli_options *options, tdx_error *err) {
    tdx_connection connection;
    tdx_buf request;
    tdx_buf response;
    tdx_depth *depths;
    size_t depth_count = 0;
    size_t attempt;
    FILE *stream;
    int result = TDX_ERR;

    if (options->security_count == 0) {
        tdx_error_set(err, "probe needs at least one --security");
        return TDX_ERR;
    }
    depths = (tdx_depth *)calloc(options->security_count, sizeof(*depths));
    if (!depths) {
        tdx_error_set(err, "out of memory for %zu records", options->security_count);
        return TDX_ERR;
    }
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;
    tdx_buf_init(&request);
    tdx_buf_init(&response);
    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s", options->output);
        goto done;
    }
    if (tdx_quote_build_depth_request(options->securities, options->security_count,
                                      &request, err) != TDX_OK)
        goto done;

    for (attempt = 0; attempt < options->pool.count; ++attempt) {
        char address[80];
        size_t index;
        tdx_endpoint_address(&options->pool.items[attempt], address, sizeof(address));
        if (tdx_connection_open(&connection, &options->pool.items[attempt],
                                options->timeout_ms, err) != TDX_OK) {
            fprintf(stderr, "warning: %s: %s\n", address, err->message);
            continue;
        }
        fprintf(stderr, "endpoint=%s server_name=%s requested=%zu\n", address,
                connection.server_name[0] ? connection.server_name : "(unknown)",
                options->security_count);
        if (tdx_connection_call(&connection, TDX_CMD_DEPTH, request.data, request.len,
                                &response, err) != TDX_OK) {
            fprintf(stderr, "warning: %s: %s\n", address, err->message);
            tdx_connection_close(&connection);
            continue;
        }
        tdx_connection_close(&connection);
        if (tdx_quote_parse_depth_response(response.data, response.len,
                                           options->securities, options->security_count,
                                           depths, options->security_count, &depth_count,
                                           err) != TDX_OK)
            goto done;
        for (index = 0; index < depth_count; ++index) {
            depth_json(stream, &depths[index]);
            fputc('\n', stream);
        }
        fprintf(stderr, "received %zu/%zu depth records\n", depth_count,
                options->security_count);
        result = TDX_OK;
        goto done;
    }
    tdx_error_set(err, "every candidate endpoint failed");

done:
    tdx_buf_free(&request);
    tdx_buf_free(&response);
    free(depths);
    if (stream && stream != stdout)
        fclose(stream);
    return result;
}

/* ------------------------------------------------------------------ */
/* securities                                                          */
/* ------------------------------------------------------------------ */

static int command_securities(const cli_options *options, tdx_error *err) {
    tdx_security_list list;
    tdx_connection connection;
    size_t market_index;
    size_t index;
    FILE *stream;
    size_t emitted = 0;
    int result = TDX_ERR;

    tdx_security_list_init(&list);
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;
    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s", options->output);
        return TDX_ERR;
    }
    if (tdx_connection_open(&connection, &options->pool.items[0], options->timeout_ms,
                            err) != TDX_OK)
        goto done;
    fprintf(stderr, "endpoint=%s server_name=%s\n", options->pool.items[0].host,
            connection.server_name[0] ? connection.server_name : "(unknown)");
    for (market_index = 0; market_index < options->market_count; ++market_index) {
        if (tdx_directory_fetch(&connection, options->markets[market_index],
                                TDX_DIRECTORY_PAGE_MAX, &list, err) != TDX_OK)
            goto done;
    }
    for (index = 0; index < list.count; ++index) {
        if (!tdx_security_matches_category(&list.items[index], options->category))
            continue;
        security_json(stream, &list.items[index]);
        fputc('\n', stream);
        emitted++;
        if (options->limit && emitted >= options->limit)
            break;
    }
    fprintf(stderr, "directory=%zu emitted=%zu category=%s\n", list.count, emitted,
            options->category);
    result = TDX_OK;

done:
    tdx_connection_close(&connection);
    tdx_security_list_free(&list);
    if (stream && stream != stdout)
        fclose(stream);
    return result;
}

/* ------------------------------------------------------------------ */
/* universe                                                            */
/* ------------------------------------------------------------------ */

static int build_universe(const cli_options *options, tdx_code **out,
                          size_t *out_count, tdx_error *err) {
    tdx_code *codes;
    tdx_security_list list;
    tdx_connection connection;
    size_t market_index;
    size_t index;
    size_t count = 0;

    if (options->security_count > 0) {
        codes = (tdx_code *)calloc(options->security_count, sizeof(*codes));
        if (!codes) {
            tdx_error_set(err, "out of memory for %zu securities",
                          options->security_count);
            return TDX_ERR;
        }
        memcpy(codes, options->securities, options->security_count * sizeof(*codes));
        *out = codes;
        *out_count = options->security_count;
        return TDX_OK;
    }

    tdx_security_list_init(&list);
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;
    if (tdx_connection_open(&connection, &options->pool.items[0], options->timeout_ms,
                            err) != TDX_OK) {
        tdx_security_list_free(&list);
        return TDX_ERR;
    }
    fprintf(stderr, "universe endpoint=%s server_name=%s\n",
            options->pool.items[0].host,
            connection.server_name[0] ? connection.server_name : "(unknown)");
    for (market_index = 0; market_index < options->market_count; ++market_index) {
        if (tdx_directory_fetch(&connection, options->markets[market_index],
                                TDX_DIRECTORY_PAGE_MAX, &list, err) != TDX_OK) {
            tdx_connection_close(&connection);
            tdx_security_list_free(&list);
            return TDX_ERR;
        }
    }
    tdx_connection_close(&connection);

    codes = (tdx_code *)calloc(list.count ? list.count : 1, sizeof(*codes));
    if (!codes) {
        tdx_security_list_free(&list);
        tdx_error_set(err, "out of memory for %zu securities", list.count);
        return TDX_ERR;
    }
    for (index = 0; index < list.count; ++index) {
        if (!tdx_security_matches_category(&list.items[index], options->category))
            continue;
        codes[count].market_id = list.items[index].market_id;
        memcpy(codes[count].code, list.items[index].code, sizeof(codes[count].code));
        count++;
        if (options->limit && count >= options->limit)
            break;
    }
    fprintf(stderr, "universe directory=%zu selected=%zu category=%s markets=%zu\n",
            list.count, count, options->category, options->market_count);
    tdx_security_list_free(&list);
    if (count == 0) {
        free(codes);
        tdx_error_set(err, "the universe is empty for category %s", options->category);
        return TDX_ERR;
    }
    *out = codes;
    *out_count = count;
    return TDX_OK;
}

/* ------------------------------------------------------------------ */
/* sweep                                                               */
/* ------------------------------------------------------------------ */

static int command_sweep(const cli_options *options, tdx_error *err) {
    tdx_code *codes = NULL;
    size_t count = 0;
    tdx_depth *records = NULL;
    tdx_sweep_options sweep;
    FILE *stream;
    size_t round;
    int result = TDX_ERR;

    if (build_universe(options, &codes, &count, err) != TDX_OK)
        return TDX_ERR;
    records = (tdx_depth *)calloc(count, sizeof(*records));
    if (!records) {
        tdx_error_set(err, "out of memory for %zu records", count);
        free(codes);
        return TDX_ERR;
    }
    sweep.connections = options->connections;
    sweep.batch_size = options->batch_size;
    sweep.timeout_ms = options->timeout_ms;
    sweep.max_attempts = 3;

    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s", options->output);
        goto done;
    }
    if (stream != stdout)
        fputs("[\n", stream);

    for (round = 0; round < options->iterations; ++round) {
        tdx_sweep_stats stats;
        size_t index;
        if (tdx_sweep(codes, count, &options->pool, &sweep, records, count, &stats,
                      err) != TDX_OK) {
            fprintf(stderr,
                    "round %zu failed: %s (records %zu/%zu, failed batches %zu, "
                    "%lld ms)\n",
                    round + 1, err->message, stats.records, stats.securities,
                    stats.failed_batches, (long long)stats.elapsed_ms);
            goto done;
        }
        fprintf(stderr,
                "round %zu: %zu/%zu records, %zu batches, %zu sessions, %zu requests, "
                "%zu retries, %lld ms, %.0f records/s\n",
                round + 1, stats.records, stats.securities, stats.batches,
                stats.connections_opened, stats.upstream_requests, stats.retries,
                (long long)stats.elapsed_ms,
                stats.elapsed_ms > 0
                    ? (double)stats.records * 1000.0 / (double)stats.elapsed_ms
                    : 0.0);
        if (round + 1 != options->iterations)
            continue;
        for (index = 0; index < count; ++index) {
            if (stream == stdout) {
                depth_json(stream, &records[index]);
                fputc('\n', stream);
            } else {
                fputs("  ", stream);
                depth_json(stream, &records[index]);
                fputs(index + 1 < count ? ",\n" : "\n", stream);
            }
        }
    }
    if (stream != stdout) {
        fputs("]\n", stream);
        fprintf(stderr, "wrote %zu records to %s\n", count, options->output);
    }
    result = TDX_OK;

done:
    if (stream && stream != stdout)
        fclose(stream);
    free(records);
    free(codes);
    return result;
}

/* ------------------------------------------------------------------ */
/* watch                                                               */
/* ------------------------------------------------------------------ */

static int command_watch(const cli_options *options, tdx_error *err) {
    tdx_code *codes = NULL;
    size_t count = 0;
    tdx_depth *records = NULL;
    tdx_state state;
    tdx_pool *pool = NULL;
    tdx_sweep_options sweep;
    FILE *stream;
    size_t round = 0;
    int result = TDX_ERR;
    int state_ready = 0;
    uint64_t total_events = 0;

    if (build_universe(options, &codes, &count, err) != TDX_OK)
        return TDX_ERR;
    records = (tdx_depth *)calloc(count, sizeof(*records));
    if (!records) {
        tdx_error_set(err, "out of memory for %zu records", count);
        free(codes);
        return TDX_ERR;
    }
    if (tdx_state_init(&state, count, err) != TDX_OK)
        goto done;
    state_ready = 1;
    sweep.connections = options->connections;
    sweep.batch_size = options->batch_size;
    sweep.timeout_ms = options->timeout_ms;
    sweep.max_attempts = 3;
    if (tdx_pool_create(&pool, &options->pool, &sweep, err) != TDX_OK)
        goto done;

    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s", options->output);
        goto done;
    }

    for (round = 0;; ++round) {
        tdx_sweep_stats stats;
        size_t index;
        size_t changed = 0;

        if (tdx_pool_run(pool, codes, count, records, count, &stats,
                      err) != TDX_OK) {
            fprintf(stderr, "round %zu failed: %s (%zu/%zu records, %lld ms)\n",
                    round + 1, err->message, stats.records, stats.securities,
                    (long long)stats.elapsed_ms);
            goto done;
        }
        for (index = 0; index < count; ++index) {
            tdx_diff_mask mask = 0;
            const tdx_state_entry *entry = NULL;
            char id[16];
            if (tdx_state_apply(&state, &records[index], &mask, &entry, err) != TDX_OK)
                goto done;
            if (mask == 0)
                continue;
            changed++;
            total_events++;
            tdx_code_id(&records[index].security, id, sizeof(id));
            fputs("{\"type\":", stream);
            json_string(stream, (mask & TDX_DIFF_NEW) ? "snapshot" : "change");
            fprintf(stream, ",\"round\":%zu,\"security_id\":\"%s\",\"changed\":",
                    round + 1, id);
            diff_names_json(stream, mask);
            fprintf(stream, ",\"updates\":%llu,\"record\":",
                    (unsigned long long)(entry ? entry->updates : 0));
            depth_json(stream, &records[index]);
            fputs("}\n", stream);
        }
        if (changed == 0) {
            fputs("{\"type\":\"heartbeat\",\"round\":", stream);
            fprintf(stream, "%zu,\"subscribed\":%zu,\"total_events\":%llu}\n",
                    round + 1, count, (unsigned long long)total_events);
        }
        fflush(stream);
        fprintf(stderr,
                "round %zu: %zu/%zu records, %zu changed (%.0f%%), %zu batches, "
                "%lld ms, %zu sessions\n",
                round + 1, stats.records, stats.securities, changed,
                count ? (double)changed * 100.0 / (double)count : 0.0, stats.batches,
                (long long)stats.elapsed_ms, stats.connections_opened);

        if (options->iterations != 0 && round + 1 >= options->iterations)
            break;
        tdx_sleep_ms(options->interval_ms);
    }
    result = TDX_OK;

done:
    if (pool)
        tdx_pool_destroy(pool);
    if (state_ready)
        tdx_state_free(&state);
    if (stream && stream != stdout)
        fclose(stream);
    free(records);
    free(codes);
    return result;
}

/* ------------------------------------------------------------------ */
/* serve                                                               */
/* ------------------------------------------------------------------ */

typedef struct serve_fetch_context {
    const tdx_code *codes;
    size_t code_count;
    tdx_pool *pool;
    tdx_sweep_stats last;
} serve_fetch_context;

/* Bridges the hub's fetch hook onto one parallel sweep round. */
static int serve_fetch(void *context, tdx_depth *out, size_t capacity, size_t *count,
                       tdx_error *err) {
    serve_fetch_context *state = (serve_fetch_context *)context;
    tdx_sweep_stats stats;
    memset(&stats, 0, sizeof(stats));
    if (tdx_pool_run(state->pool, state->codes, state->code_count, out,
                     capacity, &stats, err) != TDX_OK) {
        state->last = stats;
        return TDX_ERR;
    }
    state->last = stats;
    *count = stats.records;
    return TDX_OK;
}

static int command_serve(const cli_options *options, tdx_error *err) {
    tdx_code *codes = NULL;
    size_t count = 0;
    tdx_hub *hub = NULL;
    tdx_pool *pool = NULL;
    tdx_hub_options hub_options;
    tdx_serve_options serve_options;
    tdx_sweep_options sweep;
    serve_fetch_context fetch;
    int result = TDX_ERR;

    if (build_universe(options, &codes, &count, err) != TDX_OK)
        return TDX_ERR;
    sweep.connections = options->connections;
    sweep.batch_size = options->batch_size;
    sweep.timeout_ms = options->timeout_ms;
    sweep.max_attempts = 3;
    memset(&fetch, 0, sizeof(fetch));
    fetch.codes = codes;
    fetch.code_count = count;

    if (tdx_pool_create(&pool, &options->pool, &sweep, err) != TDX_OK)
        goto done;
    fetch.pool = pool;
    tdx_hub_options_default(&hub_options);
    hub_options.max_subscribers = options->max_subscribers;
    hub_options.subscriber_queue_limit = TDX_HUB_DEFAULT_QUEUE_LIMIT;
    hub_options.interval_ms = options->interval_ms;
    hub_options.heartbeat_ms = options->heartbeat_ms;
    hub_options.idle_interval_ms = options->idle_interval_ms;
    hub_options.idle_rounds = options->idle_rounds;
    if (tdx_hub_create(&hub, codes, count, &hub_options, serve_fetch, &fetch,
                       err) != TDX_OK)
        goto done;
    if (tdx_hub_start(hub, err) != TDX_OK)
        goto done;
    tdx_serve_options_default(&serve_options);
    serve_options.port = options->port;
    fprintf(stderr, "polling %zu securities every %d ms over %zu sessions\\n", count,
            options->interval_ms, options->connections);
    if (tdx_serve_run(hub, codes, count, &serve_options, err) != TDX_OK)
        goto done;
    result = TDX_OK;

done:
    if (pool)
        tdx_pool_destroy(pool);
    if (hub)
        tdx_hub_destroy(hub);
    free(codes);
    return result;
}

int main(int argc, char **argv) {
    tdx_error error;
    cli_options options;

    if (argc < 2) {
        usage();
        return 2;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0 ||
        strcmp(argv[1], "help") == 0) {
        usage();
        return 0;
    }
    error.message[0] = '\0';
    if (parse_options(argc - 1, argv + 1, &options, &error) != TDX_OK) {
        fprintf(stderr, "error: %s\n", error.message);
        return 2;
    }
    if (strcmp(argv[1], "probe") == 0) {
        if (command_probe(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "securities") == 0) {
        if (command_securities(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "sweep") == 0) {
        if (command_sweep(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "serve") == 0) {
        if (command_serve(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "watch") == 0) {
        if (command_watch(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    fprintf(stderr, "unknown command: %s\n\n", argv[1]);
    usage();
    return 2;
}
