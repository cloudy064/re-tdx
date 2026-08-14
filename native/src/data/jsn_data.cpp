#include "tdx/jsn_data.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string relative_resource(const fs::path& path, const fs::path& root) {
    std::error_code error;
    auto value = path_utf8(fs::relative(path, root, error));
    if (error) value = path_utf8(path.filename());
    std::replace(value.begin(), value.end(), '\\', '/');
    return value;
}

std::vector<fs::path> jsn_paths(const fs::path& root) {
    if (!fs::is_directory(root)) throw Error("JSN input directory does not exist: " + path_utf8(root));
    std::vector<fs::path> result;
    std::error_code error;
    for (fs::recursive_directory_iterator iterator(root, error), end; iterator != end; iterator.increment(error)) {
        if (error) throw Error("cannot scan JSN directory: " + error.message());
        if (!iterator->is_regular_file()) continue;
        auto extension = lower_ascii(path_utf8(iterator->path().extension()));
        if (extension == ".jsn") result.push_back(iterator->path());
    }
    std::sort(result.begin(), result.end(), [&](const fs::path& left, const fs::path& right) {
        return lower_ascii(relative_resource(left, root)) < lower_ascii(relative_resource(right, root));
    });
    return result;
}

int header_index(const std::vector<std::string>& headers, std::string_view name) {
    const auto found = std::find(headers.begin(), headers.end(), name);
    return found == headers.end() ? -1 : static_cast<int>(found - headers.begin());
}

std::set<std::pair<std::string, std::string>> member_keys(const Json& value) {
    std::set<std::pair<std::string, std::string>> result;
    for (auto token : split(jsn_scalar_text(value), ',')) {
        token = trim(std::move(token));
        if (token.empty()) continue;
        const auto separator = token.find('|');
        if (separator == std::string::npos || separator + 1 == token.size()) continue;
        result.insert({trim(token.substr(0, separator)), trim(token.substr(separator + 1))});
    }
    return result;
}

std::string normalized_market(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "sz") return "0";
    if (value == "sh") return "1";
    if (value == "bj") return "2";
    if (value.empty() || !std::all_of(value.begin(), value.end(), [](char ch) { return ch >= '0' && ch <= '9'; }))
        throw Error("--market must be a numeric ID or sz/sh/bj");
    return value;
}

Json row_object(const JsnTable& table, const Json::Array& row) {
    Json value = Json::object();
    for (std::size_t index = 0; index < table.headers.size(); ++index)
        value[table.headers[index]] = row[index];
    return value;
}

int parse_limit(const std::string& text) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size() || value < 0) throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error("--max-results must be a non-negative integer");
    }
}

void catalog_help() {
    std::cout <<
        "Usage: tdx-tool jsn catalog [options]\n\n"
        "Offline native catalog of downloaded JSN tables.\n\n"
        "Options:\n"
        "  --input-dir PATH        Default output/tdx-jsn\n"
        "  --output PATH           Default output/tdx-jsn-catalog-native.json\n"
        "  --compact               Compact JSON\n";
}

void query_help() {
    std::cout <<
        "Usage: tdx-tool jsn query --market ID --code CODE [options]\n\n"
        "Scan every downloaded JSN and return direct, referenced, and $S_ZQDM matches.\n\n"
        "Options:\n"
        "  --input-dir PATH        Default output/tdx-jsn\n"
        "  --output PATH           Write JSON instead of stdout\n"
        "  --max-results N         Safety limit; 0 means unlimited (default 10000)\n"
        "  --compact               Compact JSON\n";
}

}  // namespace

std::string jsn_scalar_text(const Json& value) {
    if (value.is_null()) return "";
    if (value.is_string()) return value.as_string();
    if (value.is_bool()) return value.as_bool() ? "True" : "False";
    if (value.is_number()) {
        const double number = value.as_number();
        std::ostringstream output;
        if (std::isfinite(number) && std::floor(number) == number && std::fabs(number) <= 9.0e15)
            output << std::fixed << std::setprecision(0) << number;
        else output << std::setprecision(15) << number;
        return output.str();
    }
    throw Error("JSN table cell is not scalar");
}

