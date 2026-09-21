/* main.c - command line front end for the C L1 streamer.
 *
 * Commands:
 *   probe       one 0x0547 batch, decoded records as JSON
 *   securities  enumerate the server security directory
 *   sweep       poll a whole universe through a parallel connection pool
 *   watch       repeated sweeps with change detection, JSONL events on stdout
 *   day         replay one historical session from a zst_cache .img, JSONL
 *   trades      L1 trade details (minute resolution) for today or one date
 *   kline       multi-period K-lines (0x052D)
 *   timeline    today's intraday time-share series (0x0537)
 *   auction     call-auction point series (0x056A)
 *   snapshot    batched whole-universe L1 snapshot (0x054C)
 *   finance     batched fundamental data (0x0010)
 *   capital     share-capital changes and ex-rights events (0x000F)
 *   limits      the special price-limit list (0x0452)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tdx_auction.h"
#include "tdx_auction_json.h"
#include "tdx_capital.h"
#include "tdx_capital_json.h"
#include "tdx_directory.h"
#include "tdx_finance.h"
#include "tdx_finance_json.h"
#include "tdx_gbbq.h"
#include "tdx_limits.h"
#include "tdx_limits_json.h"
#include "tdx_snapshot.h"
#include "tdx_snapshot_json.h"
#include "tdx_format.h"
#include "tdx_hub.h"
#include "tdx_kline.h"
#include "tdx_kline_json.h"
#include "tdx_serve.h"
#include "tdx_l1.h"
#include "tdx_pool.h"
#include "tdx_state.h"
#include "tdx_thread.h"
#include "tdx_timeline.h"
#include "tdx_timeline_json.h"
#include "tdx_trades.h"
#include "tdx_trades_json.h"
#include "tdx_zst_day.h"

#define TDX_CLI_SECURITY_MAX 8192
#define TDX_CLI_ROOT_MAX 512

static void usage(void) {
    printf("tdx-l1stream %s - standalone C client for TongDaXin L1 quotes\n\n",
           TDX_L1_VERSION);
    printf("Usage:\n");
    printf("  tdx-l1stream probe --security [MARKET:]CODE [options]\n");
    printf("  tdx-l1stream securities [--market sz,sh,bj] [options]\n");
    printf("  tdx-l1stream sweep [--market sz,sh,bj] [-j N] [options]\n");
    printf("  tdx-l1stream watch [--market sz,sh,bj] [-j N] [options]\n");
    printf("  tdx-l1stream day --security CODE --date YYYYMMDD [options]\n");
    printf("  tdx-l1stream trades --security CODE [--date YYYYMMDD] [options]\n");
    printf("  tdx-l1stream kline --security CODE --period PERIOD [options]\n");
    printf("  tdx-l1stream timeline --security CODE [options]\n");
    printf("  tdx-l1stream auction --security CODE [options]\n");
    printf("  tdx-l1stream snapshot --security CODE [--security CODE ...] [options]\n");
    printf("  tdx-l1stream finance --security CODE [--security CODE ...] [options]\n");
    printf("  tdx-l1stream capital --security CODE [--security CODE ...] [options]\n");
    printf("  tdx-l1stream limits [--start N] [options]\n\n");
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
    printf("day:\n");
    printf("  --date YYYYMMDD      session to replay, required\n");
    printf("  --cache-dir PATH     default <root>\\T0002\\zst_cache\n");
    printf("  --refresh            transfer again even when the cache holds it\n");
    printf("  --no-cache           never read or write the local cache\n");
    printf("  --raw                add the merged tag map to each line\n");
    printf("  --changed-only       drop records that changed nothing\n");
    printf("  --max-records N      cap how many snapshots are emitted\n");
    printf("  --quiet              suppress the cache and transfer notes\n\n");
    printf("trades:\n");
    printf("  --date YYYYMMDD      omit for today (0x0FC5), give it for history (0x0FC6)\n");
    printf("  --page-size N        records per request, default 1800 today / 2000 history\n");
    printf("  --max-pages N        paging safety limit, default %d\n\n", TDX_TRADES_MAX_PAGES);
    printf("kline:\n");
    printf("  --period NAME        time|1m|5m|15m|30m|60m|day|week|month, required\n");
    printf("  --start N            first record, counted back from the newest\n");
    printf("  --page-size N        records per request, default 800\n");
    printf("  --max-pages N        pages to walk, default %d\n", TDX_KLINE_PAGES_MAX);
    printf("  --index / --stock    force the index record shape; default auto\n");
    printf("  --date YYYYMMDD      keep only that day's bars (intraday periods)\n\n");
    printf("timeline:\n");
    printf("  --date YYYYMMDD      historical series; 0x0FB4's request shape is still\n");
    printf("                       open, so only the today command works today\n\n");
    printf("auction:\n");
    printf("  --selector N         0 = opening auction only (default), non-zero = both\n");
    printf("  --start N            first record\n");
    printf("  --limit N            records to ask for, 1..%u, default 200\n\n",
           (unsigned)TDX_AUCTION_LIMIT_MAX);
    printf("snapshot:\n");
    printf("  --batch-size N       securities per request; the server caps this command\n");
    printf("                       at %u, so a larger value is reduced to %u\n\n",
           (unsigned)TDX_SNAPSHOT_BATCH_MAX, (unsigned)TDX_SNAPSHOT_BATCH_MAX);
    printf("finance:\n");
    printf("  --batch-size N       securities per request, default 100, max %u\n\n",
           (unsigned)TDX_FINANCE_BATCH_MAX);
    printf("capital:\n");
    printf("  --local              read the local encrypted GBBQ file instead of asking\n");
    printf("                       0x000F; the two sources describe the same events\n");
    printf("  --gbbq PATH          the file to read; default <root>\\T0002\\hq_cache\\gbbq\n");
    printf("  --max-records N      per-security record cap, default %u; the 0x000F reply\n",
           (unsigned)TDX_CAPITAL_RECORDS_MAX);
    printf("                       header names one security, so the network source sends\n");
    printf("                       one at a time\n\n");
    printf("limits:\n");
    printf("  --start N            row to resume from, default 0\n");
    printf("  --max-records N      cap how many rows the walk takes, default %u\n\n",
           (unsigned)TDX_LIMITS_MAX_RECORDS);
    printf("serve:\n");
    printf("  --port N             listen port on 127.0.0.1, default 8790\n");
    printf("  --max-subscribers N  concurrent SSE readers, default 16\n");
    printf("  --heartbeat-ms N     idle event for a subscriber, default 15000\n\n");
    printf("  --tier-warm-ms N     warm tier cadence, 0 = auto (3x hot)\n");
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
    int tier_warm_ms;
    int idle_interval_ms;
    int idle_rounds;
    int port;
    size_t limit;
    int markets[3];
    size_t market_count;
    char category[TDX_DIRECTORY_CATEGORY_MAX];
    const char *output;
    const char *date;
    const char *cache_dir;
    char root[TDX_CLI_ROOT_MAX];
    size_t max_records;
    int refresh;
    int no_cache;
    int raw_tags;
    int changed_only;
    int quiet;
    int page_size;
    int max_pages;
    const char *period;
    int start;
    int index_mode; /* -1 stock, 0 auto, 1 index */
    unsigned selector;
    int capital_local;   /* read the local encrypted GBBQ file instead of 0x000F */
    const char *gbbq_path; /* NULL means <root>\T0002\hq_cache\gbbq */
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
    options->tier_warm_ms = 0;
    options->idle_interval_ms = 0;
    options->idle_rounds = 30;
    options->port = 8790;
    /* Zero means "the command's own default"; the trade feed and the K-line walk
     * have different ceilings, so neither may bake its own in here. */
    options->max_pages = 0;
    options->page_size = 0;
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
        /* Boolean flags carry no value, so they are settled before the key/value
         * pairing below; otherwise a trailing flag would look like a missing
         * operand. */
        if (strcmp(argument, "--refresh") == 0) {
            options->refresh = 1;
            continue;
        }
        if (strcmp(argument, "--no-cache") == 0) {
            options->no_cache = 1;
            continue;
        }
        if (strcmp(argument, "--raw") == 0) {
            options->raw_tags = 1;
            continue;
        }
        if (strcmp(argument, "--changed-only") == 0) {
            options->changed_only = 1;
            continue;
        }
        if (strcmp(argument, "--quiet") == 0) {
            options->quiet = 1;
            continue;
        }
        if (strcmp(argument, "--index") == 0) {
            options->index_mode = 1;
            continue;
        }
        if (strcmp(argument, "--stock") == 0) {
            options->index_mode = -1;
            continue;
        }
        if (strcmp(argument, "--local") == 0) {
            options->capital_local = 1;
            continue;
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
        } else if (strcmp(argument, "--tier-warm-ms") == 0) {
            int parsed = atoi(value);
            if (parsed < 0 || parsed > 600000) {
                tdx_error_set(err, "--tier-warm-ms must be in 0..600000");
                return TDX_ERR;
            }
            options->tier_warm_ms = parsed;
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
        } else if (strcmp(argument, "--date") == 0) {
            options->date = value;
        } else if (strcmp(argument, "--cache-dir") == 0) {
            options->cache_dir = value;
        } else if (strcmp(argument, "--max-records") == 0) {
            int parsed = atoi(value);
            if (parsed < 1 || parsed > 10000000) {
                tdx_error_set(err, "--max-records must be in 1..10000000");
                return TDX_ERR;
            }
            options->max_records = (size_t)parsed;
        } else if (strcmp(argument, "--page-size") == 0) {
            int parsed = atoi(value);
            if (parsed < 1 || parsed > 65535) {
                tdx_error_set(err, "--page-size must be in 1..65535");
                return TDX_ERR;
            }
            options->page_size = parsed;
        } else if (strcmp(argument, "--max-pages") == 0) {
            int parsed = atoi(value);
            if (parsed < 1 || parsed > TDX_TRADES_MAX_PAGES) {
                tdx_error_set(err, "--max-pages must be in 1..%d", TDX_TRADES_MAX_PAGES);
                return TDX_ERR;
            }
            options->max_pages = parsed;
        } else if (strcmp(argument, "--period") == 0) {
            options->period = value;
        } else if (strcmp(argument, "--start") == 0) {
            int parsed = atoi(value);
            if (parsed < 0 || parsed > TDX_KLINE_START_MAX) {
                tdx_error_set(err, "--start must be in 0..%d", TDX_KLINE_START_MAX);
                return TDX_ERR;
            }
            options->start = parsed;
        } else if (strcmp(argument, "--selector") == 0) {
            int parsed = atoi(value);
            if (parsed < 0 || parsed > 1000000) {
                tdx_error_set(err, "--selector must be in 0..1000000");
                return TDX_ERR;
            }
            options->selector = (unsigned)parsed;
        } else if (strcmp(argument, "--gbbq") == 0) {
            options->gbbq_path = value;
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
    if (root)
        snprintf(options->root, sizeof(options->root), "%s", root);
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
    tdx_code *batch; /* scratch, one entry per code */
    tdx_pool *pool;
    tdx_sweep_stats last;
} serve_fetch_context;

