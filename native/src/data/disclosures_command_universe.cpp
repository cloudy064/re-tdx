#include "disclosures_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn_data.hpp"

#include <filesystem>
#include <iterator>
#include <string>
#include <utility>

namespace tdx::disclosure_detail {
namespace fs = std::filesystem;
namespace {

void append_all(std::vector<DisclosureBackfillSecurity>& target,
                std::vector<DisclosureBackfillSecurity> rows) {
    target.insert(target.end(), std::make_move_iterator(rows.begin()),
                  std::make_move_iterator(rows.end()));
}

}  // namespace

std::vector<DisclosureBackfillSecurity> resolve_disclosure_universe(
    const DisclosureCommandOptions& options, const BlockData& block_data,
    const fs::path& root) {
    const auto& directory = block_data.securities;
    const auto& query = options.query;
    std::vector<DisclosureBackfillSecurity> securities;
    if (!query.market.empty() || !query.code.empty()) {
        if (query.market.empty() || query.code.empty())
            throw Error("batch --market and --code must be supplied together");
        securities.push_back(parse_backfill_security(
            query.market + ":" + query.code, directory));
    }
    for (const auto& value : options.security_values)
        securities.push_back(parse_backfill_security(value, directory));
    for (const auto& value : split(options.securities_text, ','))
        if (!trim(value).empty())
            securities.push_back(parse_backfill_security(value, directory));
    for (const auto& value : options.input_values)
        append_all(securities, read_backfill_input(native_path(value), directory));
    if (options.watchlist) {
        const auto watchlist = root / "T0002" / "blocknew" / "zxg.blk";
        if (!fs::is_regular_file(watchlist))
            throw Error("TDX watchlist is missing: " + path_utf8(watchlist));
        append_all(securities, parse_tdx_watchlist_securities(
            read_text_utf8(watchlist), directory));
    }
    for (const auto& value : options.block_values)
        append_all(securities, block_backfill_securities(block_data, value));
    securities = deduplicate_backfill_securities(std::move(securities));
    if (securities.empty()) throw Error("disclosure security universe is empty");
    if (static_cast<int>(securities.size()) > options.max_securities)
        throw Error("disclosure security universe has " +
                    std::to_string(securities.size()) +
                    " securities; raise --max-securities explicitly");
    return securities;
}

}  // namespace tdx::disclosure_detail