std::vector<JsnTable> load_jsn_tables(const fs::path& path) {
    const auto document = Json::parse(decode_gbk(read_bytes(path)));
    if (!document.is_array()) throw Error("JSN root is not an array: " + path_utf8(path));
    std::vector<JsnTable> result;
    for (const auto& group : document.as_array()) {
        if (!group.is_object()) throw Error("JSN result group is not an object");
        const auto& header_value = group.at("colheader");
        const auto& data_value = group.at("data");
        if (!header_value.is_array() || !data_value.is_array())
            throw Error("JSN result group lacks colheader/data arrays");
        JsnTable table;
        for (const auto& header : header_value.as_array()) table.headers.push_back(jsn_scalar_text(header));
        for (const auto& row : data_value.as_array()) {
            if (!row.is_array() || row.as_array().size() != table.headers.size())
                throw Error("JSN row width differs from colheader: " + path_utf8(path));
            table.rows.push_back(row.as_array());
        }
        result.push_back(std::move(table));
    }
    return result;
}

struct JsnIndex::Impl {
    struct Resource {
        std::string name;
        std::uint64_t size{};
        std::vector<JsnTable> tables;
    };
    struct Match {
        std::size_t resource{};
        std::size_t group{};
        std::size_t row{};
        std::uint8_t mask{};
    };
    std::vector<Resource> resources;
    std::map<std::string, std::vector<Match>> by_security;
    Json catalog_document;
};

JsnIndex::JsnIndex(const fs::path& input) : impl_(std::make_unique<Impl>()) {
    Json catalog_rows = Json::array();
    std::uint64_t total_bytes = 0, total_rows = 0;
    for (const auto& path : jsn_paths(input)) {
        Impl::Resource resource;
        resource.name = relative_resource(path, input);
        resource.size = static_cast<std::uint64_t>(fs::file_size(path));
        resource.tables = load_jsn_tables(path);
        const auto resource_index = impl_->resources.size();
        std::set<std::string> columns;
        std::set<std::pair<std::string, std::string>> securities, members;
        std::uint64_t rows = 0, security_links = 0;
        for (std::size_t group = 0; group < resource.tables.size(); ++group) {
            const auto& table = resource.tables[group];
            columns.insert(table.headers.begin(), table.headers.end());
            const std::array<std::pair<std::string_view, std::string_view>, 3> pairs{{
                {"$SC", "$ZQDM"}, {"$SC1", "$ZQDM1"}, {"sc", "$ZQDM"},
            }};
            std::array<std::pair<int, int>, 3> pair_indexes{};
            for (std::size_t pair = 0; pair < pairs.size(); ++pair)
                pair_indexes[pair] = {header_index(table.headers, pairs[pair].first),
                                      header_index(table.headers, pairs[pair].second)};
            const int member_index = header_index(table.headers, "$S_ZQDM");
            for (std::size_t row_index = 0; row_index < table.rows.size(); ++row_index) {
                ++rows;
                const auto& row = table.rows[row_index];
                std::map<std::string, std::uint8_t> row_matches;
                for (std::size_t pair = 0; pair < pairs.size(); ++pair) {
                    const auto [market_index, code_index] = pair_indexes[pair];
                    if (market_index < 0 || code_index < 0) continue;
                    const auto market = jsn_scalar_text(row[market_index]);
                    const auto code = jsn_scalar_text(row[code_index]);
                    if (market.empty() || code.empty()) continue;
                    securities.insert({market, code});
                    ++security_links;
                    row_matches[market + "|" + code] |= static_cast<std::uint8_t>(1U << pair);
                }
                if (member_index >= 0) {
                    const auto keys = member_keys(row[member_index]);
                    members.insert(keys.begin(), keys.end());
                    for (const auto& [market, code] : keys)
                        row_matches[market + "|" + code] |= 0x08;
                }
                for (const auto& [key, mask] : row_matches)
                    impl_->by_security[key].push_back(
                        Impl::Match{resource_index, group, row_index, mask});
            }
        }
        total_bytes += resource.size;
        total_rows += rows;
        Json item = Json::object();
        item["resource"] = resource.name;
        item["size"] = resource.size;
        item["groups"] = static_cast<std::uint64_t>(resource.tables.size());
        item["rows"] = rows;
        item["unique_securities"] = static_cast<std::uint64_t>(securities.size());
        item["security_links"] = security_links;
        item["unique_member_securities"] = static_cast<std::uint64_t>(members.size());
        Json headers = Json::array();
        for (const auto& column : columns) headers.push_back(column);
        item["columns"] = std::move(headers);
        catalog_rows.push_back(std::move(item));
        impl_->resources.push_back(std::move(resource));
    }
    impl_->catalog_document = Json::object();
    impl_->catalog_document["schema"] = "tdx-jsn-catalog-native-v1";
    Json counts = Json::object();
    counts["resources"] = static_cast<std::uint64_t>(impl_->resources.size());
    counts["bytes"] = total_bytes;
    counts["rows"] = total_rows;
    counts["security_keys"] = static_cast<std::uint64_t>(impl_->by_security.size());
    impl_->catalog_document["counts"] = std::move(counts);
    impl_->catalog_document["resources"] = std::move(catalog_rows);
}

