/* main.c - command line front end for the C L1 streamer.
 *
 * Commands:
 *   probe       one 0x0547 batch, decoded records as JSON
 *   securities  enumerate the server security directory
 *   sweep       poll a whole universe through a parallel connection pool
 *   watch       repeated sweeps with change detection, JSONL events on stdout
 *   day         replay one historical session from a zst_cache .img, JSONL
 *   daily       local .day daily bars for one security
 *   trades      L1 trade details (minute resolution) for today or one date
 *   kline       multi-period K-lines (0x052D)
 *   timeline    today's intraday time-share series (0x0537)
 *   auction     call-auction point series (0x056A)
 *   snapshot    batched whole-universe L1 snapshot (0x054C)
 *   finance     batched fundamental data (0x0010)
 *   capital     share-capital changes and ex-rights events (0x000F)
 *   limits      the special price-limit list (0x0452)
 *   jsn         fetch a JSN resource and emit its rows (0x02C5 / 0x06B9 + JSON)
 *   convertible fetch and join the six convertible-bond documents
 *   pending     the announced-but-unlisted convertible-bond plans
 *   subscription convertible-bond subscription events with derived valuation
 *   newbond     the new-bond projection reconciled against the subscriptions
 *   professional parse a local professional-data .dat file (stock/market/board)
 *   pricing     convertible-bond terms joined with live quotes and valued
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tdx_auction.h"
#include "tdx_auction_json.h"
#include "tdx_bonds.h"
#include "tdx_bonds_json.h"
#include "tdx_capital.h"
#include "tdx_capital_json.h"
#include "tdx_convertible.h"
#include "tdx_convertible_join.h"
#include "tdx_convertible_json.h"
#include "tdx_directory.h"
#include "tdx_daily.h"
#include "tdx_daily_json.h"
#include "tdx_download.h"
#include "tdx_finance.h"
#include "tdx_finance_json.h"
#include "tdx_gbbq.h"
#include "tdx_jsn.h"
#include "tdx_limits.h"
#include "tdx_pending.h"
#include "tdx_newbond.h"
#include "tdx_professional.h"
#include "tdx_professional_finance.h"
#include "tdx_zip.h"
#include "tdx_professional_json.h"
#include "tdx_newbond_json.h"
#include "tdx_pricing.h"
#include "tdx_pricing_json.h"
#include "tdx_subscription.h"
#include "tdx_subscription_json.h"
#include "tdx_pending_json.h"
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
    printf("  tdx-l1stream daily --security CODE [--root DIR] [--input PATH]\n                        [--max-records N] [options]\n");
    printf("  tdx-l1stream trades --security CODE [--date YYYYMMDD] [options]\n");
    printf("  tdx-l1stream kline --security CODE --period PERIOD [options]\n");
    printf("  tdx-l1stream timeline --security CODE [options]\n");
    printf("  tdx-l1stream auction --security CODE [options]\n");
    printf("  tdx-l1stream snapshot --security CODE [--security CODE ...] [options]\n");
    printf("  tdx-l1stream finance --security CODE [--security CODE ...] [options]\n");
    printf("  tdx-l1stream capital --security CODE [--security CODE ...] [options]\n");
    printf("  tdx-l1stream limits [--start N] [options]\n");
    printf("  tdx-l1stream jsn --resource PATH [options]\n");
    printf("  tdx-l1stream convertible [options]\n");
    printf("  tdx-l1stream pending [options]\n");
    printf("  tdx-l1stream subscription [options]\n");
    printf("  tdx-l1stream pricing [--date YYYYMMDD] [options]\n");
    printf("  tdx-l1stream newbond [options]\n");
    printf("  tdx-l1stream professional --input PATH [--kind stock|market|board]\n"
           "                            [--field N] [--from YYYYMMDD] [--to YYYYMMDD]\n\n");
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
    printf("jsn:\n");
    printf("  --resource PATH      resource under the prefix, e.g. list/zq_aaa201.jsn\n");
    printf("  --prefix NAME        resource prefix, default bi\n");
    printf("  --bonds              map the rows as bond reference rows instead of raw\n");
    printf("                       cells; the size column's unit then follows the\n");
    printf("                       resource's own profile\n");
    printf("  --convertible        map the rows as convertible-bond overview rows; only\n");
    printf("                       the overview document, not the reference's six-document\n");
    printf("                       join\n");
    printf("  --schedule           also expand the coupon schedule arrays\n\n");
    printf("convertible:\n");
    printf("  --max-records N      cap how many bonds the join emits, default %u\n\n",
           (unsigned)TDX_CONVERTIBLE_KEYS_MAX);
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
    const char *resource;
    const char *prefix;
    int bonds;
    int convertible;
    int schedule;
    /* professional: a local file, its kind, a field id and a date range.  The id cannot
     * reuse --index, which already means "the security is an index". */
    const char *input;
    const char *zip;
    const char *code;
    const char *kind;
    unsigned field_id;
    int from_date;
    int to_date;
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
        if (strcmp(argument, "--bonds") == 0) {
            options->bonds = 1;
            continue;
        }
        if (strcmp(argument, "--convertible") == 0) {
            options->convertible = 1;
            continue;
        }
        if (strcmp(argument, "--schedule") == 0) {
            options->schedule = 1;
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
        } else if (strcmp(argument, "--resource") == 0) {
            options->resource = value;
        } else if (strcmp(argument, "--prefix") == 0) {
            options->prefix = value;
        } else if (strcmp(argument, "--zip") == 0) {
            options->zip = value;
        } else if (strcmp(argument, "--code") == 0) {
            options->code = value;
        } else if (strcmp(argument, "--input") == 0) {
            options->input = value;
        } else if (strcmp(argument, "--kind") == 0) {
            options->kind = value;
        } else if (strcmp(argument, "--field") == 0) {
            const long parsed = strtol(value, NULL, 10);
            if (parsed < 0 || parsed > 255) {
                tdx_error_set(err, "--field must be in 0..255");
                return TDX_ERR;
            }
            options->field_id = (unsigned)parsed;
        } else if (strcmp(argument, "--from") == 0) {
            const long parsed = strtol(value, NULL, 10);
            if (parsed < 0 || parsed > 99991231) {
                tdx_error_set(err, "--from must be YYYYMMDD");
                return TDX_ERR;
            }
            options->from_date = (int)parsed;
        } else if (strcmp(argument, "--to") == 0) {
            const long parsed = strtol(value, NULL, 10);
            if (parsed < 0 || parsed > 99991231) {
                tdx_error_set(err, "--to must be YYYYMMDD");
                return TDX_ERR;
            }
            options->to_date = (int)parsed;
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
                       tdx_depth *out, size_t *batches_out, tdx_error *err) {
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
        /* The batches a failed round managed to send still cost upstream, so they are
         * reported rather than discarded. */
        if (batches_out)
            *batches_out = stats.batches;
        state->last = stats;
        return TDX_ERR;
    }
    if (batches_out)
        *batches_out = stats.batches;
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
    fprintf(stderr, "polling %zu securities every %d ms over %zu sessions\n", count,
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

/* ------------------------------------------------------------------ */
/* jsn                                                                 */
/* ------------------------------------------------------------------ */

/* Fetches one JSN resource through the file-transfer commands, converts it from
 * GBK and flattens its groups into one JSONL row per record. */
static int command_jsn(const cli_options *options, tdx_error *err) {
    char remote[TDX_JSN_RESOURCE_MAX + 16];
    char endpoint[80];
    tdx_buf raw;
    tdx_buf utf8;
    tdx_buf line;
    tdx_jsn_document document;
    tdx_file_info info;
    FILE *stream;
    size_t group_index;
    size_t row;
    size_t group_count = 0;
    size_t row_count = 0;
    size_t bonds_named = 0;
    size_t bonds_underlying = 0;
    size_t bonds_sized = 0;
    size_t convertible_complete = 0;
    size_t convertible_exchangeable = 0;
    size_t convertible_underlying = 0;
    int status = TDX_ERR;

    if (!options->resource || !*options->resource) {
        tdx_error_set(err, "jsn needs --resource");
        return TDX_ERR;
    }
    if (tdx_jsn_remote_path(options->resource, options->prefix, remote, sizeof(remote), err) !=
        TDX_OK)
        return TDX_ERR;
    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->output ? options->output : "<stdout>");
        return TDX_ERR;
    }
    tdx_buf_init(&raw);
    tdx_buf_init(&utf8);
    tdx_buf_init(&line);
    tdx_jsn_document_init(&document);
    memset(endpoint, 0, sizeof(endpoint));
    memset(&info, 0, sizeof(info));

    /* The transfer verifies the announced digest when there is one: a short or
     * reordered chunk stream must never look like a complete resource. */
    if (tdx_download_resource(&options->pool, options->timeout_ms, remote, 1, &raw, &info,
                              endpoint, sizeof(endpoint), err) != TDX_OK)
        goto close_output;
    if (!options->quiet)
        fprintf(stderr, "jsn %s: %zu bytes, md5=%s, endpoint=%s\n", remote, raw.len,
                info.md5[0] ? info.md5 : "(none)", endpoint);
    if (tdx_jsn_gbk_to_utf8(raw.data, raw.len, &utf8, err) != TDX_OK)
        goto close_output;
    if (tdx_jsn_parse(utf8.data, utf8.len, &document, err) != TDX_OK)
        goto close_output;

    for (group_index = 0; group_index < tdx_jsn_group_count(&document); ++group_index) {
        const tdx_jsn_group *group = &document.groups[group_index];
        size_t column;

        for (row = 0; row < group->row_count; ++row) {
            tdx_buf_clear(&line);
            if (options->convertible) {
                tdx_convertible_row bond;
                tdx_error step;
                step.message[0] = '\0';
                if (tdx_convertible_normalize(&document, group, row, &bond, &step) != TDX_OK) {
                    *err = step;
                    goto close_output;
                }
                if (tdx_convertible_format(&line, &bond, remote, group_index, row, err) !=
                    TDX_OK)
                    goto close_output;
                if (bond.core_terms_complete)
                    convertible_complete++;
                if (bond.kind == TDX_CONVERTIBLE_EXCHANGEABLE)
                    convertible_exchangeable++;
                if (bond.has_underlying)
                    convertible_underlying++;
                if (tdx_buf_push(&line, '\n', err) != TDX_OK)
                    goto close_output;
                if (fwrite(line.data, 1, line.len, stream) != line.len) {
                    tdx_error_set(err, "cannot write the convertible-bond stream");
                    goto close_output;
                }
                continue;
            }
            if (options->bonds) {
                tdx_bond_row bond;
                tdx_error step;
                step.message[0] = '\0';
                if (tdx_bonds_normalize(&document, group, row, remote, &bond, &step) != TDX_OK) {
                    /* A row whose identity cannot be formed cannot be attributed to
                     * a security, so it is reported rather than skipped. */
                    *err = step;
                    goto close_output;
                }
                if (tdx_bonds_format(&line, &bond, remote, group_index, row, err) != TDX_OK)
                    goto close_output;
                if (bond.name_resolved)
                    bonds_named++;
                if (bond.has_underlying)
                    bonds_underlying++;
                if (bond.has_source_scale)
                    bonds_sized++;
                if (options->schedule) {
                    /* Inserted before the closing brace so the row stays one object. */
                    if (line.len > 0)
                        line.len--;
                    if (tdx_buf_append(&line, ",\"coupon_schedule\":", 19, err) != TDX_OK)
                        goto close_output;
                    if (tdx_bonds_format_schedule(&line, &document, group, row, "FXRQXL",
                                                  "FXLLXL", 512, err) != TDX_OK)
                        goto close_output;
                    if (tdx_buf_append(&line, ",\"remaining_coupon_schedule\":", 29, err) != TDX_OK)
                        goto close_output;
                    if (tdx_bonds_format_schedule(&line, &document, group, row, "SYFXRQXL",
                                                  "SYFXLLXL", 512, err) != TDX_OK)
                        goto close_output;
                    if (tdx_buf_push(&line, '}', err) != TDX_OK)
                        goto close_output;
                }
                if (tdx_buf_push(&line, '\n', err) != TDX_OK)
                    goto close_output;
                if (fwrite(line.data, 1, line.len, stream) != line.len) {
                    tdx_error_set(err, "cannot write the bond stream");
                    goto close_output;
                }
                continue;
            }
            if (tdx_buf_append_printf(&line, err,
                                      "{\"type\":\"jsn_row\",\"resource\":\"%s\",\"group\":%zu,"
                                      "\"row\":%zu",
                                      remote, group_index, row) != TDX_OK)
                goto close_output;
            for (column = 0; column < group->column_count; ++column) {
                const char *name = tdx_jsn_column_name(&document, group, column);
                if (tdx_buf_append(&line, ",", 1, err) != TDX_OK)
                    goto close_output;
                /* The column name comes from the resource, so it is escaped rather
                 * than pasted: a header with a quote in it must not break the row. */
                if (tdx_format_json_string(&line, name ? name : "", err) != TDX_OK)
                    goto close_output;
                if (tdx_buf_append(&line, ":", 1, err) != TDX_OK)
                    goto close_output;
                if (tdx_jsn_cell_json(&document, group, row, column, &line, err) != TDX_OK)
                    goto close_output;
            }
            if (tdx_buf_push(&line, '}', err) != TDX_OK)
                goto close_output;
            if (tdx_buf_push(&line, '\n', err) != TDX_OK)
                goto close_output;
            if (fwrite(line.data, 1, line.len, stream) != line.len) {
                tdx_error_set(err, "cannot write the jsn stream");
                goto close_output;
            }
        }
    }
    /* Captured before the document is freed, because reading them afterwards
     * would be reading freed memory. */
    group_count = document.group_count;
    row_count = document.row_count;
    tdx_buf_clear(&line);
    if (options->convertible) {
        if (tdx_convertible_format_summary(&line, row_count, convertible_complete,
                                           convertible_exchangeable, convertible_underlying,
                                           remote, endpoint, err) != TDX_OK)
            goto close_output;
    } else if (options->bonds) {
        if (tdx_bonds_format_summary(&line, row_count, bonds_named, bonds_underlying,
                                     bonds_sized, remote,
                                     tdx_bonds_scale_name(tdx_bonds_profile(remote).scale),
                                     endpoint, err) != TDX_OK)
            goto close_output;
    } else if (tdx_buf_append_printf(&line, err,
                                     "{\"type\":\"jsn_summary\",\"resource\":\"%s\",\"bytes\":%zu,"
                                     "\"md5\":\"%s\",\"groups\":%zu,\"rows\":%zu,\"endpoint\":",
                                     remote, raw.len, info.md5, group_count, row_count) != TDX_OK) {
        goto close_output;
    } else if (tdx_format_json_string(&line, endpoint, err) != TDX_OK) {
        goto close_output;
    } else if (tdx_buf_append(&line, "}", 1, err) != TDX_OK) {
        goto close_output;
    }
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the jsn summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    if (options->output)
        fclose(stream);
    tdx_jsn_document_free(&document);
    tdx_buf_free(&raw);
    tdx_buf_free(&utf8);
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->quiet)
        fprintf(stderr, "jsn: %zu groups, %zu rows\n", group_count, row_count);
    return status;
}

