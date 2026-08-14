#include "tdx/fund_calendar.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr const char* kResource = "list/func_jjrl301_1.jsn";

const Json* field(const Json& row, std::string_view name) {
    if (!row.is_object()) return nullptr;
    const auto found = row.as_object().find(name);
    return found == row.as_object().end() ? nullptr : &found->second;
}
std::string text_value(const Json& row, std::string_view name) {
    const auto* value = field(row, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}
bool digits(const std::string& value) {
    return value.size() == 6 && std::all_of(value.begin(), value.end(),
        [](char ch) { return ch >= '0' && ch <= '9'; });
}
int integer_value(const Json& row, std::string_view name) {
    const auto value = text_value(row, name);
    try { std::size_t used=0; const auto parsed=std::stoi(value,&used); return used==value.size()?parsed:-1; }
    catch (...) { return -1; }
}
std::string market_name(int market) {
    if (market == 0) return "sz";
    if (market == 1) return "sh";
    if (market == 33) return "fund";
    return "m" + std::to_string(market);
}
std::string market_prefix(int market) {
    if (market == 0) return "SZ";
    if (market == 1) return "SH";
    if (market == 33) return "FUND";
    return "M" + std::to_string(market);
}
std::string now_text() {
    const auto now=std::time(nullptr); std::tm local{};
#ifdef _WIN32
    localtime_s(&local,&now);
#else
    localtime_r(&now,&local);
#endif
    std::ostringstream output; output<<local_timestamp_text(local); return output.str();
}
fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}
int bounded(const std::string& value,std::string_view name,int low,int high){
    try{std::size_t used=0;const auto parsed=std::stoi(value,&used);if(used!=value.size()||parsed<low||parsed>high)throw std::invalid_argument("range");return parsed;}
    catch(...){throw Error(std::string(name)+" must be in "+std::to_string(low)+".."+std::to_string(high));}
}
Json load_local_resource(const fs::path& root) {
    const auto path=root/native_path(kResource);
    if(!fs::is_regular_file(path))throw Error("local fund-calendar resource is unavailable: "+path_utf8(path));
    const auto tables=load_jsn_tables(path);Json rows=Json::array();
    for(std::size_t group=0;group<tables.size();++group){const auto& table=tables[group];for(const auto& cells:table.rows){Json row=Json::object();for(std::size_t column=0;column<table.headers.size();++column)row[table.headers[column]]=cells[column];row["_group"]=static_cast<std::uint64_t>(group);rows.push_back(std::move(row));}}
    Json result=Json::object();result["resource"]=kResource;result["size"]=static_cast<std::uint64_t>(fs::file_size(path));result["row_count"]=static_cast<std::uint64_t>(rows.size());result["endpoint"]="local-jsn:"+path_utf8(path);result["rows"]=std::move(rows);return result;
}
Json source_summary(const Json& document,std::size_t normalized_rows){Json result=Json::object();for(const auto* name:{"resource","size","row_count","endpoint"})result[name]=document.at(name);result["normalized_row_count"]=static_cast<std::uint64_t>(normalized_rows);return result;}
}  // namespace

Json normalize_fund_calendar_rows(const Json& rows) {
    if(!rows.is_array())throw Error("fund-calendar rows must be an array");
    Json result=Json::array();std::uint64_t rank=0;
    for(const auto& row:rows.as_array()){
        ++rank;const auto market=integer_value(row,"$SC");const auto code=text_value(row,"$ZQDM");
        if(market<0||!digits(code))continue;
        Json security=Json::object();security["market_id"]=market;security["market"]=market_name(market);
        security["code"]=code;security["security_id"]=market_prefix(market)+code;
        security["name"]=text_value(row,"ZQJC");security["name_resolved"]=!security.at("name").as_string().empty();
        Json item=Json::object();item["security"]=std::move(security);item["event_type"]=text_value(row,"sjlx");
        item["event_date"]=text_value(row,"date");item["content"]=text_value(row,"sjnr");
        item["fund_category"]=text_value(row,"jjlb");item["source_resource"]=kResource;item["source_rank"]=rank;item["raw"]=row;
        item["event_id"]=item.at("security").at("security_id").as_string()+":"+item.at("event_date").as_string()+":"+item.at("event_type").as_string()+":"+std::to_string(rank);
        result.push_back(std::move(item));
    }
    return result;
}

