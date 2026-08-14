#include "tdx/historical_securities.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <charconv>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <tuple>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr std::size_t maximum_file_size = 2 * 1024 * 1024;
constexpr std::size_t maximum_line_size = 512;
constexpr std::size_t maximum_records = 100000;

bool digits(std::string_view value, std::size_t length) {
    return value.size() == length &&
           std::all_of(value.begin(), value.end(), [](char ch) {
               return ch >= '0' && ch <= '9';
           });
}

int parse_market(std::string_view text, const fs::path& path,
                 std::size_t line) {
    int value{};
    const auto parsed = std::from_chars(
        text.data(), text.data() + text.size(), value);
    if (text.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != text.data() + text.size() || value < 0 || value > 2)
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " market must be 0, 1, or 2");
    return value;
}

std::string market_name(int market) {
    if (market == 0) return "sz";
    if (market == 1) return "sh";
    return "bj";
}

std::string market_prefix(int market) {
    if (market == 0) return "SZ";
    if (market == 1) return "SH";
    return "BJ";
}

int requested_market(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value.empty() || value == "all") return -1;
    if (value == "sz" || value == "0") return 0;
    if (value == "sh" || value == "1") return 1;
    if (value == "bj" || value == "2") return 2;
    throw Error("market must be all, sz/sh/bj, or 0/1/2");
}

std::string generated_at() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output;
    output << local_timestamp_text(local);
    return output.str();
}

const Security* current_security(
    const HistoricalSecurityName& record,
    const SecurityCatalog& current_securities) {
    const auto found = current_securities.find({record.market_id, record.code});
    return found == current_securities.end() ? nullptr : &found->second;
}

bool matches(const HistoricalSecurityName& record,
             const HistoricalSecurityQuery& query, int market,
             const Security* current) {
    if (market >= 0 && record.market_id != market) return false;
    if (!query.code.empty() && record.code != query.code) return false;
    if (query.presence == "current" && !current) return false;
    if (query.presence == "absent" && current) return false;
    const auto needle = lower_ascii(query.query);
    if (needle.empty()) return true;
    const auto haystack = lower_ascii(
        market_name(record.market_id) + " " + record.code + " " +
        record.name + " " + (current ? current->name : std::string{}));
    return haystack.find(needle) != std::string::npos;
}

bool less_by(const HistoricalSecurityName& left,
             const HistoricalSecurityName& right,
             const HistoricalSecurityQuery& query,
             const SecurityCatalog& current_securities) {
    if (query.sort == "name")
        return std::tie(left.name, left.market_id, left.code) <
               std::tie(right.name, right.market_id, right.code);
    if (query.sort == "presence") {
        const bool left_present = current_security(left, current_securities);
        const bool right_present = current_security(right, current_securities);
        return std::tie(left_present, left.market_id, left.code) <
               std::tie(right_present, right.market_id, right.code);
    }
    return std::tie(left.market_id, left.code) <
           std::tie(right.market_id, right.code);
}

Json record_json(const HistoricalSecurityName& record,
                 const Security* current) {
    Json security = Json::object();
    security["market_id"] = record.market_id;
    security["market"] = market_name(record.market_id);
    security["code"] = record.code;
    security["security_id"] = market_prefix(record.market_id) + record.code;
    security["name"] = current ? current->name : record.name;
    security["name_resolved"] = true;

    Json result = Json::object();
    result["record_id"] = market_prefix(record.market_id) + record.code;
    result["security"] = std::move(security);
    result["compatibility_name"] = record.name;
    result["current_name"] = current ? Json(current->name) : Json(nullptr);
    result["current_directory_present"] = current != nullptr;
    result["name_differs_from_current"] = current && current->name != record.name;
    result["source_line"] = static_cast<std::uint64_t>(record.source_line);
    return result;
}

Json source_json(const fs::path& path, std::size_t rows) {
    Json source = Json::object();
    source["file"] = path.filename().string();
    source["path"] = path_utf8(path);
    source["size"] = static_cast<std::uint64_t>(fs::file_size(path));
    source["row_count"] = static_cast<std::uint64_t>(rows);
    source["endpoint"] = "local-file:" + path_utf8(path);
    return source;
}

}  // namespace

HistoricalSecurityNameCatalog load_local_historical_security_names(
    const fs::path& root) {
    const auto path = root / "T0002" / "hq_cache" / "pttab.dat";
    if (!fs::is_regular_file(path))
        throw Error("local historical-security resource is unavailable: " +
                    path_utf8(path));
    if (fs::file_size(path) > maximum_file_size)
        throw Error("local historical-security resource exceeds safety limit: " +
                    path_utf8(path));

    std::istringstream input(decode_gbk(read_bytes(path)));
    HistoricalSecurityNameCatalog records;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line.size() > maximum_line_size)
            throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                        " line exceeds safety limit");
        if (records.size() >= maximum_records)
            throw Error("local historical-security record count exceeds safety limit: " +
                        path_utf8(path));
        auto fields = split(line, ',');
        for (auto& field : fields) field = trim(std::move(field));
        if (fields.size() != 3)
            throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                        " expected exactly 3 columns");
        HistoricalSecurityName record;
        record.market_id = parse_market(fields[0], path, line_number);
        record.code = fields[1];
        record.name = fields[2];
        record.source_line = line_number;
        if (!digits(record.code, 6))
            throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                        " code must contain exactly six digits");
        if (record.name.empty())
            throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                        " name must not be empty");
        const auto key = std::make_pair(record.market_id, record.code);
        if (!records.emplace(key, std::move(record)).second)
            throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                        " duplicate market/code");
    }
    if (!input.eof())
        throw Error("failed while reading local historical-security resource: " +
                    path_utf8(path));
    return records;
}