/* Bridges the hub's fetch hook onto one parallel sweep round. */
static int serve_fetch(void *context, const size_t *indices, size_t count,
                       tdx_depth *out, tdx_error *err) {
    serve_fetch_context *state = (serve_fetch_context *)context;
    tdx_sweep_stats stats;
    size_t index;

    if (count > state->code_count) {
        tdx_error_set(err, "the hub asked for %zu of %zu securities", count,
                      state->code_count);
        return TDX_ERR;
    }
    /* The pool works on securities, the hub schedules by universe index. */
    for (index = 0; index < count; ++index)
        state->batch[index] = state->codes[indices[index]];

    memset(&stats, 0, sizeof(stats));
    if (tdx_pool_run(state->pool, state->batch, count, out, count, &stats, err) !=
        TDX_OK) {
        state->last = stats;
        return TDX_ERR;
    }
    state->last = stats;
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
    fetch.batch = (tdx_code *)calloc(count, sizeof(*fetch.batch));
    if (!fetch.batch) {
        tdx_error_set(err, "out of memory for the poll scratch list");
        goto done;
    }

    if (tdx_pool_create(&pool, &options->pool, &sweep, err) != TDX_OK)
        goto done;
    fetch.pool = pool;
    tdx_hub_options_default(&hub_options);
    hub_options.max_subscribers = options->max_subscribers;
    hub_options.subscriber_queue_limit = TDX_HUB_DEFAULT_QUEUE_LIMIT;
    hub_options.interval_ms = options->interval_ms;
    hub_options.heartbeat_ms = options->heartbeat_ms;
    hub_options.tier_warm_ms = options->tier_warm_ms;
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
    free(fetch.batch);
    if (pool)
        tdx_pool_destroy(pool);
    if (hub)
        tdx_hub_destroy(hub);
    free(codes);
    return result;
}

