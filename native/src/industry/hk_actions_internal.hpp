#pragma once

#include "tdx/common.hpp"
#include "tdx/hk_actions.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace tdx::hk_actions_detail {

struct ActionRecord {
    std::string code;
    std::string date;
    std::string description;
    double cumulative_multiplier{};
    double cumulative_offset{};
    double previous_multiplier{1.0};
    double previous_offset{};
    std::string source_file;
};

std::shared_ptr<const std::vector<ActionRecord>> load_records(
    const std::filesystem::path& root);
std::vector<std::string> classify_flags(const std::string& description);
std::string primary_kind(const std::vector<std::string>& flags,
                         const ActionRecord& record);
Json record_json(const ActionRecord& record);

}  // namespace tdx::hk_actions_detail