Json load_local_historical_securities(
    const fs::path& root, const HistoricalSecurityQuery& input,
    const SecurityCatalog& current_securities) {
    HistoricalSecurityQuery query = input;
    query.market = lower_ascii(trim(query.market));
    query.code = trim(query.code);
    query.query = trim(query.query);
    query.presence = lower_ascii(trim(query.presence));
    query.sort = lower_ascii(trim(query.sort));
    query.order = lower_ascii(trim(query.order));
    const int market = requested_market(query.market);
    if (!query.code.empty() && !digits(query.code, 6))
        throw Error("code must contain exactly six digits");
    if (query.presence != "all" && query.presence != "current" &&
        query.presence != "absent")
        throw Error("presence must be all, current, or absent");
    if (query.sort != "code" && query.sort != "name" &&
        query.sort != "presence")
        throw Error("sort must be code, name, or presence");
    if (query.order != "asc" && query.order != "desc")
        throw Error("order must be asc or desc");
    if (query.offset < 0 || query.offset > 1000000)
        throw Error("offset must be in 0..1000000");
    if (query.limit < 1 || query.limit > 10000)
        throw Error("limit must be in 1..10000");

    const auto catalog = load_local_historical_security_names(root);
    std::vector<const HistoricalSecurityName*> matched;
    for (const auto& [key, record] : catalog) {
        const auto* current = current_security(record, current_securities);
        if (matches(record, query, market, current)) matched.push_back(&record);
    }
    std::stable_sort(matched.begin(), matched.end(), [&](const auto* left,
                                                          const auto* right) {
        const bool less = less_by(*left, *right, query, current_securities);
        const bool reverse = less_by(*right, *left, query, current_securities);
        if (!less && !reverse) return left->source_line < right->source_line;
        return query.order == "asc" ? less : reverse;
    });

    std::size_t current_count = 0;
    std::size_t changed_count = 0;
    std::size_t sz_count = 0;
    std::size_t sh_count = 0;
    std::size_t bj_count = 0;
    for (const auto* record : matched) {
        const auto* current = current_security(*record, current_securities);
        if (current) {
            ++current_count;
            if (current->name != record->name) ++changed_count;
        }
        if (record->market_id == 0) ++sz_count;
        else if (record->market_id == 1) ++sh_count;
        else ++bj_count;
    }

    const auto begin = std::min<std::size_t>(query.offset, matched.size());
    const auto end = std::min<std::size_t>(
        matched.size(), begin + static_cast<std::size_t>(query.limit));
    Json records = Json::array();
    for (std::size_t index = begin; index < end; ++index) {
        const auto* record = matched[index];
        records.push_back(record_json(
            *record, current_security(*record, current_securities)));
    }

    Json filters = Json::object();
    filters["market"] = query.market.empty() ? "all" : query.market;
    filters["code"] = query.code.empty() ? Json(nullptr) : Json(query.code);
    filters["q"] = query.query;
    filters["presence"] = query.presence;
    filters["sort"] = query.sort;
    filters["order"] = query.order;
    filters["offset"] = query.offset;
    filters["limit"] = query.limit;

    Json summary = Json::object();
    summary["shenzhen"] = static_cast<std::uint64_t>(sz_count);
    summary["shanghai"] = static_cast<std::uint64_t>(sh_count);
    summary["beijing"] = static_cast<std::uint64_t>(bj_count);
    summary["current_directory_present"] =
        static_cast<std::uint64_t>(current_count);
    summary["absent_from_current_directory"] =
        static_cast<std::uint64_t>(matched.size() - current_count);
    summary["name_differs_from_current"] =
        static_cast<std::uint64_t>(changed_count);

    Json semantics = Json::object();
    semantics["native_loader"] = "TDXRun.dll sub_10043E10";
    semantics["native_format"] = "market,code,name";
    semantics["native_code_width"] = 6;
    semantics["native_ui_name_limit_bytes"] = 8;
    semantics["name_preservation"] =
        "the C++ reader preserves the full GB18030 resource name instead of the native dialog's 8-byte display truncation";
    semantics["scope"] =
        "compatibility lookup table containing both current and historical instruments; absence from the current TNF directory is reported, not inferred solely from the file name";

    const auto path = root / "T0002" / "hq_cache" / "pttab.dat";
    Json result = Json::object();
    result["schema"] = "tdx-market-historical-securities-native-v1";
    result["generated_at"] = generated_at();
    result["source_mode"] = "local";
    result["availability"] = matched.empty() ? "empty" : "local";
    result["match_count"] = static_cast<std::uint64_t>(matched.size());
    result["returned"] = static_cast<std::uint64_t>(records.size());
    result["has_more"] = end < matched.size();
    result["next_offset"] = end < matched.size()
        ? Json(static_cast<std::uint64_t>(end)) : Json(nullptr);
    result["filters"] = std::move(filters);
    result["summary"] = std::move(summary);
    result["records"] = std::move(records);
    result["sources"] = Json::array();
    result["sources"].push_back(source_json(path, catalog.size()));
    result["semantics"] = std::move(semantics);
    Json transport = Json::object();
    transport["kind"] = "local-files";
    transport["network_requests"] = 0;
    result["transport"] = std::move(transport);
    return result;
}

}  // namespace tdx
