#include "tdx/index_events.hpp"
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

constexpr std::size_t maximum_file_size = 1024 * 1024;
constexpr std::size_t maximum_line_size = 2048;
constexpr std::size_t maximum_records = 10000;
constexpr std::string_view native_detail_prefix = "http://www.treeid/dlg";
constexpr std::string_view default_detail_template =
    "http://www.treeid/dlghttp://page1.tdx.com.cn:7615/site/tdx-pc-content/"
    "page-cjzx.html?type=1&recid=%d&zxflag=0";

struct IndexEvent {
    int native_kind{};
    std::string benchmark;
    std::string month_day;
    std::string occurrence_date;
    std::string chart_date;
    std::string title;
    std::int64_t event_id{};
    std::string source_file;
    std::size_t source_line{};
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

std::int64_t parse_event_id(std::string_view text, const fs::path& path,
                            std::size_t line) {
    std::int64_t value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || result.ec != std::errc{} ||
        result.ptr != text.data() + text.size() || value <= 0)
        throw Error(path_utf8(path) + ":" + std::to_string(line) +
                    " invalid event id");
    return value;
}

std::string benchmark_for_kind(int kind) {
    if (kind == 0) return "shanghai-composite";
    if (kind == 1) return "hang-seng";
    return "nasdaq-composite";
}

std::string occurrence_date(const std::string& chart_date,
                            const std::string& month_day) {
    int year = std::stoi(chart_date.substr(0, 4));
    const auto chart_month_day = chart_date.substr(4, 4);
    if (month_day >= "1201" && chart_month_day <= "0131" &&
        month_day > chart_month_day)
        --year;
    std::ostringstream result;
    result << std::setw(4) << std::setfill('0') << year << month_day;
    return result.str();
}

IndexEvent parse_event(const std::string& line, const fs::path& path,
                       std::size_t line_number, bool typed) {
    auto fields = split(line, '|');
    for (auto& field : fields) field = trim(std::move(field));
    const std::size_t expected = typed ? 5 : 4;
    if (fields.size() != expected)
        throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                    " expected " + std::to_string(expected) + " columns");
    if (!digits(fields[0], 4) || !digits(fields[1], 8))
        throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                    " invalid event dates");
    if (fields[2].empty())
        throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                    " event title is empty");
    const int kind = typed ? parse_integer(fields[4], "event kind", path,
                                            line_number) : 0;
    if (kind < (typed ? 1 : 0) || kind > (typed ? 2 : 0))
        throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                    " event kind is invalid");
    IndexEvent event;
    event.native_kind = kind;
    event.benchmark = benchmark_for_kind(kind);
    event.month_day = fields[0];
    event.chart_date = fields[1];
    event.occurrence_date = occurrence_date(event.chart_date, event.month_day);
    event.title = fields[2];
    event.event_id = parse_event_id(fields[3], path, line_number);
    event.source_file = path.filename().string();
    event.source_line = line_number;
    return event;
}

std::vector<IndexEvent> load_file(const fs::path& path, bool typed) {
    if (!fs::is_regular_file(path))
        throw Error("local index-events resource is unavailable: " +
                    path_utf8(path));
    if (fs::file_size(path) > maximum_file_size)
        throw Error("local index-events resource exceeds safety limit: " +
                    path_utf8(path));
    std::istringstream input(decode_gbk(read_bytes(path)));
    std::vector<IndexEvent> events;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line.size() > maximum_line_size)
            throw Error(path_utf8(path) + ":" + std::to_string(line_number) +
                        " line exceeds safety limit");
        if (events.size() >= maximum_records)
            throw Error("local index-events record count exceeds safety limit: " +
                        path_utf8(path));
        events.push_back(parse_event(line, path, line_number, typed));
    }
    if (!input.eof())
        throw Error("failed while reading local index-events resource: " +
                    path_utf8(path));
    return events;
}

std::string configured_detail_template(const fs::path& root,
                                       std::string& source) {
    const auto path = root / "T0002" / "hq_cache" / "neednote.dat";
    if (fs::is_regular_file(path) && fs::file_size(path) <= maximum_file_size) {
        std::istringstream input(decode_gbk(read_bytes(path)));
        std::string section;
        std::string line;
        while (std::getline(input, line)) {
            line = trim(std::move(line));
            if (line.empty() || line.front() == ';' || line.front() == '#') continue;
            if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
                section = lower_ascii(trim(line.substr(1, line.size() - 2)));
                continue;
            }
            const auto equals = line.find('=');
            if (equals == std::string::npos || section != "url") continue;
            const auto key = lower_ascii(trim(line.substr(0, equals)));
            const auto value = trim(line.substr(equals + 1));
            if (key == "zseventurl" && !value.empty()) {
                source = path_utf8(path) + " [URL]/ZSEventUrl";
                return value;
            }
        }
    }
    source = "TdxW.exe sub_404530 default ZSEventUrl";
    return std::string(default_detail_template);
}

