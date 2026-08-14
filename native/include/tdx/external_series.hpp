#pragma once

#include "tdx/json.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace tdx {

struct ExternalSeriesIndexRecord {
    int market_id{};
    std::string code;
    int point_count{};
    std::uint64_t start_point{};
    std::size_t source_index{};
};

struct ExternalSeriesPoint {
    int date{};
    int time{};
    float value{};
};

struct ExternalSeriesSelection {
    int dataset_id{};
    std::filesystem::path index_path;
    std::filesystem::path data_path;
    bool index_exists{};
    bool data_exists{};
    std::vector<ExternalSeriesIndexRecord> index_records;
    bool security_found{};
    std::optional<ExternalSeriesIndexRecord> selected;
    std::vector<ExternalSeriesPoint> points;
    int range_start_date{};
    int range_end_date{99991231};
    std::uint64_t range_matched_point_count{};
    bool host_limit_truncated{};
};

ExternalSeriesSelection load_external_series(
    const std::filesystem::path& tdx_root,
    int dataset_id,
    std::optional<int> market_id = std::nullopt,
    const std::string& code = {},
    std::size_t point_limit = 30000,
    int start_date = 0,
    int end_date = 99991231);

Json align_external_series_to_kline(
    const Json& kline_document,
    const std::vector<ExternalSeriesPoint>& points,
    int mode);

Json external_series_document(
    const std::filesystem::path& tdx_root,
    int dataset_id,
    std::optional<int> market_id = std::nullopt,
    const std::string& code = {},
    std::size_t point_limit = 30000,
    int start_date = 0,
    int end_date = 99991231);

int command_formulas_external_series(const std::vector<std::string>& args);

}  // namespace tdx
