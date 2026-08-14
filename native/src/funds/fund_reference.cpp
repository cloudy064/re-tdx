#include "tdx/fund_reference.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx {
namespace {

struct ViewDefinition {
    std::string_view name;
    std::string_view kind;
};

constexpr std::array<ViewDefinition, 4> view_catalog{{
    {"all", ""},
    {"snapshot", "fund-snapshot"},
    {"etf-mapping", "etf-mapping"},
    {"lof-mapping", "lof-mapping"},
}};

constexpr std::size_t maximum_file_size = 8 * 1024 * 1024;
constexpr std::size_t maximum_line_size = 4096;
constexpr std::size_t maximum_records = 100000;

std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t begin = 0;
    for (;;) {
        const auto comma = line.find(',', begin);
        fields.push_back(trim(line.substr(
            begin, comma == std::string::npos ? std::string::npos : comma - begin)));
        if (comma == std::string::npos) break;
        begin = comma + 1;
    }
    return fields;
}

bool six_digits(std::string_view value) {
    return value.size() == 6 &&
           std::all_of(value.begin(), value.end(), [](char ch) {
               return ch >= '0' && ch <= '9';
           });
}

bool eight_digits(std::string_view value) {
    return value.size() == 8 &&
           std::all_of(value.begin(), value.end(), [](char ch) {
               return ch >= '0' && ch <= '9';
           });
}

int parse_integer(std::string_view text, std::string_view field,
                  const fs::path& path, std::size_t line) {
    int value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || result.ec != std::errc{} ||
        result.ptr != text.data() + text.size())
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " invalid " + std::string(field));
    return value;
}

std::optional<double> parse_optional_number(
    const std::string& text, std::string_view field,
    const fs::path& path, std::size_t line) {
    if (text.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const double value = std::stod(text, &used);
        if (used != text.size() || !std::isfinite(value) || value < 0.0)
            throw std::invalid_argument("value");
        return value;
    } catch (...) {
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " invalid " + std::string(field));
    }
}

std::string today_yyyymmdd() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output;
    output << std::put_time(&local, "%Y%m%d");
    return output.str();
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

std::string market_name(int market) {
    return market == 0 ? "sz" : "sh";
}

std::string market_prefix(int market) {
    return market == 0 ? "SZ" : "SH";
}

Json security_json(
    int market, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    Json result = Json::object();
    result["market_id"] = market;
    result["market"] = market_name(market);
    result["code"] = code;
    result["security_id"] = market_prefix(market) + code;
    const auto found = securities.find({market, code});
    const auto name = found == securities.end() ? std::string{} : found->second.name;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    return result;
}

Json nullable_number(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

void validate_file(const fs::path& path) {
    if (!fs::is_regular_file(path))
        throw Error("local fund-reference resource is unavailable: " + path_utf8(path));
    const auto size = fs::file_size(path);
    if (size > maximum_file_size)
        throw Error("local fund-reference resource exceeds safety limit: " +
                    path_utf8(path));
}

template <typename Callback>
std::size_t for_each_line(const fs::path& path, Callback&& callback) {
    validate_file(path);
    std::ifstream input(path, std::ios::binary);
    if (!input) throw Error("failed to open local fund-reference resource: " + path_utf8(path));
    std::string line;
    std::size_t line_number = 0;
    std::size_t records = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line.size() > maximum_line_size)
            throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                        " line exceeds safety limit");
        if (++records > maximum_records)
            throw Error("local fund-reference record count exceeds safety limit: " +
                        path_utf8(path));
        callback(split_csv(line), line_number);
    }
    if (!input.eof())
        throw Error("failed while reading local fund-reference resource: " + path_utf8(path));
    return records;
}

