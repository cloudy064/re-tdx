#include "disclosures_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/security_directory.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>
#include <thread>

namespace tdx::disclosure_detail {
namespace fs = std::filesystem;

DisclosureBackfillSecurity parse_backfill_security(
    std::string value,
    const std::map<std::pair<int, std::string>, Security>& securities,
    std::string name) {
    value = lower_ascii(trim(std::move(value)));
    int id = -1;
    std::string code;
    const auto colon = value.find(':');
    if (colon != std::string::npos) {
        id = mainland_market_id(value.substr(0, colon));
        code = value.substr(colon + 1);
    } else if (value.size() == 8 &&
               (value.rfind("sz", 0) == 0 || value.rfind("sh", 0) == 0 ||
                value.rfind("bj", 0) == 0)) {
        id = mainland_market_id(value.substr(0, 2));
        code = value.substr(2);
    } else if (value.size() == 7 && value.front() >= '0' && value.front() <= '2' &&
               digits(value.substr(1), 6)) {
        id = value.front() - '0';
        code = value.substr(1);
    } else {
        code = value;
        if (code.rfind("92", 0) == 0 || (!code.empty() &&
            (code.front() == '4' || code.front() == '8'))) id = 2;
        else if (!code.empty() && (code.front() == '6' || code.front() == '9')) id = 1;
        else id = 0;
    }
    if (!digits(code, 6)) throw Error("invalid backfill security: " + value);
    const auto found = securities.find({id, code});
    if (name.empty() && found != securities.end()) name = found->second.name;
    return DisclosureBackfillSecurity{market_name(id), code, std::move(name)};
}

void append_backfill_json_securities(
    const Json& value,
    const std::map<std::pair<int, std::string>, Security>& securities,
    std::vector<DisclosureBackfillSecurity>& result) {
    if (value.is_string()) {
        result.push_back(parse_backfill_security(value.as_string(), securities));
        return;
    }
    if (value.is_array()) {
        for (const auto& item : value.as_array())
            append_backfill_json_securities(item, securities, result);
        return;
    }
    if (!value.is_object()) return;
    const auto market = text_value(value, "market");
    const auto code = text_value(value, "code");
    auto name = text_value(value, "name");
    if (name.empty()) name = text_value(value, "security_name");
    if (!market.empty() && digits(code, 6)) {
        result.push_back(parse_backfill_security(market + ":" + code, securities, name));
        return;
    }
    const auto security_id = text_value(value, "security_id");
    if (!security_id.empty()) {
        try {
            result.push_back(parse_backfill_security(security_id, securities, name));
            return;
        } catch (const Error&) {}
    }
    for (const auto* key : {
             "security", "securities", "items", "members", "rows", "matches"}) {
        const auto found = value.as_object().find(key);
        if (found != value.as_object().end())
            append_backfill_json_securities(found->second, securities, result);
    }
}

std::vector<std::string> csv_fields(std::string_view line) {
    std::vector<std::string> result;
    std::string value;
    bool quoted = false;
    for (std::size_t index = 0; index < line.size(); ++index) {
        const char ch = line[index];
        if (ch == '"') {
            if (quoted && index + 1 < line.size() && line[index + 1] == '"') {
                value.push_back('"');
                ++index;
            } else quoted = !quoted;
        } else if (ch == ',' && !quoted) {
            result.push_back(trim(std::move(value)));
            value.clear();
        } else value.push_back(ch);
    }
    result.push_back(trim(std::move(value)));
    return result;
}

std::vector<DisclosureBackfillSecurity> read_backfill_input(
    const fs::path& path,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    auto text = read_text_utf8(path);
    const auto first = text.find_first_not_of(" \t\r\n");
    std::vector<DisclosureBackfillSecurity> result;
    if (first != std::string::npos && (text[first] == '[' || text[first] == '{')) {
        append_backfill_json_securities(Json::parse(text), securities, result);
    } else {
        const auto watchlist = parse_tdx_watchlist_securities(text, securities);
        result.insert(result.end(), watchlist.begin(), watchlist.end());
        std::istringstream lines(text);
        std::vector<std::string> header;
        for (std::string line; std::getline(lines, line);) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            line = trim(std::move(line));
            if (line.empty() || line.front() == '#') continue;
            try {
                const auto columns = csv_fields(line);
                if (header.empty() && std::any_of(columns.begin(), columns.end(),
                    [](const std::string& column) {
                        const auto lowered = lower_ascii(column);
                        return lowered == "code" || lowered == "security_id";
                    })) {
                    header.reserve(columns.size());
                    for (const auto& column : columns)
                        header.push_back(lower_ascii(column));
                    continue;
                }
                if (!header.empty()) {
                    const auto index_of = [&](std::string_view name) -> std::size_t {
                        const auto found = std::find(header.begin(), header.end(), name);
                        return found == header.end()
                            ? static_cast<std::size_t>(-1)
                            : static_cast<std::size_t>(found - header.begin());
                    };
                    const auto security_index = index_of("security_id");
                    const auto market_index = index_of("market");
                    const auto market_id_index = index_of("market_id");
                    const auto code_index = index_of("code");
                    auto name_index = index_of("security_name");
                    if (name_index == static_cast<std::size_t>(-1))
                        name_index = index_of("name");
                    const auto name = name_index < columns.size()
                        ? columns[name_index] : std::string{};
                    if (security_index < columns.size())
                        result.push_back(parse_backfill_security(
                            columns[security_index], securities, name));
                    else if (code_index < columns.size() &&
                             (market_index < columns.size() ||
                              market_id_index < columns.size()))
                        result.push_back(parse_backfill_security(
                            columns[market_index < columns.size()
                                ? market_index : market_id_index] + ":" +
                            columns[code_index], securities, name));
                    continue;
                }
                if (columns.size() >= 2)
                    result.push_back(parse_backfill_security(
                        columns[0] + ":" + columns[1], securities,
                        columns.size() >= 3 ? columns[2] : std::string{}));
                else
                    result.push_back(parse_backfill_security(line, securities));
            } catch (const Error&) {
                // Headers, comments and non-mainland expansion symbols are ignored.
            }
        }
    }
    if (result.empty())
        throw Error("backfill input contains no mainland six-digit securities: " +
                    path_utf8(path));
    return result;
}

