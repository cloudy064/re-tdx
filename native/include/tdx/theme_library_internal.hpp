#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"
#include "tdx/theme_library.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::theme_library_detail {

// Field access: client JSN objects use inconsistent key casing, so lookups
// fall back to a case-insensitive scan.
const Json* value_ptr(const Json& value, std::string_view key);
std::string text_value(const Json& value, std::string_view key);
std::optional<double> number_value(const Json& value, std::string_view key);
std::optional<std::uint64_t> unsigned_value(const Json& value,
                                            std::string_view key);

// Security identity
bool digits(const std::string& value, std::size_t size);
int market_id(std::string value);
std::string market_name(int id);
std::string market_prefix(int id);
Json security_document(
    int market, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);

// Membership parsing: "market|code" tokens, deduplicated while reporting the
// raw token count so declared/actual mismatches stay observable.
std::vector<std::pair<int, std::string>> parse_members(const std::string& value,
                                                       std::size_t* raw_count);

const Json& document_for_resource(const Json& documents,
                                  const std::string& resource);

// Free-text search over a theme summary, including object keys.
bool json_contains(const Json& value, const std::string& needle);

// Drops the heavyweight members/raw fields for list responses.
Json theme_summary(Json theme);

void sort_themes(Json& rows, const std::string& sort_value,
                 const std::string& order_value);

std::string now_text();
int bounded(const std::string& text, const std::string& name, int low, int high);
std::filesystem::path native_path(const std::string& value);

}  // namespace tdx::theme_library_detail
