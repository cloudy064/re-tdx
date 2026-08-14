#include "tdx/finance_eligibility.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <map>
#include <mutex>

namespace fs = std::filesystem;

namespace tdx {
namespace {

enum class Section {
    none,
    margin_financing,
    sh_stock_connect,
    sz_stock_connect,
    t0_funds,
    beijing_convertible_bonds,
};

Section section_from_heading(const std::string& value) {
    if (value == "融资融券") return Section::margin_financing;
    if (value == "沪港通SH") return Section::sh_stock_connect;
    if (value == "深港通SZ") return Section::sz_stock_connect;
    if (value == "T+0基金") return Section::t0_funds;
    if (value == "北证可转债") return Section::beijing_convertible_bonds;
    return Section::none;
}

bool parse_security_key(const std::string& value, SecurityKey& key) {
    if (value.size() != 7 || !std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) return false;
    const int market = value.front() - '0';
    if (market < 0 || market > 2) return false;
    key = {market, value.substr(1)};
    return true;
}

int normalized_market(const std::string& value) {
    const auto market = lower_ascii(trim(value));
    if (market == "0" || market == "sz") return 0;
    if (market == "1" || market == "sh") return 1;
    if (market == "2" || market == "44" || market == "bj") return 2;
    throw Error("finance eligibility market must be sz/sh/bj or 0/1/2");
}

struct CachedResource {
    fs::file_time_type modified;
    std::uintmax_t size{};
    std::shared_ptr<const FinanceEligibilityResource> resource;
};

}  // namespace

FinanceEligibilityResource parse_finance_eligibility_text(
    const std::string& utf8_text,
    std::string source_path) {
    FinanceEligibilityResource result;
    result.source_path = std::move(source_path);
    Section section = Section::none;
    for (auto line : split(utf8_text, '\n')) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line = trim(std::move(line));
        if (line.empty()) continue;
        if (line.front() == '#') {
            section = section_from_heading(trim(line.substr(1)));
            continue;
        }
        if (section == Section::none) continue;
        SecurityKey key;
        if (!parse_security_key(line, key)) continue;
        if (section == Section::margin_financing)
            result.margin_financing.insert(std::move(key));
        else if (section == Section::sh_stock_connect)
            result.sh_stock_connect.insert(std::move(key));
        else if (section == Section::sz_stock_connect)
            result.sz_stock_connect.insert(std::move(key));
        else if (section == Section::t0_funds)
            result.t0_funds.insert(std::move(key));
        else if (section == Section::beijing_convertible_bonds)
            result.beijing_convertible_bonds.insert(std::move(key));
    }
    return result;
}

FinanceEligibilityResource load_finance_eligibility_resource(const fs::path& tdx_root) {
    const auto path = tdx_root / "T0002" / "hq_cache" / "spblock.dat";
    if (!fs::is_regular_file(path))
        throw Error("TDX finance eligibility file is missing: " + path_utf8(path));
    return parse_finance_eligibility_text(decode_gbk(read_bytes(path)), path_utf8(path));
}

std::shared_ptr<const FinanceEligibilityResource> cached_finance_eligibility_resource(
    const fs::path& tdx_root) {
    const auto path = tdx_root / "T0002" / "hq_cache" / "spblock.dat";
    if (!fs::is_regular_file(path))
        throw Error("TDX finance eligibility file is missing: " + path_utf8(path));
    std::error_code error;
    const auto canonical = fs::weakly_canonical(path, error);
    const auto cache_key = path_utf8(error ? path : canonical);
    error.clear();
    const auto modified = fs::last_write_time(path, error);
    if (error) throw Error("cannot stat TDX finance eligibility file: " + path_utf8(path));
    const auto size = fs::file_size(path, error);
    if (error) throw Error("cannot size TDX finance eligibility file: " + path_utf8(path));

    static std::mutex mutex;
    static std::map<std::string, CachedResource> cache;
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = cache.find(cache_key);
        if (found != cache.end() && found->second.modified == modified &&
            found->second.size == size) return found->second.resource;
    }
    auto loaded = std::make_shared<const FinanceEligibilityResource>(
        load_finance_eligibility_resource(tdx_root));
    {
        std::lock_guard<std::mutex> lock(mutex);
        cache[cache_key] = {modified, size, loaded};
    }
    return loaded;
}

std::map<int, int> finance_eligibility_values(
    const FinanceEligibilityResource& resource,
    const std::string& market,
    const std::string& code,
    const std::set<int>& finance_ids) {
    if (code.size() != 6 || !std::all_of(code.begin(), code.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) throw Error("finance eligibility code must contain six digits");
    const SecurityKey key{normalized_market(market), code};
    std::map<int, int> result;
    if (finance_ids.count(48)) {
        const bool found = resource.sh_stock_connect.count(key) ||
                           resource.sz_stock_connect.count(key);
        result.emplace(48, found ? 1 : 0);
    }
    if (finance_ids.count(52))
        result.emplace(52, resource.margin_financing.count(key) ? 1 : 0);
    return result;
}

}  // namespace tdx
