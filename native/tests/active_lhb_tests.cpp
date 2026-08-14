#include "tdx/active_lhb.hpp"
#include "tdx/common.hpp"

#include <cmath>
#include <iostream>

namespace { void require(bool value,const char* message){if(!value)throw tdx::Error(message);} }

int main(){try{
    std::map<std::pair<int,std::string>,tdx::Security> securities;securities[{0,"000820"}]={0,"SZ","深圳","000820","神雾节能"};
    auto raw=tdx::Json::object();raw["$SC"]="0";raw["$ZQDM"]="000820";raw["cs"]="7";raw["ljb"]="222045798";raw["ljs"]="254709336";raw["zb"]="0.87";raw["zf"]="17.50";raw["date"]="20260806";auto input=tdx::Json::array();input.push_back(raw);
    auto rows=tdx::normalize_active_lhb_rows(input,tdx::active_lhb_periods()[0],securities);require(rows.size()==1,"ranking rows");const auto& row=rows.as_array()[0];require(row.at("security").at("name").as_string()=="神雾节能","name");require(row.at("event_count").as_number()==7,"events");require(std::abs(row.at("net_buy_amount_yuan").as_number()+32663538)<0.1,"net amount");require(row.at("direction").as_string()=="net-sell","direction");require(row.at("statistics_date").as_string()=="2026-08-06","date");
    auto event=tdx::Json::object();event["date"]="20260806";event["type"]="日换手率达到25.92%";event["zf"]="-8.74";event["bje"]="34733081";event["sje"]="59060648";event["hsl"]="25.92";auto event_input=tdx::Json::array();event_input.push_back(event);auto events=tdx::normalize_active_lhb_event_rows(event_input);require(events.size()==1,"event rows");require(events.as_array()[0].at("event_type").as_string()=="日换手率达到25.92%","event type");require(events.as_array()[0].at("turnover_rate_pct").as_number()==25.92,"turnover");
    rows.push_back(row);rows.as_array()[1]["event_count"]=10;rows.as_array()[1]["ranking_id"]="other";tdx::sort_active_lhb_rows(rows,"events","desc");require(rows.as_array()[0].at("event_count").as_number()==10,"sort");
    std::cout<<"active-LHB tests passed\n";return 0;}catch(const std::exception& error){std::cerr<<"active-LHB test failed: "<<error.what()<<'\n';return 1;}}
