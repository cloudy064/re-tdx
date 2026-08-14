#pragma once

#include "tdx/json.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

struct IndexEventQuery {
    std::string benchmark{"all"};
    std::string event_id;
    std::string query;
    std::string date_from;
    std::string date_to;
    std::string date_basis{"chart"};
    std::string sort{"date"};
    std::string order{"desc"};
    int offset{0};
    int limit{500};
};

Json load_local_index_events(const std::filesystem::path& root,
                             const IndexEventQuery& query);

int command_market_index_events(const std::vector<std::string>& args);

}  // namespace tdx
