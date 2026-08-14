#include "cloud_calc_config_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <utility>

namespace fs = std::filesystem;

namespace tdx::cloud_calc_detail {

bool parse_double(std::string_view source, double& value) {
    const auto text = trim(std::string(source));
    if (text.empty()) return false;
    char* end = nullptr;
    value = std::strtod(text.c_str(), &end);
    if (end != text.c_str() + text.size() || !std::isfinite(value)) return false;
    return true;
}

int parse_int(std::string_view source, int fallback) {
    double value = 0.0;
    if (!parse_double(source, value)) return fallback;
    if (value < static_cast<double>(std::numeric_limits<int>::min()) ||
        value > static_cast<double>(std::numeric_limits<int>::max())) return fallback;
    return static_cast<int>(value);
}

std::string xml_unescape(std::string value) {
    const std::pair<const char*, const char*> entities[] = {
        {"&quot;", "\""}, {"&apos;", "'"}, {"&lt;", "<"},
        {"&gt;", ">"}, {"&amp;", "&"},
    };
    for (const auto& [encoded, decoded] : entities) {
        std::size_t at = 0;
        while ((at = value.find(encoded, at)) != std::string::npos) {
            value.replace(at, std::char_traits<char>::length(encoded), decoded);
            at += std::char_traits<char>::length(decoded);
        }
    }
    return value;
}

std::size_t tag_end(std::string_view text, std::size_t begin) {
    char quote = 0;
    for (std::size_t index = begin; index < text.size(); ++index) {
        const char ch = text[index];
        if (quote) {
            if (ch == quote) quote = 0;
        } else if (ch == '\'' || ch == '"') {
            quote = ch;
        } else if (ch == '>') {
            return index;
        }
    }
    throw Error("unterminated XML tag");
}

std::map<std::string, std::string, std::less<>> attributes(std::string_view tag) {
    std::map<std::string, std::string, std::less<>> result;
    std::size_t cursor = 0;
    while (cursor < tag.size() && tag[cursor] != '<') ++cursor;
    if (cursor < tag.size()) ++cursor;
    while (cursor < tag.size() && !std::isspace(static_cast<unsigned char>(tag[cursor])) &&
           tag[cursor] != '>' && tag[cursor] != '/') ++cursor;
    while (cursor < tag.size()) {
        while (cursor < tag.size() &&
               (std::isspace(static_cast<unsigned char>(tag[cursor])) || tag[cursor] == '/')) ++cursor;
        if (cursor >= tag.size() || tag[cursor] == '>') break;
        const auto name_begin = cursor;
        while (cursor < tag.size() &&
               (std::isalnum(static_cast<unsigned char>(tag[cursor])) ||
                tag[cursor] == '_' || tag[cursor] == '-' || tag[cursor] == ':')) ++cursor;
        if (cursor == name_begin) { ++cursor; continue; }
        auto name = lower_ascii(std::string(tag.substr(name_begin, cursor - name_begin)));
        while (cursor < tag.size() && std::isspace(static_cast<unsigned char>(tag[cursor]))) ++cursor;
        if (cursor >= tag.size() || tag[cursor] != '=') {
            result.emplace(std::move(name), "");
            continue;
        }
        ++cursor;
        while (cursor < tag.size() && std::isspace(static_cast<unsigned char>(tag[cursor]))) ++cursor;
        if (cursor >= tag.size() || (tag[cursor] != '\'' && tag[cursor] != '"')) {
            const auto value_begin = cursor;
            while (cursor < tag.size() && !std::isspace(static_cast<unsigned char>(tag[cursor])) &&
                   tag[cursor] != '>') ++cursor;
            result.emplace(std::move(name), xml_unescape(std::string(tag.substr(value_begin, cursor - value_begin))));
            continue;
        }
        const char quote = tag[cursor++];
        const auto value_begin = cursor;
        while (cursor < tag.size() && tag[cursor] != quote) ++cursor;
        result.emplace(std::move(name), xml_unescape(std::string(tag.substr(value_begin, cursor - value_begin))));
        if (cursor < tag.size()) ++cursor;
    }
    return result;
}

std::string attr(const std::map<std::string, std::string, std::less<>>& values,
                 std::string_view name) {
    const auto found = values.find(name);
    return found == values.end() ? std::string{} : found->second;
}

bool tag_at(std::string_view text, std::size_t at, std::string_view name) {
    if (at + 1 + name.size() > text.size() || text[at] != '<' ||
        text.substr(at + 1, name.size()) != name) return false;
    const auto next = at + 1 + name.size();
    return next >= text.size() || std::isspace(static_cast<unsigned char>(text[next])) ||
           text[next] == '>' || text[next] == '/';
}

bool xml_comment_at(std::string_view text, std::size_t at) {
    const auto open = text.rfind("<!--", at);
    if (open == std::string_view::npos) return false;
    const auto close = text.rfind("-->", at);
    return close == std::string_view::npos || open > close;
}

std::vector<std::string> calc_refs(std::string_view source) {
    std::vector<std::string> result;
    if (trim(std::string(source)).empty()) return result;
    for (auto value : split(source, ',')) {
        value = trim(std::move(value));
        if (!value.empty()) result.push_back(std::move(value));
    }
    return result;
}

Config parse_config(const fs::path& path) {
    const auto text = decode_gbk(read_bytes(path));
    Config result;
    result.path = path;
    std::size_t cursor = 0;
    while ((cursor = text.find("<unit", cursor)) != std::string::npos) {
        if (xml_comment_at(text, cursor)) {
            const auto close = text.find("-->", cursor);
            cursor = close == std::string::npos ? text.size() : close + 3;
            continue;
        }
        if (!tag_at(text, cursor, "unit")) { cursor += 5; continue; }
        const auto open_end = tag_end(text, cursor);
        const auto unit_attrs = attributes(std::string_view(text).substr(cursor, open_end - cursor + 1));
        const auto close = text.find("</unit", open_end + 1);
        const auto body_end = close == std::string::npos ? text.size() : close;
        Unit unit;
        unit.id = attr(unit_attrs, "id");
        unit.file = attr(unit_attrs, "file");
        unit.refunit = trim(attr(unit_attrs, "refunit"));
        std::size_t item_cursor = open_end + 1;
        std::size_t ordinal = 0;
        while ((item_cursor = text.find("<item", item_cursor)) != std::string::npos &&
               item_cursor < body_end) {
            if (xml_comment_at(text, item_cursor)) {
                const auto comment_end = text.find("-->", item_cursor);
                item_cursor = comment_end == std::string::npos ? body_end : comment_end + 3;
                continue;
            }
            if (!tag_at(text, item_cursor, "item")) { item_cursor += 5; continue; }
            const auto item_end = tag_end(text, item_cursor);
            if (item_end > body_end) break;
            const auto values = attributes(std::string_view(text).substr(
                item_cursor, item_end - item_cursor + 1));
            Column column;
            column.code = attr(values, "code");
            column.name = attr(values, "name");
            column.datatype = attr(values, "datatype");
            column.calc = trim(attr(values, "calc"));
            column.refs = calc_refs(attr(values, "calcref"));
            column.syscol = trim(attr(values, "syscol"));
            column.refzqdm = trim(attr(values, "refzqdm"));
            column.calctype = parse_int(attr(values, "calctype"));
            column.calcflag = parse_int(attr(values, "calcflag"));
            column.ordinal = ordinal++;
            unit.columns.push_back(std::move(column));
            item_cursor = item_end + 1;
        }
        result.units.push_back(std::move(unit));
        if (close == std::string::npos) break;
        cursor = tag_end(text, close) + 1;
    }
    return result;
}

std::vector<fs::path> config_paths(const fs::path& root_or_cfg) {
    if (fs::is_regular_file(root_or_cfg)) return {fs::weakly_canonical(root_or_cfg)};
    fs::path directory = root_or_cfg;
    if (fs::is_directory(directory / "T0002" / "cloud_cfg"))
        directory /= fs::path("T0002") / "cloud_cfg";
    if (!fs::is_directory(directory))
        throw Error("cloud_cfg directory or CFG file does not exist: " + path_utf8(root_or_cfg));
    std::vector<fs::path> result;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;
        if (lower_ascii(entry.path().extension().string()) == ".cfg") result.push_back(entry.path());
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        return path_utf8(left.filename()) < path_utf8(right.filename());
    });
    return result;
}


}  // namespace tdx::cloud_calc_detail
