#include "jsn_variants_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/tqlex.hpp"
#include "tdx/utf8.hpp"

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

std::string relative_resource(const fs::path& path, const fs::path& root) {
    std::error_code error;
    auto value = path_utf8(fs::relative(path, root, error));
    if (error) value = path_utf8(path.filename());
    std::replace(value.begin(), value.end(), '\\', '/');
    return value;
}

std::string regex_escape(char ch) {
    static const std::string special = R"(\.^$|()[]{}*+?)";
    return special.find(ch) == std::string::npos ? std::string(1, ch)
                                                  : std::string("\\") + ch;
}

std::regex template_regex(const std::string& resource) {
    std::string expression = "^";
    std::size_t offset = 0;
    while (offset < resource.size()) {
        const auto start = resource.find("$$$", offset);
        if (start == std::string::npos) {
            for (std::size_t i = offset; i < resource.size(); ++i)
                expression += regex_escape(resource[i]);
            break;
        }
        for (std::size_t i = offset; i < start; ++i) expression += regex_escape(resource[i]);
        const auto end = resource.find("$$", start + 3);
        if (end == std::string::npos) {
            for (std::size_t i = start; i < resource.size(); ++i)
                expression += regex_escape(resource[i]);
            break;
        }
        expression += "[^/]+";
        offset = end + 2;
    }
    expression += '$';
    return std::regex(expression, std::regex::ECMAScript | std::regex::icase);
}

std::optional<ConcreteTemplateMatch> match_concrete_template(
    const std::string& concrete,
    const JsnResourceTemplate& candidate) {
    std::string expression = "^";
    std::vector<std::string> names;
    std::size_t literal_score = 0;
    std::size_t offset = 0;
    while (offset < candidate.resource.size()) {
        const auto start = candidate.resource.find("$$$", offset);
        if (start == std::string::npos) {
            for (std::size_t index = offset; index < candidate.resource.size(); ++index)
                expression += regex_escape(candidate.resource[index]);
            literal_score += candidate.resource.size() - offset;
            break;
        }
        for (std::size_t index = offset; index < start; ++index)
            expression += regex_escape(candidate.resource[index]);
        literal_score += start - offset;
        const auto end = candidate.resource.find("$$", start + 3);
        if (end == std::string::npos) return std::nullopt;
        const auto name = candidate.resource.substr(start + 3, end - start - 3);
        names.push_back(name);
        expression += lower_ascii(name) == "sc" ? "(31|47|48|70|74|[012])" : "([^/]+)";
        offset = end + 2;
    }
    expression += '$';
    std::smatch match;
    const std::regex pattern(expression, std::regex::ECMAScript | std::regex::icase);
    if (!std::regex_match(concrete, match, pattern)) return std::nullopt;
    ConcreteTemplateMatch result;
    result.value = &candidate;
    result.literal_score = literal_score;
    for (std::size_t index = 0; index < names.size(); ++index)
        result.keys[names[index]] = match[index + 1].str();
    return result;
}

std::vector<ConcreteTemplateMatch> concrete_template_matches(
    const std::string& resource,
    const std::vector<JsnResourceTemplate>& templates) {
    std::vector<ConcreteTemplateMatch> result;
    const auto folded_resource = lower_ascii(resource);
    for (const auto& candidate : templates) {
        if (candidate.placeholders.empty()) {
            if (folded_resource == lower_ascii(candidate.resource))
                result.push_back({&candidate, {}, candidate.resource.size()});
            continue;
        }
        const auto placeholder = candidate.resource.find("$$$");
        const auto prefix = lower_ascii(candidate.resource.substr(0, placeholder));
        if (folded_resource.rfind(prefix, 0) != 0) continue;
        if (auto match = match_concrete_template(resource, candidate))
            result.push_back(std::move(*match));
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        if (left.literal_score != right.literal_score)
            return left.literal_score > right.literal_score;
        return lower_ascii(left.value->resource) < lower_ascii(right.value->resource);
    });
    return result;
}

bool security_column(const std::string& value) {
    const auto name = lower_ascii(value);
    return name == "$sc" || name == "$zqdm" || name == "$sc1" ||
           name == "$zqdm1" || name == "$s_zqdm" || name == "sc" ||
           name == "zqdm" || name == "zqdm1";
}

bool numeric_text(const std::string& text, bool& integer) {
    const auto value = trim(text);
    if (value.empty()) return false;
    char* end = nullptr;
    errno = 0;
    const auto number = std::strtod(value.c_str(), &end);
    if (errno == ERANGE || end != value.c_str() + value.size() || !std::isfinite(number))
        return false;
    integer = value.find_first_of(".eE") == std::string::npos;
    return true;
}

bool date_like_text(const std::string& text) {
    const auto value = trim(text);
    auto digits = [&](std::size_t begin, std::size_t end) {
        if (end > value.size()) return false;
        for (std::size_t index = begin; index < end; ++index)
            if (!std::isdigit(static_cast<unsigned char>(value[index]))) return false;
        return true;
    };
    if (value.size() >= 8 && digits(0, 8) &&
        (value.size() == 8 || value[8] == ' ' || value[8] == 'T')) return true;
    if (value.size() >= 10 && digits(0, 4) && digits(5, 7) && digits(8, 10) &&
        (value[4] == '-' || value[4] == '/' || value[4] == '.') &&
        value[7] == value[4] &&
        (value.size() == 10 || value[10] == ' ' || value[10] == 'T')) return true;
    return false;
}

