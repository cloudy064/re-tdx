#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct SpecializedMetricsQuery {
    std::string view{"all"};
    std::string query;
    std::string market;
    std::string code;
    bool refresh{};
    int limit{5000};
    int cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_specialized_metrics_rows(
    const std::string& resource,
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});

class SpecializedMetricsService {
public:
    explicit SpecializedMetricsService(
        std::filesystem::path root,
        std::map<std::pair<int, std::string>, Security> securities = {},
        std::filesystem::path jsn_root = {});
    Json query(const SpecializedMetricsQuery& options);

private:
    Json fetch_master(const SpecializedMetricsQuery& options, bool& refreshed,
                      int& age_seconds);

    std::filesystem::path root_;
    std::filesystem::path jsn_root_;
    std::map<std::pair<int, std::string>, Security> securities_;
    Json cache_;
    std::time_t cache_time_{};
};

int command_market_specialized_metrics(const std::vector<std::string>& args);

}  // namespace tdx
