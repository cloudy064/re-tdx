#include "tdx/benchmark_analysis.hpp"
#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool close(double a, double b, double e=1e-7) { return std::abs(a-b)<=e; }
tdx::Json one(std::initializer_list<std::pair<const std::string, tdx::Json>> fields) { tdx::Json row=tdx::Json::object(); for(const auto& [k,v]:fields) row[k]=v; tdx::Json rows=tdx::Json::array(); rows.push_back(std::move(row)); return rows; }
}
int main() { try {
    std::map<std::pair<int,std::string>,tdx::Security> securities;
    securities[{1,"603758"}]={1,"sh","上海","603758","秦安股份"};
    auto stage=tdx::normalize_benchmark_analysis_rows("list/func_jzfx216_1.jsn",one({{"$ZQDM","603758"},{"$SC","1"},{"price1","6.21"},{"price2","9.53"},{"dpzaf","44.24"},{"hyzaf","26.64"},{"zaf1","53.46"},{"zaf2","9.22"},{"zaf3","26.82"}}),securities).as_array().front();
    require(stage.at("kind").as_string()=="stocks","stock stage kind"); require(stage.at("stage_open").as_bool(),"current stage open"); require(close(stage.at("security_return_pct").as_number(),53.46),"return percent");
    auto suspension=tdx::normalize_benchmark_analysis_rows("list/func_jzfx401_1.jsn",one({{"$ZQDM","603758"},{"$SC","1"},{"tpdate","20260707"},{"fpdate","20260714"},{"tpts","5"},{"price3","9.53"},{"zaf1","0.20"},{"zaf2","-8.48"},{"yy","重大事项"}}),securities).as_array().front();
    require(suspension.at("reason").as_string()=="重大事项","suspension reason"); require(close(suspension.at("suspension_trading_days").as_number(),5),"days");
    auto fresh=tdx::normalize_benchmark_analysis_rows("list/func_jzfx501_1.jsn",one({{"$ZQDM","603758"},{"$SC","1"},{"price1","37.26"},{"zaf1","-14.69"},{"zaf2","-9.04"},{"zaf3","-14.12"}}),securities).as_array().front();
    require(close(fresh.at("excess_market_3m_pct").as_number(),-5.65),"client relative calculation");
    std::cout<<"benchmark analysis tests passed\n"; return 0;
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;} }
