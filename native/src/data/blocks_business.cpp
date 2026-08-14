#include "blocks_internal.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace tdx {
namespace detail = block_detail;
namespace {

struct CachedMainBusinessCatalog {
    fs::file_time_type modified;
    std::uintmax_t size{};
    std::shared_ptr<const MainBusinessCatalog> catalog;
};

struct CachedSecurityCatalog {
    std::array<fs::file_time_type, detail::block_markets.size()> modified;
    std::array<std::uintmax_t, detail::block_markets.size()> sizes{};
    std::shared_ptr<const SecurityCatalog> catalog;
};

}  // namespace

MainBusinessCatalog parse_main_business_catalog(
    std::string_view text, std::string source_path) {
    MainBusinessCatalog result;
    result.source_path = std::move(source_path);
    for (const auto& line : detail::text_lines(text)) {
        if (line.empty()) continue;
        const auto fields = split(line, '|');
        if (fields.size() < 3) continue;
        // Mirrors TdxW sub_4F5560/sub_4FA420: market and code use atol,
        // MAINBUSINESS is the third pipe-delimited field, and the first
        // matching record wins.
        const auto key = std::make_pair(
            detail::tdx_atol(fields[0]), detail::tdx_atol(fields[1]));
        result.records.emplace(key, fields[2]);
        // sub_4F5560 parses fields four and five with atof into float members
        // of the same 18-byte record.  sub_4FA4A0/sub_4FA510 expose those
        // members as SAFESCORE and SHINESCORE through callback type 167.
        if (fields.size() >= 4)
            result.safety_scores.emplace(
                key, static_cast<double>(
                         static_cast<float>(std::atof(fields[3].c_str()))));
        if (fields.size() >= 5)
            result.shine_scores.emplace(
                key, static_cast<double>(
                         static_cast<float>(std::atof(fields[4].c_str()))));
    }
    return result;
}

MainBusinessCatalog load_main_business_catalog(const fs::path& root) {
    const auto path = root / "T0002" / "hq_cache" / "specgpext.txt";
    MainBusinessCatalog result;
    result.source_path = path_utf8(path);
    // The enhanced-function file is optional in TdxW.  A missing file yields
    // an empty type-167 field rather than making formula evaluation fail.
    if (!fs::is_regular_file(path)) return result;
    return parse_main_business_catalog(
        decode_gbk(read_bytes(path)), path_utf8(path));
}

std::shared_ptr<const MainBusinessCatalog> cached_main_business_catalog(
    const fs::path& root) {
    const auto path = root / "T0002" / "hq_cache" / "specgpext.txt";
    std::error_code error;
    const auto canonical = fs::weakly_canonical(path, error);
    const auto key = path_utf8(error ? path : canonical);
    error.clear();
    const bool exists = fs::is_regular_file(path, error) && !error;
    const auto modified = exists ? fs::last_write_time(path, error)
                                 : fs::file_time_type{};
    if (error) throw Error("cannot stat MAINBUSINESS file: " + path_utf8(path));
    const auto size = exists ? fs::file_size(path, error) : 0;
    if (error) throw Error("cannot size MAINBUSINESS file: " + path_utf8(path));

    static std::mutex mutex;
    static std::map<std::string, CachedMainBusinessCatalog> cache;
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = cache.find(key);
        if (found != cache.end() && found->second.modified == modified &&
            found->second.size == size)
            return found->second.catalog;
    }
    auto loaded = std::make_shared<const MainBusinessCatalog>(
        load_main_business_catalog(root));
    {
        std::lock_guard<std::mutex> lock(mutex);
        cache[key] = {modified, size, loaded};
    }
    return loaded;
}

std::shared_ptr<const SecurityCatalog> cached_security_catalog(
    const fs::path& root) {
    std::error_code error;
    const auto canonical = fs::weakly_canonical(root, error);
    const auto key = path_utf8(error ? root : canonical);
    std::array<fs::file_time_type, detail::block_markets.size()> modified{};
    std::array<std::uintmax_t, detail::block_markets.size()> sizes{};
    std::size_t index = 0;
    for (const auto& market : detail::block_markets) {
        const auto path = root / "T0002" / "hq_cache" /
                          std::string(market.tnf_name);
        error.clear();
        if (!fs::is_regular_file(path, error) || error)
            throw Error("security master is missing: " + path_utf8(path));
        modified[index] = fs::last_write_time(path, error);
        if (error) throw Error("cannot stat security master: " + path_utf8(path));
        sizes[index] = fs::file_size(path, error);
        if (error) throw Error("cannot size security master: " + path_utf8(path));
        ++index;
    }

    static std::mutex mutex;
    static std::map<std::string, CachedSecurityCatalog> cache;
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = cache.find(key);
        if (found != cache.end() && found->second.modified == modified &&
            found->second.sizes == sizes)
            return found->second.catalog;
    }
    auto data = load_blocks(root, {});
    auto loaded = std::make_shared<const SecurityCatalog>(
        std::move(data.securities));
    {
        std::lock_guard<std::mutex> lock(mutex);
        cache[key] = {modified, sizes, loaded};
    }
    return loaded;
}

std::string main_business_for(const MainBusinessCatalog& catalog,
                              int market_id, const std::string& code) {
    const auto found = catalog.records.find(
        {market_id, detail::tdx_atol(code)});
    return found == catalog.records.end() ? std::string{} : found->second;
}

std::optional<double> safety_score_for(const MainBusinessCatalog& catalog,
                                       int market_id, const std::string& code) {
    const auto found = catalog.safety_scores.find(
        {market_id, detail::tdx_atol(code)});
    if (found == catalog.safety_scores.end()) return std::nullopt;
    return found->second;
}

std::optional<double> shine_score_for(const MainBusinessCatalog& catalog,
                                      int market_id, const std::string& code) {
    const auto found = catalog.shine_scores.find(
        {market_id, detail::tdx_atol(code)});
    if (found == catalog.shine_scores.end()) return std::nullopt;
    return found->second;
}

int tdx_user_industry_mode(const fs::path& root) {
    auto path = root / "T0002" / "user.ini";
    if (!fs::is_regular_file(path)) path = root / "T0002" / "user_def.ini";
    if (!fs::is_regular_file(path)) return 2;
    bool other_section = false;
    for (auto line : detail::text_lines(decode_gbk(read_bytes(path)))) {
        line = trim(std::move(line));
        if (line.empty() || line.front() == ';' || line.front() == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            other_section = lower_ascii(trim(line.substr(1, line.size() - 2))) == "other";
            continue;
        }
        if (!other_section) continue;
        const auto separator = line.find('=');
        if (separator == std::string::npos ||
            lower_ascii(trim(line.substr(0, separator))) != "usetdxl3hy")
            continue;
        const int value = detail::tdx_atol(trim(line.substr(separator + 1)));
        // Mirrors the unsigned range check in TdxW sub_8BB5D0: negative and
        // values above 2 fall back to ordinary mode 0.
        return value >= 0 && value <= 2 ? value : 0;
    }
    return 2;
}

}  // namespace tdx
