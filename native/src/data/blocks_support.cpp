#include "blocks_internal.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace tdx::block_detail {

const BlockFamilyDefinition* find_block_family(std::string_view key) {
    const auto found = std::find_if(
        block_families.begin(), block_families.end(),
        [key](const BlockFamilyDefinition& item) { return item.key == key; });
    return found == block_families.end() ? nullptr : &*found;
}

const MarketDefinition* find_block_market(int market_id) {
    const auto found = std::find_if(
        block_markets.begin(), block_markets.end(),
        [market_id](const MarketDefinition& item) { return item.id == market_id; });
    return found == block_markets.end() ? nullptr : &*found;
}

std::string block_family_name(std::string_view key) {
    const auto* family = find_block_family(key);
    if (!family) throw Error("unknown block family: " + std::string(key));
    return std::string(family->display_name);
}

std::vector<std::string> text_lines(std::string_view text) {
    std::vector<std::string> result;
    std::istringstream input{std::string(text)};
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        result.push_back(std::move(line));
    }
    return result;
}

int strict_int(const std::string& text, std::string_view context) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size()) throw std::invalid_argument("tail");
        return value;
    } catch (...) {
        throw Error(std::string(context) + ": invalid integer: " + text);
    }
}

int tdx_atol(const std::string& text) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        return used ? value : 0;
    } catch (...) {
        return 0;
    }
}

std::string fixed_ascii(const std::uint8_t* data, std::size_t size) {
    const auto end = std::find(data, data + size, 0);
    return trim(std::string(reinterpret_cast<const char*>(data),
                            static_cast<std::size_t>(end - data)));
}

std::string fixed_gbk(const std::uint8_t* data, std::size_t size) {
    const auto end = std::find(data, data + size, 0);
    return trim(decode_gbk(Bytes(data, end)));
}

Security unresolved_security(int market_id, std::string code) {
    const auto* market = find_block_market(market_id);
    return market == nullptr
        ? Security{market_id, "M" + std::to_string(market_id),
                   "市场" + std::to_string(market_id), std::move(code), ""}
        : Security{market_id, std::string(market->code),
                   std::string(market->display_name), std::move(code), ""};
}

std::string parent_key(const std::string& source_key) {
    return source_key.size() <= 3 ? "" : source_key.substr(0, source_key.size() - 2);
}

int source_level(const std::string& source_key) {
    if (source_key.empty() ||
        (source_key.front() != 'T' && source_key.front() != 'X'))
        throw Error("invalid industry source key: " + source_key);
    return static_cast<int>((source_key.size() - 1) / 2);
}

std::string infoharbor_family(std::string_view prefix) {
    if (prefix == "GN") return "concept";
    if (prefix == "FG") return "style";
    if (prefix == "ZS") return "index";
    throw Error("unsupported infoharbor family: " + std::string(prefix));
}

}  // namespace tdx::block_detail
