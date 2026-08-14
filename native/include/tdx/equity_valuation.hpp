#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct EquityValuationQuery {
    std::string view{"pe-industries"};
    std::string market;
    std::string code;
    std::string industry_code;
    std::string industry_market{"1"};
    std::string start_date;
    std::string end_date;
    std::string pe_type{"ttm"};
    double required_return_rate_pct{3.0};
    bool refresh{};
    int limit{4000};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_equity_valuation_rows(
    const std::string& view, const Json& rows,
    const BlockData& blocks = {});

class EquityValuationService {
public:
    EquityValuationService(std::filesystem::path root, BlockData blocks = {});
    Json query(const EquityValuationQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };

    std::filesystem::path root_;
    BlockData blocks_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_equity_valuation(const std::vector<std::string>& args);

}  // namespace tdx
