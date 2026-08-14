#include "tdx/fund_calendar.hpp"
#include <iostream>
#include <stdexcept>
namespace{void require(bool c,const char*m){if(!c)throw std::runtime_error(m);}}
int main(){try{tdx::Json row=tdx::Json::object();row["$ZQDM"]="027099";row["$SC"]="33";row["ZQJC"]="平安盈悦6个月持有期混合A";row["sjlx"]="基金开放申购";row["date"]="20260814";row["sjnr"]="开放申购，起购金额1元";row["jjlb"]="FOF";tdx::Json rows=tdx::Json::array();rows.push_back(std::move(row));const auto item=tdx::normalize_fund_calendar_rows(rows).as_array().front();require(item.at("security").at("market").as_string()=="fund","fund market");require(item.at("event_type").as_string()=="基金开放申购","event type");require(item.at("fund_category").as_string()=="FOF","category");std::cout<<"fund calendar tests passed\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
