#include "tdx/shape_match.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::size_t nonnegative_index(const std::string& value) {
    try {
        if (value.empty() || value.front() == '-' || value.front() == '+')
            throw std::invalid_argument("index");
        std::size_t used = 0;
        const auto parsed = std::stoull(value, &used);
        if (used != value.size()) throw std::invalid_argument("index");
        return static_cast<std::size_t>(parsed);
    } catch (...) {
        throw Error("--template-index must be a non-negative integer");
    }
}

int timeout_value(const std::string& value) {
    try {
        std::size_t used = 0;
        const auto parsed = std::stoi(value, &used);
        if (used != value.size() || parsed < 1 || parsed > 600000)
            throw std::invalid_argument("timeout");
        return parsed;
    } catch (...) {
        throw Error("--timeout-ms must be in 1..600000");
    }
}

int bounded_value(const std::string& value, const char* option,
                  int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const auto parsed = std::stoi(value, &used);
        if (used != value.size() || parsed < minimum || parsed > maximum)
            throw std::invalid_argument("range");
        return parsed;
    } catch (...) {
        throw Error(std::string(option) + " must be in " +
                    std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

}  // namespace

int command_market_shape_match(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market shape-match [options]\n\n"
            "Inspect TDXDeep shapematch.dat, score one security, or scan a universe.\n\n"
            "Options:\n"
            "  --view NAME          library|score|scan (default library)\n"
            "  --root PATH          TDX installation root\n"
            "  --file PATH          Override T0002/shapematch.dat\n"
            "  --template NAME      Exact template name for score view\n"
            "  --template-index N   Template record index for score view\n"
            "  --candidate PATH     Existing tdx-minute-v1 JSON (offline score)\n"
            "  --input PATH         Offline K-line bundle for scan view\n"
            "  --security ID        Repeatable sz:000001/sh600000 scan candidate\n"
            "  --block ID           Local block id, code or unique exact name\n"
            "  --all                Scan the template library's local scope\n"
            "  --market MARKET      Candidate market when K-lines are loaded\n"
            "  --code CODE          Candidate security code\n"
            "  --source MODE        auto|local|network (scan default local)\n"
            "  --period PERIOD      Override template period for K-line loading\n"
            "  --kind KIND          auto|stock|index (default auto)\n"
            "  --timeout-ms N       Network timeout (default 10000)\n"
            "  --workers N          Scan workers, 1..16 (default 4)\n"
            "  --max-candidates N   Scan safety cap, 1..10000 (default 10000)\n"
            "  --max-network-requests N  Explicit scan network cap (default 0)\n"
            "  --limit N            Ranked scan result cap (default 1000)\n"
            "  --include-unmatched  Include eligible misses in ranked results\n"
            "  --output PATH        Default output/tdx-market-shape-match.json\n"
            "  --compact            Write compact JSON\n\n"
            "The native score is the weighted trailing Pearson correlation used by\n"
            "TDXDeep. Scan requires exactly one universe option. It stays local by\n"
            "default; auto/network never exceed --max-network-requests. No original\n"
            "DLL is loaded by the command.\n";
        return 0;
    }
    ShapeMatchQuery query;
    query.view = args.take_option("--view", "library");
    query.template_name = args.take_option("--template");
    if (args.has("--template-index"))
        query.template_index = nonnegative_index(
            args.take_option("--template-index"));
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.source = args.take_option(
        "--source", lower_ascii(trim(query.view)) == "scan" ? "local" : "auto");
    query.period = args.take_option("--period");
    query.kind = args.take_option("--kind", "auto");
    query.securities = args.take_options("--security");
    query.block = args.take_option("--block");
    query.all_securities = args.take_flag("--all");
    query.include_unmatched = args.take_flag("--include-unmatched");
    query.workers = bounded_value(
        args.take_option("--workers", "4"), "--workers", 1, 16);
    query.max_candidates = bounded_value(
        args.take_option("--max-candidates", "10000"),
        "--max-candidates", 1, 10000);
    query.max_network_requests = bounded_value(
        args.take_option("--max-network-requests", "0"),
        "--max-network-requests", 0, 10000);
    query.result_limit = bounded_value(
        args.take_option("--limit", "1000"), "--limit", 1, 10000);
    if (args.has("--timeout-ms"))
        query.timeout_ms = timeout_value(args.take_option("--timeout-ms"));
    const auto root_text = args.take_option("--root");
    const auto file_text = args.take_option("--file");
    const auto candidate_text = args.take_option("--candidate");
    const auto input_text = args.take_option("--input");
    if (!file_text.empty()) query.library_path = native_path(file_text);
    if (!candidate_text.empty()) query.candidate_path = native_path(candidate_text);
    if (!input_text.empty()) query.scan_input_path = native_path(input_text);
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-shape-match.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    const auto document = load_shape_match(root, query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "wrote TDXDeep shape-match result -> "
              << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
