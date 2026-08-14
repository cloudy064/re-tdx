#include "tdx/hyzt.hpp"

#include "tdx/blocks.hpp"
#include "tdx/common.hpp"
#include "tdx/json.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

const std::set<std::string> required_headers{
    "$ZQDM", "$SC", "TDXHY", "$ZQDM1", "$SC1", "hyPE", "hyPB", "$S_ZQDM", "sszt"};

struct Industry {
    std::string code;
    int market_id{};
    std::string name, pe_ttm, pb_mrq;
    std::set<std::string> declared_members;
};
struct Stock {
    std::string security_id;
    int market_id{};
    std::string code, industry_code, industry_name, name;
    std::vector<std::string> themes;
};

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string value_text(const Json& value) {
    if (value.is_null()) return "";
    if (value.is_string()) return value.as_string();
    if (value.is_bool()) return value.as_bool() ? "True" : "False";
    if (value.is_number()) {
        const double number = value.as_number();
        std::ostringstream output;
        if (std::isfinite(number) && std::floor(number) == number &&
            std::fabs(number) <= 9.0e15) output << std::fixed << std::setprecision(0) << number;
        else output << std::setprecision(15) << number;
        return output.str();
    }
    throw Error("JSN table cell is not scalar");
}

int market_id_from(std::string value, std::string_view field) {
    value = trim(std::move(value));
    try {
        std::size_t used = 0;
        const int id = std::stoi(value, &used);
        if (used != value.size() || id < 0) throw std::invalid_argument("market");
        return id;
    } catch (...) {
        throw Error(std::string(field) + " has an invalid market ID: " + value);
    }
}

std::string security_id(int market_id, const std::string& code) {
    if (market_id == 0) return "SZ" + code;
    if (market_id == 1) return "SH" + code;
    if (market_id == 2) return "BJ" + code;
    return std::to_string(market_id) + code;
}

std::set<std::string> parse_member_list(const std::string& value) {
    std::set<std::string> result;
    for (auto item : split(value, ',')) {
        item = trim(std::move(item));
        if (item.empty()) continue;
        const auto bar = item.find('|');
        if (bar == std::string::npos || bar + 1 == item.size())
            throw Error("invalid industry member token: " + item);
        result.insert(security_id(market_id_from(item.substr(0, bar), "$S_ZQDM"),
                                  item.substr(bar + 1)));
    }
    return result;
}

std::vector<std::string> parse_themes(const std::string& value) {
    static const std::string delimiter = "、";
    std::vector<std::string> result;
    std::set<std::string> seen;
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto end = value.find(delimiter, start);
        auto theme = trim(value.substr(start, end == std::string::npos ? value.size() - start : end - start));
        if (!theme.empty() && seen.insert(theme).second) result.push_back(std::move(theme));
        if (end == std::string::npos) break;
        start = end + delimiter.size();
    }
    return result;
}

std::vector<std::map<std::string, std::string>> load_rows(const fs::path& path) {
    const auto document = Json::parse(decode_gbk(read_bytes(path)));
    if (!document.is_array()) throw Error("industry/theme JSN root is not an array");
    std::vector<std::map<std::string, std::string>> result;
    std::size_t group_index = 0;
    for (const auto& group : document.as_array()) {
        if (!group.is_object()) throw Error("JSN result group is not an object");
        const auto& header_json = group.at("colheader");
        const auto& data_json = group.at("data");
        if (!header_json.is_array() || !data_json.is_array())
            throw Error("JSN group has no colheader/data arrays");
        std::vector<std::string> headers;
        std::set<std::string> header_set;
        for (const auto& item : header_json.as_array()) {
            auto name = value_text(item);
            headers.push_back(name);
            header_set.insert(std::move(name));
        }
        for (const auto& required : required_headers)
            if (!header_set.count(required))
                throw Error("JSN group " + std::to_string(group_index) + " lacks field " + required);
        std::size_t row_index = 0;
        for (const auto& row : data_json.as_array()) {
            if (!row.is_array() || row.as_array().size() != headers.size())
                throw Error("JSN table row width mismatch at group/row " +
                            std::to_string(group_index) + "/" + std::to_string(row_index));
            std::map<std::string, std::string> record;
            for (std::size_t column = 0; column < headers.size(); ++column)
                record[headers[column]] = value_text(row.as_array()[column]);
            result.push_back(std::move(record));
            ++row_index;
        }
        ++group_index;
    }
    return result;
}

