#include "server_pool_internal.hpp"

#include "server_core_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/tpool.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx::server_detail {
namespace {

bool valid_history_date(int value) {
    const int year = value / 10000;
    const int month = value / 100 % 100;
    const int day = value % 100;
    if (year < 1000 || month < 1 || month > 12 || day < 1) return false;
    static constexpr int days_per_month[]{
        0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int maximum = days_per_month[month];
    if (month == 2 &&
        (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)))
        maximum = 29;
    return day <= maximum;
}

int history_date(const RequestTarget& target, const std::string& name) {
    const auto value = trim(query_value(target, name));
    if (value.empty()) return 0;
    if (value.size() != 8 ||
        !std::all_of(value.begin(), value.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        }))
        throw Error("TPool history " + name + " date must be YYYYMMDD");
    const int result = parse_bounded(value, name, 10000101, 99991231);
    if (!valid_history_date(result))
        throw Error("TPool history " + name + " date must be YYYYMMDD");
    return result;
}

std::string history_directory_filter(const RequestTarget& target,
                                     const std::string& name) {
    auto value = trim(query_value(target, name));
    if (value.size() > 255 || value == "." || value == ".." ||
        value.find('\0') != std::string::npos ||
        value.find('/') != std::string::npos ||
        value.find('\\') != std::string::npos ||
        value.find(':') != std::string::npos)
        throw Error("TPool history " + name +
                    " must be one directory name, not a path");
    return value;
}

std::string root_relative_path(const fs::path& canonical_root,
                               const std::string& source) {
    const auto candidate = fs::weakly_canonical(from_utf8(source));
    if (!path_starts_with(candidate, canonical_root))
        throw Error("TPool history source escaped the configured TDX root");
    auto result = path_utf8(candidate.lexically_relative(canonical_root));
    std::replace(result.begin(), result.end(), '\\', '/');
    if (result.empty() || result == "." || result.rfind("../", 0) == 0)
        throw Error("TPool history source has no safe root-relative path");
    return result;
}

void project_root_relative_paths(Json& document, const fs::path& root) {
    const auto canonical_root = fs::weakly_canonical(root);
    document.as_object().erase("root");
    document["path_scope"] = "tdx-root-relative";
    for (auto& directory : document["directories"].as_array()) {
        directory["path"] = root_relative_path(
            canonical_root, directory.at("path").as_string());
    }
    for (auto& file : document["files"].as_array()) {
        file["source"] = root_relative_path(
            canonical_root, file.at("source").as_string());
    }
}

void project_catalog_root_relative_paths(Json& document,
                                         const fs::path& root) {
    const auto canonical_root = fs::weakly_canonical(root);
    document.as_object().erase("root");
    document["path_scope"] = "tdx-root-relative";
    for (auto& directory : document["directories"].as_array())
        directory["path"] = root_relative_path(
            canonical_root, directory.at("path").as_string());
    for (auto& pool : document["pools"].as_array())
        pool["source"] = root_relative_path(
            canonical_root, pool.at("source").as_string());
}

void replace_all(std::string& value, const std::string& needle,
                 std::string_view replacement) {
    if (needle.empty()) return;
    std::size_t offset = 0;
    while ((offset = value.find(needle, offset)) != std::string::npos) {
        value.replace(offset, needle.size(), replacement);
        offset += replacement.size();
    }
}

[[noreturn]] void throw_sanitized_scan_error(const fs::path& root,
                                             const std::exception& error) {
    std::string message = error.what();
    replace_all(message, path_utf8(root), "<tdx-root>");
    std::error_code ignored;
    const auto canonical = fs::weakly_canonical(root, ignored);
    if (!ignored) replace_all(message, path_utf8(canonical), "<tdx-root>");
    throw Error(message);
}

}  // namespace

