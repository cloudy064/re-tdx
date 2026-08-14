#include "tdx/active_lhb.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx {
namespace {

const Json* value_ptr(const Json& value, std::string_view key) {
    if (!value.is_object()) return nullptr;
    const auto exact = value.as_object().find(key);
    if (exact != value.as_object().end()) return &exact->second;
    const auto wanted = lower_ascii(std::string(key));
    for (const auto& [name, child] : value.as_object())
        if (lower_ascii(name) == wanted) return &child;
    return nullptr;
}

std::string text_value(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found ? trim(jsn_scalar_text(*found)) : std::string{};
}

std::optional<double> number_value(const Json& value, std::string_view key) {
    auto raw = text_value(value, key);
    if (raw.empty() || raw == "-" || raw == "--") return std::nullopt;
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(raw, &used);
        return used == raw.size() && std::isfinite(parsed)
            ? std::optional<double>(parsed) : std::nullopt;
    } catch (...) { return std::nullopt; }
}

std::optional<double> json_number(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found && found->is_number()
        ? std::optional<double>(found->as_number()) : std::nullopt;
}

std::string json_text(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found && found->is_string() ? found->as_string() : std::string{};
}

Json number_json(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

bool digits(const std::string& value, std::size_t size) {
    return value.size() == size &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

std::string iso_date(const std::string& value) {
    if (!digits(value, 8)) return value;
    return value.substr(0, 4) + "-" + value.substr(4, 2) + "-" + value.substr(6, 2);
}

int market_id(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "0" || value == "sz") return 0;
    if (value == "1" || value == "sh") return 1;
    if (value == "2" || value == "44" || value == "bj") return 2;
    throw Error("market must be sz/sh/bj or 0/1/2");
}

std::string market_name(int id) { return id == 0 ? "sz" : id == 1 ? "sh" : "bj"; }
std::string market_prefix(int id) { return id == 0 ? "SZ" : id == 1 ? "SH" : "BJ"; }

Json security_document(
    int id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const auto found = securities.find({id, code});
    Json result = Json::object();
    result["market"] = market_name(id); result["market_id"] = id;
    result["code"] = code; result["security_id"] = market_prefix(id) + code;
    result["name"] = found == securities.end() ? "" : found->second.name;
    result["name_resolved"] = found != securities.end();
    return result;
}

std::string direction(double net) {
    return net > 0.005 ? "net-buy" : net < -0.005 ? "net-sell" : "flat";
}

std::string now_text() {
    const auto now = std::time(nullptr); std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now)) throw Error("cannot read local time");
#else
    if (!localtime_r(&now, &local)) throw Error("cannot read local time");
#endif
    std::ostringstream output; output << local_timestamp_text(local);
    return output.str();
}