/* ------------------------------------------------------------------ */
/* day                                                                 */
/* ------------------------------------------------------------------ */

/* Resolves where the .img cache lives.  A cache hit lets the command work
 * completely offline, which is the whole reason the client keeps one. */
static int day_cache_dir(const cli_options *options, char *buffer, size_t buffer_size,
                         const char **out, tdx_error *err) {
    if (options->no_cache) {
        *out = NULL;
        return TDX_OK;
    }
    if (options->cache_dir) {
        *out = options->cache_dir;
        return TDX_OK;
    }
    if (!options->root[0]) {
        tdx_error_set(err, "day needs --cache-dir or --root, or --no-cache to always transfer");
        return TDX_ERR;
    }
    if (snprintf(buffer, buffer_size, "%s/T0002/zst_cache", options->root) >=
        (int)buffer_size) {
        tdx_error_set(err, "--root is too long to derive a cache path from");
        return TDX_ERR;
    }
    *out = buffer;
    return TDX_OK;
}

static int command_day(const cli_options *options, tdx_error *err) {
    tdx_zst_day_options day;
    tdx_zst_day_result result;
    char cache_buffer[TDX_CLI_ROOT_MAX + 32];
    const char *cache_dir = NULL;
    FILE *stream;
    int status;

    if (options->security_count != 1) {
        tdx_error_set(err, "day needs exactly one --security");
        return TDX_ERR;
    }
    if (!options->date) {
        tdx_error_set(err, "day needs --date YYYYMMDD");
        return TDX_ERR;
    }
    if (day_cache_dir(options, cache_buffer, sizeof(cache_buffer), &cache_dir, err) != TDX_OK)
        return TDX_ERR;
    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->output ? options->output : "<stdout>");
        return TDX_ERR;
    }

    memset(&day, 0, sizeof(day));
    memset(&result, 0, sizeof(result));
    day.security = options->securities[0];
    snprintf(day.date, sizeof(day.date), "%s", options->date);
    day.cache_dir = cache_dir;
    day.refresh = options->refresh;
    day.raw_tags = options->raw_tags;
    day.changed_only = options->changed_only;
    day.limit = options->max_records;
    day.quiet = options->quiet;
    day.out = stream;

    status = tdx_zst_day_run(&options->pool, options->timeout_ms, &day, &result, err);
    if (options->output)
        fclose(stream);
    if (status != TDX_OK)
        return TDX_ERR;
    if (options->quiet)
        return TDX_OK;
    fprintf(stderr,
            "day %s %s: records=%zu emitted=%zu unchanged=%zu bytes=%zu source=%s "
            "endpoint=%s md5=%s\n",
            options->date, options->securities[0].code, result.records, result.emitted,
            result.silent, result.bytes, result.source,
            result.endpoint[0] ? result.endpoint : "-", result.md5[0] ? result.md5 : "-");
    if (result.dropped_tags)
        fprintf(stderr, "warning: %zu tags did not fit the replay map\n", result.dropped_tags);
    return TDX_OK;
}

