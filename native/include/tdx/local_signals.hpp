#pragma once

#include "tdx/json.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace tdx {

enum class LocalSignalNamespace {
    system,
    user,
};

struct LocalSignalCatalogEntry {
    LocalSignalNamespace signal_namespace{LocalSignalNamespace::system};
    int signal_id{};
    int kind{};
    int catalog_value{};
    std::string name;
    std::size_t source_index{};
};

struct LocalSignalCatalog {
    std::filesystem::path system_path;
    std::filesystem::path user_path;
    bool system_exists{};
    bool user_exists{};
    std::vector<LocalSignalCatalogEntry> entries;
};

struct LocalSignalPoint {
    int date{};
    float value{};
};

struct LocalSignalSelection {
    LocalSignalNamespace signal_namespace{LocalSignalNamespace::system};
    int signal_id{};
    int market_id{};
    std::string code;
    std::filesystem::path path;
    bool exists{};
    std::uint64_t source_record_count{};
    std::uint64_t matched_record_count{};
    bool host_limit_truncated{};
    int range_start_date{};
    int range_end_date{99991231};
    std::vector<LocalSignalPoint> points;
};

LocalSignalCatalog load_local_signal_catalog(
    const std::filesystem::path& tdx_root);

LocalSignalSelection load_local_signal_series(
    const std::filesystem::path& tdx_root,
    LocalSignalNamespace signal_namespace,
    int signal_id,
    int market_id,
    const std::string& code,
    std::size_t point_limit = 30000,
    int start_date = 0,
    int end_date = 99991231);

Json align_local_signal_to_kline(
    const Json& kline_document,
    const std::vector<LocalSignalPoint>& points,
    int mode);

Json local_signal_document(
    const std::filesystem::path& tdx_root,
    std::optional<LocalSignalNamespace> signal_namespace = std::nullopt,
    std::optional<int> signal_id = std::nullopt,
    std::optional<int> market_id = std::nullopt,
    const std::string& code = {},
    std::size_t point_limit = 30000,
    int start_date = 0,
    int end_date = 99991231);

int command_formulas_local_signals(const std::vector<std::string>& args);

}  // namespace tdx
