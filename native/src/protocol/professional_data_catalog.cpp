#include "professional_data_internal.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <utility>

namespace tdx::professional_data_detail {

const std::map<int, std::string>& stock_names() {
    static const std::map<int, std::string> names{
        {1,"股东人数"},{2,"龙虎榜买卖总额"},{3,"融资余额/融券余量"},{4,"大宗交易"},
        {5,"增减持成交"},{6,"陆股通持股量"},{7,"陆股通市场净买入"},{8,"机构龙虎榜卖方"},
        {9,"机构龙虎榜买方"},{10,"近三月机构调研"},{11,"融资买入/偿还"},{12,"融券卖出/偿还"},
        {13,"融资净买入/融券净卖出"},{14,"涨停板上成交"},{15,"涨跌停状态/封单"},
        {16,"总市值"},{17,"龙虎榜营业部"},{18,"龙虎榜沪深股通"},{19,"每周股票质押数量"},
        {20,"每周股票质押比例"},{21,"股息率"},{22,"封成比/封流比"},{23,"拟增减持数量"},
        {24,"首次涨停时间/最大封单"},{25,"盘前盘后成交量"},{26,"拟增减持金额"},
        {27,"人气排名"},{28,"股票回购"},{29,"复牌/更名标志"},{30,"分红送转"},
        {31,"转融券期初/期末"},{32,"转融券融出"},{33,"跌停板上成交"},
        {34,"首次跌停时间/最大封单"},{35,"实际增减持数量"},{36,"竞价涨停买"},
        {37,"龙虎榜连续交易日"},{38,"近一年涨停次数/溢价次数"},
        {39,"首板封板率/次日红盘率"},{40,"连板率/最后涨停时间"},
        {41,"配股股权登记日"},{42,"龙虎榜专业机构净额"},{43,"配股实施"},{44,"股票评分"}
    };
    return names;
}

const std::map<int, std::string>& market_names() {
    static const std::map<int, std::string> names{
        {1,"市场融资/融券余额"},{2,"沪深股通资金流入"},{3,"涨停/曾涨停家数"},
        {4,"跌停/曾跌停家数"},{5,"上证50期指净持仓"},{6,"沪深300期指净持仓"},
        {7,"中证500期指净持仓"},{8,"ETF规模份额/净申赎"},{9,"沪市月新开A股账户"},
        {10,"市场增减持"},{11,"溢价/折价大宗交易"},{12,"限售解禁"},{13,"市场总分红"},
        {14,"市场总募资"},{15,"封板成功/失败资金"},{16,"龙虎榜买卖总额"},
        {17,"龙虎榜机构买卖"},{18,"龙虎榜营业部买卖"},{19,"龙虎榜沪深股通买卖"},
        {20,"沪深股通净买入"},{21,"无限售质押率"},{22,"有限售质押率"},
        {23,"连板家数"},{24,"非ST涨停/跌停家数"},{25,"市场融资买入/融券卖出"},
        {26,"每周市场质押比"},{27,"央行公开市场净投放"},{28,"历史新高/新低家数"},
        {29,"120日新高/新低家数"},{30,"市场高度/二板以上家数"},{31,"上涨/下跌家数"},
        {32,"20日新高/新低家数"},{33,"涨停/跌停总封单"},{34,"涨跌股成交量"},
        {35,"换手板家数/回封率"},{36,"曾涨停/曾跌停家数"},{37,"转融券市值/余额"},
        {38,"ETF规模金额/净申赎"},{39,"涨跌5%家数"},{40,"陆股通成交额/笔数"},
        {41,"中证1000期指净持仓"},{42,"沪深股通成交额"}
    };
    return names;
}

const std::map<int, std::string>& board_names() {
    static const std::map<int, std::string> names{
        {5,"市盈率TTM"},{6,"市净率MRQ"},{7,"市销率TTM"},{8,"市现率TTM"},
        {9,"上涨/下跌家数"},{10,"板块总市值"},{11,"板块流通市值"},
        {12,"涨停/曾涨停家数"},{13,"跌停/曾跌停家数"},
        {14,"市场高度/二板以上家数"},{15,"融资/融券余额"},
        {16,"沪深股通资金流入"},{17,"开盘成交额/成交量"},
        {18,"板块股息率"},{19,"板块自由流通市值"}
    };
    return names;
}

std::string field_name(std::string_view kind, int id) {
    const auto& names = kind == "stock" ? stock_names() :
                        kind == "board" ? board_names() : market_names();
    const auto found = names.find(id);
    return found == names.end() ? std::string{} : found->second;
}

Json catalog_document(int timeout_ms) {
    const auto finance = fetch_manifest("tdxfin/gpcw.txt", timeout_ms, 4 * 1024 * 1024);
    const auto trading = fetch_manifest("tdxgp/gpszsh.txt", timeout_ms, 4 * 1024 * 1024);
    Json packages = Json::array();
    for (const auto& item : finance) {
        Json row = Json::object(); row["name"] = item.name; row["md5"] = item.md5;
        row["size"] = static_cast<std::uint64_t>(item.size); packages.push_back(std::move(row));
    }
    Json result = Json::object(); result["schema"] = "tdx-professional-catalog-v1";
    result["official_base_url"] = std::string(data_root);
    result["finance_packages"] = std::move(packages);
    result["trading_file_count"] = static_cast<std::uint64_t>(trading.size());
    result["market_file"] = "gpsh999999.dat";
    result["market_file_available"] = std::any_of(trading.begin(), trading.end(), [](const auto& item) {
        return item.name == "gpsh999999.dat";
    });
    result["market_direct_url"] = std::string(data_root) + "tdxgp/gpsh999999.dat";
    return result;
}

}  // namespace tdx::professional_data_detail

namespace tdx {

Json fetch_professional_catalog_document(int timeout_ms) {
    return professional_data_detail::catalog_document(timeout_ms);
}

}  // namespace tdx
