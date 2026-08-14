#include "stats_internal.hpp"

#include "tdx/common.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {

using namespace stats_detail;

int command_market_stats(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market stats [options]\n\n"
            "Native 0x06B9 zhb.zip download and tdxstat/tdxstat2/tipinfo parser.\n\n"
            "Options:\n"
            "  --security CODE        Optional repeatable security filter\n"
            "  --valuation            With one security, add exact $PE/$PES/$PETTM/$PBMRQ\n"
            "  --local                Read T0002/hq_cache instead of downloading\n"
            "  --stats-dir PATH       Override local statistics directory\n"
            "  --remote-path PATH     Default zhb.zip\n"
            "  --chunk-size N         1..60000 (default 30000)\n"
            "  --host HOST[:PORT]     Repeatable; default connect.cfg HQHOST primary-first\n"
            "  --timeout-ms N         Default 10000\n"
            "  --root PATH            TDX installation root\n"
            "  --output PATH          Default output/tdx-stats-native.json\n"
            "  --compact              Compact JSON\n";
        return 0;
    }
    const auto securities = args.take_options("--security");
    const bool valuation = args.take_flag("--valuation");
    const bool local = args.take_flag("--local");
    const auto stats_dir_text = args.take_option("--stats-dir");
    const auto remote_path = args.take_option("--remote-path", "zhb.zip");
    const auto chunk_size = static_cast<std::uint32_t>(parse_integer(
        args.take_option("--chunk-size", "30000"), "--chunk-size", 1,
        static_cast<int>(maximum_chunk_size)));
    const int timeout_ms = parse_integer(args.take_option("--timeout-ms", "10000"),
                                         "--timeout-ms", 1, 600000);
    const auto hosts = args.take_options("--host");
    const auto root_text = args.take_option("--root");
    const auto output_text = args.take_option("--output", "output/tdx-stats-native.json");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    if (valuation && securities.size() != 1)
        throw Error("--valuation requires exactly one --security");
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : fs::u8path(root_text));
    const auto blocks = load_blocks(root, {});
    TdxStatsResource resource;
    std::string endpoint;
    std::string server_name;
    std::size_t archive_size = 0;
    Json transport;
    if (local) {
        const auto directory = stats_dir_text.empty()
            ? root / "T0002" / "hq_cache" : fs::u8path(stats_dir_text);
        resource = load_local_stats(directory);
    } else {
        auto downloaded = download_stats_resource(
            hosts, remote_path, chunk_size, timeout_ms, root);
        resource = std::move(downloaded.resource);
        endpoint = downloaded.endpoint.address();
        server_name = downloaded.server_name;
        archive_size = downloaded.archive_size;
        transport = std::move(downloaded.transport);
    }
    auto document = stats_resource_document(resource, endpoint, server_name, archive_size,
                                            securities, &blocks,
                                            local ? nullptr : &transport);
    if (valuation) {
        const auto selected = parse_security(securities.front());
        document["valuation"] = fetch_security_valuation_document(
            root, resource, selected.market_id, selected.code, timeout_ms, &blocks);
    }
    const auto output = fs::u8path(output_text);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "parsed tdxstat=" << document.at("stat_count").as_number()
              << ", tdxstat2=" << document.at("stat2_count").as_number()
              << ", returned=" << document.at("returned").as_number()
              << " -> " << path_utf8(output) << '\n';
    return 0;
}


}  // namespace tdx

