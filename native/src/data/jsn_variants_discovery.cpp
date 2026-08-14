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

struct SnapshotFile {
    std::string resource;
    std::uint64_t bytes{};
    std::uint64_t rows{};
    std::string sha256;
    std::set<std::string> columns;
};

std::string optional_string(const Json& value, std::string_view name) {
    if (!value.is_object()) return {};
    const auto found = value.as_object().find(std::string(name));
    return found != value.as_object().end() && found->second.is_string()
        ? found->second.as_string() : std::string{};
}

std::uint64_t optional_uint(const Json& value, std::string_view name) {
    if (!value.is_object()) return 0;
    const auto found = value.as_object().find(std::string(name));
    return found != value.as_object().end() && found->second.is_number() &&
        found->second.as_number() >= 0
        ? static_cast<std::uint64_t>(found->second.as_number()) : 0;
}

std::set<std::string> optional_string_set(const Json& value, std::string_view name) {
    std::set<std::string> result;
    if (!value.is_object()) return result;
    const auto found = value.as_object().find(std::string(name));
    if (found == value.as_object().end() || !found->second.is_array()) return result;
    for (const auto& item : found->second.as_array())
        if (item.is_string()) result.insert(item.as_string());
    return result;
}

std::map<std::string, SnapshotFile> snapshot_files(const Json& source) {
    const Json* snapshot = &source;
    if (!source.is_object()) throw Error("invalid JSN discovery baseline: expected object");
    const auto nested = source.as_object().find("snapshot");
    if (nested != source.as_object().end()) snapshot = &nested->second;
    if (!snapshot->is_object()) throw Error("invalid JSN discovery baseline snapshot");
    const auto found = snapshot->as_object().find("files");
    if (found == snapshot->as_object().end() || !found->second.is_array())
        throw Error("invalid JSN discovery baseline: files array is missing");
    std::map<std::string, SnapshotFile> result;
    for (const auto& item : found->second.as_array()) {
        SnapshotFile file;
        file.resource = optional_string(item, "resource");
        if (file.resource.empty()) continue;
        file.bytes = optional_uint(item, "bytes");
        file.rows = optional_uint(item, "rows");
        file.sha256 = optional_string(item, "sha256");
        file.columns = optional_string_set(item, "columns");
        result[lower_ascii(file.resource)] = std::move(file);
    }
    return result;
}

Json snapshot_document(const fs::path& root,
                       const std::vector<DownloadedResource>& downloaded,
                       const std::string& captured_at) {
    Json files = Json::array();
    for (const auto& file : downloaded) {
        Json row = Json::object();
        row["resource"] = file.resource;
        row["bytes"] = file.bytes;
        row["rows"] = file.rows;
        row["sha256"] = file.sha256;
        row["columns"] = strings(file.columns);
        files.push_back(std::move(row));
    }
    Json result = Json::object();
    result["schema"] = "tdx-jsn-discovery-snapshot-v1";
    result["captured_at"] = captured_at;
    result["scan_root"] = path_utf8(root);
    result["files"] = std::move(files);
    return result;
}

std::set<std::string> set_difference_of(const std::set<std::string>& left,
                                        const std::set<std::string>& right) {
    std::set<std::string> result;
    std::set_difference(left.begin(), left.end(), right.begin(), right.end(),
                        std::inserter(result, result.end()));
    return result;
}

int discovery_score(const std::string& status, bool recognized, bool typed,
                    const DownloadedResource* file,
                    std::size_t new_column_count) {
    int score = 0;
    if (status == "added") score += 100;
    else if (status == "changed") score += 60;
    else if (status == "removed") score += 45;
    else if (status == "unbaselined") score += 25;
    if (!recognized) score += 80;
    if (!typed) score += 60;
    if (file && file->rows) score += 20;
    if (file && file->security_related) score += 30;
    score += static_cast<int>(std::min<std::size_t>(new_column_count * 10, 50));
    if (file && !file->parse_error.empty()) score -= 50;
    return score;
}

Json key_values_json(const std::map<std::string, std::string>& values) {
    Json result = Json::object();
    for (const auto& [key, value] : values) result[key] = value;
    return result;
}

}  // namespace tdx::jsn_variant_detail