JsnIndex::~JsnIndex() = default;
JsnIndex::JsnIndex(JsnIndex&&) noexcept = default;
JsnIndex& JsnIndex::operator=(JsnIndex&&) noexcept = default;

Json JsnIndex::query_security(const std::string& market_value,
                              const std::string& code_value,
                              int max_results) const {
    const auto market = normalized_market(market_value);
    const auto code = trim(code_value);
    if (code.size() != 6 || !std::all_of(code.begin(), code.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) throw Error("code must contain exactly six digits");
    if (max_results < 1 || max_results > 10000)
        throw Error("max_results must be in 1..10000");
    const auto found = impl_->by_security.find(market + "|" + code);
    Json matches = Json::array();
    std::set<std::string> matched_resources;
    const std::size_t total = found == impl_->by_security.end() ? 0 : found->second.size();
    if (found != impl_->by_security.end()) {
        for (const auto& indexed : found->second)
            matched_resources.insert(impl_->resources.at(indexed.resource).name);
        for (const auto& indexed : found->second) {
            if (matches.size() >= static_cast<std::size_t>(max_results)) break;
            const auto& resource = impl_->resources.at(indexed.resource);
            const auto& table = resource.tables.at(indexed.group);
            const auto& row = table.rows.at(indexed.row);
            Json matched_by = Json::array();
            const std::array<std::pair<std::string, std::string>, 3> pairs{{
                {"$SC", "$ZQDM"}, {"$SC1", "$ZQDM1"}, {"sc", "$ZQDM"},
            }};
            for (std::size_t pair = 0; pair < pairs.size(); ++pair) {
                if (!(indexed.mask & (1U << pair))) continue;
                Json fields = Json::array();
                fields.push_back(pairs[pair].first);
                fields.push_back(pairs[pair].second);
                matched_by.push_back(std::move(fields));
            }
            if (indexed.mask & 0x08) {
                Json fields = Json::array();
                fields.push_back("$S_ZQDM");
                matched_by.push_back(std::move(fields));
            }
            Json match = Json::object();
            match["resource"] = resource.name;
            match["group"] = static_cast<std::uint64_t>(indexed.group);
            match["row_index"] = static_cast<std::uint64_t>(indexed.row);
            match["matched_by"] = std::move(matched_by);
            match["record"] = row_object(table, row);
            matches.push_back(std::move(match));
        }
    }
    Json document = Json::object();
    document["schema"] = "tdx-jsn-security-query-native-v2";
    document["market"] = market;
    document["code"] = code;
    document["resource_count"] = static_cast<std::uint64_t>(matched_resources.size());
    document["match_count"] = static_cast<std::uint64_t>(total);
    document["returned"] = static_cast<std::uint64_t>(matches.size());
    document["truncated"] = total > matches.size();
    document["matches"] = std::move(matches);
    return document;
}

Json JsnIndex::catalog() const { return impl_->catalog_document; }
std::size_t JsnIndex::resource_count() const noexcept { return impl_->resources.size(); }
std::size_t JsnIndex::security_key_count() const noexcept { return impl_->by_security.size(); }

Json catalog_jsn_document(const fs::path& input) {
    Json resources = Json::array();
    std::uint64_t total_bytes = 0, total_rows = 0;
    for (const auto& path : jsn_paths(input)) {
        const auto tables = load_jsn_tables(path);
        std::set<std::string> columns;
        std::set<std::pair<std::string, std::string>> securities, members;
        std::uint64_t rows = 0, security_links = 0;
        for (const auto& table : tables) {
            columns.insert(table.headers.begin(), table.headers.end());
            const std::array<std::pair<std::string_view, std::string_view>, 3> pairs{{
                {"$SC", "$ZQDM"}, {"$SC1", "$ZQDM1"}, {"sc", "$ZQDM"},
            }};
            const int member_index = header_index(table.headers, "$S_ZQDM");
            for (const auto& row : table.rows) {
                ++rows;
                for (const auto& [market_field, code_field] : pairs) {
                    const int market_index = header_index(table.headers, market_field);
                    const int code_index = header_index(table.headers, code_field);
                    if (market_index < 0 || code_index < 0) continue;
                    const auto row_market = jsn_scalar_text(row[market_index]);
                    const auto row_code = jsn_scalar_text(row[code_index]);
                    if (!row_market.empty() && !row_code.empty()) {
                        securities.insert({row_market, row_code});
                        ++security_links;
                    }
                }
                if (member_index >= 0) {
                    const auto keys = member_keys(row[member_index]);
                    members.insert(keys.begin(), keys.end());
                }
            }
        }
        const auto size = static_cast<std::uint64_t>(fs::file_size(path));
        total_bytes += size;
        total_rows += rows;
        Json item = Json::object();
        item["resource"] = relative_resource(path, input);
        item["size"] = size;
        item["groups"] = static_cast<std::uint64_t>(tables.size());
        item["rows"] = rows;
        item["unique_securities"] = static_cast<std::uint64_t>(securities.size());
        item["security_links"] = security_links;
        item["unique_member_securities"] = static_cast<std::uint64_t>(members.size());
        Json headers = Json::array();
        for (const auto& column : columns) headers.push_back(column);
        item["columns"] = std::move(headers);
        resources.push_back(std::move(item));
    }
    Json document = Json::object();
    document["schema"] = "tdx-jsn-catalog-native-v1";
    Json counts = Json::object();
    counts["resources"] = static_cast<std::uint64_t>(resources.size());
    counts["bytes"] = total_bytes;
    counts["rows"] = total_rows;
    document["counts"] = std::move(counts);
    document["resources"] = std::move(resources);
    return document;
}

Json query_jsn_security_document(const fs::path& input, const std::string& market_value,
                                 const std::string& code_value, int max_results) {
    const auto market = normalized_market(market_value);
    const auto code = trim(code_value);
    if (code.size() != 6 || !std::all_of(code.begin(), code.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) throw Error("code must contain exactly six digits");
    if (max_results < 1 || max_results > 10000)
        throw Error("max_results must be in 1..10000");
    Json matches = Json::array();
    std::set<std::string> matched_resources;
    bool truncated = false;
    for (const auto& path : jsn_paths(input)) {
        if (truncated) break;
        const auto resource = relative_resource(path, input);
        const auto tables = load_jsn_tables(path);
        for (std::size_t group = 0; group < tables.size() && !truncated; ++group) {
            const auto& table = tables[group];
            const std::array<std::pair<std::string_view, std::string_view>, 3> pairs{{
                {"$SC", "$ZQDM"}, {"$SC1", "$ZQDM1"}, {"sc", "$ZQDM"},
            }};
            const int member_index = header_index(table.headers, "$S_ZQDM");
            for (std::size_t row_index = 0; row_index < table.rows.size(); ++row_index) {
                Json matched_by = Json::array();
                const auto& row = table.rows[row_index];
                for (const auto& [market_field, code_field] : pairs) {
                    const int market_index = header_index(table.headers, market_field);
                    const int code_index = header_index(table.headers, code_field);
                    if (market_index < 0 || code_index < 0) continue;
                    if (jsn_scalar_text(row[market_index]) == market &&
                        jsn_scalar_text(row[code_index]) == code) {
                        Json fields = Json::array();
                        fields.push_back(std::string(market_field));
                        fields.push_back(std::string(code_field));
                        matched_by.push_back(std::move(fields));
                    }
                }
                if (member_index >= 0 && member_keys(row[member_index]).count({market, code})) {
                    Json fields = Json::array();
                    fields.push_back("$S_ZQDM");
                    matched_by.push_back(std::move(fields));
                }
                if (matched_by.size() == 0) continue;
                Json match = Json::object();
                match["resource"] = resource;
                match["group"] = static_cast<std::uint64_t>(group);
                match["row_index"] = static_cast<std::uint64_t>(row_index);
                match["matched_by"] = std::move(matched_by);
                match["record"] = row_object(table, row);
                matches.push_back(std::move(match));
                matched_resources.insert(resource);
                if (matches.size() >= static_cast<std::size_t>(max_results)) {
                    truncated = true;
                    break;
                }
            }
        }
    }
    Json document = Json::object();
    document["schema"] = "tdx-jsn-security-query-native-v1";
    document["market"] = market;
    document["code"] = code;
    document["resource_count"] = static_cast<std::uint64_t>(matched_resources.size());
    document["match_count"] = static_cast<std::uint64_t>(matches.size());
    document["truncated"] = truncated;
    document["matches"] = std::move(matches);
    return document;
}

int command_jsn_catalog(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) { catalog_help(); return 0; }
    const auto input_text = args.take_option("--input-dir", "output/tdx-jsn");
    const auto output_text = args.take_option("--output", "output/tdx-jsn-catalog-native.json");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto input = from_utf8(input_text);
    Json resources = Json::array();
    std::uint64_t total_bytes = 0, total_rows = 0;
    for (const auto& path : jsn_paths(input)) {
        const auto tables = load_jsn_tables(path);
        std::set<std::string> columns;
        std::set<std::pair<std::string, std::string>> securities, members;
        std::uint64_t rows = 0, security_links = 0;
        for (const auto& table : tables) {
            columns.insert(table.headers.begin(), table.headers.end());
            const std::array<std::pair<std::string_view, std::string_view>, 3> pairs{{
                {"$SC", "$ZQDM"}, {"$SC1", "$ZQDM1"}, {"sc", "$ZQDM"},
            }};
            const int member_index = header_index(table.headers, "$S_ZQDM");
            for (const auto& row : table.rows) {
                ++rows;
                for (const auto& [market_field, code_field] : pairs) {
                    const int market_index = header_index(table.headers, market_field);
                    const int code_index = header_index(table.headers, code_field);
                    if (market_index < 0 || code_index < 0) continue;
                    const auto market = jsn_scalar_text(row[market_index]);
                    const auto code = jsn_scalar_text(row[code_index]);
                    if (!market.empty() && !code.empty()) {
                        securities.insert({market, code});
                        ++security_links;
                    }
                }
                if (member_index >= 0) {
                    const auto keys = member_keys(row[member_index]);
                    members.insert(keys.begin(), keys.end());
                }
            }
        }
        const auto size = static_cast<std::uint64_t>(fs::file_size(path));
        total_bytes += size;
        total_rows += rows;
        Json item = Json::object();
        item["resource"] = relative_resource(path, input);
        item["size"] = size;
        item["md5"] = md5_file(path);
        item["groups"] = static_cast<std::uint64_t>(tables.size());
        item["rows"] = rows;
        item["unique_securities"] = static_cast<std::uint64_t>(securities.size());
        item["security_links"] = security_links;
        item["unique_member_securities"] = static_cast<std::uint64_t>(members.size());
        Json headers = Json::array();
        for (const auto& column : columns) headers.push_back(column);
        item["columns"] = std::move(headers);
        resources.push_back(std::move(item));
    }
    Json document = Json::object();
    document["schema"] = "tdx-jsn-catalog-native-v1";
    Json counts = Json::object();
    counts["resources"] = static_cast<std::uint64_t>(resources.size());
    counts["bytes"] = total_bytes;
    counts["rows"] = total_rows;
    document["counts"] = std::move(counts);
    document["resources"] = std::move(resources);
    const auto output = from_utf8(output_text);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "cataloged " << document.at("counts").at("resources").as_number()
              << " JSN resources, " << total_rows << " rows, " << total_bytes
              << " bytes -> " << path_utf8(output) << '\n';
    return 0;
}

int command_jsn_query(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) { query_help(); return 0; }
    const auto input_text = args.take_option("--input-dir", "output/tdx-jsn");
    const auto market = normalized_market(args.take_option("--market"));
    const auto code = trim(args.take_option("--code"));
    const auto output_text = args.take_option("--output");
    const int max_results = parse_limit(args.take_option("--max-results", "10000"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    if (code.empty()) throw Error("jsn query requires --code");
    const auto input = from_utf8(input_text);
    Json matches = Json::array();
    std::set<std::string> matched_resources;
    bool truncated = false;
    for (const auto& path : jsn_paths(input)) {
        if (truncated) break;
        const auto resource = relative_resource(path, input);
        const auto tables = load_jsn_tables(path);
        for (std::size_t group = 0; group < tables.size() && !truncated; ++group) {
            const auto& table = tables[group];
            const std::array<std::pair<std::string_view, std::string_view>, 3> pairs{{
                {"$SC", "$ZQDM"}, {"$SC1", "$ZQDM1"}, {"sc", "$ZQDM"},
            }};
            const int member_index = header_index(table.headers, "$S_ZQDM");
            for (std::size_t row_index = 0; row_index < table.rows.size(); ++row_index) {
                Json matched_by = Json::array();
                const auto& row = table.rows[row_index];
                for (const auto& [market_field, code_field] : pairs) {
                    const int market_index = header_index(table.headers, market_field);
                    const int code_index = header_index(table.headers, code_field);
                    if (market_index < 0 || code_index < 0) continue;
                    if (jsn_scalar_text(row[market_index]) == market &&
                        jsn_scalar_text(row[code_index]) == code) {
                        Json fields = Json::array();
                        fields.push_back(std::string(market_field));
                        fields.push_back(std::string(code_field));
                        matched_by.push_back(std::move(fields));
                    }
                }
                if (member_index >= 0 && member_keys(row[member_index]).count({market, code})) {
                    Json fields = Json::array();
                    fields.push_back("$S_ZQDM");
                    matched_by.push_back(std::move(fields));
                }
                if (matched_by.size() == 0) continue;
                Json match = Json::object();
                match["resource"] = resource;
                match["group"] = static_cast<std::uint64_t>(group);
                match["row_index"] = static_cast<std::uint64_t>(row_index);
                match["matched_by"] = std::move(matched_by);
                match["record"] = row_object(table, row);
                matches.push_back(std::move(match));
                matched_resources.insert(resource);
                if (max_results && matches.size() >= static_cast<std::size_t>(max_results)) {
                    truncated = true;
                    break;
                }
            }
        }
    }
    Json document = Json::object();
    document["schema"] = "tdx-jsn-security-query-native-v1";
    document["market"] = market;
    document["code"] = code;
    document["resource_count"] = static_cast<std::uint64_t>(matched_resources.size());
    document["match_count"] = static_cast<std::uint64_t>(matches.size());
    document["truncated"] = truncated;
    document["matches"] = std::move(matches);
    const auto text = document.dump(compact ? -1 : 2) + "\n";
    if (output_text.empty()) std::cout << text;
    else {
        const auto output = from_utf8(output_text);
        atomic_write_text(output, text);
        std::cout << "matched " << document.at("match_count").as_number() << " rows in "
                  << document.at("resource_count").as_number() << " resources -> "
                  << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
