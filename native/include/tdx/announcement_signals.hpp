#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct AnnouncementSignalsQuery {
    std::string view{"selected"};
    std::string market;
    std::string code;
    std::string query;
    std::string direction{"all"};
    std::string announcement_type;
    std::string from;
    std::string to;
    std::string sort{"date"};
    std::string order{"desc"};
    bool include_history{true};
    bool refresh{};
    int offset{};
    int limit{500};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_announcement_signal_rows(
    const Json& rows, const std::string& source_family,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_announcement_history_rows(
    const Json& rows, int market_id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
void sort_announcement_signal_rows(Json& rows, const std::string& sort,
                                   const std::string& order);

class AnnouncementSignalsService {
public:
    explicit AnnouncementSignalsService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const AnnouncementSignalsQuery& options);

private:
    struct CachedDocument {
        Json document;
        std::time_t fetched_at{};
    };

    Json fetch(const std::string& resource, bool refresh,
               int cache_ttl_seconds, int timeout_ms, bool& fetched);

    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_announcement_signals(const std::vector<std::string>& args);

}  // namespace tdx
