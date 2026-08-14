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

struct FundCalendarQuery {
    std::string query;
    std::string market;
    std::string code;
    std::string event_type;
    std::string category;
    std::string date_from;
    std::string date_to;
    std::string order{"asc"};
    bool include_raw{true};
    bool refresh{};
    int limit{5000};
    int cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_fund_calendar_rows(const Json& rows);

class FundCalendarService {
public:
    explicit FundCalendarService(std::filesystem::path jsn_root = {});
    Json query(const FundCalendarQuery& options);

private:
    Json fetch_master(const FundCalendarQuery& options, bool& refreshed,
                      int& age_seconds);
    std::filesystem::path jsn_root_;
    Json cache_;
    std::time_t cache_time_{};
};

int command_market_fund_calendar(const std::vector<std::string>& args);

}  // namespace tdx
