#pragma once

#include "tdx/formula_engine.hpp"
#include "tdx/formula_context.hpp"

#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/formulas.hpp"
#include "tdx/minute.hpp"
#include "tdx/security_directory.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace tdx::formula_scan_detail {

const Json* optional(const Json& object, std::string_view key);
std::filesystem::path from_utf8(const std::string& value);
std::string upper_ascii(std::string value);
const Json& find_formula(const Json& library, const std::string& code);
std::map<std::string, double> parse_parameters(
    const std::vector<std::string>& items);
int integer_option(Args& args, std::string_view name, int fallback,
                   int minimum, int maximum);

struct SecurityKey {
    std::string market;
    std::string code;
    std::string name;
};

bool has_finance_dependency(const Json& analysis);
Json analyze_selected_formula(const Json& formula);
SecurityKey parse_security(std::string value);
std::vector<SecurityKey> securities_from_document(const Json& document);
std::vector<Json> klines_from_document(const Json& document);

struct FetchOutcome {
    std::size_t index{};
    Json kline;
    std::string error;
};

std::vector<FetchOutcome> fetch_klines(
    const std::vector<SecurityKey>& securities,
    const std::string& period, int pages, int page_size,
    int timeout, int workers, const std::filesystem::path& cache_dir,
    bool refresh, const std::filesystem::path& root,
    const Json& analysis, const Json* formula_library,
    const std::map<std::string, double>& formula_parameters,
    bool point_in_time_finance,
    const std::string& adjustment_mode, const std::string& anchor_date,
    int adjustment_cache_ttl, bool refresh_adjustment);

struct FormulaScanInvocation {
    Json result;
    bool compact{};
    std::string output;
};

void print_scan_help();
void print_formula_watch_help();
std::string formula_watch_time_text();
std::string formula_watch_configuration_id(
    const std::vector<std::string>& scan_args, const Json& scan);
void require_distinct_formula_watch_paths(
    const std::vector<std::string>& scan_args,
    const std::string& output, const std::string& state_file,
    const std::string& block_output);
FormulaScanInvocation execute_formula_scan_once(
    const std::vector<std::string>& raw_args, bool force_refresh);

}  // namespace tdx::formula_scan_detail
