#pragma once

#include "tdx/json.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace tdx {

struct ExternalSignalRecord {
    bool system_namespace{};
    int market_id{};
    std::string code;
    int numeric_code{};
    int external_id{};
    std::string text;
    float value{};
    std::size_t source_line{};
};

struct ExternalSignalCatalog {
    std::filesystem::path user_path;
    std::filesystem::path system_path;
    bool user_exists{};
    bool system_exists{};
    std::vector<ExternalSignalRecord> records;
};

ExternalSignalCatalog load_external_signal_catalog(
    const std::filesystem::path& tdx_root);

const ExternalSignalRecord* find_external_signal(
    const ExternalSignalCatalog& catalog,
    bool system_namespace,
    int market_id,
    const std::string& code,
    int external_id);

Json external_signal_catalog_document(
    const std::filesystem::path& tdx_root,
    std::optional<int> market_id = std::nullopt,
    const std::string& code = {},
    std::optional<bool> system_namespace = std::nullopt,
    std::optional<int> external_id = std::nullopt);

int command_formulas_external_signals(const std::vector<std::string>& args);

}  // namespace tdx
