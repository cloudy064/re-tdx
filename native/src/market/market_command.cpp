#include "market_internal.hpp"

#include <iostream>

namespace tdx {
namespace {

void snapshot_help() {
    std::cout
        << "Usage: tdx-tool market snapshot --security [MARKET:]CODE [options]\n"
           "Options: --security repeatable, --host repeatable, --root PATH, "
           "--timeout-ms N, --batch-size N, --output PATH, --compact\n";
}

void speed_help() {
    std::cout
        << "Usage: tdx-tool market speed --security [MARKET:]CODE [options]\n"
           "Native public 0x053E rise speed plus five-level L1; options match "
           "market snapshot.\n";
}

void depth_help() {
    std::cout
        << "Usage: tdx-tool market depth --security [MARKET:]CODE [options]\n"
           "Native 0x0547 five-level order book; options match market snapshot.\n";
}

} // namespace

int command_market_snapshot(const std::vector<std::string> &raw_args) {
    using namespace market_detail;
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        snapshot_help();
        return 0;
    }
    const auto selected =
        parse_common_options(args, "output/tdx-market-snapshot-native.json");
    args.require_empty();
    const auto downloaded = download_snapshot_batches(selected);
    const auto names = load_blocks(selected.root, {}).securities;
    const auto document = snapshot_document(downloaded, selected, names);
    atomic_write_text(selected.output,
                      document.dump(selected.compact ? -1 : 2) + "\n");
    std::cout << "received " << downloaded.rows.size() << '/'
              << selected.codes.size() << " snapshots -> "
              << path_utf8(selected.output) << '\n';
    return 0;
}

int command_market_speed(const std::vector<std::string> &raw_args) {
    using namespace market_detail;
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        speed_help();
        return 0;
    }
    const auto selected =
        parse_common_options(args, "output/tdx-market-speed-native.json");
    args.require_empty();
    const auto downloaded = download_speed_batches(selected);
    const auto names = load_blocks(selected.root, {}).securities;
    const auto document = speed_document(downloaded, selected, names);
    atomic_write_text(selected.output,
                      document.dump(selected.compact ? -1 : 2) + "\n");
    std::cout << "received " << downloaded.rows.size() << '/'
              << selected.codes.size() << " speed records -> "
              << path_utf8(selected.output) << '\n';
    return 0;
}

int command_market_depth(const std::vector<std::string> &raw_args) {
    using namespace market_detail;
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        depth_help();
        return 0;
    }
    const auto selected =
        parse_common_options(args, "output/tdx-market-depth-native.json");
    args.require_empty();
    const auto blocks = load_blocks(selected.root, {});
    MarketL1Session session(selected.root, &blocks, session_options(selected));
    const auto document = session.poll_depth(display_codes(selected));
    atomic_write_text(selected.output,
                      document.dump(selected.compact ? -1 : 2) + "\n");
    std::cout << "received " << document.at("received").as_number() << '/'
              << selected.codes.size() << " depth records -> "
              << path_utf8(selected.output) << '\n';
    return 0;
}

} // namespace tdx
