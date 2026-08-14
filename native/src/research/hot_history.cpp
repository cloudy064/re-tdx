#include "tdx/hot_history.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/historical_securities.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <string_view>
#include <tuple>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr std::size_t maximum_file_size = 8 * 1024 * 1024;
constexpr std::size_t maximum_line_size = 8192;
constexpr std::size_t maximum_records = 100000;

struct HotRecord {
    int market{};
    std::string code;
    std::string start_date;
    std::string end_date;
    int trading_days{};
    std::string theme;
    double interval_return_pct{};
    double peak_return_pct{};
    std::string client_analysis;
    std::string analysis;
    std::size_t source_line{};
    std::size_t source_columns{};
};

bool digits(std::string_view value, std::size_t length) {
    return value.size() == length &&
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

double parse_number(const std::string& text, std::string_view field,
                    const fs::path& path, std::size_t line) {
    try {
        std::size_t used = 0;
        const double value = std::stod(text, &used);
        if (used != text.size() || !std::isfinite(value))
            throw std::invalid_argument("number");
        return value;
    } catch (...) {
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " invalid " + std::string(field));
    }
}

std::string join_tail(const std::vector<std::string>& fields,
                      std::size_t first) {
    std::string result;
    for (std::size_t index = first; index < fields.size(); ++index) {
        if (index != first) result.push_back('|');
        result += fields[index];
    }
    return trim(std::move(result));
}

HotRecord parse_record(const std::string& line, const fs::path& path,
                       std::size_t line_number) {
    auto fields = split(line, '|');
    for (auto& field : fields) field = trim(std::move(field));
    if (fields.size() < 9)
        throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                    " expected at least 9 columns");

    HotRecord record;
    record.market = parse_integer(fields[0], "market", path, line_number);
    if (record.market < 0 || record.market > 2)
        throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                    " market must be 0, 1, or 2");
    record.code = fields[1];
    if (!digits(record.code, 6))
        throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                    " invalid security code");
    record.start_date = fields[2];
    record.end_date = fields[3];
    if (!digits(record.start_date, 8) || !digits(record.end_date, 8) ||
        record.start_date > record.end_date)
        throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                    " invalid hot interval dates");
    record.trading_days = parse_integer(fields[4], "trading days", path,
                                        line_number);
    if (record.trading_days < 1)
        throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                    " trading days must be positive");
    record.theme = fields[5];
    record.interval_return_pct = parse_number(
        fields[6], "interval return", path, line_number);
    record.peak_return_pct = parse_number(
        fields[7], "peak return", path, line_number);
    record.client_analysis = fields[8];
    record.analysis = join_tail(fields, 8);
    record.source_line = line_number;
    record.source_columns = fields.size();
    return record;
}

std::vector<HotRecord> load_records(const fs::path& path) {
    if (!fs::is_regular_file(path))
        throw Error("local hot-history resource is unavailable: " +
                    path_utf8(path));
    if (fs::file_size(path) > maximum_file_size)
        throw Error("local hot-history resource exceeds safety limit: " +
                    path_utf8(path));
    const auto text = decode_gbk(read_bytes(path));
    std::istringstream input(text);
    std::vector<HotRecord> records;
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
            throw Error("local hot-history record count exceeds safety limit: " +
                        path_utf8(path));
        records.push_back(parse_record(line, path, line_number));
    }
    if (!input.eof())
        throw Error("failed while reading local hot-history resource: " +
                    path_utf8(path));
    return records;
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

int requested_market(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized.empty() || normalized == "all") return -1;
    if (normalized == "sz" || normalized == "0") return 0;
    if (normalized == "sh" || normalized == "1") return 1;
    if (normalized == "bj" || normalized == "2") return 2;
    throw Error("market must be all, sz/sh/bj, or 0/1/2");
}

