#include "tdx/roadshows.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/tqlex.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr const char* kEntry = "CWSearch.tzx_rcache";
constexpr const char* kMasterKey = "ly:1_zxly";

std::string now_text() {
    const auto now = std::time(nullptr);
    std::tm value{};
#ifdef _WIN32
    localtime_s(&value, &now);
#else
    localtime_r(&now, &value);
#endif
    std::ostringstream output;
    output << local_timestamp_text(value);
    return output.str();
}

const Json* field(const Json& row, std::string_view name) {
    if (!row.is_object()) return nullptr;
    const auto direct = row.as_object().find(std::string(name));
    if (direct != row.as_object().end()) return &direct->second;
    const auto folded = lower_ascii(std::string(name));
    for (const auto& [key, value] : row.as_object())
        if (lower_ascii(key) == folded) return &value;
    return nullptr;
}

std::string text(const Json& row, std::string_view name) {
    const auto* value = field(row, name);
    if (!value || value->is_null()) return {};
    if (value->is_string()) return trim(value->as_string());
    if (value->is_number()) {
        std::ostringstream output;
        output << std::setprecision(15) << value->as_number();
        return output.str();
    }
    return {};
}

Json nullable_text(const Json& row, std::string_view name) {
    const auto value = text(row, name);
    return value.empty() ? Json(nullptr) : Json(value);
}

bool digits(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return ch >= '0' && ch <= '9';
    });
}

std::string compact_date(std::string value, std::string_view name) {
    value = trim(std::move(value));
    if (value.empty()) return value;
    if (value.size() == 10 && value[4] == '-' && value[7] == '-')
        value = value.substr(0, 4) + value.substr(5, 2) + value.substr(8, 2);
    if (value.size() != 8 || !digits(value))
        throw Error(std::string(name) + " must be YYYYMMDD or YYYY-MM-DD");
    std::tm date{};
    date.tm_year = std::stoi(value.substr(0, 4)) - 1900;
    date.tm_mon = std::stoi(value.substr(4, 2)) - 1;
    date.tm_mday = std::stoi(value.substr(6, 2));
    date.tm_hour = 12;
    const int year = date.tm_year, month = date.tm_mon, day = date.tm_mday;
    if (std::mktime(&date) == -1 || date.tm_year != year || date.tm_mon != month ||
        date.tm_mday != day)
        throw Error(std::string(name) + " is outside the supported calendar range");
    return value;
}

std::string display_date(const std::string& value) {
    return value.size() == 8 ? value.substr(0, 4) + "-" + value.substr(4, 2) + "-" +
                                  value.substr(6, 2)
                            : value;
}

int market_id(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "sz" || value == "0") return 0;
    if (value == "sh" || value == "1") return 1;
    if (value == "bj" || value == "2" || value == "44") return 2;
    throw Error("market must be sz, sh, bj, 0, 1, 2, or 44");
}

std::string market_name(int value) {
    if (value == 0) return "sz";
    if (value == 1) return "sh";
    if (value == 2 || value == 44) return "bj";
    return {};
}

std::string security_id(int market, const std::string& code) {
    return market == 0 ? "SZ" + code : market == 1 ? "SH" + code : "BJ" + code;
}

bool transient_error(const std::string& message) {
    for (const auto* token : {"TQLEX HTTP status 429", "TQLEX HTTP status 502",
                              "TQLEX HTTP status 503", "TQLEX HTTP status 504",
                              "TQLEX server returned ErrorCode 4", "WinHttpSendRequest failed",
                              "WinHttpReceiveResponse failed"})
        if (message.find(token) != std::string::npos) return true;
    return false;
}