/* ------------------------------------------------------------------ */
/* convertible                                                         */
/* ------------------------------------------------------------------ */

/* The six documents one at a time, then the join.  Each transfer verifies its own
 * digest, so a short or reordered chunk stream cannot look like a whole document. */
static int command_convertible(const cli_options *options, tdx_error *err) {
    /* Six core documents, then the exchangeable-bond substitute overview and the
     * projection that fills only the ten fields the reference allows it to fill. */
    static const char *const resources[8] = {
        TDX_CONVERTIBLE_OVERVIEW_RESOURCE,   TDX_CONVERTIBLE_PROGRESS_RESOURCE,
        TDX_CONVERTIBLE_COUPONS_RESOURCE,    TDX_CONVERTIBLE_SELLBACK_RESOURCE,
        TDX_CONVERTIBLE_REDEMPTION_RESOURCE, TDX_CONVERTIBLE_REVISION_RESOURCE,
        TDX_CONVERTIBLE_EXCHANGEABLE_RESOURCE,
        TDX_CONVERTIBLE_EXCHANGEABLE_PROJECTION_RESOURCE,
    };
    static tdx_jsn_document documents[8];
    static tdx_buf raw;
    static tdx_buf utf8;
    static tdx_buf line;
    static tdx_code keys[TDX_CONVERTIBLE_KEYS_MAX];
    tdx_convertible_documents set;
    tdx_convertible_row row;
    tdx_convertible_extra extra;
    tdx_convertible_join_flags flags;
    tdx_file_info info;
    char remote[128];
    char endpoint[80];
    FILE *stream;
    size_t index;
    size_t key_count = 0;
    size_t union_count = 0;
    size_t emitted = 0;
    size_t complete = 0;
    size_t from_overview = 0;
    size_t only_elsewhere = 0;
    size_t sold_by_overview = 0;
    size_t supplemented = 0;
    size_t projection_verified = 0;
    size_t projection_fields = 0;
    int status = TDX_ERR;

    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->output ? options->output : "<stdout>");
        return TDX_ERR;
    }
    tdx_buf_init(&raw);
    tdx_buf_init(&utf8);
    tdx_buf_init(&line);
    memset(endpoint, 0, sizeof(endpoint));
    for (index = 0; index < 8; ++index)
        tdx_jsn_document_init(&documents[index]);

    for (index = 0; index < 8; ++index) {
        memset(&info, 0, sizeof(info));
        tdx_buf_clear(&raw);
        tdx_buf_clear(&utf8);
        if (tdx_jsn_remote_path(resources[index], "bi", remote, sizeof(remote), err) != TDX_OK)
            goto close_output;
        if (tdx_download_resource(&options->pool, options->timeout_ms, remote, 1, &raw, &info,
                                  endpoint, sizeof(endpoint), err) != TDX_OK)
            goto close_output;
        if (tdx_jsn_gbk_to_utf8(raw.data, raw.len, &utf8, err) != TDX_OK)
            goto close_output;
        if (tdx_jsn_parse(utf8.data, utf8.len, &documents[index], err) != TDX_OK)
            goto close_output;
        if (!options->quiet)
            fprintf(stderr, "convertible %s: %zu bytes, md5=%s, %zu rows\n", remote, raw.len,
                    info.md5, documents[index].row_count);
    }

    set.overview = &documents[0];
    set.progress = &documents[1];
    set.coupons = &documents[2];
    set.sellback = &documents[3];
    set.redemption = &documents[4];
    set.revision = &documents[5];
    set.exchangeable = &documents[6];
    set.projection = &documents[7];
    if (tdx_convertible_keys(&set, keys, TDX_CONVERTIBLE_KEYS_MAX, &key_count, &union_count,
                             err) != TDX_OK)
        goto close_output;
    if (!options->quiet)
        fprintf(stderr, "convertible: the eight documents name %zu distinct bonds\n",
                union_count);

    for (index = 0; index < key_count; ++index) {
        tdx_error step;
        step.message[0] = '\0';
        if (options->max_records > 0 && emitted >= (size_t)options->max_records)
            break;
        if (tdx_convertible_join(&set, &keys[index], &row, &extra, &flags, &step) != TDX_OK) {
            *err = step;
            goto close_output;
        }
        tdx_buf_clear(&line);
        if (tdx_convertible_format_joined(&line, &row, &extra, &flags, resources[0], err) !=
            TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the convertible stream");
            goto close_output;
        }
        emitted++;
        if (row.core_terms_complete)
            complete++;
        if (flags.from_overview)
            from_overview++;
        else
            only_elsewhere++;
        if (flags.from_overview && extra.has_remaining_balance_100m_yuan)
            sold_by_overview++;
        if (flags.exchangeable_supplemented)
            supplemented++;
        if (flags.exchangeable_projection_verified)
            projection_verified++;
        projection_fields += flags.projection_fields_used;
    }

    tdx_buf_clear(&line);
    if (tdx_buf_append_printf(&line, err,
                              "{\"type\":\"convertible_join_summary\",\"documents\":8,"
                              "\"endpoint\":\"%s\",\"union_keys\":%zu,\"rows\":%zu,"
                              "\"rows_core_terms_complete\":%zu,\"rows_from_overview\":%zu,"
                              "\"rows_only_elsewhere\":%zu,\"rows_exchangeable_supplemented\":%zu,"
                              "\"rows_projection_verified\":%zu,\"projection_fields_used\":%zu}",
                              endpoint, union_count, emitted, complete, from_overview,
                              only_elsewhere, supplemented, projection_verified,
                              projection_fields) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the convertible summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    if (options->output)
        fclose(stream);
    for (index = 0; index < 8; ++index)
        tdx_jsn_document_free(&documents[index]);
    tdx_buf_free(&raw);
    tdx_buf_free(&utf8);
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->quiet)
        fprintf(stderr,
                "convertible: %zu of %zu bonds joined, %zu only in a non-overview document, "
                "%zu exchangeable, %zu projection fields used\n",
                emitted, union_count, only_elsewhere, supplemented, projection_fields);
    return status;
}

