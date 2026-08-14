#include "ratings_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx::ratings_detail {

std::vector<std::string> nonempty_null_tokens(const std::string& text) {
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start < text.size()) {
        auto end = text.find('\0', start);
        if (end == std::string::npos) end = text.size();
        auto token = trim(text.substr(start, end - start));
        token.erase(std::remove_if(token.begin(), token.end(), [](unsigned char ch) {
            return ch < 0x20;
        }), token.end());
        if (!token.empty()) result.push_back(std::move(token));
        start = end + 1;
    }
    return result;
}

void load_hk_ag_names(const fs::path& path,
                      std::map<std::string, std::string>& result) {
    if (!fs::is_regular_file(path)) return;
    const auto text = decode_gbk(read_bytes(path));
    for (auto line : split(text, '\n')) {
        line = trim(std::move(line));
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto fields = split(line, '|');
        if (fields.size() >= 2 && digits(trim(fields[0]), 5) &&
            !trim(fields[1]).empty())
            result.emplace(trim(fields[0]), trim(fields[1]));
    }
}

void load_hk_target_names(const fs::path& path,
                          std::map<std::string, std::string>& result) {
    if (!fs::is_regular_file(path)) return;
    const auto text = decode_gbk(read_bytes(path));
    for (auto line : split(text, '\n')) {
        line = trim(std::move(line));
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto fields = split(line, ',');
        if (fields.size() >= 4 && trim(fields[2]) == "PH" &&
            digits(trim(fields[3]), 5) && !trim(fields[1]).empty())
            result.emplace(trim(fields[3]), trim(fields[1]));
    }
}

void load_relation_names(const fs::path& path,
                         std::map<std::string, std::string>& result) {
    if (!fs::is_regular_file(path)) return;
    const auto tokens = nonempty_null_tokens(decode_gbk(read_bytes(path)));
    for (std::size_t index = 2; index < tokens.size(); ++index) {
        if (tokens[index] != "H股" || !digits(tokens[index - 2], 5) ||
            tokens[index - 1].empty()) continue;
        result.emplace(tokens[index - 2], tokens[index - 1]);
    }
}

}  // namespace tdx::ratings_detail

namespace tdx {

using namespace ratings_detail;

std::map<std::string, std::string> load_hong_kong_security_names(
    const fs::path& root) {
    std::map<std::string, std::string> result;
    const auto cache = root / "T0002" / "hq_cache";
    load_relation_names(cache / "relation.dat", result);
    load_hk_ag_names(cache / "tdxhkag.cfg", result);
    load_hk_target_names(cache / "code2targ.ini", result);
    return result;
}

}  // namespace tdx
