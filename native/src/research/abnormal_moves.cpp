#include "tdx/abnormal_moves.hpp"
#include "tdx/time.hpp"

#include "tdx/cloud_resilience.hpp"
#include "tdx/cloud_workflow.hpp"
#include "tdx/common.hpp"
#include "tdx/pbrpc.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;
namespace tdx {
namespace {

struct TypeDefinition {
    int code;
    const char *name;
    bool possible_suspension_check;
};

constexpr std::array<TypeDefinition, 19> type_definitions{{
    {101, "涨幅偏离较高", false},
    {102, "跌幅偏离较高", false},
    {103, "日振幅较高", false},
    {104, "日换手率较高", false},
    {105, "无价格涨跌幅限制的证券", false},
    {121, "3日涨幅偏离值累计较高", true},
    {122, "3日跌幅偏离值累计较高", true},
    {123, "3日换手率累计较高", false},
    {131, "10日内3次同正向异常波动", true},
    {132, "10日内3次同负向异常波动", true},
    {133, "10日涨幅偏离值达100%", true},
    {134, "10日跌幅偏离值达50%", true},
    {135, "30日涨幅偏离值达200%", true},
    {136, "30日跌幅偏离值达70%", true},
    {141, "出现异常波动停牌1", false},
    {142, "出现异常波动停牌2", false},
    {143, "出现异常波动停牌3", false},
    {181, "退市整理的证券", false},
    {191, "实施特别停牌的证券", false},
}};

const TypeDefinition *type_definition(int code) {
    for (const auto &definition : type_definitions)
        if (definition.code == code)
            return &definition;
    return nullptr;
}

fs::path native_path(const std::string &value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string now_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream out;
    out << local_timestamp_text(local);
    return out.str();
}

bool digits(const std::string &v) {
    return std::all_of(v.begin(), v.end(), [](unsigned char c) { return c >= '0' && c <= '9'; });
}
std::string compact_date(std::string v) {
    v = trim(std::move(v));
    if (v.size() == 10 && v[4] == '-' && v[7] == '-')
        v = v.substr(0, 4) + v.substr(5, 2) + v.substr(8, 2);
    if (v.size() != 8 || !digits(v))
        throw Error("date must be YYYYMMDD or YYYY-MM-DD");
    std::tm t{};
    t.tm_year = std::stoi(v.substr(0, 4)) - 1900;
    t.tm_mon = std::stoi(v.substr(4, 2)) - 1;
    t.tm_mday = std::stoi(v.substr(6, 2));
    t.tm_hour = 12;
    const int y = t.tm_year, m = t.tm_mon, d = t.tm_mday;
    if (std::mktime(&t) == -1 || t.tm_year != y || t.tm_mon != m || t.tm_mday != d)
        throw Error("date is outside the supported calendar range");
    return v;
}
std::string display_date(const std::string &v) {
    return v.size() == 8 ? v.substr(0, 4) + "-" + v.substr(4, 2) + "-" + v.substr(6, 2) : v;
}
std::string date_days_ago(int days) {
    auto n = std::time(nullptr) - static_cast<std::time_t>(days) * 86400;
    std::tm t{};
#ifdef _WIN32
    localtime_s(&t, &n);
#else
    localtime_r(&n, &t);
#endif
    std::ostringstream o;
    o << std::put_time(&t, "%Y%m%d");
    return o.str();
}

const Json *field(const Json &r, std::string_view k) {
    if (!r.is_object())
        return nullptr;
    auto f = r.as_object().find(k);
    if (f != r.as_object().end())
        return &f->second;
    auto w = lower_ascii(std::string(k));
    for (const auto &[n, v] : r.as_object())
        if (lower_ascii(n) == w)
            return &v;
    return nullptr;
}
std::string text(const Json &r, std::string_view k) {
    auto *v = field(r, k);
    if (!v || v->is_null())
        return {};
    if (v->is_string())
        return trim(v->as_string());
    if (v->is_number()) {
        std::ostringstream o;
        o << std::setprecision(15) << v->as_number();
        return o.str();
    }
    return {};
}
std::optional<double> number(const Json &r, std::string_view k) {
    auto *v = field(r, k);
    if (!v || v->is_null())
        return {};
    if (v->is_number())
        return v->as_number();
    if (!v->is_string())
        return {};
    auto s = trim(v->as_string());
    s.erase(std::remove(s.begin(), s.end(), ','), s.end());
    if (s.empty() || s == "-" || s == "--")
        return {};
    try {
        std::size_t u = 0;
        double x = std::stod(s, &u);
        if (u == s.size() && std::isfinite(x))
            return x;
    } catch (...) {
    }
    return {};
}
Json num(const std::optional<double> &v) { return v ? Json(*v) : Json(nullptr); }
int integer(const std::string &s, std::string_view n, int a, int b) {
    try {
        std::size_t u = 0;
        int v = std::stoi(s, &u);
        if (u != s.size() || v < a || v > b)
            throw std::invalid_argument("range");
        return v;
    } catch (...) {
        throw Error(std::string(n) + " must be in " + std::to_string(a) + ".." + std::to_string(b));
    }
}

int board_id(std::string v) {
    v = lower_ascii(trim(v));
    if (v == "all" || v == "0")
        return 0;
    if (v == "sz-main" || v == "1")
        return 1;
    if (v == "gem" || v == "2")
        return 2;
    if (v == "sh-main" || v == "3")
        return 3;
    if (v == "star" || v == "4")
        return 4;
    if (v == "bj" || v == "5")
        return 5;
    throw Error("board must be all, sz-main, gem, sh-main, star, or bj");
}
std::string board_name(int v) {
    return v == 0   ? "all"
           : v == 1 ? "sz-main"
           : v == 2 ? "gem"
           : v == 3 ? "sh-main"
           : v == 4 ? "star"
                    : "bj";
}
int type_id(std::string v) {
    v = lower_ascii(trim(v));
    if (v == "all" || v == "0")
        return 0;
    int id = integer(v, "type", 1, 999);
    if (!type_definition(id))
        throw Error("type is not one of the 19 client anomaly codes");
    return id;
}
int market_id(std::string v) {
    int x = integer(v, "row market", 0, 44);
    if (x == 44)
        x = 2;
    if (x < 0 || x > 2)
        throw Error("abnormal-move row has invalid market");
    return x;
}
std::string market_name(int x) { return x == 0 ? "sz" : x == 1 ? "sh" : "bj"; }
std::string prefix(int x) { return x == 0 ? "SZ" : x == 1 ? "SH" : "BJ"; }
std::string time_text(std::string s) {
    s = trim(s);
    if (s.size() == 4 && s[2] == ':')
        return s + ":00";
    if (s.size() == 5 && s[2] == ':')
        return s + ":00";
    return s;
}
bool transient(const std::string &m) {
    return detail::is_transient_cloud_error(m);
}
std::string key(const AbnormalMovesQuery &q) {
    return q.board + '|' + q.type + '|' + q.date + '|' + std::to_string(q.limit);
}

Json catalog() {
    Json a = Json::array();
    for (const auto &definition : type_definitions) {
        Json x = Json::object();
        x["code"] = definition.code;
        x["name"] = definition.name;
        x["possible_suspension_check"] = definition.possible_suspension_check;
        a.push_back(std::move(x));
    }
    return a;
}
} // namespace

Json normalize_abnormal_move_rows(const Json &rows, const BlockData &blocks) {
    if (!rows.is_array())
        throw Error("abnormal move rows must be an array");
    Json out = Json::array();
    std::set<std::string> ids;
    for (const auto &r : rows.as_array()) {
        if (!r.is_object() || text(r, "Code").empty())
            continue;
        int m = market_id(text(r, "SetCode"));
        auto code = text(r, "Code"), name = text(r, "Name");
        auto f = blocks.securities.find({m, code});
        if (name.empty() && f != blocks.securities.end())
            name = f->second.name;
        int type = integer(text(r, "Type"), "row type", 1, 999);
        std::string id = prefix(m) + code + ':' + std::to_string(type);
        if (!ids.insert(id).second)
            throw Error("duplicate abnormal move identity: " + id);
        Json s = Json::object();
        s["market"] = market_name(m);
        s["market_id"] = m;
        s["code"] = code;
        s["security_id"] = prefix(m) + code;
        s["name"] = name;
        s["name_resolved"] = !name.empty();
        Json a = Json::object();
        a["code"] = type;
        const auto *definition = type_definition(type);
        a["name"] = definition ? Json(definition->name) : Json("unknown");
        a["subtype"] = text(r, "SubType");
        a["possible_suspension_check"] =
            definition ? Json(definition->possible_suspension_check) : Json(false);
        Json v = Json::object();
        v["security"] = std::move(s);
        v["anomaly"] = std::move(a);
        v["last_in"] = text(r, "LastIn") == "1";
        auto rr = text(r, "ResultRight");
        v["result_accurate"] = rr == "1" ? Json(true) : rr == "0" ? Json(false) : Json(nullptr);
        v["time"] = time_text(text(r, "Time"));
        v["change_pct"] = num(number(r, "Zdf"));
        v["price"] = num(number(r, "Now"));
        v["turnover_pct"] = num(number(r, "Hsl"));
        v["amplitude_pct"] = num(number(r, "Zaf"));
        v["amount"] = num(number(r, "Amo"));
        v["volume"] = num(number(r, "Vol"));
        v["main_net_inflow"] = num(number(r, "zlje"));
        v["main_net_inflow_5d"] = num(number(r, "zlje5d"));
        out.push_back(std::move(v));
    }
    return out;
}

AbnormalMovesService::AbnormalMovesService(fs::path root, BlockData blocks)
    : root_(std::move(root)), blocks_(std::move(blocks)) {}
Json AbnormalMovesService::query(const AbnormalMovesQuery &in) {
    AbnormalMovesQuery q = in;
    int board = board_id(q.board), type = type_id(q.type);
    q.board = board_name(board);
    q.type = std::to_string(type);
    if (!q.date.empty())
        q.date = compact_date(q.date);
    if (q.limit < 1 || q.limit > 10000 || q.cache_ttl_seconds < 0 || q.cache_ttl_seconds > 3600 ||
        q.timeout_ms < 100 || q.timeout_ms > 60000)
        throw Error("abnormal moves query limits are invalid");
    auto k = key(q);
    auto now = std::time(nullptr);
    auto cached = cache_.find(k);
    if (!q.refresh && cached != cache_.end()) {
        int age = static_cast<int>(std::max<std::time_t>(0, now - cached->second.fetched_at));
        if (age < q.cache_ttl_seconds) {
            auto d = cached->second.document;
            d["cache"]["hit"] = true;
            d["cache"]["age_seconds"] = age;
            return d;
        }
    }
    auto fetch = [&](const std::string &date) {
        Json u;
        for (int attempt = 0; attempt < 3; ++attempt) {
            try {
                u = execute_pbrpc_config(root_, "500107", {{"result", std::to_string(type)}},
                                         {{"Type", std::to_string(type)},
                                          {"MarketType", std::to_string(board)},
                                          {"DataDate", date}},
                                         {}, {}, "hq_lhb_dryc.xml", {"\"MarketType\":\"0\""},
                                         cloud_endpoints::tqlex, q.timeout_ms);
                return u;
            } catch (const Error &e) {
                if (!transient(e.what()) || attempt == 2)
                    throw;
                std::this_thread::sleep_for(std::chrono::milliseconds(250 * (attempt + 1)));
            }
        }
        return u;
    };
    std::string token = q.date.empty() ? "0" : q.date, resolved = q.date;
    Json upstream = fetch(token);
    auto raw = cloud_result_rows(upstream.at("response"));
    bool fallback = false;
    if (q.date.empty() && raw.size() == 0) {
        for (int days = 1; days <= 10; ++days) {
            auto d = date_days_ago(days);
            upstream = fetch(d);
            raw = cloud_result_rows(upstream.at("response"));
            if (raw.size()) {
                resolved = d;
                fallback = true;
                break;
            }
        }
    }
    if (resolved.empty())
        resolved = date_days_ago(0);
    auto records = normalize_abnormal_move_rows(raw, blocks_);
    std::map<std::string, std::size_t> by_type;
    for (const auto &r : records.as_array())
        ++by_type[text(r.at("anomaly"), "name")];
    bool truncated = records.size() > static_cast<std::size_t>(q.limit);
    if (truncated)
        records.as_array().resize(q.limit);
    Json counts = Json::object();
    counts["upstream_rows"] = static_cast<std::uint64_t>(raw.size());
    counts["returned"] = static_cast<std::uint64_t>(records.size());
    counts["truncated"] = truncated;
    Json groups = Json::object();
    for (const auto &[n, c] : by_type)
        groups[n] = static_cast<std::uint64_t>(c);
    Json src = Json::object();
    for (auto name : {"source_file", "module", "rpc_id", "rounds", "raw_size"})
        src[name] = upstream.at(name);
    src["request_id"] = "500107";
    Json params = Json::object();
    params["board"] = q.board;
    params["type_code"] = type;
    params["requested_date"] = q.date.empty() ? Json(nullptr) : Json(display_date(q.date));
    params["resolved_date"] = display_date(resolved);
    params["latest_fallback_used"] = fallback;
    Json cache = Json::object();
    cache["hit"] = false;
    cache["stale"] = false;
    cache["age_seconds"] = 0;
    cache["ttl_seconds"] = q.cache_ttl_seconds;
    Json d = Json::object();
    d["schema"] = "tdx-abnormal-moves-native-v1";
    d["availability"] = "live";
    d["generated_at"] = now_text();
    d["parameters"] = std::move(params);
    d["type_catalog"] = catalog();
    d["counts"] = std::move(counts);
    d["counts_by_type"] = std::move(groups);
    d["records"] = std::move(records);
    d["source"] = std::move(src);
    d["cache"] = std::move(cache);
    d["raw_response_retained"] = false;
    cache_[k] = {d, std::time(nullptr)};
    return d;
}

int command_market_abnormal_moves(const std::vector<std::string> &values) {
    Args a(values);
    if (a.take_flag("--help") || a.take_flag("-h")) {
        std::cout << "Usage: tdx-tool market abnormal-moves [options]\n\n  --board "
                     "all|sz-main|gem|sh-main|star|bj\n  --type all|CODE --date DATE\n  --root "
                     "PATH --limit N --refresh --cache-ttl-seconds N --timeout-ms N\n  --output "
                     "FILE --compact\n";
        return 0;
    }
    AbnormalMovesQuery q;
    q.board = a.take_option("--board", "all");
    q.type = a.take_option("--type", "all");
    q.date = trim(a.take_option("--date"));
    q.refresh = a.take_flag("--refresh");
    q.limit = integer(a.take_option("--limit", "5000"), "--limit", 1, 10000);
    q.cache_ttl_seconds =
        integer(a.take_option("--cache-ttl-seconds", "60"), "--cache-ttl-seconds", 0, 3600);
    q.timeout_ms = integer(a.take_option("--timeout-ms", "15000"), "--timeout-ms", 100, 60000);
    auto root_text = a.take_option("--root");
    auto output = native_path(a.take_option("--output", "output/tdx-abnormal-moves.json"));
    bool compact = a.take_flag("--compact");
    a.require_empty();
    auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    AbnormalMovesService service(root, load_blocks(root, {}));
    auto d = service.query(q);
    atomic_write_text(output, d.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << d.at("counts").at("returned").as_number()
              << " abnormal move rows -> " << path_utf8(output) << '\n';
    return 0;
}
} // namespace tdx