/* ------------------------------------------------------------------ */
/* pending                                                             */
/* ------------------------------------------------------------------ */

/* The plan list and the set reconciliation against its two projections.  The
 * projections are compared on WHICH underlying securities they name, not on what
 * values they carry, so the report is two reconciliations and one row list. */
static int command_pending(const cli_options *options, tdx_error *err) {
    static const char *const resources[3] = {
        TDX_PENDING_PRIMARY_RESOURCE,
        TDX_PENDING_PROJECTION_A_RESOURCE,
        TDX_PENDING_PROJECTION_B_RESOURCE,
    };
    static tdx_jsn_document documents[3];
    static tdx_pending_row rows[3][TDX_PENDING_ROWS_MAX];
    static size_t counts[3];
    static size_t skipped[3];
    static tdx_buf raw;
    static tdx_buf utf8;
    static tdx_buf line;
    tdx_file_info info;
    char remote[128];
    char endpoint[80];
    FILE *stream;
    size_t index;
    size_t agreeing = 0;
    int status = TDX_ERR;

    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->output ? options->output : "<stdout>");
        return TDX_ERR;
    }
    tdx_buf_init(&raw);
    tdx_buf_init(&utf8);
    tdx_buf_init(&line);
    memset(endpoint, 0, sizeof(endpoint));
    for (index = 0; index < 3; ++index) {
        tdx_jsn_document_init(&documents[index]);
        counts[index] = 0;
        skipped[index] = 0;
    }

    for (index = 0; index < 3; ++index) {
        memset(&info, 0, sizeof(info));
        tdx_buf_clear(&raw);
        tdx_buf_clear(&utf8);
        if (tdx_jsn_remote_path(resources[index], "bi", remote, sizeof(remote), err) != TDX_OK)
            goto close_output;
        if (tdx_download_resource(&options->pool, options->timeout_ms, remote, 1, &raw, &info,
                                  endpoint, sizeof(endpoint), err) != TDX_OK)
            goto close_output;
        if (tdx_jsn_gbk_to_utf8(raw.data, raw.len, &utf8, err) != TDX_OK)
            goto close_output;
        if (tdx_jsn_parse(utf8.data, utf8.len, &documents[index], err) != TDX_OK)
            goto close_output;
        if (documents[index].group_count != 1) {
            tdx_error_set(err, "%s holds %zu groups, expected one", remote,
                          documents[index].group_count);
            goto close_output;
        }
        if (tdx_pending_normalize(&documents[index], &documents[index].groups[0], rows[index],
                                  TDX_PENDING_ROWS_MAX, &counts[index], &skipped[index],
                                  err) != TDX_OK)
            goto close_output;
        if (!options->quiet)
            fprintf(stderr, "pending %s: %zu bytes, md5=%s, %zu rows (%zu skipped)\n", remote,
                    raw.len, info.md5, counts[index], skipped[index]);
    }

    for (index = 0; index < counts[0]; ++index) {
        tdx_buf_clear(&line);
        if (tdx_pending_format(&line, &rows[0][index], resources[0], index, err) != TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the pending stream");
            goto close_output;
        }
    }

    /* One report per projection, which is what the reference produces. */
    for (index = 1; index < 3; ++index) {
        tdx_pending_reconciliation reconciliation;
        if (tdx_pending_reconcile(rows[0], counts[0], rows[index], counts[index],
                                  &reconciliation, err) != TDX_OK)
            goto close_output;
        tdx_buf_clear(&line);
        if (tdx_pending_format_reconciliation(&line, &reconciliation, resources[0],
                                              resources[index], endpoint, err) != TDX_OK) {
            tdx_pending_reconciliation_free(&reconciliation);
            goto close_output;
        }
        if (reconciliation.exact_security_set)
            agreeing++;
        tdx_pending_reconciliation_free(&reconciliation);
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the pending reconciliation");
            goto close_output;
        }
    }

    tdx_buf_clear(&line);
    if (tdx_pending_format_summary(&line, counts[0], skipped[0], 2, agreeing, resources[0],
                                   endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the pending summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    if (options->output)
        fclose(stream);
    for (index = 0; index < 3; ++index)
        tdx_jsn_document_free(&documents[index]);
    tdx_buf_free(&raw);
    tdx_buf_free(&utf8);
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->quiet)
        fprintf(stderr, "pending: %zu plans, %zu skipped, %zu of 2 projections agree exactly\n",
                counts[0], skipped[0], agreeing);
    return status;
}

