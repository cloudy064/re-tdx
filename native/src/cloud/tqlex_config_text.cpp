#include "tqlex_internal.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

namespace tdx::detail {

std::string read_tqlex_config_text(const fs::path& path) {
    const auto bytes = read_bytes(path);
    if (bytes.size() >= 2 && ((bytes[0] == 0xFF && bytes[1] == 0xFE) ||
                             (bytes[0] == 0xFE && bytes[1] == 0xFF))) {
        if ((bytes.size() - 2) % 2) throw Error("odd UTF-16 config length: " + path_utf8(path));
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
    std::size_t offset = bytes.size() >= 3 && bytes[0] == 0xEF &&
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

std::string remove_xml_comments(const std::string& text) {
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

void append_utf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint <= 0x7F) output.push_back(static_cast<char>(codepoint));
    else if (codepoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0x10FFFF) {
        output.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else throw Error("XML entity codepoint is invalid");
}

std::string html_unescape(std::string value) {
    for (const auto& [entity, replacement] :
         std::array<std::pair<std::string_view, std::string_view>, 5>{{
             {"&quot;", "\""}, {"&apos;", "'"}, {"&lt;", "<"},
             {"&gt;", ">"}, {"&amp;", "&"}}}) {
        std::size_t offset = 0;
        while ((offset = value.find(entity, offset)) != std::string::npos) {
            value.replace(offset, entity.size(), replacement);
            offset += replacement.size();
        }
    }
    std::string output;
    output.reserve(value.size());
    for (std::size_t index = 0; index < value.size();) {
        if (value[index] == '&' && index + 3 < value.size() && value[index + 1] == '#') {
            const auto end = value.find(';', index + 2);
            if (end != std::string::npos) {
                const bool hex = value[index + 2] == 'x' || value[index + 2] == 'X';
                const auto digits = value.substr(index + (hex ? 3 : 2),
                                                 end - index - (hex ? 3 : 2));
                try {
                    std::size_t used = 0;
                    const auto point = std::stoul(digits, &used, hex ? 16 : 10);
                    if (used == digits.size()) {
                        append_utf8(output, static_cast<std::uint32_t>(point));
                        index = end + 1;
                        continue;
                    }
                } catch (...) {}
            }
        }
        output.push_back(value[index++]);
    }
    return output;
}

std::map<std::string, std::string> parse_xml_attributes(std::string_view text) {
    std::map<std::string, std::string> result;
    std::size_t offset = 0;
    while (offset < text.size()) {
        while (offset < text.size() && std::isspace(static_cast<unsigned char>(text[offset]))) ++offset;
        const auto name_start = offset;
        while (offset < text.size()) {
            const auto ch = static_cast<unsigned char>(text[offset]);
            if (!std::isalnum(ch) && ch != '_' && ch != '$' && ch != '-') break;
            ++offset;
        }
        if (offset == name_start) { ++offset; continue; }
        auto name = lower_ascii(std::string(text.substr(name_start, offset - name_start)));
        while (offset < text.size() && std::isspace(static_cast<unsigned char>(text[offset]))) ++offset;
        if (offset >= text.size() || text[offset] != '=') continue;
        ++offset;
        while (offset < text.size() && std::isspace(static_cast<unsigned char>(text[offset]))) ++offset;
        if (offset >= text.size() || (text[offset] != '\'' && text[offset] != '"')) continue;
        const char quote = text[offset++];
        const auto value_start = offset;
        const auto end = text.find(quote, offset);
        if (end == std::string_view::npos) throw Error("unterminated XML attribute");
        result[name] = html_unescape(std::string(text.substr(value_start, end - value_start)));
        offset = end + 1;
    }
    return result;
}

std::vector<std::string> tqlex_placeholders(std::string_view body) {
    std::vector<std::string> result;
    for (std::size_t offset = 0; offset + 4 <= body.size();) {
        const auto start = body.find("$$", offset);
        if (start == std::string_view::npos) break;
        if ((start > 0 && body[start - 1] == '$') ||
            (start + 2 < body.size() && body[start + 2] == '$')) {
            offset = start + 2;
            continue;
        }
        const auto end = body.find("$$", start + 2);
        if (end == std::string_view::npos) break;
        if (end == start + 2 || (end + 2 < body.size() && body[end + 2] == '$')) {
            offset = end + 2;
            continue;
        }
        const auto name = std::string(body.substr(start + 2, end - start - 2));
        if (std::find(result.begin(), result.end(), name) == result.end()) result.push_back(name);
        offset = end + 2;
    }
    return result;
}

std::string tqlex_request_id(std::string_view body) {
    static const std::regex pattern(
        R"((?:["']?ReqId["']?)\s*[:=]\s*["']?([0-9]+))",
        std::regex_constants::icase);
    std::cmatch match;
    const std::string text(body);
    return std::regex_search(text.c_str(), match, pattern) ? match[1].str() : "";
}

std::string single_quotes_to_json(const std::string& text) {
    std::string output;
    output.reserve(text.size() + 16);
    bool in_double = false;
    bool escaped = false;
    for (std::size_t index = 0; index < text.size(); ++index) {
        const char ch = text[index];
        if (in_double) {
            output.push_back(ch);
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') in_double = false;
            continue;
        }
        if (ch == '"') {
            in_double = true;
            output.push_back(ch);
            continue;
        }
        if (ch != '\'') {
            output.push_back(ch);
            continue;
        }
        output.push_back('"');
        bool closed = false;
        while (++index < text.size()) {
            const char inner = text[index];
            if (inner == '\'' ) { closed = true; output.push_back('"'); break; }
            if (inner == '"') output += "\\\"";
            else if (inner == '\\' && index + 1 < text.size() && text[index + 1] == '\'') {
                output.push_back('\'');
                ++index;
            } else output.push_back(inner);
        }
        if (!closed) throw Error("unterminated single-quoted request string");
    }
    return output;
}

void replace_all(std::string& text, const std::string& needle, const std::string& replacement) {
    if (needle.empty()) return;
    std::size_t offset = 0;
    while ((offset = text.find(needle, offset)) != std::string::npos) {
        text.replace(offset, needle.size(), replacement);
        offset += replacement.size();
    }
}


}  // namespace tdx::detail
