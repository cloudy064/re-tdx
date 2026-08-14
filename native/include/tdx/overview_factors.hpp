#pragma once

#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

struct OverviewFactorsQuery {
    std::string query;
    std::string signal{"all"};
    bool include_raw{true};
    bool refresh{};
    int limit{100};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_overview_factor_rows(const Json& rows);

class OverviewFactorsService {
public:
    explicit OverviewFactorsService(std::filesystem::path jsn_root = {});
    Json query(const OverviewFactorsQuery& options);

private:
    Json fetch_master(const OverviewFactorsQuery& options, bool& refreshed,
                      int& age_seconds);

    std::filesystem::path jsn_root_;
    Json cache_;
    std::time_t cache_time_{};
};

int command_market_overview_factors(const std::vector<std::string>& args);

}  // namespace tdx