int bounded(const std::string& text, const std::string& name,
            int minimum, int maximum) {
    try {
        std::size_t used = 0; const auto value = std::stoi(text, &used);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(name + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

const ActiveLhbPeriod& find_period(const std::string& value) {
    const auto wanted = lower_ascii(trim(value));
    for (const auto& period : active_lhb_periods())
        if (period.name == wanted) return period;
    throw Error("period must be 5d, month, or half-year");
}

const Json& document_for_resource(const Json& documents, const std::string& resource) {
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing active-LHB resource: " + resource);
}

Json period_summary(const Json& rows, const ActiveLhbPeriod& period) {
    std::uint64_t net_buy = 0, net_sell = 0, resolved = 0;
    double buy = 0, sell = 0, net = 0, events = 0;
    std::string date;
    for (const auto& row : rows.as_array()) {
        buy += row.at("buy_amount_yuan").as_number();
        sell += row.at("sell_amount_yuan").as_number();
        net += row.at("net_buy_amount_yuan").as_number();
        events += row.at("event_count").as_number();
        if (row.at("direction").as_string() == "net-buy") ++net_buy; else ++net_sell;
        if (row.at("security").at("name_resolved").as_bool()) ++resolved;
        date = std::max(date, row.at("statistics_date").as_string());
    }
    Json result = Json::object();
    result["period"] = period.name; result["period_label"] = period.label;
    result["rows"] = static_cast<std::uint64_t>(rows.size());
    result["names_resolved"] = resolved; result["event_count_sum"] = events;
    result["buy_amount_yuan_sum"] = buy; result["sell_amount_yuan_sum"] = sell;
    result["net_buy_amount_yuan_sum"] = net; result["net_buy_rows"] = net_buy;
    result["net_sell_rows"] = net_sell;
    result["statistics_date"] = date.empty() ? Json(nullptr) : Json(date);
    return result;
}

bool json_contains(const Json& value, const std::string& needle) {
    if (value.is_string()) return lower_ascii(value.as_string()).find(needle) != std::string::npos;
    if (value.is_number() || value.is_bool())
        return lower_ascii(jsn_scalar_text(value)).find(needle) != std::string::npos;
    if (value.is_array()) for (const auto& child : value.as_array())
        if (json_contains(child, needle)) return true;
    if (value.is_object()) for (const auto& [key, child] : value.as_object())
        if (lower_ascii(key).find(needle) != std::string::npos || json_contains(child, needle)) return true;
    return false;
}

}  // namespace

const std::vector<ActiveLhbPeriod>& active_lhb_periods() {
    static const std::vector<ActiveLhbPeriod> periods{
        {"5d", "近5日", "list/func_hylhb101_1.jsn", "13801"},
        {"month", "近一个月", "list/func_hylhb102_1.jsn", "13901"},
        {"half-year", "近半年", "list/func_hylhb104_1.jsn", "14001"},
    };
    return periods;
}

Json normalize_active_lhb_rows(
    const Json& rows, const ActiveLhbPeriod& period,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("active-LHB master rows must be an array");
    Json result = Json::array(); std::set<std::pair<int, std::string>> seen;
    std::uint64_t rank = 0;
    for (const auto& raw : rows.as_array()) {
        ++rank; const auto code = text_value(raw, "$ZQDM");
        if (!digits(code, 6)) continue;
        int market = -1; try { market = market_id(text_value(raw, "$SC")); } catch (...) { continue; }
        if (!seen.insert({market, code}).second)
            throw Error("duplicate active-LHB security in period " + period.name);
        const auto buy = number_value(raw, "ljb").value_or(0.0);
        const auto sell = number_value(raw, "ljs").value_or(0.0);
        const auto net = buy - sell;
        Json row = Json::object();
        row["ranking_id"] = period.name + ":" + market_prefix(market) + code;
        row["source_rank"] = rank; row["period"] = period.name;
        row["period_label"] = period.label; row["unit_id"] = period.unit_id;
        row["security"] = security_document(market, code, securities);
        row["event_count"] = number_value(raw, "cs").value_or(0.0);
        row["buy_amount_yuan"] = buy; row["sell_amount_yuan"] = sell;
        row["net_buy_amount_yuan"] = net; row["buy_sell_ratio"] = number_json(number_value(raw, "zb"));
        row["period_return_pct"] = number_json(number_value(raw, "zf"));
        row["direction"] = direction(net); row["statistics_date"] = iso_date(text_value(raw, "date"));
        row["raw"] = raw; result.push_back(std::move(row));
    }
    return result;
}

Json normalize_active_lhb_event_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("active-LHB event rows must be an array");
    Json result = Json::array(); std::uint64_t rank = 0;
    for (const auto& raw : rows.as_array()) {
        ++rank; const auto date = text_value(raw, "date");
        if (!digits(date, 8)) continue;
        const auto buy = number_value(raw, "bje").value_or(0.0);
        const auto sell = number_value(raw, "sje").value_or(0.0);
        Json row = Json::object(); row["source_rank"] = rank;
        row["event_date"] = iso_date(date); row["event_type"] = text_value(raw, "type");
        row["change_pct"] = number_json(number_value(raw, "zf"));
        row["buy_amount_yuan"] = buy; row["sell_amount_yuan"] = sell;
        row["net_buy_amount_yuan"] = buy - sell; row["direction"] = direction(buy - sell);
        row["turnover_rate_pct"] = number_json(number_value(raw, "hsl"));
        row["raw"] = raw; result.push_back(std::move(row));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& a, const Json& b) {
            if (a.at("event_date").as_string() != b.at("event_date").as_string())
                return a.at("event_date").as_string() > b.at("event_date").as_string();
            return a.at("source_rank").as_number() < b.at("source_rank").as_number();
        });
    return result;
}