/* ------------------------------------------------------------------ */
/* subscription                                                        */
/* ------------------------------------------------------------------ */

/* The subscription events, with the conversion value and the premium derived here
 * because the resource does not carry them. */
static int command_subscription(const cli_options *options, tdx_error *err) {
    static tdx_jsn_document document;
    static tdx_subscription_row rows[TDX_SUBSCRIPTION_ROWS_MAX];
    static tdx_buf raw;
    static tdx_buf utf8;
    static tdx_buf line;
    tdx_file_info info;
    char remote[128];
    char endpoint[80];
    FILE *stream;
    size_t count = 0;
    size_t skipped = 0;
    size_t index;
    size_t listed = 0;
    size_t with_value = 0;
    size_t with_premium = 0;
    int status = TDX_ERR;

    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->output ? options->output : "<stdout>");
        return TDX_ERR;
    }
    tdx_buf_init(&raw);
    tdx_buf_init(&utf8);
    tdx_buf_init(&line);
    tdx_jsn_document_init(&document);
    memset(&info, 0, sizeof(info));
    memset(endpoint, 0, sizeof(endpoint));

    if (tdx_jsn_remote_path(TDX_SUBSCRIPTION_RESOURCE, "bi", remote, sizeof(remote), err) !=
        TDX_OK)
        goto close_output;
    if (tdx_download_resource(&options->pool, options->timeout_ms, remote, 1, &raw, &info,
                              endpoint, sizeof(endpoint), err) != TDX_OK)
        goto close_output;
    if (tdx_jsn_gbk_to_utf8(raw.data, raw.len, &utf8, err) != TDX_OK)
        goto close_output;
    if (tdx_jsn_parse(utf8.data, utf8.len, &document, err) != TDX_OK)
        goto close_output;
    if (document.group_count != 1) {
        tdx_error_set(err, "%s holds %zu groups, expected one", remote, document.group_count);
        goto close_output;
    }
    if (tdx_subscription_normalize(&document, &document.groups[0], rows,
                                   TDX_SUBSCRIPTION_ROWS_MAX, &count, &skipped, err) != TDX_OK)
        goto close_output;
    if (!options->quiet)
        fprintf(stderr, "subscription %s: %zu bytes, md5=%s, %zu rows (%zu skipped)\n", remote,
                raw.len, info.md5, count, skipped);

    for (index = 0; index < count; ++index) {
        tdx_buf_clear(&line);
        if (tdx_subscription_format(&line, &rows[index], TDX_SUBSCRIPTION_RESOURCE, index, err) !=
            TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the subscription stream");
            goto close_output;
        }
        if (rows[index].listed)
            listed++;
        if (rows[index].has_conversion_value_yuan)
            with_value++;
        if (rows[index].has_conversion_premium_pct)
            with_premium++;
    }

    tdx_buf_clear(&line);
    if (tdx_subscription_format_summary(&line, count, skipped, listed, with_value, with_premium,
                                        TDX_SUBSCRIPTION_RESOURCE, endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the subscription summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    if (options->output)
        fclose(stream);
    tdx_jsn_document_free(&document);
    tdx_buf_free(&raw);
    tdx_buf_free(&utf8);
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->quiet)
        fprintf(stderr,
                "subscription: %zu events, %zu listed, %zu with a conversion value, %zu with a "
                "premium\n",
                count, listed, with_value, with_premium);
    return status;
}

/* ------------------------------------------------------------------ */
/* pricing                                                             */
/* ------------------------------------------------------------------ */

/* The pricing view: the resource's own terms joined with live 0x054C quotes and valued
 * with tdx_bond_math.  The quote fetch is the interesting part - the document names
 * bonds AND their underlyings, so the codes are collected in one pass and quoted in
 * batches of the snapshot command's own cap. */