std::string profile_sample(std::string value) {
    value = trim(std::move(value));
    constexpr std::size_t limit = 96;
    const auto prefix = utf8_prefix(value, limit);
    return prefix.size() < value.size() ? prefix + "..." : prefix;
}

void observe(DownloadedResource::ColumnProfile& profile, const Json& cell) {
    ++profile.seen;
    const auto text = trim(jsn_scalar_text(cell));
    if (text.empty()) return;
    ++profile.nonempty;
    bool integer = false;
    if (numeric_text(text, integer)) {
        ++profile.numeric;
        if (integer) ++profile.integer;
    }
    if (date_like_text(text)) ++profile.date_like;
    const auto lowered = lower_ascii(text);
    if (lowered.rfind("http://", 0) == 0 || lowered.rfind("https://", 0) == 0)
        ++profile.url_like;
    if (lowered.find("<html") != std::string::npos ||
        lowered.find("<div") != std::string::npos ||
        lowered.find("<span") != std::string::npos ||
        lowered.find("<p") != std::string::npos ||
        lowered.find("</") != std::string::npos)
        ++profile.html_like;
    if (profile.samples.size() < 3) profile.samples.insert(profile_sample(text));
}

Json column_profiles_json(
    const std::map<std::string, DownloadedResource::ColumnProfile>& profiles) {
    Json result = Json::object();
    for (const auto& [column, value] : profiles) {
        Json row = Json::object();
        row["seen"] = value.seen;
        row["nonempty"] = value.nonempty;
        row["numeric"] = value.numeric;
        row["integer"] = value.integer;
        row["date_like"] = value.date_like;
        row["url_like"] = value.url_like;
        row["html_like"] = value.html_like;
        Json samples = Json::array();
        for (const auto& sample : value.samples) samples.push_back(sample);
        row["samples"] = std::move(samples);
        result[column] = std::move(row);
    }
    return result;
}

std::vector<DownloadedResource> scan_downloaded(
    const fs::path& root, bool fingerprints, bool profiles) {
    std::vector<DownloadedResource> result;
    if (root.empty() || !fs::is_directory(root)) return result;
    std::vector<fs::path> paths;
    std::error_code error;
    for (fs::recursive_directory_iterator iterator(root, error), end;
         iterator != end; iterator.increment(error)) {
        if (error) throw Error("cannot scan JSN download directory: " + error.message());
        if (iterator->is_regular_file() &&
            lower_ascii(iterator->path().extension().string()) == ".jsn")
            paths.push_back(iterator->path());
    }
    std::sort(paths.begin(), paths.end(), [&](const fs::path& left, const fs::path& right) {
        return lower_ascii(relative_resource(left, root)) <
               lower_ascii(relative_resource(right, root));
    });
    for (const auto& path : paths) {
        DownloadedResource value;
        value.resource = relative_resource(path, root);
        value.bytes = static_cast<std::uint64_t>(fs::file_size(path));
        if (fingerprints) value.sha256 = sha256_file(path);
        try {
            const auto tables = load_jsn_tables(path);
            value.groups = static_cast<std::uint64_t>(tables.size());
            for (const auto& table : tables) {
                value.rows += static_cast<std::uint64_t>(table.rows.size());
                value.columns.insert(table.headers.begin(), table.headers.end());
                if (profiles) {
                    for (const auto& row : table.rows) {
                        for (std::size_t index = 0; index < table.headers.size(); ++index) {
                            auto& profile = value.profiles[table.headers[index]];
                            constexpr std::uint64_t profile_limit = 64;
                            if (profile.seen >= profile_limit) continue;
                            if (index < row.size()) observe(profile, row[index]);
                            else ++profile.seen;
                        }
                    }
                }
            }
            value.security_related = std::any_of(
                value.columns.begin(), value.columns.end(), security_column);
        } catch (const std::exception& failure) {
            value.parse_error = failure.what();
        }
        result.push_back(std::move(value));
    }
    return result;
}

DownloadAggregate aggregate_downloads(
    const JsnResourceTemplate& resource,
    const std::vector<DownloadedResource>& downloaded,
    std::set<std::string>& matched_files) {
    DownloadAggregate result;
    const auto pattern = template_regex(resource.resource);
    for (const auto& file : downloaded) {
        if (!std::regex_match(file.resource, pattern)) continue;
        ++result.files;
        result.bytes += file.bytes;
        result.groups += file.groups;
        result.rows += file.rows;
        result.columns.insert(file.columns.begin(), file.columns.end());
        result.security_related = result.security_related || file.security_related;
        if (!file.parse_error.empty()) ++result.parse_errors;
        matched_files.insert(lower_ascii(file.resource));
    }
    return result;
}

}  // namespace tdx::jsn_variant_detail