void sort_active_lhb_rows(Json& rows, const std::string& sort_value,
                          const std::string& order_value) {
    const auto sort = lower_ascii(trim(sort_value)); const auto order = lower_ascii(trim(order_value));
    const std::set<std::string> allowed{"events", "net-buy", "buy", "sell", "ratio", "return", "date", "code", "source-rank"};
    if (!allowed.count(sort)) throw Error("invalid active-LHB sort");
    if (order != "asc" && order != "desc") throw Error("order must be asc or desc");
    const bool desc = order == "desc";
    const std::map<std::string, std::string> keys{{"events","event_count"},{"net-buy","net_buy_amount_yuan"},{"buy","buy_amount_yuan"},{"sell","sell_amount_yuan"},{"ratio","buy_sell_ratio"},{"return","period_return_pct"},{"source-rank","source_rank"}};
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(), [&](const Json& a, const Json& b) {
        if (sort == "date") {
            const auto x=json_text(a,"statistics_date"), y=json_text(b,"statistics_date");
            if(x!=y) return desc?x>y:x<y;
        } else if (sort == "code") {
            const auto x=a.at("security").at("security_id").as_string(), y=b.at("security").at("security_id").as_string();
            if(x!=y) return desc?x>y:x<y;
        } else {
            const auto x=json_number(a,keys.at(sort)), y=json_number(b,keys.at(sort));
            if(x.has_value()!=y.has_value()) return x.has_value();
            if(x&&y&&*x!=*y) return desc?*x>*y:*x<*y;
        }
        return a.at("ranking_id").as_string()<b.at("ranking_id").as_string();
    });
}

ActiveLhbService::ActiveLhbService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

ActiveLhbService::FetchResult ActiveLhbService::fetch_master(const ActiveLhbQuery& options) {
    const auto now=std::time(nullptr); const int age=master_cache_.fetched_at?static_cast<int>(std::max<std::time_t>(0,now-master_cache_.fetched_at)):0;
    if(!options.refresh&&master_cache_.fetched_at&&age<options.master_cache_ttl_seconds)
        return {master_cache_.document,false,age};
    std::vector<std::string> resources; for(const auto& p:active_lhb_periods()) resources.push_back(p.resource);
    const auto documents=fetch_jsn_resources_rows(resources,"bi",options.timeout_ms);
    Json rankings=Json::object(), summaries=Json::object(), sources=Json::array();
    for(const auto& p:active_lhb_periods()) {
        const auto& source=document_for_resource(documents,p.resource);
        rankings[p.name]=normalize_active_lhb_rows(source.at("rows"),p,securities_);
        summaries[p.name]=period_summary(rankings.at(p.name),p);
        sources.push_back(jsn_source_metadata(source));
    }
    Json doc=Json::object(); doc["rankings"]=std::move(rankings); doc["period_summaries"]=std::move(summaries); doc["sources"]=std::move(sources);
    master_cache_={doc,std::time(nullptr)}; return {std::move(doc),true,0};
}

ActiveLhbService::FetchResult ActiveLhbService::fetch_resource(
    const std::string& resource, const ActiveLhbQuery& options) {
    const auto now=std::time(nullptr); const auto found=resource_cache_.find(resource);
    if(!options.refresh&&found!=resource_cache_.end()) {
        const int age=static_cast<int>(std::max<std::time_t>(0,now-found->second.fetched_at));
        if(age<options.detail_cache_ttl_seconds) return {found->second.document,false,age};
    }
    auto doc=fetch_jsn_resource_rows(resource,"bi",options.timeout_ms);
    resource_cache_[resource]={doc,std::time(nullptr)}; return {std::move(doc),true,0};
}

