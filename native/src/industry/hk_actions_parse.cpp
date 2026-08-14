#include "hk_actions_internal.hpp"
#include "hk_resource_crypto_internal.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string_view>
#include <tuple>

namespace fs = std::filesystem;

namespace tdx::hk_actions_detail {
namespace {

constexpr std::size_t kMaximumFileSize = 8 * 1024 * 1024;
constexpr std::size_t kMaximumLineSize = 4096;
constexpr std::size_t kMaximumRecords = 100000;

struct FileStamp {
    bool available{};
    std::uintmax_t size{};
    fs::file_time_type modified{};

    bool operator==(const FileStamp& other) const {
        return available == other.available && size == other.size &&
               modified == other.modified;
    }
};

struct CacheEntry {
    std::array<FileStamp, 2> stamps;
    std::shared_ptr<const std::vector<ActionRecord>> records;
};

FileStamp file_stamp(const fs::path& path) {
    FileStamp result;
    std::error_code error;
    result.available = fs::is_regular_file(path, error);
    if (!result.available || error) return result;
    result.size = fs::file_size(path, error);
    if (error) return {};
    result.modified = fs::last_write_time(path, error);
    return error ? FileStamp{} : result;
}

bool digits(std::string_view value, std::size_t count) {
    return value.size() == count &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

std::vector<std::string> split_fields(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t begin = 0;
    for (;;) {
        const auto separator = line.find('|', begin);
        fields.emplace_back(line.substr(
            begin, separator == std::string::npos
                ? std::string::npos : separator - begin));
        if (separator == std::string::npos) break;
        begin = separator + 1;
    }
    return fields;
}

double parse_number(const std::string& value, std::string_view field,
                    const fs::path& path, std::size_t line) {
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(value, &used);
        if (used != value.size() || !std::isfinite(parsed))
            throw std::invalid_argument("number");
        return parsed;
    } catch (...) {
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " invalid " + std::string(field));
    }
}

void load_file(const fs::path& path, std::vector<ActionRecord>& records) {
    const auto size = fs::file_size(path);
    if (size == 0 || size > kMaximumFileSize)
        throw Error("HK action resource has invalid size: " + path_utf8(path));
    const auto text = decode_gbk(
        hk_resource_crypto_detail::decrypt_resource(read_bytes(path)));
    std::istringstream input(text);
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line.size() > kMaximumLineSize)
            throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                        " line exceeds safety limit");
        if (records.size() >= kMaximumRecords)
            throw Error("HK action record count exceeds safety limit");
        const auto fields = split_fields(line);
        if (fields.size() != 5)
            throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                        " expected 5 columns");
        if (!digits(fields[0], 5))
            throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                        " invalid HK security code");
        if (!digits(fields[1], 8))
            throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                        " invalid action date");
        ActionRecord record;
        record.code = fields[0];
        record.date = fields[1];
        record.description = trim(fields[2]);
        record.cumulative_multiplier = parse_number(
            fields[3], "cumulative multiplier", path, line_number);
        record.cumulative_offset = parse_number(
            fields[4], "cumulative offset", path, line_number);
        record.source_file = path.filename().string();
        if (record.description.empty() || record.cumulative_multiplier <= 0.0)
            throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                        " invalid action record");
        records.push_back(std::move(record));
    }
    if (!input.eof())
        throw Error("failed while parsing HK action resource: " + path_utf8(path));
}

bool contains(const std::string& text, std::string_view value) {
    return text.find(value) != std::string::npos;
}

bool contains_numeric_ratio(const std::string& text, std::string_view marker) {
    std::size_t position = 0;
    while ((position = text.find(marker, position)) != std::string::npos) {
        const auto after = position + marker.size();
        if (position > 0 && after < text.size() &&
            text[position - 1] >= '0' && text[position - 1] <= '9' &&
            text[after] >= '0' && text[after] <= '9')
            return true;
        position = after;
    }
    return false;
}

bool marker_followed_by_digit(const std::string& text, std::string_view marker) {
    std::size_t position = 0;
    while ((position = text.find(marker, position)) != std::string::npos) {
        const auto after = position + marker.size();
        if (after < text.size() && text[after] >= '0' && text[after] <= '9')
            return true;
        position = after;
    }
    return false;
}

}  // namespace