Json parse_snapshot(const std::vector<std::string>& fields,
                    const fs::path& path, std::size_t line,
                    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (fields.size() != 7)
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " expected 7 columns");
    if (!six_digits(fields[0]))
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " invalid security code");
    const int market = parse_integer(fields[1], "security market", path, line);
    if (market < 0 || market > 1)
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " security market must be 0 or 1");
    if (!eight_digits(fields[3]))
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " snapshot date must be YYYYMMDD");
    const auto units_10k = parse_optional_number(fields[4], "fund units", path, line);
    const auto unit_reference =
        parse_optional_number(fields[5], "unit reference value", path, line);
    const auto unit_nav = parse_optional_number(fields[6], "unit NAV", path, line);

    Json record = Json::object();
    record["kind"] = "fund-snapshot";
    record["security"] = security_json(market, fields[0], securities);
    record["as_of_date"] = fields[3];
    record["fund_units_10k"] = nullable_number(units_10k);
    record["fund_units"] = units_10k ? Json(*units_10k * 10000.0) : Json(nullptr);
    record["unit_reference_value"] = nullable_number(unit_reference);
    record["unit_nav"] = nullable_number(unit_nav);
    Json native = Json::object();
    native["reserved_column_3"] = fields[2];
    native["shares_offsets"] = Json::array();
    native["shares_offsets"].push_back(86);
    native["shares_offsets"].push_back(132);
    native["unit_reference_offset"] = 244;
    native["published_unit_nav_consumed_by_host"] = false;
    record["native_semantics"] = std::move(native);
    record["source_file"] = "specjjdata.txt";
    return record;
}

Json reference_json(int market, const std::string& source_code) {
    if (source_code.empty()) return Json(nullptr);
    std::string code = source_code;
    std::string normalization;
    if (code == "IXIC" || code == "NDX" || code == "NBI") {
        code = "A_" + code;
        normalization = "native-us-index-alias";
    } else if (market == 1 && code == "000001") {
        code = "999999";
        normalization = "native-shanghai-composite-alias";
    }
    Json result = Json::object();
    result["native_market_id"] = market;
    result["market"] = market == 0 ? "sz" : market == 1 ? "sh" :
        "m" + std::to_string(market);
    result["source_code"] = source_code;
    result["code"] = code;
    result["security_id"] = "M" + std::to_string(market) + code;
    result["normalization"] = normalization.empty() ? Json(nullptr) : Json(normalization);
    return result;
}

Json parse_etf_mapping(
    const std::vector<std::string>& fields, const fs::path& path,
    std::size_t line, const std::string& as_of_date,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (fields.size() != 8)
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " expected 8 columns");
    const int market = parse_integer(fields[0], "security market", path, line);
    if (market < 0 || market > 1)
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " security market must be 0 or 1");
    if (!six_digits(fields[1]))
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " invalid security code");
    const int reference_market =
        parse_integer(fields[3], "reference market", path, line);
    if (reference_market < 0 || reference_market > 999)
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " reference market must be 0..999");
    if ((!fields[6].empty() && !eight_digits(fields[6])) ||
        (!fields[7].empty() && !eight_digits(fields[7])))
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " native window dates must be empty or YYYYMMDD");
    const bool has_window = !fields[6].empty() && !fields[7].empty();
    const int status = !has_window ? 0 :
        as_of_date < fields[6] ? 1 : as_of_date > fields[7] ? 3 : 2;
    Json record = Json::object();
    record["kind"] = "etf-mapping";
    record["security"] = security_json(market, fields[1], securities);
    record["reference_instrument"] = reference_json(reference_market, fields[2]);
    record["native_catalog_id"] = fields[4];
    Json dates = Json::object();
    dates["window_start"] = fields[6].empty() ? Json(nullptr) : Json(fields[6]);
    dates["window_end"] = fields[7].empty() ? Json(nullptr) : Json(fields[7]);
    record["dates"] = std::move(dates);
    record["native_lifecycle_status"] =
        status == 0 ? Json(nullptr) : Json(status);
    record["lifecycle_phase"] = status == 0 ? Json(nullptr) :
        status == 1 ? Json("before-window") :
        status == 2 ? Json("in-window") : Json("after-window");
    Json native = Json::object();
    native["reserved_column_6"] = fields[5];
    native["record_size"] = 48;
    record["native_semantics"] = std::move(native);
    record["source_file"] = "specetfdata.txt";
    return record;
}

