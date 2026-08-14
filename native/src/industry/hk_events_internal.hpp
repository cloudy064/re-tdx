#pragma once

#include "tdx/hk_events.hpp"
#include "tdx/common.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::detail {

struct HkEventResourceDefinition {
    std::string_view resource;
    std::string_view kind;
    std::string_view label;
    std::string_view view;
};

const std::array<HkEventResourceDefinition, 4>& hk_event_resources();
const HkEventResourceDefinition& find_hk_event_resource(
    std::string_view resource);
const HkEventResourceDefinition* find_hk_event_view(std::string_view view);
const std::vector<std::string>& hk_event_resource_paths();

const Json* json_field(const Json& row, std::string_view name);
std::string json_text(const Json& row, std::string_view name);
std::optional<double> json_number_value(const Json& row,
                                        std::string_view name);
Json json_number(const Json& row, std::string_view name);
Json json_scaled_number(const Json& row, std::string_view name, double scale);
int hk_market_id(const Json& row);
bool valid_hk_code(const std::string& code);
Json hk_security_document(const Json& row);
std::string hk_date_key(std::string value);
std::string current_time_text();
std::filesystem::path native_utf8_path(const std::string& value);
Json load_local_resource_rows(const std::filesystem::path& root,
                              const std::string& resource);
const Json& hk_document_for(const Json& documents,
                            std::string_view resource);
Json hk_source_summary(const Json& document);

}  // namespace tdx::detail
