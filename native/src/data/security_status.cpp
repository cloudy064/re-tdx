#include "tdx/security_status.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <map>
#include <mutex>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx {
namespace {

int local_date() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now)) throw Error("cannot read local date");
#else
    if (!localtime_r(&now, &local)) throw Error("cannot read local date");
#endif
    return (local.tm_year + 1900) * 10000 + (local.tm_mon + 1) * 100 +
           local.tm_mday;
}

bool digits(std::string_view value) {
    return !value.empty() &&
           std::all_of(value.begin(), value.end(), [](unsigned char ch) {
               return std::isdigit(ch) != 0;
           });
}

int loose_integer(const std::string& value) {
    if (value.empty()) return 0;
    try {
        std::size_t used = 0;
        const int parsed = std::stoi(value, &used);
        return used == value.size() ? parsed : 0;
    } catch (...) {
        return 0;
    }
}

struct CachedQuitResource {
    fs::file_time_type modified;
    std::uintmax_t size{};
    int effective_date{};
    std::shared_ptr<const SecurityQuitResource> resource;
};

}  // namespace

SecurityQuitResource parse_security_quit_text(
    const std::string& utf8_text,
    int effective_date,
    std::string source_path) {
    if (effective_date <= 0) effective_date = local_date();
    SecurityQuitResource result;
    result.source_path = std::move(source_path);
    result.effective_date = effective_date;
    for (auto line : split(utf8_text, '\n')) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line = trim(std::move(line));
        if (line.empty()) continue;
        const auto fields = split(line, '|');
        if (fields.size() < 4 || !digits(fields[0]) ||
            !digits(fields[1]) || fields[1].size() != 6)
            continue;
        const int market = loose_integer(fields[0]);
        if (market < 0 || market > 2) continue;
        ++result.parsed_record_count;
        // Mirrors TdxW sub_5A4E70 exactly: only status 0 participates and the
        // fourth token is an optional lower-bound date.  The following token
        // is intentionally not used by the native client either.
        if (loose_integer(fields[2]) != 0) continue;
        const int activation_date = loose_integer(fields[3]);
        if (activation_date == 0 || effective_date >= activation_date)
            result.active.insert({market, fields[1]});
    }
    return result;
}

SecurityQuitResource load_security_quit_resource(
    const fs::path& tdx_root,
    int effective_date) {
    const auto path = tdx_root / "T0002" / "hq_cache" /
                      "infoharbor_spec.cfg";
    if (!fs::is_regular_file(path))
        throw Error("TDX security status file is missing: " + path_utf8(path));
    return parse_security_quit_text(
        decode_gbk(read_bytes(path)),
        effective_date > 0 ? effective_date : local_date(),
        path_utf8(path));
}

std::shared_ptr<const SecurityQuitResource> cached_security_quit_resource(
    const fs::path& tdx_root,
    int effective_date) {
    const auto path = tdx_root / "T0002" / "hq_cache" /
                      "infoharbor_spec.cfg";
    if (!fs::is_regular_file(path))
        throw Error("TDX security status file is missing: " + path_utf8(path));
    if (effective_date <= 0) effective_date = local_date();
    std::error_code error;
    const auto canonical = fs::weakly_canonical(path, error);
    const auto key = path_utf8(error ? path : canonical);
    error.clear();
    const auto modified = fs::last_write_time(path, error);
    if (error) throw Error("cannot stat TDX security status file: " + path_utf8(path));
    const auto size = fs::file_size(path, error);
    if (error) throw Error("cannot size TDX security status file: " + path_utf8(path));

    static std::mutex mutex;
    static std::map<std::string, CachedQuitResource> cache;
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = cache.find(key);
        if (found != cache.end() && found->second.modified == modified &&
            found->second.size == size &&
            found->second.effective_date == effective_date)
            return found->second.resource;
    }
    auto loaded = std::make_shared<const SecurityQuitResource>(
        load_security_quit_resource(tdx_root, effective_date));
    {
        std::lock_guard<std::mutex> lock(mutex);
        cache[key] = {modified, size, effective_date, loaded};
    }
    return loaded;
}

