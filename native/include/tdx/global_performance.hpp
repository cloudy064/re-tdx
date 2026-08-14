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

using GlobalInstrumentNames =
    std::map<std::pair<int, std::string>, std::string>;

struct GlobalPerformanceQuery {
    std::string view{"all"};
    std::string query;
    std::string market;
    std::string code;
    std::string sort{"return-5d"};
    std::string order{"desc"};
    bool include_raw{true};
    bool refresh{};
    int limit{10000};
    int cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_global_performance_rows(
    const std::string& resource, const Json& rows,
    const GlobalInstrumentNames& names = {});

class GlobalPerformanceService {
public:
    explicit GlobalPerformanceService(
        std::map<std::pair<int, std::string>, Security> securities = {},
        std::filesystem::path jsn_root = {});
    Json query(const GlobalPerformanceQuery& options);

private:
    Json fetch_master(const GlobalPerformanceQuery& options, bool& refreshed,
                      int& age_seconds);
    GlobalInstrumentNames load_names() const;
    std::map<std::pair<int, std::string>, Security> securities_;
    std::filesystem::path jsn_root_;
    Json cache_;
    std::time_t cache_time_{};
};

int command_market_global_performance(const std::vector<std::string>& args);

}  // namespace tdx