/* ------------------------------------------------------------------ */
/* trades                                                              */
/* ------------------------------------------------------------------ */

static int command_trades(const cli_options *options, tdx_error *err) {
    tdx_trade_series series;
    tdx_trade_summary summary;
    tdx_buf line;
    FILE *stream;
    char endpoint[80];
    char server_date[TDX_TRADES_DATE_LENGTH + 1];
    char trading_date[TDX_TRADES_DATE_LENGTH + 1];
    const char *requested = options->date ? options->date : "";
    const char *code;
    int market_id;
    int page_size;
    int status = TDX_ERR;
    size_t index;

    if (options->security_count != 1) {
        tdx_error_set(err, "trades needs exactly one --security");
        return TDX_ERR;
    }
    if (*requested && tdx_trades_date_valid(requested, err) != TDX_OK)
        return TDX_ERR;
    market_id = options->securities[0].market_id;
    code = options->securities[0].code;
    page_size = options->page_size
                    ? options->page_size
                    : (*requested ? TDX_TRADES_PAGE_SIZE_HISTORY : TDX_TRADES_PAGE_SIZE_TODAY);

    tdx_buf_init(&line);
    tdx_trades_series_init(&series);
    memset(&summary, 0, sizeof(summary));
    memset(endpoint, 0, sizeof(endpoint));
    server_date[0] = '\0';

    if (tdx_trades_fetch_from_pool(&options->pool, options->timeout_ms, market_id, code,
                                   requested, (uint16_t)page_size,
                                   (size_t)(options->max_pages ? options->max_pages
                                                               : TDX_TRADES_MAX_PAGES),
                                   &series, endpoint, sizeof(endpoint), server_date,
                                   err) != TDX_OK)
        goto done;
    /* 0x0FC5 carries no date of its own; 0x0004 supplied the server's. */
    snprintf(trading_date, sizeof(trading_date), "%s",
             *requested ? requested : server_date);

    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->output ? options->output : "<stdout>");
        goto done;
    }
    for (index = 0; index < series.count; ++index) {
        tdx_buf_clear(&line);
        if (tdx_trades_format_tick(&line, &series.ticks[index], market_id, code, trading_date,
                                   err) != TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the trade stream");
            goto close_output;
        }
    }
    tdx_trades_summarize(&series, &summary);
    tdx_buf_clear(&line);
    if (tdx_trades_format_summary(&line, &series, &summary, market_id, code, trading_date,
                                  endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the trade summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    if (options->output)
        fclose(stream);
    if (status != TDX_OK)
        goto done;
    if (!options->quiet) {
        char first_label[8];
        char last_label[8];
        if (summary.has_times) {
            (void)tdx_trades_time_label(summary.first_time_minutes, first_label,
                                        sizeof(first_label));
            (void)tdx_trades_time_label(summary.last_time_minutes, last_label,
                                        sizeof(last_label));
        } else {
            snprintf(first_label, sizeof(first_label), "-");
            snprintf(last_label, sizeof(last_label), "-");
        }
        fprintf(stderr,
                "trades %s %s: command=%s pages=%zu ticks=%zu minutes=%zu volume_hand=%lld "
                "amount=%.2f vwap=%.4f %s..%s endpoint=%s date=%s\n",
                *requested ? requested : "today", code,
                series.history ? "0x0FC6" : "0x0FC5", series.pages, series.count,
                summary.minute_count, (long long)summary.volume_hand, summary.amount_yuan,
                summary.vwap, first_label, last_label, endpoint,
                trading_date[0] ? trading_date : "-");
    }

done:
    tdx_buf_free(&line);
    tdx_trades_series_free(&series);
    return status;
}

/* ------------------------------------------------------------------ */
/* kline                                                               */
/* ------------------------------------------------------------------ */

/* Keeps only the bars of one trading day.  The wire has no date filter, so the
 * window the caller paged through is what gets filtered. */
static size_t kline_select_date(tdx_kline_bar *bars, size_t count, int wanted) {
    size_t read_index;
    size_t write_index = 0;
    for (read_index = 0; read_index < count; ++read_index)
        if (bars[read_index].date == wanted)
            bars[write_index++] = bars[read_index];
    return write_index;
}

static int command_kline(const cli_options *options, tdx_error *err) {
    tdx_kline_series series;
    tdx_kline_period period;
    tdx_buf line;
    FILE *stream;
    char endpoint[80];
    char day_filter[TDX_TRADES_DATE_LENGTH + 1];
    const char *code;
    int market_id;
    int index_mode;
    int status = TDX_ERR;
    size_t index;
    size_t emitted = 0;

    if (options->security_count != 1) {
        tdx_error_set(err, "kline needs exactly one --security");
        return TDX_ERR;
    }
    if (!options->period) {
        tdx_error_set(err, "kline needs --period time, 1m, 5m, 15m, 30m, 60m, day, week or month");
        return TDX_ERR;
    }
    if (tdx_kline_period_parse(options->period, &period, err) != TDX_OK)
        return TDX_ERR;
    market_id = options->securities[0].market_id;
    code = options->securities[0].code;
    if (options->index_mode > 0)
        index_mode = 1;
    else if (options->index_mode < 0)
        index_mode = 0;
    else
        index_mode = market_id == 1 && tdx_kline_is_block_index_code(code);
    day_filter[0] = '\0';
    if (options->date) {
        if (tdx_trades_date_valid(options->date, err) != TDX_OK)
            return TDX_ERR;
        if (!period.intraday) {
            tdx_error_set(err, "--date only filters intraday periods; use --start for daily bars");
            return TDX_ERR;
        }
        snprintf(day_filter, sizeof(day_filter), "%s", options->date);
    }

    tdx_buf_init(&line);
    tdx_kline_series_init(&series);
    memset(endpoint, 0, sizeof(endpoint));
    if (tdx_kline_fetch_from_pool(&options->pool, options->timeout_ms, market_id, code, &period,
                                  index_mode, (uint16_t)options->start,
                                  (uint16_t)(options->page_size ? options->page_size
                                                                : TDX_KLINE_PAGE_SIZE_MAX),
                                  (size_t)(options->max_pages ? options->max_pages
                                                              : TDX_KLINE_PAGES_MAX),
                                  &series, endpoint, sizeof(endpoint), err) != TDX_OK)
        goto done;

    if (day_filter[0]) {
        int wanted = (day_filter[0] - '0') * 1000 + (day_filter[1] - '0') * 100 +
                     (day_filter[2] - '0') * 10 + (day_filter[3] - '0');
        wanted = wanted * 10000 + (day_filter[4] - '0') * 1000 + (day_filter[5] - '0') * 100 +
                 (day_filter[6] - '0') * 10 + (day_filter[7] - '0');
        series.count = kline_select_date(series.bars, series.count, wanted);
    }

    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->output ? options->output : "<stdout>");
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
    if (options->output)
        fclose(stream);
    if (status != TDX_OK)
        goto done;
    if (!options->quiet) {
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

/* ------------------------------------------------------------------ */
/* timeline                                                            */
/* ------------------------------------------------------------------ */

static int command_timeline(const cli_options *options, tdx_error *err) {
    tdx_timeline timeline;
    tdx_buf line;
    FILE *stream;
    char endpoint[80];
    const char *code;
    int market_id;
    int history;
    int status = TDX_ERR;
    size_t index;
    long volume = 0;

    if (options->security_count != 1) {
        tdx_error_set(err, "timeline needs exactly one --security");
        return TDX_ERR;
    }
    if (options->date && tdx_trades_date_valid(options->date, err) != TDX_OK)
        return TDX_ERR;
    market_id = options->securities[0].market_id;
    code = options->securities[0].code;
    history = options->date ? 1 : 0;

    tdx_buf_init(&line);
    tdx_timeline_init(&timeline);
    memset(endpoint, 0, sizeof(endpoint));
    if (tdx_timeline_fetch_from_pool(&options->pool, options->timeout_ms, market_id, code,
                                     history, options->date, &timeline, endpoint,
                                     sizeof(endpoint), err) != TDX_OK)
        goto done;

    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->output ? options->output : "<stdout>");
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
    if (options->output)
        fclose(stream);
    if (status != TDX_OK)
        goto done;
    if (!options->quiet)
        fprintf(stderr,
                "timeline %s %s: command=0x0537 points=%zu base=%.4f volume_hand=%ld "
                "endpoint=%s\n",
                code, options->date ? options->date : "today", timeline.count,
                timeline.base_price, volume, endpoint);

done:
    tdx_buf_free(&line);
    tdx_timeline_free(&timeline);
    return status;
}

/* ------------------------------------------------------------------ */
/* auction                                                             */
/* ------------------------------------------------------------------ */

static int command_auction(const cli_options *options, tdx_error *err) {
    tdx_auction_series series;
    tdx_auction_summary summary;
    tdx_buf line;
    FILE *stream;
    char endpoint[80];
    char server_date[TDX_TRADES_DATE_LENGTH + 1];
    const char *code;
    int market_id;
    unsigned limit;
    int status = TDX_ERR;
    size_t index;

    if (options->security_count != 1) {
        tdx_error_set(err, "auction needs exactly one --security");
        return TDX_ERR;
    }
    market_id = options->securities[0].market_id;
    code = options->securities[0].code;
    /* --limit defaults to 0, which the command reads as "use the usual 200". */
    limit = options->limit ? (unsigned)options->limit : 200u;
    if (limit > TDX_AUCTION_LIMIT_MAX) {
        tdx_error_set(err, "--limit must be in 1..%u for the auction",
                      (unsigned)TDX_AUCTION_LIMIT_MAX);
        return TDX_ERR;
    }

    tdx_buf_init(&line);
    tdx_auction_series_init(&series);
    memset(endpoint, 0, sizeof(endpoint));
    server_date[0] = '\0';
    if (tdx_auction_fetch_from_pool(&options->pool, options->timeout_ms, market_id, code,
                                    options->selector, (uint32_t)options->start, limit, &series,
                                    endpoint, sizeof(endpoint), server_date, err) != TDX_OK)
        goto done;

    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->output ? options->output : "<stdout>");
        goto done;
    }
    for (index = 0; index < series.count; ++index) {
        tdx_buf_clear(&line);
        if (tdx_auction_format_point(&line, &series.points[index], market_id, code, server_date,
                                     err) != TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the auction stream");
            goto close_output;
        }
    }
    tdx_auction_summarize(&series, &summary);
    tdx_buf_clear(&line);
    if (tdx_auction_format_summary(&line, &series, &summary, market_id, code, server_date,
                                   endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the auction summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    if (options->output)
        fclose(stream);
    if (status != TDX_OK)
        goto done;
    if (!options->quiet)
        fprintf(stderr,
                "auction %s %s: command=0x056A selector=%u points=%zu opening=%zu closing=%zu "
                "flips=%zu/%zu endpoint=%s date=%s\n",
                code, server_date[0] ? server_date : "today", options->selector,
                summary.point_count, summary.opening.point_count, summary.closing.point_count,
                summary.opening.unmatched_direction_flips,
                summary.closing.unmatched_direction_flips, endpoint,
                server_date[0] ? server_date : "-");

done:
    tdx_buf_free(&line);
    tdx_auction_series_free(&series);
    return status;
}

/* ------------------------------------------------------------------ */
/* snapshot                                                            */
/* ------------------------------------------------------------------ */

/* Sends the securities in batches of --batch-size and emits one JSONL line per
 * record plus a trailing summary.  Batching is the point of this command: the
 * record carries no order book, so more securities fit in one round trip than
 * 0x0547 manages.  The universe comes from --security when given and from the
 * server directory otherwise, exactly as sweep resolves it. */
static int command_snapshot(const cli_options *options, tdx_error *err) {
    tdx_connection connection;
    tdx_snapshot records[TDX_SNAPSHOT_BATCH_MAX];
    tdx_snapshot_tally tally;
    tdx_code *codes = NULL;
    size_t code_count = 0;
    tdx_buf line;
    FILE *stream;
    char endpoint[80];
    size_t batch = options->batch_size;
    size_t offset;
    int status = TDX_ERR;

    if (batch < 1) {
        tdx_error_set(err, "--batch-size must be positive");
        return TDX_ERR;
    }
    /* --batch-size is shared with the depth commands, whose cap is higher, so a
     * request above this command's server cap is reduced rather than refused -
     * but it is said out loud instead of silently. */
    if (batch > TDX_SNAPSHOT_BATCH_MAX) {
        if (!options->quiet)
            fprintf(stderr,
                    "note: 0x054C is capped at %u records per request; using %u instead of %zu\n",
                    (unsigned)TDX_SNAPSHOT_BATCH_MAX, (unsigned)TDX_SNAPSHOT_BATCH_MAX, batch);
        batch = TDX_SNAPSHOT_BATCH_MAX;
    }
    if (build_universe(options, &codes, &code_count, err) != TDX_OK)
        return TDX_ERR;
    if (code_count == 0) {
        tdx_error_set(err, "the universe is empty");
        free(codes);
        return TDX_ERR;
    }
    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->output ? options->output : "<stdout>");
        free(codes);
        return TDX_ERR;
    }
    tdx_buf_init(&line);
    tdx_snapshot_tally_init(&tally);
    memset(endpoint, 0, sizeof(endpoint));
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;

    for (offset = 0; offset < code_count; offset += batch) {
        size_t want = code_count - offset;
        size_t got = 0;
        size_t index;
        tdx_error step;

        if (want > batch)
            want = batch;
        step.message[0] = '\0';
        if (tdx_connection_open(&connection, &options->pool.items[0], options->timeout_ms,
                                &step) != TDX_OK) {
            *err = step;
            goto close_output;
        }
        if (tdx_snapshot_fetch(&connection, codes + offset, want, records,
                               TDX_SNAPSHOT_BATCH_MAX, &got, &step) != TDX_OK) {
            tdx_connection_close(&connection);
            *err = step;
            goto close_output;
        }
        tdx_endpoint_address(&options->pool.items[0], endpoint, sizeof(endpoint));
        tdx_connection_close(&connection);
        /* A truncated batch is a failure, not a short answer. */
        if (got != want) {
            tdx_error_set(err, "snapshot batch returned %zu of %zu records", got, want);
            goto close_output;
        }
        for (index = 0; index < got; ++index) {
            tdx_buf_clear(&line);
            if (tdx_snapshot_format(&line, &records[index], err) != TDX_OK)
                goto close_output;
            if (tdx_buf_push(&line, '\n', err) != TDX_OK)
                goto close_output;
            if (fwrite(line.data, 1, line.len, stream) != line.len) {
                tdx_error_set(err, "cannot write the snapshot stream");
                goto close_output;
            }
        }
        tdx_snapshot_tally_add(&tally, records, got);
    }
    tdx_buf_clear(&line);
    if (tdx_snapshot_format_summary(&line, &tally, code_count, endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the snapshot summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    if (options->output)
        fclose(stream);
    free(codes);
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->quiet)
        fprintf(stderr, "snapshot: %zu securities in batches of %zu, endpoint=%s\n",
                tally.record_count, batch, endpoint);
    return status;
}

