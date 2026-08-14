#include "blocks_internal.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {
namespace detail = block_detail;
namespace {

Json block_json(const Block& block) {
    Json value = Json::object();
    value["block_id"] = block.block_id;
    value["family"] = block.family;
    value["family_name"] = block.family_name;
    value["block_code"] = block.block_code;
    value["name"] = block.name;
    value["source_key"] = block.source_key;
    value["parent_block_id"] = block.parent_block_id;
    value["level"] = block.level;
    value["is_leaf"] = block.is_leaf;
    value["declared_count"] = block.declared_count
        ? Json(*block.declared_count) : Json(nullptr);
    value["member_count"] = block.member_count;
    value["start_date"] = block.start_date;
    value["update_date"] = block.update_date;
    value["source_file"] = block.source_file;
    return value;
}

Json member_json(const BlockMember& member) {
    Json value = Json::object();
    value["block_id"] = member.block_id;
    value["family"] = member.family;
    value["family_name"] = member.family_name;
    value["block_code"] = member.block_code;
    value["block_name"] = member.block_name;
    value["security_id"] = member.security_id;
    value["market_id"] = member.market_id;
    value["market"] = member.market;
    value["code"] = member.code;
    value["security_name"] = member.security_name;
    value["membership"] = member.membership;
    value["name_resolved"] = member.name_resolved;
    return value;
}

std::set<std::string> take_families(Args& args) {
    auto values = args.take_options("--family");
    if (values.empty()) {
        values.reserve(detail::block_families.size());
        for (const auto& family : detail::block_families)
            values.emplace_back(family.key);
    }
    std::set<std::string> result;
    for (auto& value : values) {
        value = lower_ascii(value);
        if (!detail::find_block_family(value))
            throw Error("unknown block family: " + value);
        result.insert(std::move(value));
    }
    return result;
}

std::string csv_quote(const std::string& value) {
    if (value.find_first_of(",\"\r\n") == std::string::npos) return value;
    std::string result = "\"";
    for (const char ch : value)
        result += ch == '\"' ? "\"\"" : std::string(1, ch);
    return result + '\"';
}

std::string render_blocks_csv(const std::vector<Block>& blocks) {
    std::ostringstream out;
    out << "block_id,family,family_name,block_code,name,source_key,"
           "parent_block_id,level,is_leaf,declared_count,member_count,"
           "start_date,update_date,source_file\n";
    for (const auto& item : blocks) {
        out << csv_quote(item.block_id) << ',' << item.family << ','
            << csv_quote(item.family_name) << ',' << item.block_code << ','
            << csv_quote(item.name) << ',' << item.source_key << ','
            << item.parent_block_id << ',' << item.level << ','
            << (item.is_leaf ? "true" : "false") << ',';
        if (item.declared_count) out << *item.declared_count;
        out << ',' << item.member_count << ',' << item.start_date << ','
            << item.update_date << ',' << item.source_file << '\n';
    }
    return out.str();
}

std::string render_members_csv(const std::vector<BlockMember>& members) {
    std::ostringstream out;
    out << "block_id,family,family_name,block_code,block_name,security_id,"
           "market_id,market,code,security_name,membership,name_resolved\n";
    for (const auto& item : members)
        out << csv_quote(item.block_id) << ',' << item.family << ','
            << csv_quote(item.family_name) << ',' << item.block_code << ','
            << csv_quote(item.block_name) << ',' << item.security_id << ','
            << item.market_id << ',' << item.market << ',' << item.code << ','
            << csv_quote(item.security_name) << ',' << item.membership << ','
            << (item.name_resolved ? "true" : "false") << '\n';
    return out.str();
}

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

void export_help() {
    std::cout <<
        "Usage: tdx-tool blocks export [--root PATH] [--family NAME ...] [options]\n\n"
        "Families: industry, research-industry, concept, style, index\n"
        "Options:\n"
        "  --format json|csv       Output format (default json)\n"
        "  --output-dir PATH       Output directory (default output/blocks)\n"
        "  --compact               Compact JSON\n";
}

void query_help() {
    std::cout <<
        "Usage: tdx-tool blocks query (--block TEXT | --code CODE) [options]\n\n"
        "Query a block's securities or a security's block memberships.\n"
        "Options: --root PATH, --market 0|1|2|sz|sh|bj, --family NAME, --compact\n";
}

}  // namespace