int tcalc_security_class(int market, const std::string& code) {
    const auto ch = [&](std::size_t index) {
        return index < code.size() ? code[index] : '\0';
    };
    if (market == 0) {
        switch (ch(0)) {
            case '0':
                if (ch(1) == '0')
                    return ch(2) == '2' || ch(2) == '3' || ch(2) == '4' ? 8 : 0;
                if (ch(1) == '3' || ch(1) == '8') return 1;
                return 10;
            case '1':
                if (ch(1) == '0' || ch(1) == '9') return 2;
                if (ch(1) == '1' || ch(1) == '4') return 3;
                if (ch(1) == '2') return 4;
                if (ch(1) == '3') return ch(2) == '1' ? 5 : 3;
                if (ch(1) >= '5' && ch(1) <= '8') return 6;
                return 10;
            case '2':
                if (ch(1) == '3' || ch(1) == '8') return 1;
                return ch(1) == '0' ? 7 : 10;
            case '3':
                if (ch(1) == '0') return 9;
                return ch(1) == '8' ? 1 : 10;
            default:
                return 10;
        }
    }
    if (market == 1) {
        switch (ch(0)) {
            case '0': {
                int value = 0;
                for (const unsigned char value_char : code) {
                    if (!std::isdigit(value_char)) break;
                    value = value * 10 + (value_char - '0');
                }
                return value < 1000 ? 20 : 13;
            }
            case '1':
                return ch(1) != '1' || ch(2) == '2' || ch(2) == '4' ||
                               ch(2) == '5'
                           ? 14
                           : 15;
            case '2': return 16;
            case '5':
                return ch(1) == '8' && (ch(2) == '0' || ch(2) == '2')
                           ? 12
                           : 17;
            case '6':
                return ch(1) == '8' && (ch(2) == '8' || ch(2) == '9')
                           ? 19
                           : 11;
            case '7': return ch(1) == '5' || ch(1) == '7' ? 13 : 20;
            case '9': return ch(1) == '0' && ch(2) == '0' ? 18 : 20;
            default: return 20;
        }
    }
    if (market == 2) {
        if (ch(0) == '4') return ch(1) == '3' ? 21 : 24;
        if (ch(0) == '8') {
            if (ch(1) == '3' || ch(1) == '7') return 21;
            if (ch(1) == '1') return 22;
            if (ch(1) == '2' && ch(2) != '0') return 23;
            return 24;
        }
        if (ch(0) == '9' && ch(1) == '2' && ch(2) == '0') return 21;
        return 24;
    }
    return 10;
}

bool tcalc_security_status_applicable(int market, const std::string& code) {
    if (market < 0 || market > 2) return false;
    static const std::set<int> applicable{0, 7, 8, 9, 11, 18, 19, 21};
    return applicable.count(tcalc_security_class(market, code)) != 0;
}

bool tcalc_is_st_security(int market, const std::string& code,
                          const std::string& name) {
    return tcalc_security_status_applicable(market, code) &&
           name.find("ST") != std::string::npos;
}

bool tcalc_is_t0_primary_security(
    const FinanceEligibilityResource& resource,
    int market,
    const std::string& code) {
    if (market == 0)
        return code.size() >= 3 && code[0] == '1' && code[1] == '2' &&
               code[2] >= '3';
    if (market == 1)
        return code.size() >= 3 && code[0] == '1' && code[1] == '1' &&
               code[2] != '2' && code[2] != '4' && code[2] != '5';
    return market == 2 &&
           resource.beijing_convertible_bonds.count({market, code}) != 0;
}

bool tcalc_is_t0_security(const FinanceEligibilityResource& resource,
                          int market,
                          const std::string& code) {
    return tcalc_is_t0_primary_security(resource, market, code) ||
           resource.t0_funds.count({market, code}) != 0;
}

bool tcalc_is_quit_security(const SecurityQuitResource& resource,
                            int market,
                            const std::string& code) {
    return tcalc_security_status_applicable(market, code) &&
           resource.active.count({market, code}) != 0;
}

bool tcalc_is_futures_or_option_category(int category) {
    return category == 3 || category == 12;
}

}  // namespace tdx
