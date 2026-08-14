#pragma once

#include "tdx/flow_followup.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace tdx::flow_followup_detail {

inline constexpr std::size_t placeholder_run_minimum = 20;

struct ViewAlias {
    std::string_view alias;
    std::string_view canonical;
};

inline constexpr std::array<ViewAlias, 15> view_aliases{{
    {"margin", "margin"},
    {"margin-financing", "margin"},
    {"northbound", "northbound"},
    {"stock-connect", "northbound"},
    {"financing", "financing-model"},
    {"financing-model", "financing-model"},
    {"margin-model", "financing-model"},
    {"lending", "lending-model"},
    {"lending-model", "lending-model"},
    {"securities-lending", "lending-model"},
    {"securities-lending-model", "lending-model"},
    {"northbound-inflow", "northbound-inflow-model"},
    {"northbound-inflow-model", "northbound-inflow-model"},
    {"northbound-purchase", "northbound-purchase-model"},
    {"northbound-purchase-model", "northbound-purchase-model"},
}};

struct FlowModelDefinition {
    const char* view;
    const char* title;
    const char* request_type;
    const char* bucket_request_id;
    const char* summary_request_id;
    const char* source_file;
    const char* module;
    const char* signal_kind;
    bool northbound;
    bool ten_day;
};

inline constexpr std::array<FlowModelDefinition, 4> model_definitions{{
    {"financing-model", "融资率分档与沪深300后续表现", "1", "200009",
     "200010", "rzrq_zsyc.xml", "mod_tciFetcher64.dll", "financing-rate",
     false, true},
    {"lending-model", "融券率分档与沪深300后续表现", "2", "200009",
     "200010", "rzrq_zsyc.xml", "mod_tciFetcher64.dll", "lending-rate",
     false, true},
    {"northbound-inflow-model", "北向净流入分档与沪深300后续表现", "1",
     "500501", "500502", "bxzj_zsyc3.xml", "mod_fuan.dll",
     "northbound-net-inflow", true, false},
    {"northbound-purchase-model", "北向净买入分档与沪深300后续表现", "2",
     "500501", "500502", "bxzj_zsyc3.xml", "mod_fuan.dll",
     "northbound-net-purchase", true, false},
}};

inline constexpr std::array<std::string_view, 6> available_view_names{{
    "margin", "northbound", "financing-model", "lending-model",
    "northbound-inflow-model", "northbound-purchase-model",
}};

std::filesystem::path native_path(const std::string& value);
std::string now_text();
std::string today_text();
bool digits(const std::string& value);
std::string compact_date(std::string value, std::string_view name);
std::string display_date(const std::string& value);
const Json* field(const Json& row, std::string_view key);
std::string text(const Json& row, std::string_view key);
std::optional<double> number(const Json& row, std::string_view key);
Json number_json(const std::optional<double>& value);
bool bool_value(const Json& row, std::string_view key, bool fallback = false);
std::string canonical_view(std::string value);
bool history_view(const std::string& view);
const FlowModelDefinition& model_definition(const std::string& view);
Json available_views();
int bounded(const std::string& raw, std::string_view name,
            int minimum, int maximum);
std::string cache_key(const FlowFollowupQuery& query);

void mark_placeholder_runs(Json& records);
Json horizon_statistics(const Json& records, const std::string& key);
bool transient_error(const std::string& message);
Json source_document(const Json& upstream, const Json& raw_rows,
                     const std::string& request_id);
Json tqlex_source_document(const Json& upstream, const std::string& request_id,
                           const std::string& module, std::size_t decoded_rows);

}  // namespace tdx::flow_followup_detail
