#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace tdx {
namespace detail {
namespace curated_data {

// Support functions
const Json* field(const Json& row, std::string_view name);
std::string text_value(const Json& row, std::string_view name);
std::optional<double> number_value(const Json& row, std::string_view name);
Json number(const Json& row, std::string_view name);
Json scaled(const Json& row, std::string_view name, double scale);
bool digits(const std::string& value, std::size_t count);
int source_market(const Json& row);
int parsed_market(std::string value);
std::string market_name(int market);
std::string market_prefix(int market);
std::string excerpt(const std::string& value, std::size_t limit = 240);
std::string now_text();
int bounded(const std::string& value, std::string_view name, int low, int high);

// Resource catalog
struct ResourceEntry {
    const char* path;
    const char* kind;
    const char* label;
};

extern const ResourceEntry resources[];
extern const std::size_t resource_count;

std::string kind_for(const std::string& resource);
std::string label_for(const std::string& kind);

// Security resolution
Json security_document(
    int market, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities,
    const std::map<std::string, std::string>& hong_kong_names);

std::map<std::string, std::string> load_hong_kong_names(
    const std::filesystem::path& root);

// Resource loading
Json load_local_resource_rows(
    const std::filesystem::path& root,
    const std::string& resource);

const Json& document_for(const Json& documents, std::string_view resource);

Json source_summary(const Json& document, std::size_t normalized_rows);

}  // namespace curated_data
}  // namespace detail
}  // namespace tdx
