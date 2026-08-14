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

struct StrategicThemeCategory {
    StrategicThemeCategory(std::string block_id_value, std::string name_value,
                           std::string config_name_value, std::string resource_value,
                           std::string name_field_value = "gname",
                           std::string id_prefix_value = {},
                           std::string description_field_value = {})
        : block_id(std::move(block_id_value)), name(std::move(name_value)),
          config_name(std::move(config_name_value)), resource(std::move(resource_value)),
          name_field(std::move(name_field_value)), id_prefix(std::move(id_prefix_value)),
          description_field(std::move(description_field_value)) {}

    std::string block_id;
    std::string name;
    std::string config_name;
    std::string resource;
    std::string name_field;
    std::string id_prefix;
    std::string description_field;
};

const std::vector<StrategicThemeCategory>& strategic_theme_categories();

struct StrategicThemeQuery {
    std::string view{"categories"};
    std::string category;
    std::string theme_id;
    std::string market;
    std::string code;
    std::string query;
    std::string sort{"name"};
    std::string order{"asc"};
    bool include_detail{true};
    bool refresh{};
    int offset{};
    int limit{500};
    int master_cache_ttl_seconds{900};
    int detail_cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_strategic_theme_master(
    const Json& rows, const StrategicThemeCategory& category,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_strategic_theme_details(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});

class StrategicThemeService {
public:
    StrategicThemeService(
        std::filesystem::path root,
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const StrategicThemeQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };
    struct FetchResult { Json document; bool refreshed{}; int age_seconds{}; };

    FetchResult fetch_master(const StrategicThemeQuery& options);
    FetchResult fetch_detail(const std::string& theme_id,
                             const StrategicThemeQuery& options);

    std::filesystem::path root_;
    BlockData blocks_;
    CachedDocument master_cache_;
    std::map<std::string, CachedDocument> detail_cache_;
};

int command_market_strategic_themes(const std::vector<std::string>& args);

}  // namespace tdx
