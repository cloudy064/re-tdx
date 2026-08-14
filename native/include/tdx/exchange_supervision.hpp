#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct ExchangeSupervisionQuery {
    std::string view{"current"};
    std::string market;
    std::string code;
    std::string query;
    std::string from;
    std::string to;
    std::string sort{"end-date"};
    std::string order{"desc"};
    bool pdf_only{};
    bool include_quotes{true};
    bool refresh{};
    int offset{};
    int limit{500};
    int cache_ttl_seconds{300};
    int quote_cache_ttl_seconds{5};
    int timeout_ms{15000};
};

Json normalize_exchange_supervision_rows(
    const Json& rows, const std::string& record_kind,
    const Json& quote_rows = Json::array(),
    const std::map<std::pair<int, std::string>, Security>& securities = {});
void sort_exchange_supervision_rows(Json& rows, const std::string& sort,
                                    const std::string& order);

class ExchangeSupervisionService {
public:
    ExchangeSupervisionService(
        std::filesystem::path root,
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const ExchangeSupervisionQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };
    Json fetch_resource(const std::string& resource, bool refresh,
                        int cache_ttl_seconds, int timeout_ms, bool& fetched);
    Json fetch_quotes(const std::vector<std::string>& securities, bool refresh,
                      int cache_ttl_seconds, int timeout_ms, bool& fetched);

    std::filesystem::path root_;
    BlockData blocks_;
    std::map<std::string, CachedDocument> resource_cache_;
    CachedDocument quote_cache_;
};

int command_market_exchange_supervision(const std::vector<std::string>& args);

}  // namespace tdx
