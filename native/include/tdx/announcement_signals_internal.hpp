#pragma once

#include "tdx/announcement_signals.hpp"
#include "tdx/common.hpp"

#include <optional>
#include <string_view>

namespace tdx {
namespace detail {
namespace announcement_signals {

struct ResourceSpec {
    const char* view;
    const char* resource;
    const char* label;
    const char* fields;
};

// Defined here rather than in the catalog unit because the service unit indexes
// the table directly; `extern constexpr` is ill-formed for that, so the table is
// an inline definition shared by every translation unit that includes this.
inline constexpr ResourceSpec resources[] = {
    {"selected", "list/func_zxjx101_1.jsn", "公告精选",
     "security,date,title,pdf-url,direction,type,recent-3d/10d-return"},
    {"risks", "list/func_zxjx103_1.jsn", "风险提示公告",
     "security,date,title,pdf-url,bearish,type,recent-3d/10d-return"},
};

inline constexpr std::size_t resource_count =
    sizeof(resources) / sizeof(resources[0]);

const ResourceSpec* find_resource(std::string_view view);
Json catalog_rows();

const Json* value_ptr(const Json& value, std::string_view key);
std::string text_value(const Json& value, std::string_view key);
std::string text_value_ci(const Json& value, std::string_view key);
std::optional<double> number_value(const Json& value, std::string_view key);
Json number_json(const std::optional<double>& value);

bool digits(const std::string& value, std::size_t size);
std::string iso_date(const std::string& value);
std::string compact_date(std::string value, const std::string& name);

int market_id(std::string value);
std::string market_name(int id);
std::string market_prefix(int id);
Json security_document(int id, const std::string& code,
                       const std::map<std::pair<int, std::string>, Security>& securities);

std::pair<std::string, std::string> split_title_url(const std::string& value);
std::string normalized_direction(const std::string& raw);
std::string history_resource(int id, const std::string& code);

std::string json_text(const Json& value, std::string_view key);
std::optional<double> json_number(const Json& value, std::string_view key);
bool json_contains(const Json& value, const std::string& needle);

std::string now_text();
int bounded(const std::string& text, const std::string& name, int minimum, int maximum);

Json summarize(const Json& rows);

}  // namespace announcement_signals
}  // namespace detail
}  // namespace tdx