std::string render_detail_target(std::string value, std::int64_t event_id) {
    const auto marker = value.find("%d");
    if (marker != std::string::npos)
        value.replace(marker, 2, std::to_string(event_id));
    return value;
}

std::string browser_detail_url(const std::string& native_target) {
    if (native_target.rfind(native_detail_prefix, 0) == 0)
        return native_target.substr(native_detail_prefix.size());
    return native_target;
}

std::string normalize_benchmark(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value.empty() || value == "all") return "all";
    if (value == "shanghai-composite" || value == "shanghai" ||
        value == "sh" || value == "0") return "shanghai-composite";
    if (value == "hang-seng" || value == "hangseng" || value == "hk" ||
        value == "1") return "hang-seng";
    if (value == "nasdaq-composite" || value == "nasdaq" || value == "us" ||
        value == "2") return "nasdaq-composite";
    throw Error("benchmark must be all, shanghai-composite, hang-seng, or nasdaq-composite");
}

std::string validate_date(std::string value, std::string_view name) {
    value = trim(std::move(value));
    if (!value.empty() && !digits(value, 8))
        throw Error(std::string(name) + " must be YYYYMMDD");
    return value;
}

std::int64_t requested_event_id(const std::string& text) {
    if (text.empty()) return 0;
    std::int64_t value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        value <= 0)
        throw Error("event_id must be a positive integer");
    return value;
}

std::string event_date(const IndexEvent& event, const std::string& basis) {
    return basis == "occurrence" ? event.occurrence_date : event.chart_date;
}

bool matches_query(const IndexEvent& event, const IndexEventQuery& query,
                   std::int64_t event_id) {
    if (query.benchmark != "all" && event.benchmark != query.benchmark) return false;
    if (event_id && event.event_id != event_id) return false;
    const auto date = event_date(event, query.date_basis);
    if (!query.date_from.empty() && date < query.date_from) return false;
    if (!query.date_to.empty() && date > query.date_to) return false;
    const auto needle = lower_ascii(trim(query.query));
    if (needle.empty()) return true;
    const auto haystack = lower_ascii(event.benchmark + " " + event.title + " " +
                                      std::to_string(event.event_id));
    return haystack.find(needle) != std::string::npos;
}

