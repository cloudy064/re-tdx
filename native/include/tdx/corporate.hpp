#pragma once

#include "tdx/common.hpp"
#include "tdx/json.hpp"
#include "tdx/transport.hpp"

#include <optional>
#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

Json parse_finance_batch_payload(const Bytes& payload, bool include_raw = false);
Json parse_capital_changes_payload(const Bytes& payload, bool include_raw = false);
Json parse_local_gbbq_capital_changes(
    const Bytes& data,
    const std::vector<std::string>& securities,
    bool include_raw = false,
    const std::filesystem::path& source = {});
Json load_local_capital_changes_document(
    const std::filesystem::path& root,
    const std::vector<std::string>& securities,
    bool include_raw = false,
    const std::filesystem::path& override_path = {});
Json parse_special_limits_payload(const Bytes& payload, int start_index = 0,
                                  bool include_raw = false);

struct SpecialLimitPrice {
    double upper{};
    double lower{};
};

std::optional<SpecialLimitPrice> find_special_limit_price(
    const Json& document, const std::string& market, const std::string& code);
Json apply_kline_adjustment(Json document, const Json& daily_document,
                            const Json& capital_document,
                            const std::string& mode,
                            const std::string& anchor_date = {});
std::string normalize_kline_adjustment_mode(const std::string& mode);
Json adjust_security_kline_document(
    Json document,
    const std::string& market,
    const std::string& code,
    const std::string& kind,
    const std::string& mode,
    const std::string& anchor_date = {},
    const std::filesystem::path& root = {},
    const std::vector<std::string>& hosts = {},
    int timeout_ms = 10000,
    int cache_ttl_seconds = 900,
    bool refresh_cache = false);
Json kline_adjustment_cache_document();
Json summarize_kline_adjustments(const std::vector<Json>& documents,
                                 const std::string& mode);
std::string uniform_kline_adjustment_mode(
    const std::vector<Json>& documents,
    const std::string& fallback_mode = "none");

Json fetch_finance_document(const std::vector<std::string>& securities,
                            const std::vector<Endpoint>& endpoints = {},
                            int timeout_ms = 10000, int batch_size = 80,
                            bool include_raw = false);
Json fetch_capital_changes_document(const std::vector<std::string>& securities,
                                    const std::vector<Endpoint>& endpoints = {},
                                    int timeout_ms = 10000,
                                    bool include_raw = false);
Json fetch_special_limits_document(const std::vector<Endpoint>& endpoints = {},
                                   int timeout_ms = 10000, int start_index = 0,
                                   int max_rows = 10000,
                                   bool include_raw = false);

int command_market_finance(const std::vector<std::string>& args);
int command_market_capital(const std::vector<std::string>& args);
int command_market_limits(const std::vector<std::string>& args);
int command_market_kline(const std::vector<std::string>& args);

}  // namespace tdx