std::vector<DisclosureBackfillSecurity> block_backfill_securities(
    const BlockData& data, const std::string& query) {
    const auto wanted = lower_ascii(trim(query));
    std::vector<const Block*> matches;
    for (const auto& block : data.blocks)
        if (lower_ascii(block.block_id) == wanted ||
            lower_ascii(block.block_code) == wanted || lower_ascii(block.name) == wanted)
            matches.push_back(&block);
    if (matches.empty())
        for (const auto& block : data.blocks)
            if (lower_ascii(block.block_id).find(wanted) != std::string::npos ||
                lower_ascii(block.name).find(wanted) != std::string::npos)
                matches.push_back(&block);
    if (matches.empty()) throw Error("backfill block not found: " + query);
    if (matches.size() != 1)
        throw Error("backfill block is ambiguous (" + std::to_string(matches.size()) +
                    " matches); use the exact block_id or block_code");
    std::set<std::string> selected{matches.front()->block_id};
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& block : data.blocks)
            if (!block.parent_block_id.empty() && selected.count(block.parent_block_id) &&
                selected.insert(block.block_id).second) changed = true;
    }
    std::map<std::pair<int, std::string>, DisclosureBackfillSecurity> unique;
    for (const auto& member : data.members)
        if (selected.count(member.block_id))
            unique[{member.market_id, member.code}] = DisclosureBackfillSecurity{
                lower_ascii(member.market), member.code, member.security_name};
    std::vector<DisclosureBackfillSecurity> result;
    for (auto& [key, security] : unique) {
        (void)key;
        result.push_back(std::move(security));
    }
    if (result.empty()) throw Error("backfill block has no mainland securities: " + query);
    return result;
}

std::vector<DisclosureBackfillSecurity> deduplicate_backfill_securities(
    std::vector<DisclosureBackfillSecurity> values) {
    std::map<std::string, DisclosureBackfillSecurity> unique;
    for (auto& security : values) {
        const int id = mainland_market_id(security.market);
        const auto key = market_prefix(id) + security.code;
        auto found = unique.find(key);
        if (found == unique.end()) unique.emplace(key, std::move(security));
        else if (found->second.name.empty() && !security.name.empty())
            found->second.name = std::move(security.name);
    }
    std::vector<DisclosureBackfillSecurity> result;
    for (auto& [key, security] : unique) {
        (void)key;
        result.push_back(std::move(security));
    }
    return result;
}

std::string normalized_absolute_path(const fs::path& path) {
    std::error_code error;
    auto absolute = fs::absolute(path, error);
    if (error) absolute = path;
    return lower_ascii(path_utf8(absolute.lexically_normal()));
}

}  // namespace tdx::disclosure_detail
