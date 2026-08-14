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

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string config_text(const fs::path& path) {
    const auto bytes = read_bytes(path);
    if (bytes.size() >= 2 && ((bytes[0] == 0xFF && bytes[1] == 0xFE) ||
                             (bytes[0] == 0xFE && bytes[1] == 0xFF))) {
        if ((bytes.size() - 2) % 2)
            throw Error("odd UTF-16 config length: " + path_utf8(path));
        std::wstring wide;
        wide.reserve((bytes.size() - 2) / 2);
        const bool little = bytes[0] == 0xFF;
        for (std::size_t offset = 2; offset < bytes.size(); offset += 2) {
            const auto value = little
                ? static_cast<std::uint16_t>(bytes[offset] | bytes[offset + 1] << 8)
                : static_cast<std::uint16_t>(bytes[offset] << 8 | bytes[offset + 1]);
            wide.push_back(static_cast<wchar_t>(value));
        }
        return wide_to_utf8(wide);
    }
    const std::size_t offset = bytes.size() >= 3 && bytes[0] == 0xEF &&
        bytes[1] == 0xBB && bytes[2] == 0xBF ? 3 : 0;
    const std::string raw(reinterpret_cast<const char*>(bytes.data() + offset),
                          bytes.size() - offset);
    try {
        (void)utf8_to_wide(raw);
        return raw;
    } catch (const Error&) {
        return decode_gbk(bytes);
    }
}

std::string remove_comments(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    std::size_t offset = 0;
    while (offset < text.size()) {
        const auto start = text.find("<!--", offset);
        if (start == std::string::npos) {
            result.append(text, offset, std::string::npos);
            break;
        }
        result.append(text, offset, start - offset);
        const auto end = text.find("-->", start + 4);
        if (end == std::string::npos) throw Error("unterminated XML comment");
        offset = end + 3;
    }
    return result;
}

std::string xml_unescape(std::string value) {
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 5> replacements{{
        {"&quot;", "\""}, {"&apos;", "'"}, {"&lt;", "<"},
        {"&gt;", ">"}, {"&amp;", "&"}}};
    for (const auto& [entity, replacement] : replacements) {
        std::size_t offset = 0;
        while ((offset = value.find(entity, offset)) != std::string::npos) {
            value.replace(offset, entity.size(), replacement);
            offset += replacement.size();
        }
    }
    return value;
}

std::map<std::string, std::string> attributes(std::string_view tag) {
    std::map<std::string, std::string> result;
    std::size_t offset = 0;
    while (offset < tag.size()) {
        while (offset < tag.size() &&
               std::isspace(static_cast<unsigned char>(tag[offset]))) ++offset;
        const auto begin = offset;
        while (offset < tag.size()) {
            const auto ch = static_cast<unsigned char>(tag[offset]);
            if (!std::isalnum(ch) && ch != '_' && ch != '$' && ch != '-') break;
            ++offset;
        }
        if (offset == begin) { ++offset; continue; }
        auto name = lower_ascii(std::string(tag.substr(begin, offset - begin)));
        while (offset < tag.size() &&
               std::isspace(static_cast<unsigned char>(tag[offset]))) ++offset;
        if (offset >= tag.size() || tag[offset] != '=') continue;
        ++offset;
        while (offset < tag.size() &&
               std::isspace(static_cast<unsigned char>(tag[offset]))) ++offset;
        if (offset >= tag.size() || (tag[offset] != '\'' && tag[offset] != '"')) continue;
        const char quote = tag[offset++];
        const auto value_begin = offset;
        const auto end = tag.find(quote, offset);
        if (end == std::string_view::npos) throw Error("unterminated CFG attribute");
        result[name] = xml_unescape(std::string(tag.substr(value_begin, end - value_begin)));
        offset = end + 1;
    }
    return result;
}

std::vector<std::string> unit_files(const std::string& source) {
    const auto text = remove_comments(source);
    const auto lowered = lower_ascii(text);
    std::vector<std::string> result;
    std::size_t offset = 0;
    while ((offset = lowered.find("<unit", offset)) != std::string::npos) {
        const auto boundary = offset + 5;
        if (boundary < text.size() &&
            !std::isspace(static_cast<unsigned char>(text[boundary])) &&
            text[boundary] != '>' && text[boundary] != '/') {
            offset = boundary;
            continue;
        }
        std::size_t end = boundary;
        char quote = 0;
        for (; end < text.size(); ++end) {
            const char ch = text[end];
            if (quote) { if (ch == quote) quote = 0; }
            else if (ch == '\'' || ch == '"') quote = ch;
            else if (ch == '>') break;
        }
        if (end >= text.size()) throw Error("unterminated <unit> tag");
        const auto parsed = attributes(std::string_view(text).substr(
            boundary, end - boundary));
        const auto found = parsed.find("file");
        if (found != parsed.end() && !trim(found->second).empty())
            result.push_back(trim(found->second));
        offset = end + 1;
    }
    return result;
}

std::vector<std::string> reference_ids(const std::string& value) {
    std::vector<std::string> result;
    for (auto item : split(value, ',')) {
        item = trim(std::move(item));
        if (!item.empty() && std::find(result.begin(), result.end(), item) == result.end())
            result.push_back(std::move(item));
    }
    return result;
}

std::string unit_relation_scope(const fs::path& source_file) {
    const auto stem = lower_ascii(path_utf8(source_file.stem()));
    static const std::regex page_suffix(R"(^(.+?)[0-9]{3}(?:_[0-9]+)?$)");
    std::smatch match;
    return std::regex_match(stem, match, page_suffix) ? match[1].str() : stem;
}