/* ------------------------------------------------------------------ */
/* finance                                                             */
/* ------------------------------------------------------------------ */

/* Batches the universe through 0x0010.  Unlike the quote commands this one takes
 * a whole list per request and answers with one dense fixed-size record each, so
 * the only integrity question is the length, which the parser answers strictly. */
static int command_finance(const cli_options *options, tdx_error *err) {
    tdx_connection connection;
    tdx_finance_record records[TDX_FINANCE_BATCH_MAX];
    tdx_finance_tally tally;
    tdx_code *codes = NULL;
    size_t code_count = 0;
    tdx_buf line;
    FILE *stream;
    char endpoint[80];
    size_t batch = options->batch_size;
    size_t offset;
    int status = TDX_ERR;

    if (batch < 1)
        batch = 100;
    if (batch > TDX_FINANCE_BATCH_MAX)
        batch = TDX_FINANCE_BATCH_MAX;
    if (build_universe(options, &codes, &code_count, err) != TDX_OK)
        return TDX_ERR;
    if (code_count == 0) {
        tdx_error_set(err, "the universe is empty");
        free(codes);
        return TDX_ERR;
    }
    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->output ? options->output : "<stdout>");
        free(codes);
        return TDX_ERR;
    }
    tdx_buf_init(&line);
    tdx_finance_tally_init(&tally);
    memset(endpoint, 0, sizeof(endpoint));
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;

    for (offset = 0; offset < code_count; offset += batch) {
        size_t want = code_count - offset;
        size_t got = 0;
        size_t index;
        tdx_error step;

        if (want > batch)
            want = batch;
        /* A long walk of this command does get a transient timeout from the
         * server - a whole-market run died after forty-eight clean batches - so
         * each batch is retried on a fresh connection before the command gives
         * up.  Only transport failures are retried: a decode error is a bug and
         * retrying it would just hide the message. */
        {
            int attempt;
            int done = 0;
            for (attempt = 0; attempt < TDX_FINANCE_ATTEMPTS && !done; ++attempt) {
                step.message[0] = '\0';
                if (tdx_connection_open(&connection, &options->pool.items[0],
                                        options->timeout_ms, &step) != TDX_OK) {
                    *err = step;
                    continue;
                }
                if (tdx_finance_fetch(&connection, codes + offset, want, records,
                                      TDX_FINANCE_BATCH_MAX, &got, &step) != TDX_OK) {
                    tdx_connection_close(&connection);
                    *err = step;
                    continue;
                }
                tdx_endpoint_address(&options->pool.items[0], endpoint, sizeof(endpoint));
                tdx_connection_close(&connection);
                done = 1;
            }
            if (!done)
                goto close_output;
        }
        /* The server does drop securities it holds nothing for; that is a short
         * answer rather than a failure, so it is reported, not fatal. */
        for (index = 0; index < got; ++index) {
            tdx_buf_clear(&line);
            if (tdx_finance_format(&line, &records[index], err) != TDX_OK)
                goto close_output;
            if (tdx_buf_push(&line, '\n', err) != TDX_OK)
                goto close_output;
            if (fwrite(line.data, 1, line.len, stream) != line.len) {
                tdx_error_set(err, "cannot write the finance stream");
                goto close_output;
            }
        }
        tdx_finance_tally_add(&tally, records, got);
    }
    tdx_buf_clear(&line);
    if (tdx_finance_format_summary(&line, &tally, endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the finance summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    if (options->output)
        fclose(stream);
    free(codes);
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->quiet)
        fprintf(stderr, "finance: %zu of %zu securities in batches of %zu, endpoint=%s\n",
                tally.record_count, code_count, batch, endpoint);
    return status;
}

