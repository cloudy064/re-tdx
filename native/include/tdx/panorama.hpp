#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct PanoramaQuery {
    std::string view{"catalog"};
    std::string market;
    std::string code;
    std::string query;
    bool refresh{};
    int offset{};
    int limit{200};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json compose_panorama_document(
    const PanoramaQuery& options,
    const Json& source_documents,
    const std::map<std::pair<int, std::string>, Security>& securities = {});

class PanoramaService {
public:
    explicit PanoramaService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const PanoramaQuery& options);

private:
    struct CachedDocument {
        Json document;
        std::time_t fetched_at{};
    };
    std::map<std::pair<int, std::string>, Security> securities_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_panorama(const std::vector<std::string>& args);

}  // namespace tdx
