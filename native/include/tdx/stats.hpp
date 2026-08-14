#pragma once

#include "tdx/blocks.hpp"
#include "tdx/common.hpp"
#include "tdx/json.hpp"
#include "tdx/transport.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tdx {

struct TdxStatRow {
    int market_id{};
    std::string code;
    std::optional<std::string> stats_date;
    std::optional<double> beta_60d;
    std::optional<double> pe_ttm;
    std::optional<double> pe_static;
    std::optional<double> free_float_shares_10k;
    // tdxstat.cfg column 23. TdxW copies the integer verbatim to its
    // type-0xA3 record at offset 140; TCalc splits the six decimal digits
    // into SHAPE_SHORT/MID/LONG (two digits apiece).
    std::optional<int> shape_packed;
    std::optional<int> shape_short;
    std::optional<int> shape_mid;
    std::optional<int> shape_long;
    std::optional<int> year_limit_up_days;
    std::optional<int> limit_stat_days;
    std::optional<int> limit_up_count_in_stat_days;
    std::optional<int> limit_up_streak_days;
};

struct TdxStat2Row {
    int market_id{};
    std::string code;
    std::optional<std::string> stats_date;
    std::optional<double> amount_10k;
    std::optional<double> seal_amount_10k;
    std::optional<double> prev_amount_10k;
    std::optional<double> prev_seal_amount_10k;
    std::optional<double> prev2_amount_10k;
    std::optional<double> prev2_seal_amount_10k;
    std::optional<double> open_volume_hand;
    std::optional<double> prev_open_volume_hand;
    std::optional<double> open_amount_10k;
    std::optional<double> prev_open_amount_10k;
};

// Fields 18..22 of hq_cache/tipinfo.dat are copied by TdxW into the
// type-0xA3 security record. TCalc uses them for FINANCE(88..91).
struct TdxTipInfoEventRow {
    int market_id{};
    std::string code;
    std::optional<std::string> northbound_date;
    std::optional<double> northbound_direction;
    std::optional<std::string> repurchase_plan_date;
    std::optional<double> repurchase_ratio;
    std::optional<std::string> incentive_plan_date;
};

struct TdxStatsResource {
    std::map<std::pair<int, std::string>, TdxStatRow> stat;
    std::map<std::pair<int, std::string>, TdxStat2Row> stat2;
    std::map<std::pair<int, std::string>, TdxTipInfoEventRow> tip_info_events;
    std::string source_path;
    std::optional<std::string> stats_date;
    double stats_date_coverage{};
};

struct StatsDownloadResult {
    TdxStatsResource resource;
    std::size_t archive_size{};
    Endpoint endpoint;
    std::string server_name;
    std::vector<std::string> failures;
    Json transport;
};

Bytes build_stats_file_request_data(std::string_view path,
                                    std::uint32_t offset = 0,
                                    std::uint32_t size = 30000);
Bytes parse_stats_file_chunk(const Bytes& payload, std::uint32_t request_size);
TdxStatsResource parse_stats_files(const Bytes& stat_payload,
                                   const Bytes& stat2_payload,
                                   std::string source_path,
                                   const Bytes* tip_info_payload = nullptr);
TdxStatsResource parse_stats_archive(const Bytes& payload,
                                     std::string source_path = "tdx://zhb.zip");
TdxStatsResource load_local_stats(const std::filesystem::path& directory);
StatsDownloadResult download_stats_resource(
    const std::vector<std::string>& hosts = {}, std::string path = "zhb.zip",
    std::uint32_t chunk_size = 30000, int timeout_ms = 10000,
    const std::filesystem::path& root = {});
Json stats_resource_document(const TdxStatsResource& resource,
                             const std::string& endpoint,
                             const std::string& server_name,
                             std::size_t archive_size,
                             const std::vector<std::string>& securities = {},
                             const BlockData* block_data = nullptr,
                             const Json* transport = nullptr);
Json derive_security_valuation_document(int market_id,
                                        const std::string& code,
                                        const TdxStatRow* stat,
                                        const Json* snapshot_document,
                                        const Json* finance_document,
                                        const std::vector<std::string>& errors = {});
Json fetch_security_valuation_document(const std::filesystem::path& root,
                                       const TdxStatsResource& resource,
                                       int market_id,
                                       const std::string& code,
                                       int timeout_ms = 10000,
                                       const BlockData* block_data = nullptr);
Json fetch_market_stats_document(const std::filesystem::path& root,
                                 const std::vector<std::string>& securities = {},
                                 std::string path = "zhb.zip",
                                 std::uint32_t chunk_size = 30000,
                                 int timeout_ms = 10000,
                                 const BlockData* block_data = nullptr,
                                 const std::vector<std::string>& hosts = {});
int command_market_stats(const std::vector<std::string>& args);

}  // namespace tdx
