#include "hk_finance_internal.hpp"
#include "hk_resource_crypto_internal.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx::hk_finance_detail {
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
    FileStamp stamp;
    std::shared_ptr<const std::vector<FinanceRecord>> records;
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
        if (separator == std::string::npos) return fields;
        begin = separator + 1;
    }
}

std::optional<double> parse_number(const std::string& value,
                                   const fs::path& source,
                                   std::size_t line,
                                   std::size_t column) {
    if (value.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(value, &used);
        if (used != value.size() || !std::isfinite(parsed))
            throw std::invalid_argument("number");
        return parsed;
    } catch (...) {
        throw Error(path_utf8(source) + ":" + std::to_string(line) +
                    " invalid numeric column " + std::to_string(column));
    }
}

}  // namespace

std::vector<FinanceRecord> parse_decrypted_resource(
    const Bytes& decrypted, const fs::path& source) {
    if (decrypted.empty() || decrypted.size() > kMaximumFileSize)
        throw Error("HK finance resource has invalid size: " + path_utf8(source));
    const auto text = decode_gbk(decrypted);
    std::istringstream input(text);
    std::vector<FinanceRecord> records;
    std::set<std::string> codes;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line.size() > kMaximumLineSize)
            throw Error(path_utf8(source) + ":" + std::to_string(line_number) +
                        " line exceeds safety limit");
        if (records.size() >= kMaximumRecords)
            throw Error("HK finance record count exceeds safety limit");
        auto fields = split_fields(line);
        if (fields.size() != 17)
            throw Error(path_utf8(source) + ":" + std::to_string(line_number) +
                        " expected 17 columns");
        if (!digits(fields[0], 5))
            throw Error(path_utf8(source) + ":" + std::to_string(line_number) +
                        " invalid HK security code");
        if (!codes.insert(fields[0]).second)
            throw Error(path_utf8(source) + ":" + std::to_string(line_number) +
                        " duplicate HK security code");
        for (const auto index : {2U, 12U})
            if (!fields[index].empty() && !digits(fields[index], 8))
                throw Error(path_utf8(source) + ":" +
                            std::to_string(line_number) +
                            " invalid date column " + std::to_string(index));
        if (fields[10].size() > 10)
            throw Error(path_utf8(source) + ":" + std::to_string(line_number) +
                        " classification column exceeds native limit");

        FinanceRecord record;
        for (std::size_t index = 0; index < fields.size(); ++index)
            record.raw[index] = std::move(fields[index]);
        for (std::size_t index = 1; index < record.raw.size(); ++index) {
            if (index == 10) continue;
            record.numeric[index] = parse_number(
                record.raw[index], source, line_number, index);
        }
        records.push_back(std::move(record));
    }
    if (!input.eof())
        throw Error("failed while parsing HK finance resource: " + path_utf8(source));
    if (records.empty())
        throw Error("HK finance resource contains no records: " + path_utf8(source));
    return records;
}

std::shared_ptr<const std::vector<FinanceRecord>> load_records(
    const fs::path& root) {
    const auto path = root / "T0002" / "hq_cache" / "hkcwdata.dat";
    const auto stamp = file_stamp(path);
    if (!stamp.available)
        throw Error("local HK finance resource is unavailable: " + path_utf8(path));
    if (stamp.size == 0 || stamp.size > kMaximumFileSize)
        throw Error("HK finance resource has invalid size: " + path_utf8(path));

    static std::mutex cache_mutex;
    static std::map<std::string, CacheEntry> parsed_cache;
    const auto key = path_utf8(fs::absolute(path).lexically_normal());
    std::lock_guard<std::mutex> lock(cache_mutex);
    const auto cached = parsed_cache.find(key);
    if (cached != parsed_cache.end() && cached->second.stamp == stamp)
        return cached->second.records;

    auto records = parse_decrypted_resource(
        hk_resource_crypto_detail::decrypt_resource(read_bytes(path)), path);
    auto result = std::make_shared<const std::vector<FinanceRecord>>(
        std::move(records));
    if (parsed_cache.size() >= 8 && cached == parsed_cache.end())
        parsed_cache.erase(parsed_cache.begin());
    parsed_cache[key] = CacheEntry{stamp, result};
    return result;
}

}  // namespace tdx::hk_finance_detail
