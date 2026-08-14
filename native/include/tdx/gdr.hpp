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

struct GdrQuery {
    std::string query;
    std::string market;
    std::string code;
    std::string sort{"date"};
    std::string order{"desc"};
    bool include_raw{true};
    bool refresh{};
    int limit{1000};
    int cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_gdr_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});

class GdrService {
public:
    explicit GdrService(
        std::filesystem::path root,
        std::map<std::pair<int, std::string>, Security> securities = {},
        std::filesystem::path jsn_root = {});
    Json query(const GdrQuery& options);

private:
    Json fetch_master(const GdrQuery& options, bool& refreshed, int& age_seconds);
    std::filesystem::path root_;
    std::filesystem::path jsn_root_;
    std::map<std::pair<int, std::string>, Security> securities_;
    Json cache_;
    std::time_t cache_time_{};
};

int command_market_gdr(const std::vector<std::string>& args);

}  // namespace tdx