static int command_pricing(const cli_options *options, tdx_error *err) {
    static tdx_jsn_document document;
    static tdx_buf raw;
    static tdx_buf utf8;
    static tdx_buf line;
    static tdx_code codes[TDX_PRICING_ROWS_MAX * 2];
    tdx_snapshot *quotes = NULL;
    tdx_pricing_row *rows = NULL;
    tdx_file_info info;
    char remote[128];
    char endpoint[80];
    char as_of[16];
    FILE *stream;
    size_t code_count = 0;
    size_t collected_skipped = 0;
    size_t quote_count = 0;
    size_t count = 0;
    size_t skipped = 0;
    size_t index;
    size_t complete = 0;
    size_t bond_only = 0;
    size_t terms_only = 0;
    size_t with_yield = 0;
    size_t with_pure = 0;
    size_t live_priced = 0;
    size_t pre_close_priced = 0;
    int status = TDX_ERR;

    /* The as-of date is the caller's or today's local date. */
    if (options->date && *options->date)
        snprintf(as_of, sizeof(as_of), "%s", options->date);
    else {
        time_t now = time(NULL);
        struct tm local;
        memset(&local, 0, sizeof(local));
#ifdef _WIN32
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif
        snprintf(as_of, sizeof(as_of), "%04d%02d%02d", local.tm_year + 1900, local.tm_mon + 1,
                 local.tm_mday);
    }

    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->output ? options->output : "<stdout>");
        return TDX_ERR;
    }
    tdx_buf_init(&raw);
    tdx_buf_init(&utf8);
    tdx_buf_init(&line);
    tdx_jsn_document_init(&document);
    memset(&info, 0, sizeof(info));
    memset(endpoint, 0, sizeof(endpoint));

    if (tdx_jsn_remote_path(TDX_PRICING_RESOURCE, "bi", remote, sizeof(remote), err) != TDX_OK)
        goto close_output;
    if (tdx_download_resource(&options->pool, options->timeout_ms, remote, 1, &raw, &info,
                              endpoint, sizeof(endpoint), err) != TDX_OK)
        goto close_output;
    if (tdx_jsn_gbk_to_utf8(raw.data, raw.len, &utf8, err) != TDX_OK)
        goto close_output;
    if (tdx_jsn_parse(utf8.data, utf8.len, &document, err) != TDX_OK)
        goto close_output;
    if (document.group_count != 1) {
        tdx_error_set(err, "%s holds %zu groups, expected one", remote, document.group_count);
        goto close_output;
    }
    if (tdx_pricing_collect_codes(&document, &document.groups[0], codes,
                                  TDX_PRICING_ROWS_MAX * 2, &code_count, &collected_skipped,
                                  err) != TDX_OK)
        goto close_output;
    if (!options->quiet)
        fprintf(stderr, "pricing %s: %zu bytes, md5=%s, %zu rows, %zu securities to quote\n",
                remote, raw.len, info.md5, document.groups[0].row_count, code_count);

    /* The quotes, in batches of the snapshot command's own cap. */
    if (code_count > 0) {
        quotes = (tdx_snapshot *)calloc(code_count, sizeof(*quotes));
        if (!quotes) {
            tdx_error_set(err, "out of memory for %zu quotes", code_count);
            goto close_output;
        }
        {
            tdx_connection connection;
            size_t offset;
            memset(&connection, 0, sizeof(connection));
            connection.socket_handle = (intptr_t)-1;
            if (tdx_connection_open(&connection, &options->pool.items[0], options->timeout_ms,
                                    err) != TDX_OK)
                goto close_output;
            tdx_endpoint_address(&options->pool.items[0], endpoint, sizeof(endpoint));
            for (offset = 0; offset < code_count; offset += TDX_SNAPSHOT_BATCH_MAX) {
                size_t want = code_count - offset;
                size_t got = 0;
                if (want > TDX_SNAPSHOT_BATCH_MAX)
                    want = TDX_SNAPSHOT_BATCH_MAX;
                if (tdx_snapshot_fetch(&connection, codes + offset, want, quotes + quote_count,
                                       code_count - quote_count, &got, err) != TDX_OK) {
                    tdx_connection_close(&connection);
                    goto close_output;
                }
                quote_count += got;
            }
            tdx_connection_close(&connection);
        }
        if (!options->quiet)
            fprintf(stderr, "pricing: %zu quotes for %zu securities, endpoint=%s\n", quote_count,
                    code_count, endpoint);
    }

    rows = (tdx_pricing_row *)calloc(TDX_PRICING_ROWS_MAX, sizeof(*rows));
    if (!rows) {
        tdx_error_set(err, "out of memory for the pricing rows");
        goto close_output;
    }
    if (tdx_pricing_normalize(&document, &document.groups[0], quotes, quote_count, as_of,
                              strlen(as_of), rows, TDX_PRICING_ROWS_MAX, &count, &skipped,
                              err) != TDX_OK)
        goto close_output;

    for (index = 0; index < count; ++index) {
        tdx_price_source source = TDX_PRICE_UNAVAILABLE;
        double price = 0.0;
        tdx_buf_clear(&line);
        if (tdx_pricing_format(&line, &rows[index], TDX_PRICING_RESOURCE, index, err) != TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the pricing stream");
            goto close_output;
        }
        if (rows[index].availability && strcmp(rows[index].availability, "complete") == 0)
            complete++;
        else if (rows[index].availability && strcmp(rows[index].availability, "bond-only") == 0)
            bond_only++;
        else
            terms_only++;
        if (rows[index].has_maturity_yield_pct)
            with_yield++;
        if (rows[index].has_pure_bond_value)
            with_pure++;
        if (tdx_pricing_quote_price(&rows[index].bond_quote, &price, &source)) {
            if (source == TDX_PRICE_LAST)
                live_priced++;
            else if (source == TDX_PRICE_PRE_CLOSE)
                pre_close_priced++;
        }
    }

    tdx_buf_clear(&line);
    if (tdx_pricing_format_summary(&line, count, skipped, complete, bond_only, terms_only,
                                   with_yield, with_pure, live_priced, pre_close_priced,
                                   TDX_PRICING_RESOURCE, as_of, endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the pricing summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    if (options->output)
        fclose(stream);
    free(quotes);
    free(rows);
    tdx_jsn_document_free(&document);
    tdx_buf_free(&raw);
    tdx_buf_free(&utf8);
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->quiet)
        fprintf(stderr,
                "pricing: %zu rows as of %s, %zu complete, %zu bond-only, %zu terms-only, "
                "%zu with a yield (%zu live / %zu pre-close priced)\n",
                count, as_of, complete, bond_only, terms_only, with_yield, live_priced,
                pre_close_priced);
    return status;
}

/* ------------------------------------------------------------------ */
/* newbond                                                             */
/* ------------------------------------------------------------------ */

/* The new-bond projection matched against the subscription list.  Two documents, one
 * join, and the join is the product: a projection row on its own says what someone
 * planned, and the pair says whether the plan and the event agree. */