/* ------------------------------------------------------------------ */
/* capital                                                             */
/* ------------------------------------------------------------------ */

/* One security per request, because the reply's header names exactly one - so a
 * whole-universe walk is many requests, not many records per request.  A security
 * the server holds nothing for answers with a zero count, which is reported as
 * zero records rather than as a failure. */
static int command_capital(const cli_options *options, tdx_error *err) {
    static tdx_capital_record records[TDX_CAPITAL_RECORDS_MAX];
    tdx_capital_tally tally;
    tdx_code *codes = NULL;
    size_t code_count = 0;
    tdx_connection connection;
    tdx_buf line;
    FILE *stream;
    char endpoint[80];
    size_t index;
    int status = TDX_ERR;
    int connected = 0;

    if (build_universe(options, &codes, &code_count, err) != TDX_OK)
        return TDX_ERR;
    if (code_count == 0) {
        tdx_error_set(err, "the universe is empty");
        free(codes);
        return TDX_ERR;
    }
    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->output ? options->output : "<stdout>");
        free(codes);
        return TDX_ERR;
    }
    tdx_buf_init(&line);
    tdx_capital_tally_init(&tally);
    memset(endpoint, 0, sizeof(endpoint));
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;

    if (options->capital_local) {
        /* The local file holds the whole market, so every security is answered from
         * one read of one file rather than one request each. */
        if (options->gbbq_path)
            snprintf(endpoint, sizeof(endpoint), "local:%s", options->gbbq_path);
        else if (tdx_gbbq_default_path(options->root, endpoint + 6, sizeof(endpoint) - 6,
                                       err) != TDX_OK)
            goto close_output;
        if (options->gbbq_path == NULL)
            memmove(endpoint, "local:", 6);
        for (index = 0; index < code_count; ++index) {
            size_t got = 0;
            size_t source_count = 0;
            size_t record;
            if (tdx_gbbq_load(options->gbbq_path, options->root, &codes[index], records,
                              TDX_CAPITAL_RECORDS_MAX, &got, &source_count, err) != TDX_OK)
                goto close_output;
            for (record = 0; record < got; ++record) {
                tdx_buf_clear(&line);
                if (tdx_capital_format(&line, &records[record], err) != TDX_OK)
                    goto close_output;
                if (tdx_buf_push(&line, '\n', err) != TDX_OK)
                    goto close_output;
                if (fwrite(line.data, 1, line.len, stream) != line.len) {
                    tdx_error_set(err, "cannot write the capital stream");
                    goto close_output;
                }
            }
            tdx_capital_tally_add(&tally, records, got);
        }
        status = TDX_OK;
    } else {
        if (tdx_connection_open(&connection, &options->pool.items[0], options->timeout_ms,
                                err) != TDX_OK)
            goto close_output;
        connected = 1;
        tdx_endpoint_address(&options->pool.items[0], endpoint, sizeof(endpoint));

        for (index = 0; index < code_count; ++index) {
            size_t got = 0;
            size_t blocks = 0;
            size_t record;
            tdx_error step;
            int done = 0;
            int attempt;

            /* One request per security means a long walk issues far more requests
             * than the batched commands, and it meets the same transient server
             * timeout.  Transport failures retry on a fresh connection; a decode
             * error does not, because that is a bug and retrying would hide it. */
            for (attempt = 0; attempt < TDX_FINANCE_ATTEMPTS && !done; ++attempt) {
                step.message[0] = '\0';
                if (attempt > 0) {
                    tdx_connection_close(&connection);
                    connected = 0;
                    connection.socket_handle = (intptr_t)-1;
                    if (tdx_connection_open(&connection, &options->pool.items[0],
                                            options->timeout_ms, &step) != TDX_OK) {
                        *err = step;
                        continue;
                    }
                    connected = 1;
                }
                if (tdx_capital_fetch(&connection, &codes[index], records,
                                      TDX_CAPITAL_RECORDS_MAX, &got, &blocks, &step) == TDX_OK) {
                    done = 1;
                    continue;
                }
                *err = step;
            }
            if (!done)
                goto close_output;
            for (record = 0; record < got; ++record) {
                tdx_buf_clear(&line);
                if (tdx_capital_format(&line, &records[record], err) != TDX_OK)
                    goto close_output;
                if (tdx_buf_push(&line, '\n', err) != TDX_OK)
                    goto close_output;
                if (fwrite(line.data, 1, line.len, stream) != line.len) {
                    tdx_error_set(err, "cannot write the capital stream");
                    goto close_output;
                }
            }
            tdx_capital_tally_add(&tally, records, got);
        }
        status = TDX_OK;
    }

    tdx_buf_clear(&line);
    if (tdx_capital_format_summary(&line, &tally, code_count, endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the capital summary");
        goto close_output;
    }

close_output:
    if (connected)
        tdx_connection_close(&connection);
    if (options->output)
        fclose(stream);
    free(codes);
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->quiet)
        fprintf(stderr, "capital: %zu securities, %zu records, source=%s\n", tally.securities,
                tally.record_count, endpoint);
    return status;
}

