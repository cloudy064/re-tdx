#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct StateOwnedDimension {
    std::string name;
    std::string label;
    std::string resource;
    std::string unit_id;
    std::string name_field;
};

struct StateOwnedReformQuery {
    std::string view{"groups"};
    std::string dimension{"industry"};
    std::string group_id;
    std::string market;
    std::string code;
    std::string query;
    std::string sort{"count"};
    std::string order{"desc"};
    bool include_details{true};
    bool include_quotes{true};
    bool refresh{};
    int offset{};
    int limit{500};
    int detail_limit{500};
    int master_cache_ttl_seconds{300};
    int detail_cache_ttl_seconds{300};
    int quote_cache_ttl_seconds{5};
    int timeout_ms{15000};
};

const std::vector<StateOwnedDimension>& state_owned_dimensions();
Json normalize_state_owned_groups(
    const Json& rows, const StateOwnedDimension& dimension,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_state_owned_details(
    const Json& rows, const Json& quote_rows = Json::array(),
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_state_owned_restructuring(
    const Json& rows, const Json& quote_rows = Json::array(),
    const std::map<std::pair<int, std::string>, Security>& securities = {});

class StateOwnedReformService {
public:
    StateOwnedReformService(
        std::filesystem::path root,
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const StateOwnedReformQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };
    struct FetchResult { Json document; bool refreshed{}; int age_seconds{}; };

    FetchResult fetch_master(const StateOwnedReformQuery& options);
    FetchResult fetch_resource(const std::string& resource,
                               const StateOwnedReformQuery& options);
    FetchResult fetch_quotes(const std::vector<std::string>& securities,
                             const StateOwnedReformQuery& options);

    std::filesystem::path root_;
    BlockData blocks_;
    CachedDocument master_cache_;
    std::map<std::string, CachedDocument> resource_cache_;
    std::map<std::string, CachedDocument> quote_cache_;
};

int command_market_state_owned_reform(const std::vector<std::string>& args);

}  // namespace tdx