Json parse_lof_mapping(
    const std::vector<std::string>& fields, const fs::path& path,
    std::size_t line,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (fields.size() != 6)
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " expected 6 columns");
    const int market = parse_integer(fields[0], "security market", path, line);
    if (market < 0 || market > 1)
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " security market must be 0 or 1");
    if (!six_digits(fields[1]))
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " invalid security code");
    const int reference_market =
        parse_integer(fields[3], "reference market", path, line);
    if (reference_market < 0 || reference_market > 999)
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " reference market must be 0..999");

    Json record = Json::object();
    record["kind"] = "lof-mapping";
    record["security"] = security_json(market, fields[1], securities);
    record["reference_instrument"] = reference_json(reference_market, fields[2]);
    record["native_catalog_id"] = fields[4];
    Json native = Json::object();
    native["reserved_column_6"] = fields[5];
    native["record_size"] = 40;
    record["native_semantics"] = std::move(native);
    record["source_file"] = "speclofdata.txt";
    return record;
}

int requested_market(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized.empty() || normalized == "all") return -1;
    if (normalized == "sz" || normalized == "0") return 0;
    if (normalized == "sh" || normalized == "1") return 1;
    throw Error("market must be all, sz/sh, or 0/1");
}

bool requested_view(const std::string& view, std::string_view kind) {
    const auto found = std::find_if(view_catalog.begin(), view_catalog.end(),
        [&](const ViewDefinition& entry) { return entry.name == view; });
    if (found == view_catalog.end())
        throw Error("view must be all, snapshot, etf-mapping, or lof-mapping");
    return found->kind.empty() || found->kind == kind;
}