std::string validate_date(std::string value, std::string_view name) {
    value = trim(std::move(value));
    if (!value.empty() && !digits(value, 8))
        throw Error(std::string(name) + " must be YYYYMMDD");
    return value;
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

struct ResolvedSecurityName {
    std::string name;
    std::string source{"unresolved"};
    bool current_directory_present{};
};

ResolvedSecurityName security_name(
    const HotRecord& record,
    const std::map<std::pair<int, std::string>, Security>& securities,
    const HistoricalSecurityNameCatalog& historical_names) {
    const auto found = securities.find({record.market, record.code});
    if (found != securities.end())
        return {found->second.name, "current-directory", true};
    const auto historical = historical_names.find({record.market, record.code});
    if (historical != historical_names.end())
        return {historical->second.name, "historical-compatibility", false};
    return {};
}

bool matches_query(const HotRecord& record, const HotHistoryQuery& query,
                   int market, const ResolvedSecurityName& name) {
    if (market >= 0 && record.market != market) return false;
    if (!query.code.empty() && record.code != query.code) return false;
    if (!query.date_from.empty() && record.end_date < query.date_from) return false;
    if (!query.date_to.empty() && record.start_date > query.date_to) return false;
    const auto needle = lower_ascii(trim(query.query));
    if (needle.empty()) return true;
    const auto haystack = lower_ascii(
        market_name(record.market) + " " + record.code + " " + name.name + " " +
        record.theme + " " + record.analysis);
    return haystack.find(needle) != std::string::npos;
}

Json security_json(const HotRecord& record, const ResolvedSecurityName& name) {
    Json security = Json::object();
    security["market_id"] = record.market;
    security["market"] = market_name(record.market);
    security["code"] = record.code;
    security["security_id"] = market_prefix(record.market) + record.code;
    security["name"] = name.name;
    security["name_resolved"] = !name.name.empty();
    security["name_source"] = name.source;
    return security;
}

Json record_json(const HotRecord& record, const ResolvedSecurityName& name) {
    Json result = Json::object();
    result["security"] = security_json(record, name);
    result["start_date"] = record.start_date;
    result["end_date"] = record.end_date;
    result["trading_days"] = record.trading_days;
    result["theme"] = record.theme;
    result["interval_return_pct"] = record.interval_return_pct;
    result["peak_return_pct"] = record.peak_return_pct;
    result["analysis"] = record.analysis;
    result["native_client_analysis"] = record.client_analysis;
    result["analysis_contains_embedded_delimiter"] = record.source_columns > 9;
    result["native_host_eligible"] = name.current_directory_present;
    result["source_line"] = static_cast<std::uint64_t>(record.source_line);
    return result;
}

void validate_sort(HotHistoryQuery& query) {
    query.sort = lower_ascii(trim(query.sort));
    query.order = lower_ascii(trim(query.order));
    if (query.sort != "start-date" && query.sort != "end-date" &&
        query.sort != "return" && query.sort != "peak" &&
        query.sort != "days" && query.sort != "code")
        throw Error("sort must be start-date, end-date, return, peak, days, or code");
    if (query.order != "asc" && query.order != "desc")
        throw Error("order must be asc or desc");
}

bool less_by(const HotRecord& left, const HotRecord& right,
             const std::string& sort) {
    if (sort == "end-date") return left.end_date < right.end_date;
    if (sort == "return") return left.interval_return_pct < right.interval_return_pct;
    if (sort == "peak") return left.peak_return_pct < right.peak_return_pct;
    if (sort == "days") return left.trading_days < right.trading_days;
    if (sort == "code")
        return std::tie(left.market, left.code, left.start_date) <
               std::tie(right.market, right.code, right.start_date);
    return left.start_date < right.start_date;
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

Json load_local_hot_history(
    const fs::path& root, const HotHistoryQuery& input,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    HotHistoryQuery query = input;
    query.market = lower_ascii(trim(query.market));
    query.code = trim(query.code);
    query.query = trim(query.query);
    query.date_from = validate_date(query.date_from, "from");
    query.date_to = validate_date(query.date_to, "to");
    if (!query.date_from.empty() && !query.date_to.empty() &&
        query.date_from > query.date_to)
        throw Error("from must not be later than to");
    if (!query.code.empty() && !digits(query.code, 6))
        throw Error("code must contain exactly six digits");
    if (query.offset < 0 || query.offset > 1000000)
        throw Error("offset must be in 0..1000000");
    if (query.limit < 1 || query.limit > 10000)
        throw Error("limit must be in 1..10000");
    validate_sort(query);
    const int market = requested_market(query.market);

    const auto path = root / "T0002" / "hq_cache" / "speczshot.txt";
    auto all = load_records(path);
    HistoricalSecurityNameCatalog historical_names;
    const auto historical_path = root / "T0002" / "hq_cache" / "pttab.dat";
    if (fs::is_regular_file(historical_path))
        historical_names = load_local_historical_security_names(root);
    struct Match {
        HotRecord record;
        ResolvedSecurityName name;
    };
    std::vector<Match> matches;
    for (const auto& record : all) {
        auto name = security_name(record, securities, historical_names);
        if (matches_query(record, query, market, name))
            matches.push_back({record, std::move(name)});
    }
    std::stable_sort(matches.begin(), matches.end(), [&](const Match& left,
                                                          const Match& right) {
        const bool less = less_by(left.record, right.record, query.sort);
        const bool reverse_less = less_by(right.record, left.record, query.sort);
        if (!less && !reverse_less)
            return std::tie(left.record.market, left.record.code,
                            left.record.start_date, left.record.source_line) <
                   std::tie(right.record.market, right.record.code,
                            right.record.start_date, right.record.source_line);
        return query.order == "asc" ? less : reverse_less;
    });

    std::set<std::pair<int, std::string>> distinct_securities;
    std::size_t resolved = 0;
    std::size_t embedded_delimiters = 0;
    std::string earliest_start;
    std::string latest_end;
    double maximum_return = -std::numeric_limits<double>::infinity();
    double maximum_peak = -std::numeric_limits<double>::infinity();
    for (const auto& match : matches) {
        const auto& record = match.record;
        distinct_securities.emplace(record.market, record.code);
        if (match.name.current_directory_present) ++resolved;
        if (record.source_columns > 9) ++embedded_delimiters;
        if (earliest_start.empty() || record.start_date < earliest_start)
            earliest_start = record.start_date;
        if (latest_end.empty() || record.end_date > latest_end)
            latest_end = record.end_date;
        maximum_return = std::max(maximum_return, record.interval_return_pct);
        maximum_peak = std::max(maximum_peak, record.peak_return_pct);
    }

    Json records = Json::array();
    const auto begin = std::min<std::size_t>(
        static_cast<std::size_t>(query.offset), matches.size());
    const auto end = std::min(matches.size(), begin +
        static_cast<std::size_t>(query.limit));
    for (std::size_t index = begin; index < end; ++index)
        records.push_back(record_json(matches[index].record, matches[index].name));

    Json summary = Json::object();
    summary["security_count"] =
        static_cast<std::uint64_t>(distinct_securities.size());
    summary["native_host_eligible_records"] = static_cast<std::uint64_t>(resolved);
    summary["historical_name_fallback_records"] =
        static_cast<std::uint64_t>(std::count_if(
            matches.begin(), matches.end(), [](const Match& match) {
                return match.name.source == "historical-compatibility";
            }));
    summary["unresolved_security_records"] =
        static_cast<std::uint64_t>(std::count_if(
            matches.begin(), matches.end(), [](const Match& match) {
                return match.name.name.empty();
            }));
    summary["embedded_delimiter_records"] =
        static_cast<std::uint64_t>(embedded_delimiters);
    summary["earliest_start_date"] = earliest_start.empty() ? Json(nullptr) : Json(earliest_start);
    summary["latest_end_date"] = latest_end.empty() ? Json(nullptr) : Json(latest_end);
    summary["maximum_interval_return_pct"] = matches.empty()
        ? Json(nullptr) : Json(maximum_return);
    summary["maximum_peak_return_pct"] = matches.empty()
        ? Json(nullptr) : Json(maximum_peak);

    Json filters = Json::object();
    filters["market"] = query.market.empty() ? "all" : query.market;
    filters["code"] = query.code.empty() ? Json(nullptr) : Json(query.code);
    filters["q"] = query.query;
    filters["from"] = query.date_from.empty() ? Json(nullptr) : Json(query.date_from);
    filters["to"] = query.date_to.empty() ? Json(nullptr) : Json(query.date_to);
    filters["sort"] = query.sort;
    filters["order"] = query.order;
    filters["offset"] = query.offset;
    filters["limit"] = query.limit;

    Json semantics = Json::object();
    semantics["native_loader"] = "TdxW.exe sub_5DFD00";
    semantics["native_record_size_bytes"] = 1110;
    semantics["interval"] =
        "start/end K-line highlight interval; trading_days follows the market calendar";
    semantics["interval_return_pct"] =
        "end close return from the adjusted close immediately before start_date";
    semantics["peak_return_pct"] =
        "highest intrainterval high return from the same adjusted baseline";
    semantics["analysis_recovery"] =
        "the native client consumes pipe column 9 only; analysis rejoins later columns while native_client_analysis preserves the exact client-visible segment";
    semantics["historical_name_fallback"] =
        "pttab.dat compatibility names fill records absent from the current TNF directory without claiming that the native host can still request them";

    Json result = Json::object();
    result["schema"] = "tdx-market-hot-history-native-v1";
    result["generated_at"] = generated_at();
    result["source_mode"] = "local";
    result["mode"] = query.code.empty() ? "catalog" : "security";
    result["availability"] = matches.empty() ? "empty" : "local";
    result["match_count"] = static_cast<std::uint64_t>(matches.size());
    result["returned"] = static_cast<std::uint64_t>(records.size());
    result["has_more"] = end < matches.size();
    result["next_offset"] = end < matches.size()
        ? Json(static_cast<std::uint64_t>(end)) : Json(nullptr);
    result["filters"] = std::move(filters);
    result["summary"] = std::move(summary);
    result["records"] = std::move(records);
    result["sources"] = Json::array();
    result["sources"].push_back(source_json(path, all.size()));
    if (!historical_names.empty())
        result["sources"].push_back(
            source_json(historical_path, historical_names.size()));
    result["semantics"] = std::move(semantics);
    Json transport = Json::object();
    transport["kind"] = "local-files";
    transport["network_requests"] = 0;
    result["transport"] = std::move(transport);
    return result;
}

}  // namespace tdx
