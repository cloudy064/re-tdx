#include "corporate_internal.hpp"

#include "tdx/blocks.hpp"
#include "tdx/blocks_quote.hpp"
#include "tdx/common.hpp"
#include "tdx/hk_actions.hpp"
#include "tdx/minute.hpp"
#include "tdx/security_identity.hpp"
#include "tdx/session_audit.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <set>

namespace fs = std::filesystem;

namespace tdx {

using namespace corporate_detail;
namespace {

Json block_quote_json(const Block& block) {
    Json value = Json::object();
    value["block_id"] = block.block_id;
    value["family"] = block.family;
    value["family_name"] = block.family_name;
    value["block_code"] = block.block_code;
    value["name"] = block.name;
    value["level"] = block.level;
    value["is_leaf"] = block.is_leaf;
    value["member_count"] = block.member_count;
    return value;
}

}  // namespace

int command_market_finance(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help")) {
        std::cout << "Usage: tdx-tool market finance --security CODE [--security CODE ...] "
                     "[--root PATH] [--host HOST:PORT] [--batch-size N] [--include-raw] [--output FILE]\n";
        return 0;
    }
    const auto securities = args.take_options("--security");
    const auto root_text = args.take_option("--root");
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    const auto endpoints = command_endpoints(args, root);
    const int timeout_ms = bounded_integer(args.take_option("--timeout-ms", "10000"),
                                           "--timeout-ms", 1, 600000);
    const int batch_size = bounded_integer(args.take_option("--batch-size", "80"),
                                           "--batch-size", 1, 1000);
    const bool include_raw = args.take_flag("--include-raw");
    const bool compact = args.take_flag("--compact");
    const auto output = native_path(args.take_option("--output", "output/tdx-market-finance.json"));
    args.require_empty();
    const auto document = fetch_finance_document(securities, endpoints, timeout_ms, batch_size,
                                                 include_raw);
    write_document(output, document, compact);
    std::cout << "received " << static_cast<std::uint64_t>(document.at("received").as_number())
              << '/' << static_cast<std::uint64_t>(document.at("requested").as_number())
              << " finance records -> " << path_utf8(output) << '\n';
    return 0;
}

int command_market_capital(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help")) {
        std::cout << "Usage: tdx-tool market capital --security CODE [--security CODE ...] "
                     "[--source online|local] [--gbbq-path FILE] [--root PATH] "
                     "[--host HOST:PORT] [--include-raw] [--output FILE]\n";
        return 0;
    }
    const auto securities = args.take_options("--security");
    const auto source = lower_ascii(trim(args.take_option("--source", "online")));
    if (source != "online" && source != "local")
        throw Error("--source must be online or local");
    const auto gbbq_path_text = args.take_option("--gbbq-path");
    if (source == "online" && !gbbq_path_text.empty())
        throw Error("--gbbq-path requires --source local");
    const auto root_text = args.take_option("--root");
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    const auto endpoints = command_endpoints(args, root);
    const int timeout_ms = bounded_integer(args.take_option("--timeout-ms", "10000"),
                                           "--timeout-ms", 1, 600000);
    const bool include_raw = args.take_flag("--include-raw");
    const bool compact = args.take_flag("--compact");
    const auto output = native_path(args.take_option("--output", "output/tdx-market-capital.json"));
    args.require_empty();
    const auto document = source == "local"
        ? load_local_capital_changes_document(
              root, securities, include_raw,
              gbbq_path_text.empty() ? fs::path{} : native_path(gbbq_path_text))
        : fetch_capital_changes_document(securities, endpoints, timeout_ms,
                                         include_raw);
    write_document(output, document, compact);
    std::cout << "received " << static_cast<std::uint64_t>(document.at("event_count").as_number())
              << " capital-change events -> " << path_utf8(output) << '\n';
    return 0;
}

int command_market_limits(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help")) {
        std::cout << "Usage: tdx-tool market limits [--start-index N] [--max-rows N] "
                     "[--root PATH] [--host HOST:PORT] [--include-raw] [--output FILE]\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    const auto endpoints = command_endpoints(args, root);
    const int timeout_ms = bounded_integer(args.take_option("--timeout-ms", "10000"),
                                           "--timeout-ms", 1, 600000);
    const int start_index = bounded_integer(args.take_option("--start-index", "0"),
                                            "--start-index", 0, 65535);
    const int max_rows = bounded_integer(args.take_option("--max-rows", "10000"),
                                         "--max-rows", 1, 65536);
    const bool include_raw = args.take_flag("--include-raw");
    const bool compact = args.take_flag("--compact");
    const auto output = native_path(args.take_option("--output", "output/tdx-market-limits.json"));
    args.require_empty();
    const auto document = fetch_special_limits_document(endpoints, timeout_ms, start_index,
                                                        max_rows, include_raw);
    write_document(output, document, compact);
    std::cout << "received " << static_cast<std::uint64_t>(document.at("count").as_number())
              << " special-limit records -> " << path_utf8(output) << '\n';
    return 0;
}