int bounded(const std::string& raw, std::string_view name, int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(raw, &used);
        if (used != raw.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

bool contains_folded(const std::string& value, const std::string& needle) {
    return needle.empty() || lower_ascii(value).find(lower_ascii(needle)) != std::string::npos;
}

Json roadshow_result_rows(const Json& response) {
    const auto* sets = field(response, "ResultSets");
    if (!sets || !sets->is_array() || sets->as_array().empty())
        throw Error("roadshow result set does not exist");
    const auto& result_set = sets->as_array().front();
    const auto* content = field(result_set, "Content");
    if (!content || !content->is_array())
        throw Error("roadshow Content must be an array");
    std::vector<std::string> columns;
    if (const auto* names = field(result_set, "ColName"); names && names->is_array()) {
        for (const auto& name : names->as_array()) {
            if (!name.is_string() || name.as_string().empty())
                throw Error("roadshow ColName contains an invalid name");
            columns.push_back(name.as_string());
        }
    } else if (const auto* descriptions = field(result_set, "ColDes");
               descriptions && descriptions->is_array()) {
        for (const auto& description : descriptions->as_array()) {
            const auto name = text(description, "Name");
            if (name.empty()) throw Error("roadshow ColDes contains an invalid name");
            columns.push_back(name);
        }
    } else {
        throw Error("roadshow result has neither ColName nor ColDes");
    }
    Json rows = Json::array();
    for (const auto& raw : content->as_array()) {
        if (raw.is_object()) { rows.push_back(raw); continue; }
        if (!raw.is_array() || raw.as_array().size() != columns.size())
            throw Error("roadshow row width does not match column names");
        Json row = Json::object();
        for (std::size_t index = 0; index < columns.size(); ++index)
            row[columns[index]] = raw.as_array()[index];
        rows.push_back(std::move(row));
    }
    return rows;
}

}  // namespace

Json normalize_roadshow_rows(const Json& rows, bool security_detail, int selected_market,
                             const std::string& selected_code,
                             const std::string& selected_name) {
    if (!rows.is_array()) throw Error("roadshow rows must be an array");
    Json result = Json::array();
    std::set<std::string> identities;
    for (const auto& row : rows.as_array()) {
        if (!row.is_object()) continue;
        const auto title = text(row, "title");
        const auto date = text(row, "start_date");
        if (title.empty() && date.empty()) continue;
        int market = selected_market;
        std::string code = selected_code, name = selected_name;
        if (!security_detail) {
            const auto raw_market = text(row, "stock_market");
            try { market = market_id(raw_market); } catch (...) { continue; }
            code = text(row, "stock_code");
            name = text(row, "stock_name");
            if (code.size() != 6 || !digits(code)) continue;
        }
        Json security = Json::object();
        security["market"] = market_name(market);
        security["market_id"] = market;
        security["code"] = code;
        security["security_id"] = security_id(market, code);
        security["name"] = name.empty() ? Json(nullptr) : Json(name);

        Json item = Json::object();
        item["security"] = std::move(security);
        const auto record_id = text(row, "rec_id");
        item["record_id"] = record_id.empty() ? Json(nullptr) : Json(record_id);
        item["title"] = title;
        item["roadshow_type"] = nullable_text(row, "roadshow_type");
        item["start_date"] = display_date(date);
        item["start_time"] = nullable_text(row, "start_time");
        item["end_time"] = nullable_text(row, "end_time");
        item["summary"] = nullable_text(row, "summary");
        item["url"] = nullable_text(row, "url");
        item["title_image"] = nullable_text(row, "title_image");
        item["plate_code"] = nullable_text(row, "stock_plates");
        item["listing_status_code"] = nullable_text(row, "liststatus");
        const auto identity = !record_id.empty() ? record_id
            : security_id(market, code) + '|' + date + '|' + text(row, "start_time") + '|' + title;
        if (!identities.insert(identity).second) continue;
        item["event_id"] = identity;
        result.push_back(std::move(item));
    }
    return result;
}

RoadshowService::RoadshowService(fs::path root, BlockData blocks, RoadshowFetcher fetcher)
    : root_(std::move(root)), blocks_(std::move(blocks)), fetcher_(std::move(fetcher)) {
    if (!fetcher_) fetcher_ = [](const std::string& key, int timeout_ms) {
        Json request = Json::object();
        request["action"] = "get";
        request["key"] = key;
        return query_tqlex(kEntry, request, cloud_endpoints::tqlex, timeout_ms);
    };
}

Json RoadshowService::query(const RoadshowQuery& input) {
    RoadshowQuery options = input;
    options.market = lower_ascii(trim(options.market));
    options.code = trim(options.code);
    options.query = trim(options.query);
    options.type = trim(options.type);
    options.status = trim(options.status);
    options.start_date = compact_date(options.start_date, "start_date");
    options.end_date = compact_date(options.end_date, "end_date");
    if (!options.start_date.empty() && !options.end_date.empty() &&
        options.start_date > options.end_date)
        throw Error("start_date must not exceed end_date");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    int selected_market = -1;
    if (!options.code.empty()) {
        selected_market = market_id(options.market);
        options.market = market_name(selected_market);
        if (options.code.size() != 6 || !digits(options.code))
            throw Error("code must contain exactly six digits");
    }
    if (options.offset < 0 || options.offset > 1000000 || options.limit < 1 ||
        options.limit > 10000 || options.cache_ttl_seconds < 0 ||
        options.cache_ttl_seconds > 86400 || options.timeout_ms < 100 ||
        options.timeout_ms > 600000)
        throw Error("roadshow query limits are invalid");

    const bool security_detail = selected_market >= 0;
    const auto key = security_detail
        ? "ly:" + std::to_string(selected_market) + '_' + options.code
        : std::string(kMasterKey);
    const auto now = std::time(nullptr);
    CacheEntry cached;
    bool has_cache = false;
    {
        std::lock_guard<std::mutex> guard(cache_mutex_);
        const auto found = cache_.find(key);
        if (found != cache_.end()) { cached = found->second; has_cache = true; }
    }

    Json raw_rows;
    bool cache_hit = false, stale = false;
    int attempts = 0;
    std::string upstream_error;
    const int cached_age = has_cache
        ? static_cast<int>(std::max<std::time_t>(0, now - cached.fetched_at)) : 0;
    if (!options.refresh && has_cache && cached_age < options.cache_ttl_seconds) {
        raw_rows = cached.rows;
        cache_hit = true;
    } else {
        for (int attempt = 0; attempt < 3; ++attempt) {
            try {
                ++attempts;
                const auto upstream = fetcher_(key, options.timeout_ms);
                raw_rows = roadshow_result_rows(upstream);
                {
                    std::lock_guard<std::mutex> guard(cache_mutex_);
                    cache_[key] = CacheEntry{raw_rows, std::time(nullptr)};
                }
                upstream_error.clear();
                break;
            } catch (const Error& error) {
                upstream_error = error.what();
                if (!transient_error(upstream_error)) throw;
                if (attempt == 2) {
                    if (!has_cache) throw;
                    raw_rows = cached.rows;
                    cache_hit = true;
                    stale = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(250 * (attempt + 1)));
            }
        }
    }

    std::string selected_name;
    if (security_detail) {
        const auto found = blocks_.securities.find({selected_market, options.code});
        if (found != blocks_.securities.end()) selected_name = found->second.name;
    }
    auto records = normalize_roadshow_rows(raw_rows, security_detail, selected_market,
                                           options.code, selected_name);
    auto& values = records.as_array();
    values.erase(std::remove_if(values.begin(), values.end(), [&](const Json& item) {
        const auto& security = item.at("security");
        const auto date = text(item, "start_date");
        std::string compact = date;
        compact.erase(std::remove(compact.begin(), compact.end(), '-'), compact.end());
        if (!options.start_date.empty() && compact < options.start_date) return true;
        if (!options.end_date.empty() && compact > options.end_date) return true;
        if (!options.type.empty() && text(item, "roadshow_type") != options.type) return true;
        if (!options.status.empty() && text(item, "listing_status_code") != options.status)
            return true;
        if (!options.query.empty()) {
            const auto haystack = text(security, "name") + " " + text(security, "code") +
                                  " " + text(item, "title") + " " +
                                  text(item, "roadshow_type") + " " + text(item, "summary");
            if (!contains_folded(haystack, options.query)) return true;
        }
        return false;
    }), values.end());
    std::stable_sort(values.begin(), values.end(), [](const Json& left, const Json& right) {
        const auto left_key = text(left, "start_date") + ' ' + text(left, "start_time");
        const auto right_key = text(right, "start_date") + ' ' + text(right, "start_time");
        if (left_key != right_key) return left_key > right_key;
        return text(left.at("security"), "security_id") <
               text(right.at("security"), "security_id");
    });
    const auto matched = values.size();
    const auto begin = std::min<std::size_t>(static_cast<std::size_t>(options.offset), matched);
    const auto end = std::min<std::size_t>(begin + static_cast<std::size_t>(options.limit), matched);
    Json page = Json::array();
    for (std::size_t index = begin; index < end; ++index) page.push_back(values[index]);

    std::map<std::string, std::size_t> type_counts;
    for (const auto& item : values) ++type_counts[text(item, "roadshow_type")];
    Json types = Json::array();
    for (const auto& [name, count] : type_counts) {
        Json item = Json::object(); item["name"] = name.empty() ? Json(nullptr) : Json(name);
        item["count"] = static_cast<std::uint64_t>(count); types.push_back(std::move(item));
    }

    Json counts = Json::object();
    counts["upstream_rows"] = static_cast<std::uint64_t>(raw_rows.size());
    counts["matched"] = static_cast<std::uint64_t>(matched);
    counts["returned"] = static_cast<std::uint64_t>(page.size());
    counts["offset"] = options.offset;
    counts["limit"] = options.limit;
    counts["has_more"] = end < matched;

    Json filters = Json::object();
    filters["q"] = options.query.empty() ? Json(nullptr) : Json(options.query);
    filters["type"] = options.type.empty() ? Json(nullptr) : Json(options.type);
    filters["status"] = options.status.empty() ? Json(nullptr) : Json(options.status);
    filters["start_date"] = options.start_date.empty() ? Json(nullptr) : Json(display_date(options.start_date));
    filters["end_date"] = options.end_date.empty() ? Json(nullptr) : Json(display_date(options.end_date));

    Json source = Json::object();
    source["transport"] = "TQLEX reqformat=2 KV";
    source["entry"] = kEntry;
    source["request_id"] = Json(nullptr);
    source["source_file"] = "gp_sc_szly.xml";
    source["config_present"] = fs::exists(root_ / "T0002" / "cloud_cfg" / "gp_sc_szly.xml");
    source["action"] = "get";
    source["key"] = key;
    source["attempts"] = attempts;
    source["template_selection"] = "entry+key (template has no ReqId)";

    Json cache = Json::object();
    cache["hit"] = cache_hit;
    cache["stale"] = stale;
    cache["age_seconds"] = cache_hit ? cached_age : 0;
    cache["ttl_seconds"] = options.cache_ttl_seconds;
    cache["upstream_error"] = upstream_error.empty() ? Json(nullptr) : Json(upstream_error);

    Json result = Json::object();
    result["schema"] = "tdx-roadshows-native-v1";
    result["availability"] = stale ? "stale-cache" : "live";
    result["generated_at"] = now_text();
    result["mode"] = security_detail ? "security" : "market";
    if (security_detail) {
        Json security = Json::object(); security["market"] = options.market;
        security["market_id"] = selected_market; security["code"] = options.code;
        security["security_id"] = security_id(selected_market, options.code);
        security["name"] = selected_name.empty() ? Json(nullptr) : Json(selected_name);
        result["security"] = std::move(security);
    } else result["security"] = Json(nullptr);
    result["filters"] = std::move(filters);
    result["counts"] = std::move(counts);
    result["types"] = std::move(types);
    result["records"] = std::move(page);
    result["source"] = std::move(source);
    result["cache"] = std::move(cache);
    return result;
}

int command_market_roadshows(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market roadshows [options]\n\n"
            "  --market sz|sh|bj --code SECURITY   Select one security\n"
            "  --q TEXT --type NAME --status CODE  Filter the market/security history\n"
            "  --start DATE --end DATE --offset N --limit N\n"
            "  --root PATH --refresh --cache-ttl-seconds N --timeout-ms N\n"
            "  --output FILE --compact\n";
        return 0;
    }
    RoadshowQuery query;
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.query = args.take_option("--q");
    query.type = args.take_option("--type");
    query.status = args.take_option("--status");
    query.start_date = args.take_option("--start");
    query.end_date = args.take_option("--end");
    query.offset = bounded(args.take_option("--offset", "0"), "--offset", 0, 1000000);
    query.limit = bounded(args.take_option("--limit", "100"), "--limit", 1, 10000);
    query.refresh = args.take_flag("--refresh");
    query.cache_ttl_seconds = bounded(args.take_option("--cache-ttl-seconds", "300"),
                                      "--cache-ttl-seconds", 0, 86400);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 600000);
    const auto root_text = args.take_option("--root");
    const auto output_text = args.take_option("--output", "output/tdx-roadshows.json");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : fs::u8path(root_text));
    RoadshowService service(root, load_blocks(root, {}));
    const auto result = service.query(query);
    const auto output = fs::u8path(output_text);
    atomic_write_text(output, result.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << result.at("counts").at("returned").as_number()
              << " roadshow rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