Json query_tpool_catalog(const fs::path& root,
                         const RequestTarget& target) {
    if (!target.query.empty())
        throw Error("TPool catalog does not accept query parameters");
    if (root.empty() || !fs::is_directory(root))
        throw Error("TPool catalog requires an existing configured TDX root");
    try {
        auto result = inspect_tpool_root_document(root);
        project_catalog_root_relative_paths(result, root);
        return result;
    } catch (const std::exception& error) {
        throw_sanitized_scan_error(root, error);
    }
}

Json query_tpool_history(const fs::path& root, const RequestTarget& target) {
    static const std::set<std::string, std::less<>> allowed{
        "pool", "cell", "kind", "from", "to", "limit"};
    for (const auto& [name, value] : target.query) {
        (void)value;
        if (!allowed.count(name))
            throw Error("TPool history query does not accept parameter: " + name);
    }
    if (root.empty() || !fs::is_directory(root))
        throw Error("TPool history requires an existing configured TDX root");

    const auto pool = history_directory_filter(target, "pool");
    const auto cell = history_directory_filter(target, "cell");
    const auto kind = lower_ascii(trim(query_value(target, "kind", "all")));
    const int from = history_date(target, "from");
    const int to = history_date(target, "to");
    const int limit = parse_bounded(
        query_value(target, "limit", "1000"), "limit", 1, 10000);

    try {
        auto result = inspect_tpool_history_root_document(
            root, pool, cell, kind, from, to, static_cast<std::size_t>(limit));
        project_root_relative_paths(result, root);
        return result;
    } catch (const std::exception& error) {
        throw_sanitized_scan_error(root, error);
    }
}

Json query_tpool_file_evaluation(const fs::path& root,
                                 const RequestTarget& target,
                                 const Json& formulas) {
    static const std::set<std::string, std::less<>> allowed{
        "source", "pages", "page_size", "timeout_ms", "limit"};
    for (const auto& [name, value] : target.query) {
        (void)value;
        if (!allowed.count(name))
            throw Error("TPool evaluation query does not accept parameter: " +
                        name);
    }
    if (root.empty() || !fs::is_directory(root))
        throw Error("TPool evaluation requires an existing configured TDX root");
    const auto source = trim(query_value(target, "source"));
    if (source.empty()) throw Error("pool evaluation requires source");
    if (source.size() > 1024 || source.find('\0') != std::string::npos ||
        source.find('\\') != std::string::npos ||
        source.find(':') != std::string::npos || source.front() == '/')
        throw Error("pool source must be a root-relative XML path");

    try {
        const auto canonical_root = fs::weakly_canonical(root);
        const auto relative = from_utf8(source);
        if (relative.is_absolute() || relative.has_root_name() ||
            relative.has_root_directory())
            throw Error("pool source must be a root-relative XML path");
        const auto candidate = fs::weakly_canonical(canonical_root / relative);
        if (!path_starts_with(candidate, canonical_root) ||
            lower_ascii(candidate.extension().string()) != ".xml" ||
            !fs::is_regular_file(candidate))
            throw Error("pool source must be an existing root-relative XML file");

        auto result = evaluate_tpool_file_document(
            candidate,
            parse_bounded(query_value(target, "pages", "1"), "pages", 1,
                          20),
            parse_bounded(query_value(target, "page_size", "800"),
                          "page_size", 1, 800),
            parse_bounded(query_value(target, "timeout_ms", "10000"),
                          "timeout_ms", 1, 600000),
            parse_bounded(query_value(target, "limit", "20"), "limit", 1,
                          200),
            &formulas, root);
        const auto safe_source = root_relative_path(
            canonical_root, path_utf8(candidate));
        result["source"] = safe_source;
        result["path_scope"] = "tdx-root-relative";
        const auto inspection = result.as_object().find("inspection");
        if (inspection != result.as_object().end() &&
            inspection->second.is_object())
            inspection->second["source"] = safe_source;
        return result;
    } catch (const std::exception& error) {
        throw_sanitized_scan_error(root, error);
    }
}

}  // namespace tdx::server_detail