FundCalendarService::FundCalendarService(fs::path jsn_root):jsn_root_(std::move(jsn_root)){}

Json FundCalendarService::fetch_master(const FundCalendarQuery& options,bool& refreshed,int& age_seconds){
    const auto now=std::time(nullptr);age_seconds=cache_time_?static_cast<int>(std::max<std::time_t>(0,now-cache_time_)):0;
    if(!options.refresh&&cache_time_&&age_seconds<options.cache_ttl_seconds){refreshed=false;return cache_;}
    Json document;if(!options.refresh&&!jsn_root_.empty()){try{document=load_local_resource(jsn_root_);}catch(...){}}
    if(!document.is_object()){const auto documents=fetch_jsn_resources_rows({kResource},"bi",options.timeout_ms);if(!documents.is_array()||documents.as_array().empty())throw Error("missing fund-calendar resource");document=documents.as_array().front();}
    auto records=normalize_fund_calendar_rows(document.at("rows"));Json result=Json::object();result["records"]=std::move(records);result["source"]=source_summary(document,result.at("records").size());cache_=result;cache_time_=std::time(nullptr);refreshed=true;age_seconds=0;return result;
}

Json FundCalendarService::query(const FundCalendarQuery& options){
    if(options.limit<1||options.limit>10000)throw Error("limit must be in 1..10000");
    if(options.market.empty()!=options.code.empty())throw Error("market and code must be provided together");
    const auto selected_market=lower_ascii(trim(options.market));
    if(!selected_market.empty()&&selected_market!="sz"&&selected_market!="sh"&&selected_market!="fund")throw Error("market must be sz/sh/fund");
    if(!options.code.empty()&&!digits(options.code))throw Error("code must contain six digits");
    if(options.order!="asc"&&options.order!="desc")throw Error("order must be asc or desc");
    for(const auto& [value,name]:std::vector<std::pair<std::string,std::string>>{{options.date_from,"date_from"},{options.date_to,"date_to"}})
        if(!value.empty()&&!(value.size()==8&&std::all_of(value.begin(),value.end(),[](char ch){return ch>='0'&&ch<='9';})))throw Error(name+" must be YYYYMMDD");
    bool refreshed=false;int age_seconds=0;const auto master=fetch_master(options,refreshed,age_seconds);const auto needle=lower_ascii(trim(options.query));
    const auto selected_type=lower_ascii(trim(options.event_type)),selected_category=lower_ascii(trim(options.category));Json records=Json::array();
    for(const auto& row:master.at("records").as_array()){
        const auto& security=row.at("security");if(!selected_market.empty()&&(security.at("market").as_string()!=selected_market||security.at("code").as_string()!=options.code))continue;
        const auto event_date=text_value(row,"event_date");if(!options.date_from.empty()&&event_date<options.date_from)continue;if(!options.date_to.empty()&&event_date>options.date_to)continue;
        if(!selected_type.empty()&&lower_ascii(text_value(row,"event_type")).find(selected_type)==std::string::npos)continue;
        if(!selected_category.empty()&&lower_ascii(text_value(row,"fund_category")).find(selected_category)==std::string::npos)continue;
        if(!needle.empty()&&lower_ascii(row.dump(-1)).find(needle)==std::string::npos)
            continue;
        records.push_back(row);
    }
    const bool ascending=options.order=="asc";std::sort(records.as_array().begin(),records.as_array().end(),[&](const Json& l,const Json& r){const auto ld=text_value(l,"event_date"),rd=text_value(r,"event_date");if(ld!=rd)return ascending?ld<rd:ld>rd;return l.at("event_id").as_string()<r.at("event_id").as_string();});
    std::map<std::string,std::uint64_t> types,categories;std::set<std::string> funds,dates;
    for(const auto& row:records.as_array()){++types[text_value(row,"event_type")];++categories[text_value(row,"fund_category")];funds.insert(row.at("security").at("security_id").as_string());dates.insert(text_value(row,"event_date"));}
    const auto matched=records.size();if(static_cast<int>(records.size())>options.limit)records.as_array().resize(static_cast<std::size_t>(options.limit));if(!options.include_raw)for(auto& row:records.as_array())row.as_object().erase("raw");
    Json type_counts=Json::object();for(const auto& [key,value]:types)type_counts[key]=value;Json category_counts=Json::object();for(const auto& [key,value]:categories)category_counts[key]=value;
    Json summary=Json::object();summary["unique_funds"]=static_cast<std::uint64_t>(funds.size());summary["event_type_count"]=static_cast<std::uint64_t>(types.size());summary["category_count"]=static_cast<std::uint64_t>(categories.size());summary["earliest_date"]=dates.empty()?"":*dates.begin();summary["latest_date"]=dates.empty()?"":*dates.rbegin();summary["event_types"]=std::move(type_counts);summary["categories"]=std::move(category_counts);
    Json sources=Json::array();sources.push_back(master.at("source"));Json result=Json::object();result["schema"]="tdx-market-fund-calendar-native-v1";result["generated_at"]=now_text();result["order"]=options.order;result["mode"]=selected_market.empty()?"catalog":"security";result["availability"]=matched?"live":"empty";result["match_count"]=static_cast<std::uint64_t>(matched);result["returned"]=static_cast<std::uint64_t>(records.size());result["records"]=std::move(records);result["summary"]=std::move(summary);result["sources"]=std::move(sources);result["semantics"]="JJRL fund event calendar with source event type, date, content and category. Market 33 is an off-exchange fund namespace; exchange-listed funds retain sz/sh markets.";Json cache=Json::object();cache["refreshed"]=refreshed;cache["age_seconds"]=age_seconds;result["cache"]=std::move(cache);return result;
}