std::shared_ptr<const std::vector<ActionRecord>> load_records(
    const fs::path& root) {
    const auto cache = root / "T0002" / "hq_cache";
    const std::array<const char*, 2> names{{"hkqxinfo2.dat", "hkqxinfo.dat"}};
    const std::array<FileStamp, 2> stamps{{
        file_stamp(cache / names[0]), file_stamp(cache / names[1]),
    }};
    static std::mutex cache_mutex;
    static std::map<std::string, CacheEntry> parsed_cache;
    const auto cache_key = path_utf8(fs::absolute(root).lexically_normal());
    std::lock_guard<std::mutex> lock(cache_mutex);
    const auto cached = parsed_cache.find(cache_key);
    if (cached != parsed_cache.end() && cached->second.stamps == stamps)
        return cached->second.records;

    std::vector<ActionRecord> records;
    std::size_t available = 0;
    for (std::size_t index = 0; index < names.size(); ++index) {
        const auto* name = names[index];
        const auto path = cache / name;
        if (!stamps[index].available) continue;
        ++available;
        load_file(path, records);
    }
    if (!available)
        throw Error("local HK action resources are unavailable under " +
                    path_utf8(cache));

    std::stable_sort(records.begin(), records.end(),
        [](const ActionRecord& left, const ActionRecord& right) {
            if (left.code != right.code) return left.code < right.code;
            return left.date < right.date;
        });

    std::vector<ActionRecord> unique;
    unique.reserve(records.size());
    std::set<std::tuple<std::string, std::string, std::string, double, double>> seen;
    for (auto& record : records) {
        const auto key = std::make_tuple(
            record.code, record.date, record.description,
            record.cumulative_multiplier, record.cumulative_offset);
        if (seen.insert(key).second) unique.push_back(std::move(record));
    }

    std::string current_code;
    double previous_multiplier = 1.0;
    double previous_offset = 0.0;
    for (auto& record : unique) {
        if (record.code != current_code) {
            current_code = record.code;
            previous_multiplier = 1.0;
            previous_offset = 0.0;
        }
        record.previous_multiplier = previous_multiplier;
        record.previous_offset = previous_offset;
        previous_multiplier = record.cumulative_multiplier;
        previous_offset = record.cumulative_offset;
    }
    auto result = std::make_shared<const std::vector<ActionRecord>>(
        std::move(unique));
    if (parsed_cache.size() >= 8 && cached == parsed_cache.end())
        parsed_cache.erase(parsed_cache.begin());
    parsed_cache[cache_key] = CacheEntry{stamps, result};
    return result;
}

std::vector<std::string> classify_flags(const std::string& description) {
    std::vector<std::string> flags;
    if (contains(description, "股息") || contains(description, "派息") ||
        contains(description, "红利"))
        flags.emplace_back("dividend");
    if (contains(description, "红股") || contains(description, "送股") ||
        contains(description, "以股代息") || contains(description, "股代息") ||
        marker_followed_by_digit(description, "送"))
        flags.emplace_back("bonus");
    if (contains(description, "供股") || contains(description, "配股") ||
        contains_numeric_ratio(description, "供") ||
        marker_followed_by_digit(description, "供"))
        flags.emplace_back("rights");
    if (contains(description, "拆细") || contains(description, "拆股") ||
        marker_followed_by_digit(description, "拆"))
        flags.emplace_back("split");
    if (contains(description, "合股") || contains(description, "股份合并") ||
        contains(description, "合并股份") ||
        marker_followed_by_digit(description, "合"))
        flags.emplace_back("consolidation");
    return flags;
}

std::string primary_kind(const std::vector<std::string>& flags,
                         const ActionRecord& record) {
    if (flags.size() > 1) return "mixed";
    if (flags.size() == 1) return flags.front();
    if (std::abs(record.cumulative_multiplier - record.previous_multiplier) > 1e-8 ||
        std::abs(record.cumulative_offset - record.previous_offset) > 1e-8)
        return "adjustment";
    return "other";
}

Json record_json(const ActionRecord& record) {
    const auto flags = classify_flags(record.description);
    const auto share_multiplier =
        record.cumulative_multiplier / record.previous_multiplier;
    const auto additive_adjustment =
        (record.cumulative_offset - record.previous_offset) /
        record.previous_multiplier;

    Json security = Json::object();
    security["market"] = "hk";
    security["code"] = record.code;
    security["security_id"] = "HK" + record.code;

    Json flag_json = Json::array();
    for (const auto& flag : flags) flag_json.push_back(flag);

    Json factors = Json::object();
    factors["previous_cumulative_multiplier"] = record.previous_multiplier;
    factors["previous_cumulative_offset"] = record.previous_offset;
    factors["cumulative_multiplier"] = record.cumulative_multiplier;
    factors["cumulative_offset"] = record.cumulative_offset;
    factors["event_share_multiplier"] = share_multiplier;
    factors["event_additive_adjustment"] = additive_adjustment;
    factors["changes_share_basis"] = std::abs(share_multiplier - 1.0) > 1e-8;
    factors["changes_price_basis"] = std::abs(additive_adjustment) > 1e-8;

    Json result = Json::object();
    result["kind"] = primary_kind(flags, record);
    result["flags"] = std::move(flag_json);
    result["security"] = std::move(security);
    result["date"] = record.date;
    result["description"] = record.description;
    result["factors"] = std::move(factors);
    result["source_file"] = record.source_file;
    return result;
}

}  // namespace tdx::hk_actions_detail