Json ActiveLhbService::query(const ActiveLhbQuery& input) {
    ActiveLhbQuery options=input; const auto& period=find_period(options.period);
    options.query=trim(options.query); options.direction=lower_ascii(trim(options.direction));
    options.sort=lower_ascii(trim(options.sort)); options.order=lower_ascii(trim(options.order));
    if(!std::set<std::string>{"all","net-buy","net-sell","flat"}.count(options.direction))
        throw Error("direction must be all, net-buy, net-sell, or flat");
    if(options.market.empty()!=options.code.empty()) throw Error("market and code must be provided together");
    int selected_market=-1; if(!options.market.empty()) {selected_market=market_id(options.market); options.market=market_name(selected_market); if(!digits(options.code,6)) throw Error("code must contain six digits");}
    if(options.offset<0||options.offset>1000000||options.limit<1||options.limit>5000||options.detail_limit<1||options.detail_limit>5000)
        throw Error("active-LHB pagination is invalid");
    if(options.master_cache_ttl_seconds<0||options.master_cache_ttl_seconds>86400||options.detail_cache_ttl_seconds<0||options.detail_cache_ttl_seconds>86400||options.timeout_ms<100||options.timeout_ms>60000)
        throw Error("active-LHB cache or timeout value is invalid");
    auto master=fetch_master(options); const auto& source_rows=master.document.at("rankings").at(period.name);
    Json filtered=Json::array(); const auto needle=lower_ascii(options.query);
    for(const auto& row:source_rows.as_array()) {
        const auto& security=row.at("security");
        if(selected_market>=0&&static_cast<int>(security.at("market_id").as_number())!=selected_market) continue;
        if(!options.code.empty()&&security.at("code").as_string()!=options.code) continue;
        if(options.direction!="all"&&row.at("direction").as_string()!=options.direction) continue;
        if (!needle.empty() && !json_contains(row, needle)) continue;
        filtered.push_back(row);
    }
    sort_active_lhb_rows(filtered,options.sort,options.order); const auto matched=filtered.size();
    Json paged=Json::array(); for(std::size_t i=static_cast<std::size_t>(options.offset);i<filtered.size()&&paged.size()<static_cast<std::size_t>(options.limit);++i) paged.push_back(filtered.as_array()[i]);
    Json selected=paged.size()?paged.as_array().front():Json(nullptr), events=Json::array(), errors=Json::array();
    Json sources=master.document.at("sources"); bool detail_refreshed=false; int detail_age=0;
    if(selected_market>=0&&options.include_details&&!selected.is_null()) {
        const auto resource="hylhb"+period.unit_id+"/"+std::to_string(selected_market)+options.code+".jsn";
        try { auto detail=fetch_resource(resource,options); detail_refreshed=detail.refreshed; detail_age=detail.age_seconds; sources.push_back(jsn_source_metadata(detail.document)); events=normalize_active_lhb_event_rows(detail.document.at("rows")); if(events.size()>static_cast<std::size_t>(options.detail_limit)) events.as_array().resize(static_cast<std::size_t>(options.detail_limit)); }
        catch(const std::exception& error){Json item=Json::object();item["resource"]=resource;item["message"]=error.what();errors.push_back(std::move(item));}
    }
    const auto health=jsn_sources_health(sources);
    Json result=Json::object(); result["schema"]="tdx-market-active-lhb-native-v1"; result["generated_at"]=now_text();
    result["period"]=period.name; result["period_label"]=period.label; result["unit_id"]=period.unit_id;
    result["mode"]=selected_market>=0?"security":"ranking";
    result["availability"]=health.at("stale").as_bool()?"stale-cache":errors.size()?"partial":matched?"live":"empty";
    result["summary"]=master.document.at("period_summaries").at(period.name);
    result["period_summaries"]=master.document.at("period_summaries"); result["selected_ranking"]=selected;
    result["rankings"]=std::move(paged); result["events"]=std::move(events); result["detail_errors"]=std::move(errors);
    Json counts=Json::object(); counts["matched"]=static_cast<std::uint64_t>(matched); counts["returned"]=static_cast<std::uint64_t>(result.at("rankings").size()); counts["events"]=static_cast<std::uint64_t>(result.at("events").size()); result["counts"]=std::move(counts);
    Json filters=Json::object();filters["market"]=options.market.empty()?Json(nullptr):Json(options.market);filters["code"]=options.code.empty()?Json(nullptr):Json(options.code);filters["query"]=options.query;filters["direction"]=options.direction;filters["sort"]=options.sort;filters["order"]=options.order;result["filters"]=std::move(filters);
    result["sources"]=std::move(sources);result["upstream_health"]=health;
    Json cache=Json::object();cache["master_refreshed"]=master.refreshed;cache["master_age_seconds"]=master.age_seconds;cache["detail_refreshed"]=detail_refreshed;cache["detail_age_seconds"]=detail_age;result["cache"]=std::move(cache);
    result["semantics"]="TDX HYLHB is the active Dragon-Tiger ranking, not an industry ranking. It aggregates all LHB appearances, buy/sell amounts and period return over 5 days, one month or half a year; per-security hylhb<unit> details expose every abnormal-move type, date, change, turnover and buy/sell amounts. This is distinct from single-day broker-seat LHB and institution-only LHB. Current quote columns in the client are host fields and are not fabricated here.";
    return result;
}

