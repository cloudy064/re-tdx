#include "jsn_variants_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/tqlex.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx::jsn_variant_detail {

bool binding_matches(const std::string& resource, const std::string& pattern) {
    const auto value = lower_ascii(resource);
    const auto match = lower_ascii(pattern);
    if (!match.empty() && match.back() == '*')
        return value.rfind(match.substr(0, match.size() - 1), 0) == 0;
    return value == match;
}

Json strings(const std::vector<std::string>& values) {
    Json result = Json::array();
    for (const auto& value : values) result.push_back(value);
    return result;
}

Json strings(const std::set<std::string>& values) {
    Json result = Json::array();
    for (const auto& value : values) result.push_back(value);
    return result;
}

std::string family_name(const std::string& resource) {
    auto value = lower_ascii(resource);
    const auto slash = value.find('/');
    if (slash != std::string::npos && value.rfind("list/", 0) != 0)
        return value.substr(0, slash);
    if (value.rfind("list/", 0) == 0) value.erase(0, 5);
    if (value.rfind("func_", 0) == 0) value.erase(0, 5);
    if (value.size() > 4 && value.substr(value.size() - 4) == ".jsn")
        value.resize(value.size() - 4);
    const auto digit = std::find_if(value.begin(), value.end(), [](char ch) {
        return std::isdigit(static_cast<unsigned char>(ch));
    });
    if (digit != value.end()) value.erase(digit, value.end());
    while (!value.empty() && (value.back() == '_' || value.back() == '-')) value.pop_back();
    return value.empty() ? "other" : value;
}

int gap_score(const JsnResourceTemplate& resource, const DownloadAggregate& downloaded) {
    int score = resource.placeholders.empty() ? 20 : 10;
    if (downloaded.files) score += 100;
    if (downloaded.rows) score += 20;
    if (downloaded.rows >= 100) score += 10;
    if (downloaded.rows >= 1000) score += 10;
    if (downloaded.security_related) score += 40;
    if (resource.source_files.size() > 1) score += 5;
    if (downloaded.parse_errors) score -= 50;
    return score;
}

Json download_json(const DownloadAggregate& value) {
    Json result = Json::object();
    result["matched_files"] = value.files;
    result["bytes"] = value.bytes;
    result["groups"] = value.groups;
    result["rows"] = value.rows;
    result["columns"] = strings(value.columns);
    result["security_related"] = value.security_related;
    result["parse_error_count"] = value.parse_errors;
    return result;
}

std::string timestamp_text() {
    const auto now = std::chrono::system_clock::now();
    const auto value = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &value);
#else
    gmtime_r(&value, &utc);
#endif
    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

}  // namespace tdx::jsn_variant_detail