Json string_array(const std::set<std::string>& values) {
    Json result = Json::array();
    for (const auto& value : values) result.push_back(value);
    return result;
}
Json string_array(const std::vector<std::string>& values) {
    Json result = Json::array();
    for (const auto& value : values) result.push_back(value);
    return result;
}

std::optional<double> optional_decimal(std::string value,
                                       std::string_view field) {
    value = trim(std::move(value));
    if (value.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const double result = std::stod(value, &used);
        if (used != value.size() || !std::isfinite(result))
            throw std::invalid_argument("number");
        return result;
    } catch (...) {
        throw Error(std::string(field) + " has an invalid number: " + value);
    }
}

fs::path industry_valuation_resource_path(const fs::path& jsn_root) {
    if (!jsn_root.empty() && fs::is_regular_file(jsn_root)) return jsn_root;
    const auto root = jsn_root.empty() ? fs::path("output") / "tdx-jsn" : jsn_root;
    return root / "list" / "func_gx_hyzt101_1.jsn";
}

void print_help() {
    std::cout <<
        "Usage: tdx-tool hyzt extract [options]\n\n"
        "Build the industry tree, stock-industry links, and theme reverse index from JSN.\n\n"
        "Options:\n"
        "  --input PATH            Default output/tdx-jsn/list/func_gx_hyzt101_1.jsn\n"
        "  --root PATH             TDX root for local industry hierarchy/security names\n"
        "  --output PATH           Default output/tdx-hyzt-model-native.json\n"
        "  --no-tree               Keep cloud leaf industries only\n"
        "  --no-themes             Omit theme reverse index\n"
        "  --compact               Compact JSON\n";
}

}  // namespace

std::shared_ptr<const IndustryValuationCatalog>
load_industry_valuation_catalog(const std::filesystem::path& jsn_root) {
    const auto source = fs::absolute(industry_valuation_resource_path(jsn_root));
    if (!fs::is_regular_file(source))
        throw Error("industry valuation JSN resource is missing: " + path_utf8(source));
    std::error_code error;
    const auto size = fs::file_size(source, error);
    if (error) throw Error("cannot stat industry valuation JSN resource: " + path_utf8(source));
    const auto write_time = fs::last_write_time(source, error);
    if (error) throw Error("cannot stat industry valuation JSN timestamp: " + path_utf8(source));

    struct CacheEntry {
        std::uintmax_t size{};
        fs::file_time_type write_time{};
        std::shared_ptr<const IndustryValuationCatalog> catalog;
    };
    static std::mutex cache_mutex;
    static std::map<std::string, CacheEntry> cache;
    const auto key = path_utf8(source.lexically_normal());
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        const auto found = cache.find(key);
        if (found != cache.end() && found->second.size == size &&
            found->second.write_time == write_time)
            return found->second.catalog;
    }

    const auto rows = load_rows(source);
    auto catalog = std::make_shared<IndustryValuationCatalog>();
    catalog->source_path = source;
    catalog->source_size = static_cast<std::uint64_t>(size);
    catalog->row_count = static_cast<std::uint64_t>(rows.size());
    std::map<std::string, std::set<std::string>> members;
    for (const auto& row : rows) {
        const int market_id = market_id_from(row.at("$SC"), "$SC");
        const auto stock_code = trim(row.at("$ZQDM"));
        const int industry_market = market_id_from(row.at("$SC1"), "$SC1");
        const auto industry_code = trim(row.at("$ZQDM1"));
        const auto industry_name = trim(row.at("TDXHY"));
        if (stock_code.empty() || industry_code.empty() || industry_name.empty())
            throw Error("stock code, industry code, or industry name is empty");
        const IndustryValuationRecord candidate{
            industry_market, industry_code, industry_name,
            optional_decimal(row.at("hyPE"), "hyPE"),
            optional_decimal(row.at("hyPB"), "hyPB"), 0};
        const auto found = catalog->records.find(industry_code);
        if (found == catalog->records.end()) {
            catalog->records.emplace(industry_code, candidate);
        } else if (found->second.market_id != candidate.market_id ||
                   found->second.name != candidate.name ||
                   found->second.pe != candidate.pe ||
                   found->second.pb_mrq != candidate.pb_mrq) {
            throw Error("inconsistent industry valuation records: " + industry_code);
        }
        members[industry_code].insert(security_id(market_id, stock_code));
    }
    for (auto& [code, record] : catalog->records)
        record.member_count = static_cast<std::uint64_t>(members[code].size());

    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        cache[key] = CacheEntry{size, write_time, catalog};
    }
    return catalog;
}

