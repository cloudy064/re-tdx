#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"
#include "tdx/tqlex.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct InstitutionQuery {
    std::string market;
    std::string code;
    bool refresh{};
    int cache_ttl_seconds{300};
    int timeout_ms{10000};
};

struct HolderQuery {
    std::string holder_id;
    std::string variant_id;
    std::string holder_name;
    std::string reference_code;
    std::string stock_code;
    bool refresh{};
    int offset{};
    int limit{100};
    int cache_ttl_seconds{900};
    int detail_cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json parse_holder_reference(const std::string& url,
                            const std::string& fallback_reference_code = {});
Json tqlex_result_rows(const Json& response);
Json normalize_holder_history_rows(const Json& rows);
Json normalize_holder_stock_rows(const Json& rows);

class InstitutionService {
public:
    struct CachedDocument {
        Json document;
        std::time_t fetched_at{};
    };

    explicit InstitutionService(
        std::map<std::pair<int, std::string>, Security> securities = {},
        TqlexTransport holder_transport = {},
        std::filesystem::path cache_directory = {});
    Json query_security(const InstitutionQuery& options);
    Json query_holder(const HolderQuery& options);

private:
    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::string, CachedDocument> security_cache_;
    std::map<std::string, CachedDocument> holder_cache_;
    std::map<std::string, CachedDocument> detail_cache_;
    TqlexTransport holder_transport_;
    std::filesystem::path cache_directory_;
};

int command_market_institution(const std::vector<std::string>& args);

}  // namespace tdx
