#include "market_internal.hpp"

namespace fs = std::filesystem;

namespace tdx {
namespace {

market_detail::CommonOptions service_options(
    const fs::path &root, const std::vector<std::string> &securities,
    int timeout_ms) {
    using namespace market_detail;
    if (securities.empty())
        throw Error("at least one security is required");
    CommonOptions selected;
    selected.root = root;
    selected.timeout_ms =
        positive_integer(std::to_string(timeout_ms), "timeout_ms", 600000);
    selected.batch_size = 80;
    auto endpoint_selection = load_public_quote_endpoints(root);
    selected.endpoints = std::move(endpoint_selection.endpoints);
    selected.endpoint_source = std::move(endpoint_selection.source);
    selected.available_endpoint_count = endpoint_selection.available_endpoint_count;
    selected.primary_configured = endpoint_selection.primary_configured;
    std::set<std::pair<int, std::string>> seen;
    for (const auto &value : securities) {
        auto code = parse_security(value);
        if (seen.insert(code.key()).second)
            selected.codes.push_back(std::move(code));
    }
    return selected;
}

const std::map<std::pair<int, std::string>, Security> &security_names(
    const BlockData *provided, const BlockData &loaded) {
    return provided ? provided->securities : loaded.securities;
}

} // namespace

Json fetch_market_snapshot_document(const fs::path &root,
                                    const std::vector<std::string> &securities,
                                    int timeout_ms, const BlockData *block_data) {
    using namespace market_detail;
    const auto selected = service_options(root, securities, timeout_ms);
    const auto downloaded = download_snapshot_batches(selected);
    const auto loaded_blocks = block_data ? BlockData{} : load_blocks(root, {});
    return snapshot_document(downloaded, selected,
                             security_names(block_data, loaded_blocks));
}

Json fetch_market_speed_document(const fs::path &root,
                                 const std::vector<std::string> &securities,
                                 int timeout_ms, const BlockData *block_data) {
    using namespace market_detail;
    const auto selected = service_options(root, securities, timeout_ms);
    const auto downloaded = download_speed_batches(selected);
    const auto loaded_blocks = block_data ? BlockData{} : load_blocks(root, {});
    return speed_document(downloaded, selected,
                          security_names(block_data, loaded_blocks));
}

Json fetch_market_depth_document(const fs::path &root,
                                 const std::vector<std::string> &securities,
                                 int timeout_ms, const BlockData *block_data) {
    MarketL1SessionOptions options;
    options.timeout_ms = timeout_ms;
    MarketL1Session session(root, block_data, std::move(options));
    return session.poll_depth(securities);
}

} // namespace tdx