static int command_newbond(const cli_options *options, tdx_error *err) {
    static tdx_jsn_document subscription_document;
    static tdx_jsn_document projection_document;
    static tdx_subscription_row subscriptions[TDX_SUBSCRIPTION_ROWS_MAX];
    static tdx_newbond_row projections[TDX_NEWBOND_ROWS_MAX];
    static tdx_buf raw;
    static tdx_buf utf8;
    static tdx_buf line;
    tdx_file_info info;
    char remote[128];
    char endpoint[80];
    FILE *stream;
    size_t subscription_count = 0;
    size_t subscription_skipped = 0;
    size_t projection_count = 0;
    size_t projection_skipped = 0;
    size_t index;
    tdx_newbond_reconciliation reconciliation;
    int have_reconciliation = 0;
    int status = TDX_ERR;
    /* Kept for the closing message: the report is freed on the way out, so reading it
     * afterwards would print zeros - which is exactly what it did before. */
    size_t reported_projection_rows = 0;
    size_t reported_primary_rows = 0;
    size_t reported_by_code = 0;
    size_t reported_by_underlying = 0;
    size_t reported_unmatched = 0;
    size_t reported_date_mismatches = 0;
    size_t reported_size_mismatches = 0;
    int reported_exact = 0;

    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->output ? options->output : "<stdout>");
        return TDX_ERR;
    }
    tdx_buf_init(&raw);
    tdx_buf_init(&utf8);
    tdx_buf_init(&line);
    tdx_jsn_document_init(&subscription_document);
    tdx_jsn_document_init(&projection_document);
    memset(endpoint, 0, sizeof(endpoint));

    /* The projection, which is the smaller document, is read first so a failure costs
     * nothing.  It needs no quotes and no date. */
    memset(&info, 0, sizeof(info));
    if (tdx_jsn_remote_path(TDX_NEWBOND_PROJECTION_RESOURCE, "bi", remote, sizeof(remote),
                           err) != TDX_OK)
        goto close_output;
    if (tdx_download_resource(&options->pool, options->timeout_ms, remote, 1, &raw, &info,
                              endpoint, sizeof(endpoint), err) != TDX_OK)
        goto close_output;
    if (tdx_jsn_gbk_to_utf8(raw.data, raw.len, &utf8, err) != TDX_OK)
        goto close_output;
    if (tdx_jsn_parse(utf8.data, utf8.len, &projection_document, err) != TDX_OK)
        goto close_output;
    if (projection_document.group_count != 1) {
        tdx_error_set(err, "%s holds %zu groups, expected one", remote,
                      projection_document.group_count);
        goto close_output;
    }
    if (tdx_newbond_normalize(&projection_document, &projection_document.groups[0], projections,
                              TDX_NEWBOND_ROWS_MAX, &projection_count, &projection_skipped,
                              err) != TDX_OK)
        goto close_output;
    if (!options->quiet)
        fprintf(stderr, "newbond %s: %zu bytes, md5=%s, %zu rows (%zu skipped)\n", remote,
                raw.len, info.md5, projection_count, projection_skipped);

    /* The subscription list, which supplies the rows to compare against. */
    tdx_buf_clear(&raw);
    tdx_buf_clear(&utf8);
    memset(&info, 0, sizeof(info));
    if (tdx_jsn_remote_path(TDX_SUBSCRIPTION_RESOURCE, "bi", remote, sizeof(remote), err) !=
        TDX_OK)
        goto close_output;
    if (tdx_download_resource(&options->pool, options->timeout_ms, remote, 1, &raw, &info,
                              endpoint, sizeof(endpoint), err) != TDX_OK)
        goto close_output;
    if (tdx_jsn_gbk_to_utf8(raw.data, raw.len, &utf8, err) != TDX_OK)
        goto close_output;
    if (tdx_jsn_parse(utf8.data, utf8.len, &subscription_document, err) != TDX_OK)
        goto close_output;
    if (subscription_document.group_count != 1) {
        tdx_error_set(err, "%s holds %zu groups, expected one", remote,
                      subscription_document.group_count);
        goto close_output;
    }
    if (tdx_subscription_normalize(&subscription_document, &subscription_document.groups[0],
                                   subscriptions, TDX_SUBSCRIPTION_ROWS_MAX, &subscription_count,
                                   &subscription_skipped, err) != TDX_OK)
        goto close_output;
    if (!options->quiet)
        fprintf(stderr, "newbond %s: %zu bytes, md5=%s, %zu rows (%zu skipped)\n", remote,
                raw.len, info.md5, subscription_count, subscription_skipped);

    if (tdx_newbond_reconcile(subscriptions, subscription_count, projections, projection_count,
                              &reconciliation, err) != TDX_OK)
        goto close_output;
    have_reconciliation = 1;
    reported_projection_rows = reconciliation.projection_rows;
    reported_primary_rows = reconciliation.primary_rows;
    reported_by_code = reconciliation.exact_code_matches;
    reported_by_underlying = reconciliation.underlying_only_matches;
    reported_unmatched = reconciliation.unmatched_projection_rows;
    reported_date_mismatches = reconciliation.subscription_date_mismatch_count;
    reported_size_mismatches = reconciliation.issue_size_mismatch_count;
    reported_exact = reconciliation.exact_projection;

    for (index = 0; index < projection_count; ++index) {
        tdx_buf_clear(&line);
        if (tdx_newbond_format_row(&line, &projections[index], &reconciliation.matches[index],
                                   index, err) != TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the projection stream");
            goto close_output;
        }
    }
    tdx_buf_clear(&line);
    if (tdx_newbond_format_reconciliation(&line, &reconciliation, TDX_SUBSCRIPTION_RESOURCE,
                                          TDX_NEWBOND_PROJECTION_RESOURCE, endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the reconciliation");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    if (options->output)
        fclose(stream);
    if (have_reconciliation)
        tdx_newbond_reconciliation_free(&reconciliation);
    tdx_jsn_document_free(&subscription_document);
    tdx_jsn_document_free(&projection_document);
    tdx_buf_free(&raw);
    tdx_buf_free(&utf8);
    tdx_buf_free(&line);
    if (status != TDX_OK)
        return status;
    if (!options->quiet)
        fprintf(stderr,
                "newbond: %zu projection rows against %zu subscriptions, %zu by code, %zu by "
                "underlying, %zu unmatched, %zu date mismatches, %zu size mismatches, "
                "exact=%d\n",
                reported_projection_rows, reported_primary_rows, reported_by_code,
                reported_by_underlying, reported_unmatched, reported_date_mismatches,
                reported_size_mismatches, reported_exact);
    return status;
}

/* ------------------------------------------------------------------ */
/* professional --zip: the quarterly finance packages                  */
/* ------------------------------------------------------------------ */

/* The argument must be a string literal: sizeof sizes it, and a computed expression
 * decays to a pointer, so `cond ? "a" : "b"` would copy sizeof(char*) - 1 bytes. */
#define FINANCE_LITERAL(buf, err, text) tdx_buf_append((buf), (text), sizeof(text) - 1, (err))

/* A number that is absent renders null: a field the record does not carry is not zero. */
static int append_finance_number(tdx_buf *out, int has, double value, tdx_error *err) {
    if (!has)
        return tdx_buf_append(out, "null", 4, err);
    return tdx_buf_append_printf(out, err, "%.6f", value);
}

/* One ZIP from tdxfin/gpcw.txt, whose single member is the quarterly table.  The member
 * is not copied: 5,570 records of 584 floats is 13 MB, and the accessors read one field
 * at a time out of the decompressed buffer. */
static int command_professional_finance(const cli_options *options, tdx_error *err) {
    static tdx_buf archive;
    static tdx_buf member;
    static tdx_buf line;
    tdx_zip_entry entries[TDX_ZIP_ENTRIES_MAX];
    tdx_zip_entry chosen;
    tdx_profinance_document document;
    FILE *input = NULL;
    FILE *stream = NULL;
    long length;
    size_t entry_count = 0;
    size_t emitted = 0;
    size_t index;
    int status = TDX_ERR;

    tdx_buf_init(&archive);
    tdx_buf_init(&member);
    tdx_buf_init(&line);
    input = fopen(options->zip, "rb");
    if (!input) {
        tdx_error_set(err, "cannot open %s", options->zip);
        goto done;
    }
    if (fseek(input, 0, SEEK_END) != 0 || (length = ftell(input)) < 0 ||
        fseek(input, 0, SEEK_SET) != 0) {
        tdx_error_set(err, "cannot size %s", options->zip);
        goto done;
    }
    if (length > 0) {
        if (tdx_buf_reserve(&archive, (size_t)length, err) != TDX_OK)
            goto done;
        if (fread(archive.data, 1, (size_t)length, input) != (size_t)length) {
            tdx_error_set(err, "cannot read %s", options->zip);
            goto done;
        }
        archive.len = (size_t)length;
    }
    fclose(input);
    input = NULL;

    if (tdx_zip_entries(archive.data, archive.len, entries, TDX_ZIP_ENTRIES_MAX, &entry_count,
                        err) != TDX_OK)
        goto done;
    /* The member is named either by --kind (reused as the member name here) or by being
     * the only one, which is what the packages have. */
    if (!tdx_zip_find(entries, entry_count, options->kind ? options->kind : entries[0].name,
                      &chosen)) {
        tdx_error_set(err, "%s holds no member named %s", options->zip,
                      options->kind ? options->kind : entries[0].name);
        goto done;
    }
    if (tdx_zip_extract(archive.data, archive.len, &chosen, &member, err) != TDX_OK)
        goto done;
    if (tdx_profinance_parse(member.data, member.len, &document, err) != TDX_OK)
        goto done;

    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->output ? options->output : "<stdout>");
        goto done;
    }
    tdx_buf_clear(&line);
    /* The paths go through the string formatter: a raw %s would put a Windows path's
     * backslashes into the JSON unescaped, and "C:\Users\..." is not JSON - \U is not a
     * JSON escape at all, so the line was rejected by any parser that read it. */
    if (FINANCE_LITERAL(&line, err, "{\"type\":\"finance_document\",\"source\":") != TDX_OK)
        goto done;
    if (tdx_format_json_string(&line, options->zip, err) != TDX_OK)
        goto done;
    if (FINANCE_LITERAL(&line, err, ",\"member\":") != TDX_OK)
        goto done;
    if (tdx_format_json_string(&line, chosen.name, err) != TDX_OK)
        goto done;
    if (tdx_buf_append_printf(&line, err,
                              ",\"member_bytes\":%u,\"member_crc\":\"%08x\","
                              "\"version\":%u,\"report_date\":%u,\"records\":%zu,"
                              "\"field_count\":%zu,\"index_size\":%zu,\"data_size\":%zu,"
                              "\"named_fields\":[%u,%u]}",
                              chosen.uncompressed_size, chosen.crc, document.version,
                              document.report_date, document.record_count, document.field_count,
                              document.index_size, document.data_size,
                              TDX_PROFINANCE_FIELD_REVENUE_YOY,
                              TDX_PROFINANCE_FIELD_PROFIT_YOY) != TDX_OK)
        goto done;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
        fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the finance document");
        goto done;
    }

    if (options->code && *options->code) {
        /* One security: every column, most of them unnamed. */
        for (index = 0; index < document.record_count; ++index) {
            tdx_profinance_record_view view;
            unsigned field;
            if (tdx_profinance_record_at(&document, index, &view, err) != TDX_OK)
                goto done;
            if (strncmp(view.code, options->code, 6) != 0)
                continue;
            tdx_buf_clear(&line);
            if (tdx_buf_append_printf(&line, err,
                                      "{\"type\":\"finance_record\",\"code\":\"%s\","
                                      "\"market_id\":%d,\"report_date\":%u,\"field_count\":%zu}",
                                      view.code, view.market_id, document.report_date,
                                      view.field_count) != TDX_OK)
                goto done;
            if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
                fwrite(line.data, 1, line.len, stream) != line.len) {
                tdx_error_set(err, "cannot write the finance record");
                goto done;
            }
            for (field = 1; field <= view.field_count; ++field) {
                double value = 0.0;
                int has;
                const char *name;
                if (options->field_id && field != options->field_id)
                    continue;
                if (options->limit && emitted >= options->limit)
                    break;
                name = tdx_profinance_field_name(field);
                has = tdx_profinance_field(&view, field, &value);
                tdx_buf_clear(&line);
                if (tdx_buf_append_printf(&line, err,
                                          "{\"type\":\"finance_field\",\"code\":\"%s\","
                                          "\"field\":%u,\"name\":",
                                          view.code, field) != TDX_OK)
                    goto done;
                if (name) {
                    if (tdx_format_json_string(&line, name, err) != TDX_OK)
                        goto done;
                } else if (FINANCE_LITERAL(&line, err, "null") != TDX_OK) {
                    goto done;
                }
                if (FINANCE_LITERAL(&line, err, ",\"value\":") != TDX_OK)
                    goto done;
                if (append_finance_number(&line, has, value, err) != TDX_OK)
                    goto done;
                if (tdx_buf_push(&line, '}', err) != TDX_OK ||
                    tdx_buf_push(&line, '\n', err) != TDX_OK ||
                    fwrite(line.data, 1, line.len, stream) != line.len) {
                    tdx_error_set(err, "cannot write the finance field");
                    goto done;
                }
                emitted++;
            }
            break;
        }
        if (emitted == 0 && err->message[0] == '\0') {
            tdx_error_set(err, "%s holds no record for code %s", options->zip, options->code);
            goto done;
        }
    } else {
        /* The whole market: the two published fields per security. */
        for (index = 0; index < document.record_count; ++index) {
            tdx_profinance_record_view view;
            double revenue = 0.0;
            double profit = 0.0;
            int has_revenue;
            int has_profit;
            if (options->limit && emitted >= options->limit)
                break;
            if (tdx_profinance_record_at(&document, index, &view, err) != TDX_OK)
                goto done;
            has_revenue = tdx_profinance_field(&view, TDX_PROFINANCE_FIELD_REVENUE_YOY, &revenue);
            has_profit = tdx_profinance_field(&view, TDX_PROFINANCE_FIELD_PROFIT_YOY, &profit);
            tdx_buf_clear(&line);
            if (tdx_buf_append_printf(&line, err,
                                      "{\"type\":\"finance_row\",\"code\":\"%s\","
                                      "\"market_id\":%d,\"report_date\":%u,\"revenue_yoy\":",
                                      view.code, view.market_id, document.report_date) != TDX_OK)
                goto done;
            if (append_finance_number(&line, has_revenue, revenue, err) != TDX_OK)
                goto done;
            if (FINANCE_LITERAL(&line, err, ",\"profit_yoy\":") != TDX_OK)
                goto done;
            if (append_finance_number(&line, has_profit, profit, err) != TDX_OK)
                goto done;
            if (options->field_id) {
                double extra = 0.0;
                int has_extra = tdx_profinance_field(&view, options->field_id, &extra);
                if (tdx_buf_append_printf(&line, err, ",\"field_%u\":", options->field_id) !=
                    TDX_OK)
                    goto done;
                if (append_finance_number(&line, has_extra, extra, err) != TDX_OK)
                    goto done;
            }
            if (tdx_buf_push(&line, '}', err) != TDX_OK ||
                tdx_buf_push(&line, '\n', err) != TDX_OK ||
                fwrite(line.data, 1, line.len, stream) != line.len) {
                tdx_error_set(err, "cannot write the finance row");
                goto done;
            }
            emitted++;
        }
    }
    status = TDX_OK;
    if (!options->quiet)
        fprintf(stderr,
                "professional %s: member %s %u bytes crc %08x, version %u, report date %u, "
                "%zu records of %zu fields, %zu emitted\n",
                options->zip, chosen.name, chosen.uncompressed_size, chosen.crc,
                document.version, document.report_date, document.record_count,
                document.field_count, emitted);