/* ------------------------------------------------------------------ */
/* limits                                                              */
/* ------------------------------------------------------------------ */

/* Walks the special price-limit table.  The server answers one row per request,
 * so this is by far the chatty-est command here - the walk is one request per row
 * and the only thing that ends it is an empty page. */
static int command_limits(const cli_options *options, tdx_error *err) {
    static tdx_limit_record rows[TDX_LIMITS_MAX_RECORDS];
    tdx_connection connection;
    tdx_buf line;
    FILE *stream;
    char endpoint[80];
    size_t count = 0;
    unsigned next_index = 0;
    size_t index;
    unsigned start = options->start > 0 ? (unsigned)options->start : 0;
    int status = TDX_ERR;

    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->output ? options->output : "<stdout>");
        return TDX_ERR;
    }
    tdx_buf_init(&line);
    memset(endpoint, 0, sizeof(endpoint));
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;
    if (tdx_connection_open(&connection, &options->pool.items[0], options->timeout_ms, err) !=
        TDX_OK)
        goto close_output;
    tdx_endpoint_address(&options->pool.items[0], endpoint, sizeof(endpoint));
    if (tdx_limits_fetch_all(&connection, start,
                             options->max_records > 0 ? (size_t)options->max_records
                                                      : (size_t)TDX_LIMITS_MAX_RECORDS,
                             TDX_LIMITS_MAX_PAGES, rows, TDX_LIMITS_MAX_RECORDS, &count,
                             &next_index, err) != TDX_OK)
        goto close_output;

    for (index = 0; index < count; ++index) {
        tdx_buf_clear(&line);
        if (tdx_limits_format(&line, &rows[index], start + (unsigned)index, err) != TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the limit stream");
            goto close_output;
        }
    }
    tdx_buf_clear(&line);
    if (tdx_limits_format_summary(&line, rows, count, start, next_index, endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the limit summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    tdx_connection_close(&connection);
    if (options->output)
        fclose(stream);
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->quiet)
        fprintf(stderr, "limits: %zu rows from index %u, next=%u, endpoint=%s\n", count, start,
                next_index, endpoint);
    return status;
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
    if (strcmp(argv[1], "day") == 0) {
        if (command_day(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "trades") == 0) {
        if (command_trades(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "kline") == 0) {
        if (command_kline(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "timeline") == 0) {
        if (command_timeline(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "auction") == 0) {
        if (command_auction(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "snapshot") == 0) {
        if (command_snapshot(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "finance") == 0) {
        if (command_finance(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "capital") == 0) {
        if (command_capital(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "limits") == 0) {
        if (command_limits(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
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