namespace tdx {

using namespace jsn_variant_detail;

Json jsn_discovery_document(const fs::path& root,
                            const fs::path& downloaded_root,
                            const fs::path& baseline_path,
                            bool capture_baseline,
                            int top) {
    if (top < 0 || top > 1000) throw Error("top must be in 0..1000");
    const auto generated_at = timestamp_text();
    const auto templates = inventory_jsn_resource_templates(root);
    const auto bindings = coverage_bindings();
    const auto downloaded = scan_downloaded(downloaded_root, true, true);

    bool baseline_available = false;
    std::string baseline_captured_at;
    std::map<std::string, SnapshotFile> baseline_files;
    if (!baseline_path.empty() && fs::is_regular_file(baseline_path)) {
        const auto baseline_document = Json::parse(read_text_utf8(baseline_path));
        baseline_files = snapshot_files(baseline_document);
        baseline_available = true;
        const Json* snapshot = &baseline_document;
        const auto nested = baseline_document.as_object().find("snapshot");
        if (nested != baseline_document.as_object().end()) snapshot = &nested->second;
        baseline_captured_at = optional_string(*snapshot, "captured_at");
    }

    Json current_snapshot = snapshot_document(downloaded_root, downloaded, generated_at);
    std::map<std::string, const DownloadedResource*> current_files;
    for (const auto& file : downloaded) current_files[lower_ascii(file.resource)] = &file;

    std::map<std::string, std::uint64_t> statuses;
    std::uint64_t recognized_files = 0, typed_files = 0, generic_only_files = 0;
    std::uint64_t security_files = 0;
    std::uint64_t parse_error_files = 0, new_column_count = 0, removed_column_count = 0;
    Json files = Json::array();
    Json changes = Json::array();
    struct Ranked { int score{}; Json row; };
    std::vector<Ranked> ranked;

    auto describe = [&](const SnapshotFile* previous,
                        const DownloadedResource* current,
                        const std::string& status) {
        const auto& resource = current ? current->resource : previous->resource;
        const auto matches = concrete_template_matches(resource, templates);
        std::set<std::string> commands, endpoints;
        for (const auto& binding : bindings) {
            bool matches_binding = binding_matches(resource, binding.pattern);
            for (const auto& match : matches)
                matches_binding = matches_binding ||
                    binding_matches(match.value->resource, binding.pattern);
            if (!matches_binding) continue;
            commands.insert(binding.command);
            endpoints.insert(binding.endpoint);
        }
        const bool recognized = !matches.empty();
        const bool typed = !commands.empty();
        std::set<std::string> current_columns = current ? current->columns
                                                        : std::set<std::string>{};
        std::set<std::string> previous_columns = previous ? previous->columns
                                                          : std::set<std::string>{};
        const auto added_columns = set_difference_of(current_columns, previous_columns);
        const auto removed_columns = set_difference_of(previous_columns, current_columns);
        const int priority = discovery_score(
            status, recognized, typed, current, added_columns.size());

        Json matched_templates = Json::array();
        for (const auto& match : matches) matched_templates.push_back(match.value->resource);
        Json row = Json::object();
        row["resource"] = resource;
        row["status"] = status;
        row["family"] = family_name(resource);
        row["template"] = recognized ? Json(matches.front().value->resource) : Json(nullptr);
        row["matched_templates"] = std::move(matched_templates);
        row["key_values"] = recognized
            ? key_values_json(matches.front().keys) : Json::object();
        row["coverage"] = typed ? "typed-command"
            : recognized ? "generic-only" : "unrecognized";
        row["typed_commands"] = strings(commands);
        row["api_endpoints"] = strings(endpoints);
        row["bytes"] = current ? Json(current->bytes) : Json(previous->bytes);
        row["sha256"] = current ? Json(current->sha256) : Json(previous->sha256);
        row["groups"] = current ? Json(current->groups) : Json(nullptr);
        row["rows"] = current ? Json(current->rows) : Json(previous->rows);
        row["columns"] = strings(current_columns);
        row["column_profiles"] = current
            ? column_profiles_json(current->profiles) : Json::object();
        row["new_columns"] = strings(added_columns);
        row["removed_columns"] = strings(removed_columns);
        row["security_related"] = current ? current->security_related : false;
        row["parse_error"] = current && !current->parse_error.empty()
            ? Json(current->parse_error) : Json(nullptr);
        row["priority_score"] = priority;
        return row;
    };

    for (const auto& file : downloaded) {
        const auto found = baseline_files.find(lower_ascii(file.resource));
        const SnapshotFile* previous = found == baseline_files.end() ? nullptr : &found->second;
        std::string status;
        if (!baseline_available) status = "unbaselined";
        else if (!previous) status = "added";
        else {
            const bool identical = !file.sha256.empty() && !previous->sha256.empty()
                ? file.sha256 == previous->sha256
                : file.bytes == previous->bytes && file.rows == previous->rows &&
                    file.columns == previous->columns;
            status = identical ? "unchanged" : "changed";
        }
        auto row = describe(previous, &file, status);
        ++statuses[status];
        if (!row.at("template").is_null()) ++recognized_files;
        if (row.at("coverage").as_string() == "typed-command") ++typed_files;
        if (row.at("coverage").as_string() == "generic-only") ++generic_only_files;
        if (file.security_related) ++security_files;
        if (!file.parse_error.empty()) ++parse_error_files;
        new_column_count += row.at("new_columns").size();
        removed_column_count += row.at("removed_columns").size();
        files.push_back(row);
        if (status != "unchanged") {
            changes.push_back(row);
            ranked.push_back({static_cast<int>(row.at("priority_score").as_number()), row});
        }
    }
    if (baseline_available) {
        for (const auto& [key, previous] : baseline_files) {
            if (current_files.find(key) != current_files.end()) continue;
            auto row = describe(&previous, nullptr, "removed");
            ++statuses["removed"];
            removed_column_count += row.at("removed_columns").size();
            files.push_back(row);
            changes.push_back(row);
            ranked.push_back({static_cast<int>(row.at("priority_score").as_number()), row});
        }
    }
    std::sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
        if (left.score != right.score) return left.score > right.score;
        return lower_ascii(left.row.at("resource").as_string()) <
               lower_ascii(right.row.at("resource").as_string());
    });
    Json priority_changes = Json::array();
    for (std::size_t index = 0;
         index < ranked.size() && index < static_cast<std::size_t>(top); ++index)
        priority_changes.push_back(ranked[index].row);

    if (capture_baseline) {
        if (baseline_path.empty()) throw Error("baseline path is required for capture");
        atomic_write_text(baseline_path, current_snapshot.dump(2) + "\n");
    }

    Json baseline = Json::object();
    baseline["path"] = baseline_path.empty() ? Json(nullptr) : Json(path_utf8(baseline_path));
    baseline["available"] = baseline_available;
    baseline["captured_at"] = baseline_captured_at.empty()
        ? Json(nullptr) : Json(baseline_captured_at);
    baseline["file_count"] = static_cast<std::uint64_t>(baseline_files.size());
    baseline["captured_now"] = capture_baseline;

    Json summary = Json::object();
    summary["baseline_available"] = baseline_available;
    summary["current_file_count"] = static_cast<std::uint64_t>(downloaded.size());
    summary["baseline_file_count"] = static_cast<std::uint64_t>(baseline_files.size());
    summary["added_file_count"] = statuses["added"];
    summary["changed_file_count"] = statuses["changed"];
    summary["removed_file_count"] = statuses["removed"];
    summary["unchanged_file_count"] = statuses["unchanged"];
    summary["unbaselined_file_count"] = statuses["unbaselined"];
    summary["recognized_file_count"] = recognized_files;
    summary["unrecognized_file_count"] =
        static_cast<std::uint64_t>(downloaded.size()) - recognized_files;
    summary["typed_file_count"] = typed_files;
    summary["generic_only_file_count"] = generic_only_files;
    summary["security_related_file_count"] = security_files;
    summary["parse_error_file_count"] = parse_error_files;
    summary["new_column_count"] = new_column_count;
    summary["removed_column_count"] = removed_column_count;

    Json result = Json::object();
    result["schema"] = "tdx-jsn-discovery-native-v1";
    result["generated_at"] = generated_at;
    result["scan_root"] = path_utf8(downloaded_root);
    result["field_profile_sample_limit_per_column"] = 64;
    result["baseline"] = std::move(baseline);
    result["summary"] = std::move(summary);
    result["changes"] = std::move(changes);
    result["priority_changes"] = std::move(priority_changes);
    result["files"] = std::move(files);
    result["snapshot"] = std::move(current_snapshot);
    return result;
}

}  // namespace tdx
