#pragma once

#include "tdx/json.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace tdx {

struct ShapeMatchQuery {
    std::filesystem::path library_path;
    std::filesystem::path candidate_path;
    std::filesystem::path scan_input_path;
    std::string view{"library"};
    std::string template_name;
    std::optional<std::size_t> template_index;
    std::string market;
    std::string code;
    std::string source{"auto"};
    std::string period;
    std::string kind{"auto"};
    std::vector<std::string> securities;
    std::string block;
    bool all_securities{};
    bool include_unmatched{};
    int workers{4};
    int max_candidates{10000};
    int max_network_requests{};
    int result_limit{1000};
    int timeout_ms{10000};
};

float shape_series_similarity(const std::vector<float>& reference,
                              const std::vector<float>& candidate,
                              std::size_t window);

Json load_shape_match(const std::filesystem::path& root,
                      const ShapeMatchQuery& query);

int command_market_shape_match(const std::vector<std::string>& args);

}  // namespace tdx
