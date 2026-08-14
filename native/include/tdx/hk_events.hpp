#pragma once

#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

struct HkEventQuery {
    std::string view{"all"};
    std::string query;
    std::string code;
    std::string date_from;
    std::string date_to;
    bool refresh{};
    int limit{10000};
    int cache_ttl_seconds{900};
    int timeout_ms{15000};
};

struct HkShortHistoryQuery {
    std::string market{"31"};
    std::string code;
    bool refresh{};
    int pages{1};
    int page_size{800};
    int start{};
    int cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_hk_event_rows(const std::string& resource, const Json& rows);
Json normalize_hk_short_history(const Json& kline_document,
                                const Json& short_event_rows,
                                const std::string& market,
                                const std::string& code);

class HkEventService {
public:
    explicit HkEventService(std::filesystem::path jsn_root = {});
    Json query(const HkEventQuery& options);
    Json query_short_history(const HkShortHistoryQuery& options);

private:
    Json fetch_master(const HkEventQuery& options, bool& refreshed,
                      int& age_seconds);
    std::filesystem::path jsn_root_;
    Json cache_;
    std::time_t cache_time_{};
};

int command_market_hk_events(const std::vector<std::string>& args);
int command_market_hk_short_history(const std::vector<std::string>& args);

}  // namespace tdx
