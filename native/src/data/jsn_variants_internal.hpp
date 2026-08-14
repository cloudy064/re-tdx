#pragma once

#include "tdx/jsn_variants.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::jsn_variant_detail {

struct Binding {
    std::string pattern;
    std::string command;
    std::string endpoint;
};

struct DownloadedResource {
    struct ColumnProfile {
        std::uint64_t seen{};
        std::uint64_t nonempty{};
        std::uint64_t numeric{};
        std::uint64_t integer{};
        std::uint64_t date_like{};
        std::uint64_t url_like{};
        std::uint64_t html_like{};
        std::set<std::string> samples;
    };

    std::string resource;
    std::uint64_t bytes{};
    std::uint64_t groups{};
    std::uint64_t rows{};
    std::set<std::string> columns;
    std::map<std::string, ColumnProfile> profiles;
    bool security_related{};
    std::string parse_error;
    std::string sha256;
};

struct DownloadAggregate {
    std::uint64_t files{};
    std::uint64_t bytes{};
    std::uint64_t groups{};
    std::uint64_t rows{};
    std::set<std::string> columns;
    bool security_related{};
    std::uint64_t parse_errors{};
};

struct UnitDefinition {
    std::string id;
    std::string file;
    std::vector<std::string> reference_ids;
    std::string source_file;
    std::string config_name;
    std::string relation_scope;
};

struct ConcreteTemplateMatch {
    const JsnResourceTemplate* value{};
    std::map<std::string, std::string> keys;
    std::size_t literal_score{};
};

std::filesystem::path from_utf8(const std::string& value);
std::string config_text(const std::filesystem::path& path);
std::vector<UnitDefinition> unit_definitions(const std::filesystem::path& root);
std::string normalize_resource(std::string value);
std::vector<std::string> placeholders(const std::string& resource);
const std::map<std::string, std::vector<std::string>>& detail_templates();

std::string relative_resource(
    const std::filesystem::path& path, const std::filesystem::path& root);
std::vector<ConcreteTemplateMatch> concrete_template_matches(
    const std::string& resource,
    const std::vector<JsnResourceTemplate>& templates);
Json column_profiles_json(
    const std::map<std::string, DownloadedResource::ColumnProfile>& profiles);
std::vector<DownloadedResource> scan_downloaded(
    const std::filesystem::path& root,
    bool fingerprints = false,
    bool profiles = false);
DownloadAggregate aggregate_downloads(
    const JsnResourceTemplate& resource,
    const std::vector<DownloadedResource>& downloaded,
    std::set<std::string>& matched_files);

std::vector<Binding> coverage_bindings();
bool binding_matches(const std::string& resource, const std::string& pattern);
Json strings(const std::vector<std::string>& values);
Json strings(const std::set<std::string>& values);
std::string family_name(const std::string& resource);
int gap_score(
    const JsnResourceTemplate& resource,
    const DownloadAggregate& downloaded);
Json download_json(const DownloadAggregate& value);
std::string timestamp_text();
std::string optional_string(const Json& value, std::string_view name);
std::uint64_t optional_uint(const Json& value, std::string_view name);
Json key_values_json(const std::map<std::string, std::string>& values);

}  // namespace tdx::jsn_variant_detail
