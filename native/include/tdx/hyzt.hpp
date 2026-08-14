#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tdx {

struct IndustryValuationRecord {
    int market_id{};
    std::string code;
    std::string name;
    std::optional<double> pe;
    std::optional<double> pb_mrq;
    std::uint64_t member_count{};
};

struct IndustryValuationCatalog {
    std::filesystem::path source_path;
    std::uint64_t source_size{};
    std::uint64_t row_count{};
    std::map<std::string, IndustryValuationRecord> records;
};

// Load func_gx_hyzt101_1.jsn from a JSN root (or from the resource path
// itself).  The cache is invalidated by path, size and last-write time so a
// running server observes an atomic JSN refresh without reparsing every
// formula request.
std::shared_ptr<const IndustryValuationCatalog>
load_industry_valuation_catalog(const std::filesystem::path& jsn_root);

int command_hyzt_extract(const std::vector<std::string>& args);

}  // namespace tdx
