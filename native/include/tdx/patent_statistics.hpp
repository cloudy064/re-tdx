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

struct PatentStatisticsQuery {
    std::string query;
    std::string market;
    std::string code;
    std::string report_from;
    std::string report_to;
    std::string sort{"cumulative-total"};
    std::string order{"desc"};
    bool include_raw{true};
    bool refresh{};
    int limit{10000};
    int cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_patent_statistic_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});

class PatentStatisticsService {
public:
    explicit PatentStatisticsService(
        std::map<std::pair<int, std::string>, Security> securities = {},
        std::filesystem::path jsn_root = {});

    Json query(const PatentStatisticsQuery& options);

private:
    Json fetch_master(const PatentStatisticsQuery& options, bool& refreshed,
                      int& age_seconds);

    std::filesystem::path jsn_root_;
    std::map<std::pair<int, std::string>, Security> securities_;
    Json cache_;
    std::time_t cache_time_{};
};

int command_market_patent_statistics(const std::vector<std::string>& args);

}  // namespace tdx