int command_market_active_lhb(const std::vector<std::string>& raw_args) {
    Args args(raw_args); if(args.take_flag("--help")||args.take_flag("-h")){std::cout<<
        "Usage: tdx-tool market active-lhb [options]\n\n"
        "  --period 5d|month|half-year --market sz|sh|bj --code CODE\n"
        "  --query TEXT --direction all|net-buy|net-sell|flat\n"
        "  --sort events|net-buy|buy|sell|ratio|return|date|code|source-rank --order asc|desc\n"
        "  --no-details --offset N --limit N --detail-limit N --refresh\n"
        "  --master-cache-ttl N --detail-cache-ttl N --timeout-ms N --root PATH\n"
        "  --output FILE --compact\n";return 0;}
    ActiveLhbQuery q; q.period=args.take_option("--period","5d");q.market=args.take_option("--market");q.code=args.take_option("--code");q.query=args.take_option("--query");q.direction=args.take_option("--direction","all");q.sort=args.take_option("--sort","events");q.order=args.take_option("--order","desc");q.include_details=!args.take_flag("--no-details");q.offset=bounded(args.take_option("--offset","0"),"offset",0,1000000);q.limit=bounded(args.take_option("--limit","500"),"limit",1,5000);q.detail_limit=bounded(args.take_option("--detail-limit","500"),"detail-limit",1,5000);q.refresh=args.take_flag("--refresh");q.master_cache_ttl_seconds=bounded(args.take_option("--master-cache-ttl","300"),"master-cache-ttl",0,86400);q.detail_cache_ttl_seconds=bounded(args.take_option("--detail-cache-ttl","300"),"detail-cache-ttl",0,86400);q.timeout_ms=bounded(args.take_option("--timeout-ms","15000"),"timeout-ms",100,60000);
    const auto root_name=args.take_option("--root");const auto output_name=args.take_option("--output");const bool compact=args.take_flag("--compact");args.require_empty();const auto root=find_tdx_root(root_name.empty()?fs::path{}:native_path(root_name));ActiveLhbService service(load_blocks(root,{}).securities);const auto rendered=service.query(q).dump(compact?-1:2)+'\n';if(output_name.empty())std::cout<<rendered;else{const auto path=native_path(output_name);atomic_write_text(path,rendered);std::cout<<"completed active-LHB query -> "<<path_utf8(path)<<'\n';}return 0;
}

}  // namespace tdx
