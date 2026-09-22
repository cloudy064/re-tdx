#include "cli.h"
#include "tdx_trades.h"
#include "tdx_kline.h"
#include "tdx_auction.h"
#include "tdx_snapshot.h"
#include "tdx_finance.h"
#include "tdx_capital.h"
#include "tdx_limits.h"
#include "tdx_convertible_join.h"
void cli_usage(void) {
    printf("tdx-l1stream %s - standalone C client for TongDaXin L1 quotes\n\n",
           TDX_L1_VERSION);
    printf("Usage:\n");
    printf("  tdx-l1stream probe --security [MARKET:]CODE [options]\n");
    printf("  tdx-l1stream securities [--market sz,sh,bj] [options]\n");
    printf("  tdx-l1stream sweep [--market sz,sh,bj] [-j N] [options]\n");
    printf("  tdx-l1stream watch [--market sz,sh,bj] [-j N] [options]\n");
    printf("  tdx-l1stream serve [--port N] [options]\n");
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
    printf("  tdx-l1stream limit [--security CODE] [--prev PRICE] [--name NAME]\n"
           "                      [--date YYYYMMDD] [options]\n");
    printf("  tdx-l1stream seal --security CODE [--date YYYYMMDD] [options]\n");
    printf("  tdx-l1stream ranking [--category NAME] [--sort KEY] [--start N]\n"
           "                       [--limit N] [--ascending] [options]\n");
    printf("  tdx-l1stream minute --security CODE [--root DIR] [--input PATH]\n"
           "                      [--max-records N] [options]\n");
    printf("  tdx-l1stream blocks [--members] [--assignments] [--root DIR] [options]\n");
    printf("  tdx-l1stream industry [--max-records N] [options]\n");
    printf("  tdx-l1stream valuation [--security INDEX] [--max-records N] [options]\n");
    printf("  tdx-l1stream panorama [--view ID] [--max-records N] [options]\n");
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
    printf("limit:\n");
    printf("  --security CODE      the security to compute limits for\n");
    printf("  --prev PRICE         the previous close, which the rules need\n");
    printf("  --name NAME          the name, used only to recognise special treatment\n");
    printf("  --date YYYYMMDD      the as-of date for the rule switch\n");
    printf("seal:\n");
    printf("  --security CODE      one security, from its 0x0547 depth\n");
    printf("  --date YYYYMMDD      the as-of date for the limit rules\n");
    printf("ranking:\n");
    printf("  --category NAME      a name like a-shares, or a number, default a_share\n");
    printf("  --sort KEY           change-pct, amount, seal-amount, ..., or a number\n");
    printf("  --start N            first row\n");
    printf("  --limit N            rows to fetch, paged 80 at a time, default 80\n");
    printf("  --ascending          ascending instead of the default descending\n");
    printf("minute:\n");
    printf("  --input PATH         a .lc1 file to read; otherwise --security under --root\n");
    printf("  --max-records N      cap how many bars are emitted\n");
    printf("blocks:\n");
    printf("  --members            also emit every block membership\n");
    printf("  --assignments        also emit every security's industry assignment\n");
    printf("  --root PATH          TDX installation root; the three cache files come from it\n");
    printf("industry / valuation / panorama:\n");
    printf("  --security INDEX     valuation: also fetch that index's PE/PB history\n");
    printf("  --view ID            panorama: a view id, or catalog (the default) to list\n");
    printf("  --max-records N      cap how many rows are emitted\n");
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
    printf("  --interval-ms N      hot poll cadence, 1..600000, default 1000\n");
    printf("  --max-subscribers N  concurrent SSE readers, default 16\n");
    printf("  --heartbeat-ms N     idle event for a subscriber, default 15000\n\n");
    printf("  --tier-warm-ms N     warm tier cadence, 0 = auto (3x hot)\n");
    printf("  --idle-interval-ms N after N quiet rounds, slow down to this\n");
    printf("                       cadence; 0 disables, default 0\n");
    printf("  --idle-rounds N      quiet rounds before slowing, default 30\n\n");
    printf("watch:\n");
    printf("  --iterations N       rounds to run, 0 means forever (default 1)\n");
    printf("  --interval-ms N      delay between rounds, 0 only for watch\n");
    printf("                       (back-to-back), default 1000\n\n");
    printf("Output:\n");
    printf("  --output PATH        write the payload to a file instead of stdout\n");
    printf("  --help               show this message\n");
}