done:
    if (input)
        fclose(input);
    if (stream && options->output)
        fclose(stream);
    tdx_buf_free(&archive);
    tdx_buf_free(&member);
    tdx_buf_free(&line);
    return status;
}

/* ------------------------------------------------------------------ */
/* professional                                                        */
/* ------------------------------------------------------------------ */

/* Reads one professional-data .dat from disk and reports what is in it.  There is no
 * fetch here on purpose: the family is served over HTTPS, and adding TLS to this project
 * would break the deployment property it states for itself, so the file arrives from
 * outside and is parsed here - the same division the JSN resources already use. */
static int command_professional(const cli_options *options, tdx_error *err) {
    static tdx_buf raw;
    static tdx_buf line;
    tdx_professional_record *records = NULL;
    tdx_professional_id_summary *fields = NULL;
    size_t *indices = NULL;
    FILE *input = NULL;
    FILE *stream = NULL;
    long length;
    tdx_professional_kind kind = TDX_PROFESSIONAL_STOCK;
    tdx_professional_selection selection;
    size_t record_count = 0;
    size_t field_count = 0;
    size_t selected = 0;
    size_t named = 0;
    size_t unnamed = 0;
    size_t first_date = 0;
    size_t last_date = 0;
    size_t index;
    int status = TDX_ERR;

    if (options->zip && *options->zip)
        return command_professional_finance(options, err);
    if (!options->input || !*options->input) {
        tdx_error_set(err, "professional needs --input PATH or --zip PATH");
        return TDX_ERR;
    }
    if (options->kind && *options->kind &&
        !tdx_professional_kind_parse(options->kind, strlen(options->kind), &kind)) {
        tdx_error_set(err, "--kind must be stock, market or board");
        return TDX_ERR;
    }
    tdx_buf_init(&raw);
    tdx_buf_init(&line);
    input = fopen(options->input, "rb");
    if (!input) {
        tdx_error_set(err, "cannot open %s", options->input);
        goto done;
    }
    if (fseek(input, 0, SEEK_END) != 0 || (length = ftell(input)) < 0 ||
        fseek(input, 0, SEEK_SET) != 0) {
        tdx_error_set(err, "cannot size %s", options->input);
        goto done;
    }
    if (length > 0) {
        if (tdx_buf_reserve(&raw, (size_t)length, err) != TDX_OK)
            goto done;
        if (fread(raw.data, 1, (size_t)length, input) != (size_t)length) {
            tdx_error_set(err, "cannot read %s", options->input);
            goto done;
        }
        raw.len = (size_t)length;
    }
    fclose(input);
    input = NULL;

    records = (tdx_professional_record *)calloc(TDX_PROFESSIONAL_RECORDS_MAX, sizeof(*records));
    fields = (tdx_professional_id_summary *)calloc(256, sizeof(*fields));
    indices = (size_t *)calloc(TDX_PROFESSIONAL_RECORDS_MAX, sizeof(*indices));
    if (!records || !fields || !indices) {
        tdx_error_set(err, "out of memory for the professional-data buffers");
        goto done;
    }
    if (tdx_professional_parse_trading(raw.data, raw.len, records, TDX_PROFESSIONAL_RECORDS_MAX,
                                       &record_count, err) != TDX_OK)
        goto done;
    if (tdx_professional_summarize(records, record_count, kind, fields, 256, &field_count,
                                   err) != TDX_OK)
        goto done;

    selection.id = options->field_id;
    selection.from = (uint32_t)(options->from_date > 0 ? options->from_date : 0);
    selection.to = (uint32_t)(options->to_date > 0 ? options->to_date : 0);
    if (tdx_professional_select(records, record_count, &selection, indices,
                                TDX_PROFESSIONAL_RECORDS_MAX, &selected, err) != TDX_OK)
        goto done;

    for (index = 0; index < field_count; ++index) {
        if (fields[index].name)
            named++;
        else
            unnamed++;
        if (fields[index].first_date) {
            if (first_date == 0 || fields[index].first_date < first_date)
                first_date = fields[index].first_date;
            if (fields[index].last_date > last_date)
                last_date = fields[index].last_date;
        }
    }

    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->output ? options->output : "<stdout>");
        goto done;
    }
    tdx_buf_clear(&line);
    if (tdx_professional_format_document(&line, options->input, kind, raw.len, record_count,
                                         field_count, named, unnamed, selected, first_date,
                                         last_date, err) != TDX_OK)
        goto done;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto done;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the professional document");
        goto done;
    }
    for (index = 0; index < field_count; ++index) {
        tdx_buf_clear(&line);
        if (tdx_professional_format_summary(&line, &fields[index], kind, err) != TDX_OK)
            goto done;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto done;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the professional fields");
            goto done;
        }
    }
    for (index = 0; index < selected; ++index) {
        const tdx_professional_record *record = &records[indices[index]];
        tdx_buf_clear(&line);
        if (tdx_professional_format_record(&line, record,
                                           tdx_professional_field_name(kind, record->id),
                                           indices[index], err) != TDX_OK)
            goto done;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto done;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the professional records");
            goto done;
        }
    }
    status = TDX_OK;
    if (!options->quiet)
        fprintf(stderr,
                "professional %s: %zu bytes, %zu records, %zu fields (%zu named, %zu unnamed), "
                "%zu selected\n",
                options->input, raw.len, record_count, field_count, named, unnamed, selected);