int command_market_kline(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help")) {
        std::cout <<
            "Usage: tdx-tool market kline (--security CODE | --block TEXT) [options]\n\n"
            "Load multi-period K-lines and optionally apply local corporate-action factors.\n\n"
            "Options:\n"
            "  --security CODE         A-share code or expansion MARKET:CODE (for example 47:IFL9)\n"
            "  --block TEXT            Exact block id, code, or unique name from the local catalog\n"
            "  --period PERIOD         time|1m|5m|15m|30m|60m|day|week|month\n"
            "  --source SOURCE         online|local (default online)\n"
            "  --kind KIND             auto|stock|index (default auto)\n"
            "  --adjust MODE           none|qfq|hfq|fixed_qfq|fixed_hfq\n"
            "  --anchor-date DATE      Required by fixed_qfq/fixed_hfq\n"
            "  --pages N               1..20 pages (default 1)\n"
            "  --page-size N           1..800 bars per page (default 800)\n"
            "  --start N               History offset (default 0)\n"
            "  --date latest|all|DATE  Intraday date selection (default all)\n"
            "  --timeout-ms N          Socket timeout (default 10000)\n"
            "  --host HOST[:PORT]      Repeatable; default connect.cfg HQHOST primary-first\n"
            "  --root PATH             TDX installation root\n"
            "  --compact               Write compact JSON\n"
            "  --output FILE           Default output/tdx-market-kline.json\n";
        return 0;
    }
    const auto security_text = trim(args.take_option("--security"));
    const auto block_text = trim(args.take_option("--block"));
    if (security_text.empty() == block_text.empty())
        throw Error("provide exactly one of --security or --block");
    const auto period = lower_ascii(trim(args.take_option("--period", "day")));
    const auto source = lower_ascii(trim(args.take_option("--source", "online")));
    auto kind = lower_ascii(trim(args.take_option("--kind", "auto")));
    const auto adjust = lower_ascii(trim(args.take_option("--adjust", "none")));
    const auto anchor_date = trim(args.take_option("--anchor-date"));
    const int pages = bounded_integer(args.take_option("--pages", "1"),
                                      "--pages", 1, 20);
    const int page_size = bounded_integer(args.take_option("--page-size", "800"),
                                          "--page-size", 1, 800);
    const int start = bounded_integer(args.take_option("--start", "0"),
                                      "--start", 0, 65535);
    const int timeout_ms = bounded_integer(args.take_option("--timeout-ms", "10000"),
                                           "--timeout-ms", 1, 600000);
    const auto date = trim(args.take_option("--date", "all"));
    const auto hosts = args.take_options("--host");
    const auto root_text = trim(args.take_option("--root"));
    const bool compact = args.take_flag("--compact");
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-kline.json"));
    args.require_empty();
    if (source != "online" && source != "local")
        throw Error("--source must be online or local");

    fs::path root;
    try {
        root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    } catch (const Error&) {
        if (!root_text.empty()) throw;
    }

    std::optional<BlockQuoteTarget> block_target;
    SecurityCode security;
    if (!block_text.empty()) {
        const auto blocks = load_blocks(root, {
            "industry", "research-industry", "concept", "style", "index"});
        block_target = resolve_block_quote_target(blocks, block_text);
        security = SecurityCode{block_target->market_id, block_target->code};
        if (kind == "stock") throw Error("--kind stock is invalid with --block");
        if (kind == "auto") kind = "index";
    } else {
        security = parse_kline_security(security_text);
    }

    auto document = source == "local"
        ? load_local_kline_document(root, security.market(), security.code, kind,
                                    period, pages, page_size, start, date)
        : fetch_kline_document(security.market(), security.code, kind, period,
                               pages, page_size, start, date, timeout_ms,
                               root, hosts);
    if (!adjust.empty() && adjust != "none" && adjust != "raw") {
        const bool index_mode = kind == "index" ||
            (kind == "auto" && security.market_id == 1 &&
             is_tdx_block_index_code(security.code));
        if (index_mode) throw Error("corporate-action adjustment is only available for securities");
        if (is_hk_action_market(security.market())) {
            document = apply_hk_kline_adjustment(
                std::move(document), root, security.code, adjust, anchor_date);
        } else {
            if (security.market_id > 2)
                throw Error("corporate-action adjustment is unavailable for this expansion market");
            const auto daily = source == "local"
                ? load_local_kline_document(root, security.market(), security.code,
                                            "stock", "day", 20, 800, 0, "all")
                : fetch_kline_document(security.market(), security.code,
                                       "stock", "day", 20, 800, 0,
                                       "all", timeout_ms, root, hosts);
            const auto capital = source == "local"
                ? load_local_capital_changes_document(
                      root, {security.market() + ":" + security.code}, false)
                : fetch_capital_changes_document(
                      {security.market() + ":" + security.code},
                      select_public_quote_endpoints(root, hosts).endpoints,
                      timeout_ms, false);
            document = apply_kline_adjustment(std::move(document), daily, capital,
                                              adjust, anchor_date);
        }
    } else {
        document["adjustment_mode"] = "none";
    }
    if (block_target) {
        document["block"] = block_quote_json(block_target->block);
        document["name"] = block_target->block.name;
        document["name_source"] = "local-block-catalog";
        document["security_type"] = "block-index";
    }
    write_document(output, document, compact);
    std::cout << "received " << static_cast<std::uint64_t>(document.at("count").as_number())
              << ' ' << period << " bars (" << document.at("adjustment_mode").as_string()
              << ") -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