int command_blocks_export(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        export_help();
        return 0;
    }
    const auto root_text = args.take_option("--root");
    const auto families = take_families(args);
    const auto format = lower_ascii(args.take_option("--format", "json"));
    const auto output_dir_text = args.take_option(
        "--output-dir", "output/blocks");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    if (format != "json" && format != "csv")
        throw Error("--format must be json or csv");
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : from_utf8(root_text));
    const auto data = load_blocks(root, families);
    const auto output_dir = from_utf8(output_dir_text);
    std::vector<fs::path> outputs;
    if (format == "json") {
        Json document = Json::object();
        document["schema_version"] = 1;
        document["source_root"] = path_utf8(root);
        Json family_stats = Json::object();
        for (const auto& family : detail::block_families) {
            if (!families.count(std::string(family.key))) continue;
            Json item = Json::object();
            item["blocks"] = static_cast<std::uint64_t>(std::count_if(
                data.blocks.begin(), data.blocks.end(), [&](const Block& block) {
                    return block.family == family.key;
                }));
            item["memberships"] = static_cast<std::uint64_t>(std::count_if(
                data.members.begin(), data.members.end(),
                [&](const BlockMember& member) {
                    return member.family == family.key;
                }));
            family_stats[std::string(family.key)] = std::move(item);
        }
        document["families"] = std::move(family_stats);
        Json blocks = Json::array();
        for (const auto& block : data.blocks)
            blocks.push_back(block_json(block));
        document["blocks"] = std::move(blocks);
        Json members = Json::array();
        for (const auto& member : data.members)
            members.push_back(member_json(member));
        document["members"] = std::move(members);
        outputs.push_back(output_dir / "tdx-blocks.json");
        atomic_write_text(
            outputs.back(), document.dump(compact ? -1 : 2) + "\n");
    } else {
        outputs.push_back(output_dir / "tdx-blocks.csv");
        outputs.push_back(output_dir / "tdx-block-members.csv");
        atomic_write_text(
            outputs[0], std::string("\xEF\xBB\xBF") +
                            render_blocks_csv(data.blocks));
        atomic_write_text(
            outputs[1], std::string("\xEF\xBB\xBF") +
                            render_members_csv(data.members));
    }
    const auto unresolved = std::count_if(
        data.members.begin(), data.members.end(),
        [](const BlockMember& item) { return !item.name_resolved; });
    std::cout << "exported " << data.blocks.size() << " blocks, "
              << data.members.size() << " memberships, "
              << data.securities.size() << " securities, " << unresolved
              << " unresolved -> ";
    for (std::size_t index = 0; index < outputs.size(); ++index) {
        if (index) std::cout << ", ";
        std::cout << path_utf8(outputs[index]);
    }
    std::cout << '\n';
    return 0;
}

int command_blocks_query(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        query_help();
        return 0;
    }
    const auto root_text = args.take_option("--root");
    const auto families = take_families(args);
    const auto block_query = args.take_option("--block");
    const auto code_query = args.take_option("--code");
    auto market_query = lower_ascii(args.take_option("--market"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    if (block_query.empty() == code_query.empty())
        throw Error("choose exactly one of --block and --code");
    if (!market_query.empty()) {
        if (market_query == "0") market_query = "sz";
        else if (market_query == "1") market_query = "sh";
        else if (market_query == "2") market_query = "bj";
        if (market_query != "sz" && market_query != "sh" &&
            market_query != "bj")
            throw Error("--market must be sz/sh/bj or 0/1/2");
    }
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : from_utf8(root_text));
    const auto data = load_blocks(root, families);
    std::set<std::string> matched_ids;
    if (!block_query.empty()) {
        const auto normalized = lower_ascii(block_query);
        for (const auto& block : data.blocks)
            if (lower_ascii(block.block_id) == normalized ||
                lower_ascii(block.block_code) == normalized ||
                lower_ascii(block.name) == normalized)
                matched_ids.insert(block.block_id);
        if (matched_ids.empty()) {
            for (const auto& block : data.blocks)
                if (lower_ascii(block.block_id).find(normalized) !=
                        std::string::npos ||
                    lower_ascii(block.name).find(normalized) !=
                        std::string::npos)
                    matched_ids.insert(block.block_id);
        }
    } else {
        for (const auto& member : data.members)
            if (member.code == code_query &&
                (market_query.empty() ||
                 lower_ascii(member.market) == market_query))
                matched_ids.insert(member.block_id);
    }
    Json result = Json::object();
    result["query_type"] = block_query.empty() ? "security" : "block";
    result["query"] = block_query.empty() ? code_query : block_query;
    Json blocks = Json::array();
    for (const auto& block : data.blocks)
        if (matched_ids.count(block.block_id))
            blocks.push_back(block_json(block));
    Json members = Json::array();
    for (const auto& member : data.members)
        if (matched_ids.count(member.block_id) &&
            (code_query.empty() ||
             (member.code == code_query &&
              (market_query.empty() ||
               lower_ascii(member.market) == market_query))))
            members.push_back(member_json(member));
    result["block_count"] = static_cast<std::uint64_t>(blocks.size());
    result["membership_count"] =
        static_cast<std::uint64_t>(members.size());
    result["blocks"] = std::move(blocks);
    result["members"] = std::move(members);
    std::cout << result.dump(compact ? -1 : 2) << '\n';
    return 0;
}

}  // namespace tdx
