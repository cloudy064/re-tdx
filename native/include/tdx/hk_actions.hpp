#pragma once

#include "tdx/json.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tdx {

struct HkActionQuery {
    std::string code;
    std::string query;
    std::string kind{"all"};
    std::string date_from;
    std::string date_to;
    std::string order{"desc"};
    int offset{};
    int limit{200};
};

Json load_local_hk_actions(const std::filesystem::path& root,
                           const HkActionQuery& query);
bool is_hk_action_market(std::string_view market);
Json apply_hk_kline_adjustment(
    Json document,
    const std::filesystem::path& root,
    const std::string& code,
    const std::string& mode,
    const std::string& anchor_date = {});
Json build_hk_divfactor_series_document(
    const std::filesystem::path& root,
    const std::string& code,
    const Json& kline_document);

int command_market_hk_actions(const std::vector<std::string>& args);

}  // namespace tdx
