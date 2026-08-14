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

struct EmployeeQuery {
    std::string view{"catalog"};
    std::string query;
    std::string market;
    std::string code;
    std::string sort{"executive-compensation"};
    bool refresh{};
    int limit{5000};
    int cache_ttl_seconds{900};
    int timeout_ms{15000};
};

Json normalize_employee_share_plan_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);

class EmployeeService {
public:
    explicit EmployeeService(
        std::map<std::pair<int, std::string>, Security> securities = {},
        std::filesystem::path jsn_root = {});
    Json query(const EmployeeQuery& options);

private:
    Json fetch_master(const EmployeeQuery& options, bool& refreshed,
                      int& age_seconds);
    std::map<std::pair<int, std::string>, Security> securities_;
    std::filesystem::path jsn_root_;
    Json cache_;
    std::time_t cache_time_{};
};

int command_market_employees(const std::vector<std::string>& args);

}  // namespace tdx