bool matches(const Json& record, int market, const FundReferenceQuery& query) {
    const auto& security = record.at("security");
    if (market >= 0 && static_cast<int>(security.at("market_id").as_number()) != market)
        return false;
    if (!query.code.empty() && security.at("code").as_string() != query.code)
        return false;
    const auto needle = lower_ascii(trim(query.query));
    if (needle.empty()) return true;
    std::string haystack = security.at("market").as_string() + " " +
        security.at("code").as_string() + " " + security.at("name").as_string() + " " +
        record.at("kind").as_string();
    if (record.at("kind").as_string() == "etf-mapping") {
        haystack += " " + record.at("native_catalog_id").as_string();
        const auto& reference = record.at("reference_instrument");
        if (!reference.is_null())
            haystack += " " + reference.at("source_code").as_string() + " " +
                        reference.at("code").as_string();
    }
    return lower_ascii(haystack).find(needle) != std::string::npos;
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

Json load_local_fund_reference(
    const fs::path& root, const FundReferenceQuery& input,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    FundReferenceQuery query = input;
    query.view = lower_ascii(trim(query.view));
    query.code = trim(query.code);
    if (query.limit < 1 || query.limit > 10000)
        throw Error("limit must be in 1..10000");
    if (!query.code.empty() && !six_digits(query.code))
        throw Error("code must contain exactly six digits");
    const int market = requested_market(query.market);
    const std::string as_of_date = query.as_of_date.empty()
        ? today_yyyymmdd() : trim(query.as_of_date);
    if (!eight_digits(as_of_date)) throw Error("as_of_date must be YYYYMMDD");

    const bool snapshots_requested = requested_view(query.view, "fund-snapshot");
    const bool mappings_requested = requested_view(query.view, "etf-mapping");
    const bool lof_requested = requested_view(query.view, "lof-mapping");
    const auto cache = root / "T0002" / "hq_cache";
    const auto snapshot_path = cache / "specjjdata.txt";
    const auto mapping_path = cache / "specetfdata.txt";
    const auto lof_path = cache / "speclofdata.txt";
    Json all = Json::array();
    Json sources = Json::array();
    std::size_t snapshot_rows = 0;
    std::size_t mapping_rows = 0;
    std::size_t lof_rows = 0;
    if (snapshots_requested) {
        snapshot_rows = for_each_line(snapshot_path,
            [&](const auto& fields, std::size_t line) {
                all.push_back(parse_snapshot(fields, snapshot_path, line, securities));
            });
        sources.push_back(source_json(snapshot_path, snapshot_rows));
    }
    if (mappings_requested) {
        mapping_rows = for_each_line(mapping_path,
            [&](const auto& fields, std::size_t line) {
                all.push_back(parse_etf_mapping(
                    fields, mapping_path, line, as_of_date, securities));
            });
        sources.push_back(source_json(mapping_path, mapping_rows));
    }
    if (lof_requested) {
        lof_rows = for_each_line(lof_path,
            [&](const auto& fields, std::size_t line) {
                all.push_back(parse_lof_mapping(fields, lof_path, line, securities));
            });
        sources.push_back(source_json(lof_path, lof_rows));
    }

    Json records = Json::array();
    std::size_t matched = 0;
    std::array<std::size_t, 4> statuses{};
    std::size_t snapshots = 0;
    std::size_t mappings = 0;
    std::size_t lof_mappings = 0;
    std::size_t resolved_references = 0;
    for (const auto& record : all.as_array()) {
        if (!matches(record, market, query)) continue;
        ++matched;
        const auto& kind = record.at("kind").as_string();
        if (kind == "fund-snapshot") {
            ++snapshots;
        } else if (kind == "etf-mapping") {
            ++mappings;
            const auto status = record.at("native_lifecycle_status").is_null()
                ? std::size_t{0}
                : static_cast<std::size_t>(
                    record.at("native_lifecycle_status").as_number());
            if (status < statuses.size()) ++statuses[status];
            if (!record.at("reference_instrument").is_null()) ++resolved_references;
        } else {
            ++lof_mappings;
            if (!record.at("reference_instrument").is_null()) ++resolved_references;
        }
        if (records.size() < static_cast<std::size_t>(query.limit))
            records.push_back(record);
    }

    Json summary = Json::object();
    summary["fund_snapshots"] = static_cast<std::uint64_t>(snapshots);
    summary["etf_mappings"] = static_cast<std::uint64_t>(mappings);
    summary["lof_mappings"] = static_cast<std::uint64_t>(lof_mappings);
    summary["reference_instruments"] = static_cast<std::uint64_t>(resolved_references);
    summary["native_status_unset"] = static_cast<std::uint64_t>(statuses[0]);
    summary["native_status_1"] = static_cast<std::uint64_t>(statuses[1]);
    summary["native_status_2"] = static_cast<std::uint64_t>(statuses[2]);
    summary["native_status_3"] = static_cast<std::uint64_t>(statuses[3]);

    Json result = Json::object();
    result["schema"] = "tdx-market-fund-reference-native-v1";
    result["generated_at"] = generated_at();
    result["source_mode"] = "local";
    result["view"] = query.view;
    result["as_of_date"] = as_of_date;
    result["mode"] = query.code.empty() ? "catalog" : "security";
    result["availability"] = matched ? "local" : "empty";
    result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(records.size());
    result["summary"] = std::move(summary);
    result["records"] = std::move(records);
    result["sources"] = std::move(sources);
    Json transport = Json::object();
    transport["kind"] = "local-files";
    transport["network_requests"] = 0;
    result["transport"] = std::move(transport);
    result["semantics"] =
        "specjjdata fund units/reference-value/published-NAV snapshots plus specetfdata "
        "ETF and speclofdata LOF reference mappings; native host consumes units at security-record offsets "
        "86/132 and the trading reference value at 244, while retaining but not consuming "
        "the published unit NAV column.";
    return result;
}

}  // namespace tdx
