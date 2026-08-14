#include "tdx/gdr.hpp"
#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
namespace { void require(bool c,const char* m){if(!c)throw std::runtime_error(m);} bool close(double a,double b){return std::abs(a-b)<1e-7;} }
int main(){try{
    std::map<std::pair<int,std::string>,tdx::Security> securities;
    securities[{0,"000301"}]={0,"sz","深圳","000301","东方盛虹"};
    tdx::Json row=tdx::Json::object(); row["gdrdm"]="DFSH"; row["gdrmc"]="东方盛虹[DFSH]";
    row["rq"]="20260806"; row["price"]="18.800"; row["bz"]="美元"; row["ssdd"]="瑞士证券交易所";
    row["fxl"]="43492688"; row["dhbl"]="10"; row["$ZQDM"]="000301"; row["$SC"]="0";
    row["bdmc"]="东方盛虹"; row["price2"]="12.900"; row["price3"]="127.64"; row["yjl"]="-1.05";
    tdx::Json rows=tdx::Json::array(); rows.push_back(std::move(row));
    const auto item=tdx::normalize_gdr_rows(rows,securities).as_array().front();
    require(item.at("underlying").at("name").as_string()=="东方盛虹","underlying");
    require(close(item.at("conversion_ratio").as_number(),10),"ratio");
    require(close(item.at("premium_pct").as_number(),-1.05),"premium");
    std::cout<<"GDR tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