std::vector<UnitDefinition> unit_definitions(const fs::path& root) {
    const auto directory = root / "T0002" / "cloud_cfg";
    if (!fs::is_directory(directory))
        throw Error("TDX cloud_cfg directory is unavailable: " + path_utf8(directory));
    std::vector<fs::path> paths;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;
        const auto extension = lower_ascii(entry.path().extension().string());
        if (extension == ".cfg" || extension == ".xml") paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end(), [](const auto& left, const auto& right) {
        return lower_ascii(path_utf8(left.filename())) < lower_ascii(path_utf8(right.filename()));
    });
    std::vector<UnitDefinition> result;
    for (const auto& path : paths) {
        const auto source = remove_comments(config_text(path));
        const auto lowered = lower_ascii(source);
        std::size_t offset = 0;
        while ((offset = lowered.find("<unit", offset)) != std::string::npos) {
            const auto boundary = offset + 5;
            if (boundary < source.size() &&
                !std::isspace(static_cast<unsigned char>(source[boundary])) &&
                source[boundary] != '>' && source[boundary] != '/') {
                offset = boundary;
                continue;
            }
            std::size_t end = boundary;
            char quote = 0;
            for (; end < source.size(); ++end) {
                const char ch = source[end];
                if (quote) { if (ch == quote) quote = 0; }
                else if (ch == '\'' || ch == '"') quote = ch;
                else if (ch == '>') break;
            }
            if (end >= source.size())
                throw Error("unterminated <unit> tag in " + path_utf8(path));
            const auto parsed = attributes(std::string_view(source).substr(
                boundary, end - boundary));
            UnitDefinition unit;
            const auto id = parsed.find("id");
            const auto file = parsed.find("file");
            const auto refs = parsed.find("refunit");
            if (id != parsed.end()) unit.id = trim(id->second);
            if (file != parsed.end()) unit.file = trim(file->second);
            if (refs != parsed.end()) unit.reference_ids = reference_ids(refs->second);
            unit.source_file = path_utf8(path.filename());
            unit.config_name = lower_ascii(path_utf8(path.stem()));
            unit.relation_scope = unit_relation_scope(path.filename());
            if (!unit.id.empty() || !unit.file.empty()) result.push_back(std::move(unit));
            offset = end + 1;
        }
    }
    return result;
}

std::string normalize_resource(std::string value) {
    value = trim(std::move(value));
    std::replace(value.begin(), value.end(), '\\', '/');
    while (!value.empty() && value.front() == '/') value.erase(value.begin());
    if (value.empty() || value.find("../") != std::string::npos ||
        value == ".." || value.find("/..") != std::string::npos)
        throw Error("invalid JSN resource path: " + value);
    if (lower_ascii(fs::path(value).extension().string()) != ".jsn")
        throw Error("JSN resource does not end in .jsn: " + value);
    if (value.find('/') == std::string::npos) value = "list/" + value;
    return value;
}

std::vector<std::string> placeholders(const std::string& resource) {
    std::vector<std::string> result;
    std::size_t offset = 0;
    while ((offset = resource.find("$$$", offset)) != std::string::npos) {
        const auto end = resource.find("$$", offset + 3);
        if (end == std::string::npos) break;
        const auto token = resource.substr(offset, end + 2 - offset);
        if (std::find(result.begin(), result.end(), token) == result.end())
            result.push_back(token);
        offset = end + 2;
    }
    return result;
}

void add_template(std::map<std::string, JsnResourceTemplate>& resources,
                  const std::string& raw_resource,
                  const std::string& source_file) {
    const auto resource = normalize_resource(raw_resource);
    const auto key = lower_ascii(resource);
    auto [found, inserted] = resources.emplace(key, JsnResourceTemplate{});
    auto& value = found->second;
    if (inserted) {
        value.resource = resource;
        value.placeholders = placeholders(resource);
    }
    if (std::find(value.source_files.begin(), value.source_files.end(), source_file) ==
        value.source_files.end()) value.source_files.push_back(source_file);
}

}  // namespace tdx::jsn_variant_detail

namespace tdx {

using namespace jsn_variant_detail;

std::vector<JsnResourceTemplate> inventory_jsn_resource_templates(const fs::path& root) {
    std::map<std::string, JsnResourceTemplate> resources;
    for (const auto& config : inventory_cloud_configs(root, "11"))
        if (!trim(config.body).empty()) add_template(resources, config.body, config.source_file);

    const auto directory = root / "T0002" / "cloud_cfg";
    if (!fs::is_directory(directory))
        throw Error("TDX cloud_cfg directory is unavailable: " + path_utf8(directory));
    std::vector<fs::path> configs;
    for (const auto& entry : fs::directory_iterator(directory))
        if (entry.is_regular_file() && lower_ascii(entry.path().extension().string()) == ".cfg")
            configs.push_back(entry.path());
    std::sort(configs.begin(), configs.end(), [](const fs::path& left, const fs::path& right) {
        return lower_ascii(path_utf8(left.filename())) < lower_ascii(path_utf8(right.filename()));
    });
    for (const auto& path : configs) {
        for (const auto& raw : unit_files(config_text(path))) {
            const auto folded = lower_ascii(raw);
            const auto known = detail_templates().find(folded);
            if (known != detail_templates().end()) {
                for (const auto& resource : known->second)
                    add_template(resources, resource, path_utf8(path.filename()));
            } else if (lower_ascii(fs::path(raw).extension().string()) == ".jsn") {
                add_template(resources, raw, path_utf8(path.filename()));
            }
        }
    }
    std::vector<JsnResourceTemplate> result;
    result.reserve(resources.size());
    for (auto& [key, value] : resources) {
        (void)key;
        std::sort(value.source_files.begin(), value.source_files.end(), [](const auto& left, const auto& right) {
            return lower_ascii(left) < lower_ascii(right);
        });
        result.push_back(std::move(value));
    }
    return result;
}

}  // namespace tdx
