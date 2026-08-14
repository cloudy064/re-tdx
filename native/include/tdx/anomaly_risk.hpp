#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct AnomalyRiskQuery {
    std::string view{"statistics"};
    std::string query;
    std::string market;
    std::string code;
    std::string warning{"all"};
    bool warnings_only{};
    bool refresh{};
    int limit{5000};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_anomaly_statistics_rows(const Json& rows,
                                       const BlockData& blocks = {});
Json normalize_suspension_risk_rows(const Json& rows,
                                   const BlockData& blocks = {});

class AnomalyRiskService {
public:
    AnomalyRiskService(std::filesystem::path root, BlockData blocks = {});
    Json query(const AnomalyRiskQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };

    std::filesystem::path root_;
    BlockData blocks_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_anomaly_risk(const std::vector<std::string>& args);

}  // namespace tdx
