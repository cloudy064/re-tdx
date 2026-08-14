#pragma once

#include "tdx/json.hpp"

#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace tdx::formula_context_detail {

struct TradingBinding {
    std::string name;
    int id{};
    int field{};
    int type{};
};

struct OnePointBinding {
    std::string name;
    int id{};
    int field{};
    int year{};
    int mmdd{};
};

struct ProfessionalBoardTarget {
    std::string market;
    std::string code;
    std::string mode;
};

void bind_professional_series_context(
    Json& context,
    const Json& target,
    const std::string& market,
    const std::string& code,
    const std::vector<TradingBinding>& stock,
    const std::vector<TradingBinding>& board,
    const std::vector<TradingBinding>& aggregate,
    const std::set<int>& finance_bindings,
    const Json& finance_values,
    bool point_in_time_finance,
    int security_type,
    int timeout_ms);

void bind_professional_one_points(
    Json& context,
    const std::filesystem::path& root,
    const std::string& market,
    const std::string& code,
    const std::vector<OnePointBinding>& finance,
    const std::vector<OnePointBinding>& stock,
    const std::vector<OnePointBinding>& board,
    const std::vector<OnePointBinding>& aggregate,
    const std::vector<OnePointBinding>& local,
    int timeout_ms,
    const std::optional<ProfessionalBoardTarget>& board_target);

}  // namespace tdx::formula_context_detail
