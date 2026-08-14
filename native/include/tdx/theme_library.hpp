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

struct ThemeLibrarySource {
    std::string id;
    std::string name;
    std::string resource;
};

const std::vector<ThemeLibrarySource>& theme_library_sources();

struct ThemeLibraryQuery {
    std::string view{"catalog"};
    std::string source{"general"};
    std::string theme_id;
    std::string market;
    std::string code;
    std::string query;
    std::string sort{"created"};
    std::string order{"desc"};
    bool include_detail{true};
    bool include_chart{true};
    bool refresh{};
    int offset{};
    int limit{500};
    int master_cache_ttl_seconds{900};
    int detail_cache_ttl_seconds{300};
    int timeout_ms{15000};
};

Json normalize_theme_library_rows(
    const Json& rows, const ThemeLibrarySource& source,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_theme_library_details(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_theme_library_chart(const Json& rows);

class ThemeLibraryService {
public:
    ThemeLibraryService(
        std::filesystem::path root,
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const ThemeLibraryQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };
    struct FetchResult { Json document; bool refreshed{}; int age_seconds{}; };

    FetchResult fetch_master(const ThemeLibraryQuery& options);
    FetchResult fetch_dynamic(const std::string& resource,
                              const ThemeLibraryQuery& options);

    std::filesystem::path root_;
    BlockData blocks_;
    CachedDocument master_cache_;
    std::map<std::string, CachedDocument> dynamic_cache_;
};

int command_market_theme_library(const std::vector<std::string>& args);

}  // namespace tdx