int command_market_fund_calendar(const std::vector<std::string>& raw_args){
    Args args(raw_args);if(args.take_flag("--help")||args.take_flag("-h")){std::cout<<"Usage: tdx-tool market fund-calendar [options]\n\n  --query TEXT --event-type TEXT --category TEXT --date-from YYYYMMDD --date-to YYYYMMDD\n  --market sz|sh|fund --code CODE --order asc|desc --without-raw --refresh\n  --input-dir PATH --limit N --output PATH --compact\n";return 0;}
    FundCalendarQuery query;query.query=trim(args.take_option("--query"));query.event_type=trim(args.take_option("--event-type"));query.category=trim(args.take_option("--category"));query.date_from=trim(args.take_option("--date-from"));query.date_to=trim(args.take_option("--date-to"));query.market=lower_ascii(trim(args.take_option("--market")));query.code=trim(args.take_option("--code"));query.order=lower_ascii(trim(args.take_option("--order","asc")));query.include_raw=!args.take_flag("--without-raw");query.refresh=args.take_flag("--refresh");query.limit=bounded(args.take_option("--limit","5000"),"--limit",1,10000);query.timeout_ms=bounded(args.take_option("--timeout-ms","15000"),"--timeout-ms",100,60000);
    const auto input=native_path(args.take_option("--input-dir","output/tdx-jsn"));const auto output=native_path(args.take_option("--output","output/tdx-market-fund-calendar-native.json"));const bool compact=args.take_flag("--compact");args.require_empty();FundCalendarService service(input);const auto document=service.query(query);atomic_write_text(output,document.dump(compact?-1:2)+"\n");std::cout<<"returned "<<document.at("returned").as_number()<<" fund-calendar rows -> "<<path_utf8(output)<<'\n';return 0;
}

}  // namespace tdx
