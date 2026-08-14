#include "blocks_internal.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <utility>

namespace fs = std::filesystem;

namespace tdx {
namespace detail = block_detail;

std::vector<CustomBlockDirectoryEntry> load_custom_block_directory(
    const fs::path& root, std::string* source_path) {
    std::vector<CustomBlockDirectoryEntry> result{
        {"自选股", "zxg"}, {"临时条件股", "tjg"}};
    const auto directory = root / "T0002" / "blocknew";
    const auto modern = directory / "blocknew.cfg";
    const auto legacy = root / "T0002" / "block.cfg";
    if (fs::is_regular_file(modern)) {
        if (source_path) *source_path = path_utf8(modern);
        const auto data = read_bytes(modern);
        for (std::size_t offset = 0; offset + 120 <= data.size(); offset += 120) {
            result.push_back(CustomBlockDirectoryEntry{
                detail::fixed_gbk(data.data() + offset, 50),
                detail::fixed_ascii(data.data() + offset + 50, 50)});
        }
    } else if (fs::is_regular_file(legacy)) {
        if (source_path) *source_path = path_utf8(legacy);
        const auto data = read_bytes(legacy);
        for (std::size_t offset = 0; offset + 19 <= data.size(); offset += 19) {
            // TdxW zero-terminates the 10-byte name at byte 9 and the 5-byte
            // key at byte 4 before expanding them into its 120-byte records.
            result.push_back(CustomBlockDirectoryEntry{
                detail::fixed_gbk(data.data() + offset + 4, 9),
                detail::fixed_ascii(data.data() + offset + 14, 4)});
        }
    } else if (source_path) {
        *source_path = "tdxw-static-zxg-tjg-directory";
    }
    return result;
}

CustomBlockMembership custom_block_membership_for(
    const fs::path& root, int market_id, const std::string& code) {
    CustomBlockMembership result;
    const auto directory = load_custom_block_directory(
        root, &result.catalog_source);
    result.directory_count = directory.size();
    const auto target = lower_ascii(std::to_string(market_id) + code);
    for (const auto& entry : directory) {
        if (entry.key.empty()) continue;
        const auto path = root / "T0002" / "blocknew" / (entry.key + ".blk");
        if (!fs::is_regular_file(path)) continue;
        bool found = false;
        for (auto line : detail::text_lines(decode_gbk(read_bytes(path)))) {
            line = lower_ascii(trim(std::move(line)));
            if (line == target) {
                found = true;
                break;
            }
        }
        if (!found) continue;
        result.text += entry.name;
        result.text.push_back(' ');
        ++result.count;
    }
    // TdxW emits "|" when no category-2 membership exists. TCalc replaces
    // every pipe with a space and does not trim it.
    if (result.text.empty()) result.text = " ";
    return result;
}

CustomBlockCountCatalog custom_block_member_counts(const fs::path& root) {
    CustomBlockCountCatalog result;
    const auto directory = load_custom_block_directory(
        root, &result.catalog_source);
    result.entries.reserve(directory.size());
    for (std::size_t index = 0; index < directory.size(); ++index) {
        const auto& entry = directory[index];
        int count = 0;
        if (!entry.key.empty()) {
            const auto path = root / "T0002" / "blocknew" /
                              (entry.key + ".blk");
            if (fs::is_regular_file(path)) {
                for (auto line : detail::text_lines(decode_gbk(read_bytes(path))))
                    if (!trim(std::move(line)).empty()) ++count;
            }
        }
        result.entries.push_back(CustomBlockCountEntry{
            entry.name, entry.key, count, index < 2});
    }
    return result;
}

std::vector<CustomBlockSecurity> load_custom_block_members(
    const fs::path& root, const std::string& key) {
    std::vector<CustomBlockSecurity> result;
    if (key.empty()) return result;
    const auto path = root / "T0002" / "blocknew" / (key + ".blk");
    if (!fs::is_regular_file(path)) return result;
    std::set<std::pair<int, std::string>> seen;
    for (auto line : detail::text_lines(decode_gbk(read_bytes(path)))) {
        line = trim(std::move(line));
        if (line.size() < 2 ||
            !std::isdigit(static_cast<unsigned char>(line.front())))
            continue;
        const int market_id = line.front() - '0';
        auto code = trim(line.substr(1));
        if (code.empty() || !seen.insert({market_id, code}).second) continue;
        result.push_back(CustomBlockSecurity{market_id, std::move(code)});
    }
    return result;
}

std::vector<CombinationBlockDirectoryEntry> load_combination_block_directory(
    const fs::path& root, std::string* source_path) {
    std::vector<CombinationBlockDirectoryEntry> result;
    const auto path = root / "T0002" / "lc" / "lcidx.lii";
    if (source_path) *source_path = path_utf8(path);
    if (!fs::is_regular_file(path)) return result;
    const auto data = read_bytes(path);
    if (data.empty() || data.size() % 320 != 0) return result;
    const auto count = std::min<std::size_t>(data.size() / 320, 600);
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto* record = data.data() + index * 320;
        // sub_7337C0 zero-terminates offsets 10 and 19 after loading the
        // persisted record, making these exact six/eight-byte fields.
        result.push_back(CombinationBlockDirectoryEntry{
            detail::fixed_gbk(record + 11, 8),
            detail::fixed_ascii(record + 4, 6)});
    }
    return result;
}

CombinationBlockMembership combination_block_membership_for(
    const fs::path& root, int market_id, const std::string& code) {
    CombinationBlockMembership result;
    const auto directory = load_combination_block_directory(
        root, &result.catalog_source);
    result.directory_count = directory.size();
    for (const auto& entry : directory) {
        if (entry.key.empty()) continue;
        const auto path = root / "T0002" / "lc" / (entry.key + ".cis");
        if (!fs::is_regular_file(path)) continue;
        const auto data = read_bytes(path);
        if (data.empty() || data.size() % 16 != 0) continue;
        ++result.readable_member_file_count;
        const auto member_count = std::min<std::size_t>(data.size() / 16, 25000);
        bool found = false;
        for (std::size_t index = 0; index < member_count; ++index) {
            const auto* member = data.data() + index * 16;
            // sub_731BE0 zero-terminates offset 10 before TdxW converts the
            // market/code pair into its internal security identifier.
            if (static_cast<int>(read_u16_le(member)) == market_id &&
                detail::fixed_ascii(member + 2, 8) == code) {
                found = true;
                break;
            }
        }
        if (!found) continue;
        result.text += entry.name;
        result.text.push_back(' ');
        ++result.count;
    }
    // sub_4FB790 emits a single pipe for an empty result and TCalc replaces it
    // with one space. Do not trim this sentinel or the trailing separator.
    if (result.text.empty()) result.text = " ";
    return result;
}

}  // namespace tdx