bool less_by(const IndexEvent& left, const IndexEvent& right,
             const IndexEventQuery& query) {
    if (query.sort == "event-id") return left.event_id < right.event_id;
    if (query.sort == "title") return left.title < right.title;
    return event_date(left, query.date_basis) < event_date(right, query.date_basis);
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

Json event_json(const IndexEvent& event, const std::string& detail_template) {
    const auto target = render_detail_target(detail_template, event.event_id);
    Json result = Json::object();
    result["record_id"] = event.benchmark + ":" + std::to_string(event.event_id);
    result["benchmark"] = event.benchmark;
    result["native_kind"] = event.native_kind;
    result["event_id"] = event.event_id;
    result["title"] = event.title;
    result["month_day"] = event.month_day;
    result["occurrence_date"] = event.occurrence_date;
    result["chart_date"] = event.chart_date;
    result["chart_date_adjusted"] = event.occurrence_date != event.chart_date;
    result["detail_url"] = browser_detail_url(target);
    result["native_detail_target"] = target;
    result["source_file"] = event.source_file;
    result["source_line"] = static_cast<std::uint64_t>(event.source_line);
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

Json load_local_index_events(const fs::path& root,
                             const IndexEventQuery& input) {
    IndexEventQuery query = input;
    query.benchmark = normalize_benchmark(query.benchmark);
    query.event_id = trim(query.event_id);
    query.query = trim(query.query);
    query.date_from = validate_date(query.date_from, "from");
    query.date_to = validate_date(query.date_to, "to");
    query.date_basis = lower_ascii(trim(query.date_basis));
    query.sort = lower_ascii(trim(query.sort));
    query.order = lower_ascii(trim(query.order));
    if (!query.date_from.empty() && !query.date_to.empty() &&
        query.date_from > query.date_to)
        throw Error("from must not be later than to");
    if (query.date_basis != "chart" && query.date_basis != "occurrence")
        throw Error("date_basis must be chart or occurrence");
    if (query.sort != "date" && query.sort != "event-id" &&
        query.sort != "title")
        throw Error("sort must be date, event-id, or title");
    if (query.order != "asc" && query.order != "desc")
        throw Error("order must be asc or desc");
    if (query.offset < 0 || query.offset > 1000000)
        throw Error("offset must be in 0..1000000");
    if (query.limit < 1 || query.limit > 10000)
        throw Error("limit must be in 1..10000");
    const auto wanted_event_id = requested_event_id(query.event_id);

    const auto cache = root / "T0002" / "hq_cache";
    const auto domestic_path = cache / "speczsevent.txt";
    const auto overseas_path = cache / "speczsevent_ds.txt";
    auto domestic = load_file(domestic_path, false);
    auto overseas = load_file(overseas_path, true);
    std::vector<IndexEvent> all;
    all.reserve(domestic.size() + overseas.size());
    all.insert(all.end(), domestic.begin(), domestic.end());
    all.insert(all.end(), overseas.begin(), overseas.end());

    std::vector<IndexEvent> matched;
    for (const auto& event : all)
        if (matches_query(event, query, wanted_event_id)) matched.push_back(event);
    std::stable_sort(matched.begin(), matched.end(), [&](const IndexEvent& left,
                                                          const IndexEvent& right) {
        const bool less = less_by(left, right, query);
        const bool reverse_less = less_by(right, left, query);
        if (!less && !reverse_less)
            return std::tie(left.native_kind, left.event_id) <
                   std::tie(right.native_kind, right.event_id);
        return query.order == "asc" ? less : reverse_less;
    });

    std::string detail_template_source;
    const auto detail_template = configured_detail_template(
        root, detail_template_source);
    std::size_t shanghai = 0;
    std::size_t hang_seng = 0;
    std::size_t nasdaq = 0;
    std::size_t adjusted = 0;
    std::string earliest;
    std::string latest;
    for (const auto& event : matched) {
        if (event.native_kind == 0) ++shanghai;
        else if (event.native_kind == 1) ++hang_seng;
        else ++nasdaq;
        if (event.occurrence_date != event.chart_date) ++adjusted;
        const auto date = event_date(event, query.date_basis);
        if (earliest.empty() || date < earliest) earliest = date;
        if (latest.empty() || date > latest) latest = date;
    }

    const auto begin = std::min<std::size_t>(
        static_cast<std::size_t>(query.offset), matched.size());
    const auto end = std::min(matched.size(), begin +
        static_cast<std::size_t>(query.limit));
    Json records = Json::array();
    for (std::size_t index = begin; index < end; ++index)
        records.push_back(event_json(matched[index], detail_template));

    Json summary = Json::object();
    summary["shanghai_composite"] = static_cast<std::uint64_t>(shanghai);
    summary["hang_seng"] = static_cast<std::uint64_t>(hang_seng);
    summary["nasdaq_composite"] = static_cast<std::uint64_t>(nasdaq);
    summary["chart_date_adjusted"] = static_cast<std::uint64_t>(adjusted);
    summary["earliest_date"] = earliest.empty() ? Json(nullptr) : Json(earliest);
    summary["latest_date"] = latest.empty() ? Json(nullptr) : Json(latest);

    Json filters = Json::object();
    filters["benchmark"] = query.benchmark;
    filters["event_id"] = query.event_id.empty() ? Json(nullptr) : Json(query.event_id);
    filters["q"] = query.query;
    filters["from"] = query.date_from.empty() ? Json(nullptr) : Json(query.date_from);
    filters["to"] = query.date_to.empty() ? Json(nullptr) : Json(query.date_to);
    filters["date_basis"] = query.date_basis;
    filters["sort"] = query.sort;
    filters["order"] = query.order;
    filters["offset"] = query.offset;
    filters["limit"] = query.limit;

    Json semantics = Json::object();
    semantics["native_loader"] = "TdxW.exe sub_5C6C40";
    semantics["native_consumer"] = "TdxW.exe sub_990E20/sub_985B90";
    semantics["native_record_size_bytes"] = 278;
    semantics["kind_0"] = "domestic indices and 8800-series blocks";
    semantics["kind_1"] = "Hang Seng index family";
    semantics["kind_2"] = "Nasdaq/Dow/S&P US index family";
    semantics["date_mapping"] =
        "month_day is the occurrence date; chart_date is the native target-market trading-date annotation";
    semantics["detail_template_source"] = detail_template_source;

    Json sources = Json::array();
    sources.push_back(source_json(domestic_path, domestic.size()));
    sources.push_back(source_json(overseas_path, overseas.size()));
    Json result = Json::object();
    result["schema"] = "tdx-market-index-events-native-v1";
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
    result["sources"] = std::move(sources);
    result["semantics"] = std::move(semantics);
    Json transport = Json::object();
    transport["kind"] = "local-files";
    transport["network_requests"] = 0;
    result["transport"] = std::move(transport);
    return result;
}

}  // namespace tdx
