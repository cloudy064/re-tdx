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

struct ShareholderSignalsQuery {
    std::string view{"all"};
    std::string query;
    std::string market;
    std::string code;
    std::string investor_id;
    std::string investor_query;
    std::string sort{"signal"};
    std::string order{"desc"};
    bool include_raw{true};
    bool refresh{};
    int limit{5000};
    int cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_shareholder_signal_rows(
    const std::string& resource,
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_notable_investor_directory_rows(const Json& rows);
Json normalize_notable_investor_holding_rows(
    const std::string& investor_id,
    const std::string& investor_name,
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});

class ShareholderSignalsService {
public:
    explicit ShareholderSignalsService(
        std::filesystem::path root,
        std::map<std::pair<int, std::string>, Security> securities = {},
        std::filesystem::path jsn_root = {});
    Json query(const ShareholderSignalsQuery& options);

private:
    Json fetch_master(const ShareholderSignalsQuery& options, bool& refreshed,
                      int& age_seconds);
    std::filesystem::path root_;
    std::filesystem::path jsn_root_;
    std::map<std::pair<int, std::string>, Security> securities_;
    Json cache_;
    std::time_t cache_time_{};
};

int command_market_shareholder_signals(const std::vector<std::string>& args);

}  // namespace tdx