int command_hyzt_extract(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) { print_help(); return 0; }
    const auto input_text = args.take_option(
        "--input", "output/tdx-jsn/list/func_gx_hyzt101_1.jsn");
    const auto root_text = args.take_option("--root");
    const auto output_text = args.take_option("--output", "output/tdx-hyzt-model-native.json");
    const bool no_tree = args.take_flag("--no-tree");
    const bool no_themes = args.take_flag("--no-themes");
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    const auto rows = load_rows(from_utf8(input_text));
    std::map<std::string, Industry> industries;
    std::map<std::string, std::set<std::string>> direct_members;
    std::vector<Stock> stocks;
    std::set<std::string> seen_stocks;
    for (const auto& row : rows) {
        const int market_id = market_id_from(row.at("$SC"), "$SC");
        const auto code = trim(row.at("$ZQDM"));
        const int industry_market = market_id_from(row.at("$SC1"), "$SC1");
        const auto industry_code = trim(row.at("$ZQDM1"));
        const auto industry_name = trim(row.at("TDXHY"));
        if (code.empty() || industry_code.empty() || industry_name.empty())
            throw Error("stock code, industry code, or industry name is empty");
        const auto id = security_id(market_id, code);
        if (!seen_stocks.insert(id).second) throw Error("duplicate stock in industry JSN: " + id);
        Industry candidate{industry_code, industry_market, industry_name, row.at("hyPE"), row.at("hyPB"),
                           parse_member_list(row.at("$S_ZQDM"))};
        const auto found = industries.find(industry_code);
        if (found == industries.end()) industries[industry_code] = candidate;
        else if (found->second.market_id != candidate.market_id || found->second.name != candidate.name ||
                 found->second.declared_members != candidate.declared_members)
            throw Error("inconsistent industry records: " + industry_code);
        direct_members[industry_code].insert(id);
        stocks.push_back(Stock{id, market_id, code, industry_code, industry_name, "",
                               parse_themes(row.at("sszt"))});
    }
    for (const auto& [code, industry] : industries)
        if (direct_members[code] != industry.declared_members)
            throw Error(code + " industry membership mismatch: row-derived " +
                        std::to_string(direct_members[code].size()) + ", declared " +
                        std::to_string(industry.declared_members.size()));

    BlockData local;
    if (!no_tree) {
        const auto root = find_tdx_root(root_text.empty() ? fs::path{} : from_utf8(root_text));
        local = load_blocks(root, {"industry"});
        for (auto& stock : stocks) {
            const auto security = local.securities.find({stock.market_id, stock.code});
            if (security != local.securities.end()) stock.name = security->second.name;
        }
    }
    std::sort(stocks.begin(), stocks.end(), [](const Stock& left, const Stock& right) {
        return std::tie(left.market_id, left.code) < std::tie(right.market_id, right.code);
    });

    Json industry_records = Json::array();
    if (no_tree) {
        for (const auto& [code, industry] : industries) {
            Json item = Json::object();
            item["block_id"] = "industry:" + code;
            item["code"] = code;
            item["name"] = industry.name;
            item["source_key"] = "";
            item["parent_block_id"] = "";
            item["level"] = 1;
            item["is_leaf"] = true;
            item["member_count"] = static_cast<std::uint64_t>(direct_members[code].size());
            item["members"] = string_array(direct_members[code]);
            item["pe_ttm"] = industry.pe_ttm;
            item["pb_mrq"] = industry.pb_mrq;
            industry_records.push_back(std::move(item));
        }
    } else {
        std::map<std::string, const Block*> by_id;
        std::map<std::string, const Block*> leaf_by_code;
        for (const auto& block : local.blocks) {
            by_id[block.block_id] = &block;
            if (block.is_leaf) leaf_by_code[block.block_code] = &block;
        }
        std::map<std::string, std::set<std::string>> expanded;
        for (const auto& [code, members] : direct_members) {
            auto leaf = leaf_by_code.find(code);
            if (leaf == leaf_by_code.end()) throw Error("cloud industry is absent from local tree: " + code);
            const Block* current = leaf->second;
            while (current) {
                expanded[current->block_id].insert(members.begin(), members.end());
                if (current->parent_block_id.empty()) break;
                const auto parent = by_id.find(current->parent_block_id);
                if (parent == by_id.end()) throw Error("industry tree has a missing parent");
                current = parent->second;
            }
        }
        auto ordered = local.blocks;
        std::sort(ordered.begin(), ordered.end(), [](const Block& left, const Block& right) {
            return std::tie(left.level, left.source_key) < std::tie(right.level, right.source_key);
        });
        for (const auto& block : ordered) {
            Json item = Json::object();
            item["block_id"] = block.block_id;
            item["code"] = block.block_code;
            item["name"] = block.name;
            item["source_key"] = block.source_key;
            item["parent_block_id"] = block.parent_block_id;
            item["level"] = block.level;
            item["is_leaf"] = block.is_leaf;
            item["member_count"] = static_cast<std::uint64_t>(expanded[block.block_id].size());
            item["members"] = string_array(expanded[block.block_id]);
            const auto cloud = industries.find(block.block_code);
            item["pe_ttm"] = cloud == industries.end() ? "" : cloud->second.pe_ttm;
            item["pb_mrq"] = cloud == industries.end() ? "" : cloud->second.pb_mrq;
            industry_records.push_back(std::move(item));
        }
    }

    Json stock_records = Json::array();
    std::map<std::string, std::set<std::string>> theme_members;
    for (const auto& stock : stocks) {
        Json item = Json::object();
        item["security_id"] = stock.security_id;
        item["market_id"] = stock.market_id;
        item["code"] = stock.code;
        item["name"] = stock.name;
        item["industry_code"] = stock.industry_code;
        item["industry_name"] = stock.industry_name;
        item["themes"] = string_array(stock.themes);
        stock_records.push_back(std::move(item));
        if (!no_themes)
            for (const auto& theme : stock.themes) theme_members[theme].insert(stock.security_id);
    }
    std::vector<std::pair<std::string, std::set<std::string>>> ordered_themes(
        theme_members.begin(), theme_members.end());
    std::sort(ordered_themes.begin(), ordered_themes.end(), [](const auto& left, const auto& right) {
        if (left.second.size() != right.second.size()) return left.second.size() > right.second.size();
        return left.first < right.first;
    });
    Json theme_records = Json::array();
    std::size_t theme_membership_count = 0;
    for (const auto& [name, members] : ordered_themes) {
        Json item = Json::object();
        item["name"] = name;
        item["member_count"] = static_cast<std::uint64_t>(members.size());
        item["members"] = string_array(members);
        theme_records.push_back(std::move(item));
        theme_membership_count += members.size();
    }

    Json result = Json::object();
    result["schema"] = "tdx-hyzt-v1";
    Json counts = Json::object();
    counts["stocks"] = static_cast<std::uint64_t>(stocks.size());
    counts["industry_blocks"] = static_cast<std::uint64_t>(industry_records.size());
    counts["industry_leaves"] = static_cast<std::uint64_t>(industries.size());
    counts["themes"] = static_cast<std::uint64_t>(ordered_themes.size());
    counts["theme_memberships"] = static_cast<std::uint64_t>(theme_membership_count);
    result["counts"] = std::move(counts);
    result["industry_blocks"] = std::move(industry_records);
    result["stocks"] = std::move(stock_records);
    result["themes"] = std::move(theme_records);
    const auto output = from_utf8(output_text);
    atomic_write_text(output, result.dump(compact ? -1 : 2) + "\n");
    std::cout << "exported " << result.at("counts").at("industry_blocks").as_number()
              << " industry nodes, " << stocks.size() << " stocks, "
              << ordered_themes.size() << " themes -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
