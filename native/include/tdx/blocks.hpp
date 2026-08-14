#pragma once

#include "tdx/common.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

class Json;

struct Security {
    int market_id{};
    std::string market;
    std::string market_name;
    std::string code;
    std::string name;
    // TdxW security-record offset +78 (360-byte TNF layout).  It is the
    // multiplier used for quote volumes expressed in hands/lots.
    double trade_unit{};
    // TdxW security-record offset +282 in the 360-byte TNF layout.  Values 3
    // and 12 are the exact futures/option categories used by ISQHQQCODE.
    std::uint8_t tdx_category{};
    // TdxW's 360-byte TNF record offset +76.  This byte was correlated with
    // the public 0x044D security-directory decimal field; the legacy 314-byte
    // layout deliberately leaves it unavailable.
    std::optional<std::uint8_t> price_precision;
    std::string security_id() const { return market + code; }
};

using SecurityCatalog =
    std::map<std::pair<int, std::string>, Security>;

struct Block {
    std::string block_id;
    std::string family;
    std::string family_name;
    std::string block_code;
    std::string name;
    std::string source_key;
    std::string parent_block_id;
    int level{};
    bool is_leaf{};
    std::optional<int> declared_count;
    int member_count{};
    std::string start_date;
    std::string update_date;
    std::string source_file;
};

struct BlockMember {
    std::string block_id;
    std::string family;
    std::string family_name;
    std::string block_code;
    std::string block_name;
    std::string security_id;
    int market_id{};
    std::string market;
    std::string code;
    std::string security_name;
    std::string membership;
    bool name_resolved{};
};

struct IndustryAssignment {
    int market_id{};
    std::string code;
    std::string industry_code;
    std::string research_industry_code;
};

struct BlockData {
    SecurityCatalog securities;
    std::vector<Block> blocks;
    std::vector<BlockMember> members;
};

// TdxW callback type 167 obtains MAINBUSINESS from
// T0002/hq_cache/specgpext.txt.  The native client compares the numeric market
// and security code fields, so leading zeroes in the file are intentionally
// not part of the key.
struct MainBusinessCatalog {
    std::string source_path;
    std::map<std::pair<int, int>, std::string> records;
    std::map<std::pair<int, int>, double> safety_scores;
    std::map<std::pair<int, int>, double> shine_scores;
};

// TdxW command-8 category 2 enumerates two built-in local block directories
// followed by records loaded from blocknew/blocknew.cfg (120-byte records) or
// the legacy block.cfg (19-byte records).  TCalc turns the host's pipe-delimited
// result into a space-delimited string without trimming the trailing separator.
struct CustomBlockDirectoryEntry {
    std::string name;
    std::string key;
};

struct CustomBlockMembership {
    std::string text;
    int count{};
    std::string catalog_source;
    std::size_t directory_count{};
};

// BLOCKSETNUM delegates to the same command-8 catalog resolver as ZDBLOCK,
// but asks the host for the resolved block's total member count.  Preserve
// directory order because TdxW uses the first case-insensitive name match.
struct CustomBlockCountEntry {
    std::string name;
    std::string key;
    int count{};
    bool built_in{};
};

struct CustomBlockCountCatalog {
    std::string catalog_source;
    std::vector<CustomBlockCountEntry> entries;
};

struct CustomBlockSecurity {
    int market_id{};
    std::string code;
};

// TdxW command-8 category 3 materializes the terminal's combination-block
// catalog from T0002/lc/lcidx.lii.  Each 320-byte directory record names a
// matching T0002/lc/<key>.cis file whose members are 16-byte market/code
// records.  ZHBLOCK preserves catalog order and TCalc changes the host's pipe
// separators to spaces without trimming the final separator.
struct CombinationBlockDirectoryEntry {
    std::string name;
    std::string key;
};

struct CombinationBlockMembership {
    std::string text;
    int count{};
    std::string catalog_source;
    std::size_t directory_count{};
    std::size_t readable_member_file_count{};
};

std::vector<Security> parse_tnf(const Bytes& data, int market_id);
std::vector<Block> parse_industry_catalog(std::string_view text);
std::vector<IndustryAssignment> parse_industry_assignments(std::string_view text);
std::vector<BlockMember> build_industry_members(
    std::vector<Block>& blocks,
    const std::vector<IndustryAssignment>& assignments,
    const std::map<std::pair<int, std::string>, Security>& securities);
std::pair<std::vector<Block>, std::vector<BlockMember>> parse_infoharbor(
    std::string_view text,
    const std::map<std::pair<int, std::string>, Security>& securities);
BlockData load_blocks(const std::filesystem::path& root,
                      const std::set<std::string>& families);
MainBusinessCatalog parse_main_business_catalog(
    std::string_view text, std::string source_path = {});
MainBusinessCatalog load_main_business_catalog(
    const std::filesystem::path& root);
std::shared_ptr<const MainBusinessCatalog> cached_main_business_catalog(
    const std::filesystem::path& root);
// GETNAMEOFCODE delegates to TdxW callback type 120, which reads the security
// name from the local TNF directory.  Cache the three-market directory while
// invalidating it whenever any TNF file changes.
std::shared_ptr<const SecurityCatalog> cached_security_catalog(
    const std::filesystem::path& root);
// Add local security-master metadata without replacing caller-supplied JSON
// values.  This operation is pure/in-memory and never performs network I/O.
bool enrich_security_metadata(const SecurityCatalog& catalog, Json& document,
                              std::string_view market,
                              std::string_view code);
std::string main_business_for(const MainBusinessCatalog& catalog,
                              int market_id, const std::string& code);
std::optional<double> safety_score_for(const MainBusinessCatalog& catalog,
                                       int market_id, const std::string& code);
std::optional<double> shine_score_for(const MainBusinessCatalog& catalog,
                                      int market_id, const std::string& code);
int tdx_user_industry_mode(const std::filesystem::path& root);
std::vector<CustomBlockDirectoryEntry> load_custom_block_directory(
    const std::filesystem::path& root, std::string* source_path = nullptr);
CustomBlockMembership custom_block_membership_for(
    const std::filesystem::path& root, int market_id, const std::string& code);
CustomBlockCountCatalog custom_block_member_counts(
    const std::filesystem::path& root);
std::vector<CustomBlockSecurity> load_custom_block_members(
    const std::filesystem::path& root, const std::string& key);
std::vector<CombinationBlockDirectoryEntry> load_combination_block_directory(
    const std::filesystem::path& root, std::string* source_path = nullptr);
CombinationBlockMembership combination_block_membership_for(
    const std::filesystem::path& root, int market_id, const std::string& code);

int command_blocks_export(const std::vector<std::string>& args);
int command_blocks_query(const std::vector<std::string>& args);

}  // namespace tdx