done:
    if (input)
        fclose(input);
    if (stream && options->output)
        fclose(stream);
    free(records);
    free(fields);
    free(indices);
    tdx_buf_free(&raw);
    tdx_buf_free(&line);
    return status;
}

/* ------------------------------------------------------------------ */
/* daily                                                               */
/* ------------------------------------------------------------------ */

/* The local .day daily bars.  No network: this reads a file the terminal already keeps.
 * The price scale is NOT a constant - it is 100 times the wire divisor table's value, so
 * a stock is hundredths and a bond is ten-thousandths, and getting that wrong makes a bond
 * a hundred times too expensive while still looking like a number. */
static int command_daily(const cli_options *options, tdx_error *err) {
    static tdx_buf raw;
    static tdx_buf line;
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

    if (options->security_count == 1) {
        code = options->securities[0].code;
        market_id = options->securities[0].market_id;
    } else if (!options->input || !*options->input) {
        tdx_error_set(err, "daily needs one --security CODE or an --input PATH");
        return TDX_ERR;
    }
    if (options->input && *options->input) {
        snprintf(path, sizeof(path), "%s", options->input);
        /* With an explicit file the scale has to come from the code, so a caller who knows
         * it should still pass --security; otherwise the stock scale is assumed and the
         * output says which divisor was used. */
    } else if (tdx_daily_locate(options->root, market_id, code, path, sizeof(path), err) !=
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

    stream = open_output(options);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->output ? options->output : "<stdout>");
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
    if (!options->quiet)
        fprintf(stderr, "daily %s: %zu bytes, %zu bars, scale divisor %d, %zu emitted\n", path,
                raw.len, bar_count, scale, emitted);

done:
    if (input)
        fclose(input);
    if (stream && options->output)
        fclose(stream);
    free(bars);
    tdx_buf_free(&raw);
    tdx_buf_free(&line);
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
    if (strcmp(argv[1], "jsn") == 0) {
        if (command_jsn(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "convertible") == 0) {
        if (command_convertible(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "pending") == 0) {
        if (command_pending(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "subscription") == 0) {
        if (command_subscription(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "pricing") == 0) {
        if (command_pricing(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "newbond") == 0) {
        if (command_newbond(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "daily") == 0) {
        if (command_daily(&options, &error) != TDX_OK) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "professional") == 0) {
        if (command_professional(&options, &error) != TDX_OK) {
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
