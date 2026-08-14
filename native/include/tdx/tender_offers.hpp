#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct TenderOfferQuery {
    std::string market;
    std::string code;
    std::string query;
    std::string status{"all"};
    std::string from;
    std::string to;
    std::string sort{"announcement-date"};
    std::string order{"desc"};
    bool refresh{};
    int offset{};
    int limit{1000};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_tender_offer_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
void sort_tender_offer_rows(Json& rows, const std::string& sort,
                            const std::string& order);

class TenderOfferService {
public:
    explicit TenderOfferService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const TenderOfferQuery& options);

private:
    struct CachedDocument {
        Json document;
        std::time_t fetched_at{};
    };
    struct FetchResult {
        Json document;
        bool refreshed{};
        bool stale{};
        int age_seconds{};
        std::string warning;
    };

    FetchResult fetch(const TenderOfferQuery& options);

    std::map<std::pair<int, std::string>, Security> securities_;
    CachedDocument cache_;
};

int command_market_tender_offers(const std::vector<std::string>& args);

}  // namespace tdx