namespace tdx {

using namespace jsn_variant_detail;

Json jsn_variant_coverage_document(const std::vector<JsnResourceTemplate>& templates,
                                   const fs::path& downloaded_root,
                                   bool gaps_only,
                                   int top) {
    if (top < 0 || top > 1000) throw Error("top must be in 0..1000");
    const auto bindings = coverage_bindings();
    const auto downloaded = scan_downloaded(downloaded_root);
    std::set<std::string> matched_files;
    std::size_t typed = 0, generic = 0, dynamic = 0, downloaded_templates = 0;
    std::size_t generic_downloaded = 0, parse_error_templates = 0;
    std::map<std::string, std::size_t> by_command;
    struct Family {
        std::uint64_t templates{};
        std::uint64_t downloaded_templates{};
        std::uint64_t files{};
        std::uint64_t rows{};
        std::uint64_t bytes{};
        std::uint64_t security_templates{};
        std::vector<std::string> samples;
    };
    std::map<std::string, Family> gap_families;
    std::set<std::string> xml_sources, cfg_sources;
    std::size_t xml_backed = 0, cfg_backed = 0;
    struct Gap { int score{}; Json row; };
    std::vector<Gap> ranked_gaps;
    Json records = Json::array();
    for (const auto& resource : templates) {
        bool has_xml = false, has_cfg = false;
        for (const auto& source : resource.source_files) {
            const auto extension = lower_ascii(fs::path(source).extension().string());
            if (extension == ".xml") { has_xml = true; xml_sources.insert(lower_ascii(source)); }
            if (extension == ".cfg") { has_cfg = true; cfg_sources.insert(lower_ascii(source)); }
        }
        if (has_xml) ++xml_backed;
        if (has_cfg) ++cfg_backed;
        std::set<std::string> commands, endpoints;
        for (const auto& binding : bindings) {
            if (!binding_matches(resource.resource, binding.pattern)) continue;
            commands.insert(binding.command);
            endpoints.insert(binding.endpoint);
        }
        const auto local = aggregate_downloads(resource, downloaded, matched_files);
        const bool fixed = !commands.empty();
        if (fixed) {
            ++typed;
            for (const auto& command : commands) ++by_command[command];
        } else ++generic;
        if (!resource.placeholders.empty()) ++dynamic;
        if (local.files) ++downloaded_templates;
        if (!fixed && local.files) ++generic_downloaded;
        if (local.parse_errors) ++parse_error_templates;

        Json row = Json::object();
        row["resource"] = resource.resource;
        row["family"] = family_name(resource.resource);
        row["source_files"] = strings(resource.source_files);
        row["source_occurrences"] = static_cast<std::uint64_t>(resource.source_files.size());
        row["placeholders"] = strings(resource.placeholders);
        row["dynamic"] = !resource.placeholders.empty();
        row["selection_key"] = resource.placeholders.empty()
            ? Json("static-resource") : Json("master-row placeholders");
        row["coverage"] = fixed ? "typed-command" : "generic-only";
        row["typed_commands"] = strings(commands);
        row["api_endpoints"] = strings(endpoints);
        row["downloaded"] = download_json(local);
        row["gap_score"] = fixed ? Json(nullptr) : Json(gap_score(resource, local));
        if (!gaps_only || !fixed) records.push_back(row);
        if (!fixed) {
            ranked_gaps.push_back({gap_score(resource, local), row});
            auto& family = gap_families[family_name(resource.resource)];
            ++family.templates;
            if (local.files) ++family.downloaded_templates;
            family.files += local.files;
            family.rows += local.rows;
            family.bytes += local.bytes;
            if (local.security_related) ++family.security_templates;
            if (family.samples.size() < 5) family.samples.push_back(resource.resource);
        }
    }
    std::sort(ranked_gaps.begin(), ranked_gaps.end(), [](const Gap& left, const Gap& right) {
        if (left.score != right.score) return left.score > right.score;
        return lower_ascii(left.row.at("resource").as_string()) <
               lower_ascii(right.row.at("resource").as_string());
    });
    Json high_value = Json::array();
    for (std::size_t index = 0;
         index < ranked_gaps.size() && index < static_cast<std::size_t>(top); ++index)
        high_value.push_back(ranked_gaps[index].row);

    Json commands = Json::array();
    for (const auto& [command, count] : by_command) {
        Json row = Json::object();
        row["command"] = command;
        row["resource_templates"] = static_cast<std::uint64_t>(count);
        commands.push_back(std::move(row));
    }
    std::vector<std::pair<std::string, Family>> sorted_families(
        gap_families.begin(), gap_families.end());
    std::sort(sorted_families.begin(), sorted_families.end(), [](const auto& left, const auto& right) {
        if (left.second.downloaded_templates != right.second.downloaded_templates)
            return left.second.downloaded_templates > right.second.downloaded_templates;
        if (left.second.rows != right.second.rows) return left.second.rows > right.second.rows;
        if (left.second.templates != right.second.templates)
            return left.second.templates > right.second.templates;
        return lower_ascii(left.first) < lower_ascii(right.first);
    });
    Json families = Json::array();
    for (const auto& [name, value] : sorted_families) {
        Json row = Json::object();
        row["family"] = name;
        row["resource_templates"] = value.templates;
        row["downloaded_templates"] = value.downloaded_templates;
        row["matched_files"] = value.files;
        row["rows"] = value.rows;
        row["bytes"] = value.bytes;
        row["security_related_templates"] = value.security_templates;
        row["samples"] = strings(value.samples);
        families.push_back(std::move(row));
    }
    Json summary = Json::object();
    summary["resource_template_count"] = static_cast<std::uint64_t>(templates.size());
    summary["xml_source_file_count"] = static_cast<std::uint64_t>(xml_sources.size());
    summary["cfg_source_file_count"] = static_cast<std::uint64_t>(cfg_sources.size());
    summary["xml_backed_template_count"] = static_cast<std::uint64_t>(xml_backed);
    summary["cfg_backed_template_count"] = static_cast<std::uint64_t>(cfg_backed);
    summary["static_template_count"] = static_cast<std::uint64_t>(templates.size() - dynamic);
    summary["dynamic_template_count"] = static_cast<std::uint64_t>(dynamic);
    summary["typed_template_count"] = static_cast<std::uint64_t>(typed);
    summary["generic_only_template_count"] = static_cast<std::uint64_t>(generic);
    summary["downloaded_file_count"] = static_cast<std::uint64_t>(downloaded.size());
    summary["matched_downloaded_file_count"] = static_cast<std::uint64_t>(matched_files.size());
    summary["downloaded_template_count"] = static_cast<std::uint64_t>(downloaded_templates);
    summary["generic_downloaded_template_count"] = static_cast<std::uint64_t>(generic_downloaded);
    summary["parse_error_template_count"] = static_cast<std::uint64_t>(parse_error_templates);
    summary["fully_fixed"] = generic == 0 && parse_error_templates == 0;

    Json excluded = Json::array();
    Json chart = Json::object();
    chart["resource"] = "list/func_hsgt10$$unitid$$.jsn";
    chart["reason"] = "four chart-only UNITID expansions return zero-length metadata and are not downloadable business resources";
    excluded.push_back(std::move(chart));

    Json result = Json::object();
    result["schema"] = "tdx-jsn-variant-coverage-native-v1";
    result["gaps_only"] = gaps_only;
    result["downloaded_root"] = downloaded_root.empty() ? Json(nullptr) : Json(path_utf8(downloaded_root));
    result["semantics"] = "A resource template is fixed only when a typed C++ business command owns its static or master-detail path. Generic jsn download/catalog/query support alone does not count as typed coverage.";
    result["summary"] = std::move(summary);
    result["commands"] = std::move(commands);
    result["gap_families"] = std::move(families);
    result["excluded"] = std::move(excluded);
    result["high_value_gaps"] = std::move(high_value);
    result["resources"] = std::move(records);
    return result;
}

Json jsn_variant_coverage_document(const fs::path& root,
                                   const fs::path& downloaded_root,
                                   bool gaps_only,
                                   int top) {
    return jsn_variant_coverage_document(
        inventory_jsn_resource_templates(root), downloaded_root, gaps_only, top);
}

}  // namespace tdx
