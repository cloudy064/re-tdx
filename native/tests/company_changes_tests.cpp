#include "tdx/company_changes.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool close(double left, double right, double epsilon = 1e-7) {
    return std::abs(left - right) <= epsilon;
}
tdx::Json one(std::initializer_list<std::pair<const std::string, tdx::Json>> fields) {
    tdx::Json row = tdx::Json::object();
    for (const auto& [key, value] : fields) row[key] = value;
    tdx::Json rows = tdx::Json::array();
    rows.push_back(std::move(row));
    return rows;
}
}

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "000002"}] = {0, "sz", "深圳", "000002", "万科A"};
        securities[{1, "600000"}] = {1, "sh", "上海", "600000", "浦发银行"};

        const auto equity = tdx::normalize_company_change_rows(
            "list/func_zqbg104_1.jsn", one({
                {"$ZQDM", "000002"}, {"$SC", "0"}, {"date", "20170112"},
                {"NBD1", "华润"}, {"NBD2", "深圳地铁"},
                {"NBD3", "168959.98"}, {"NBD4", "15.31"},
                {"NBD5", "协议转让"}, {"JD", "完成"}, {"RQ", "20170125"}}), securities);
        const auto& e = equity.as_array().front();
        require(e.at("kind").as_string() == "major-equity", "major equity kind");
        require(e.at("security").at("name").as_string() == "万科A", "resolved name");
        require(close(e.at("shares").as_number(), 1689599800.0), "ten-thousand shares conversion");
        require(close(e.at("total_share_pct").as_number(), 15.31), "source percent");

        const auto transfers = tdx::normalize_company_change_rows(
            "list/func_zqbg109_1.jsn", one({
                {"$ZQDM", "600000"}, {"$SC", "1"}, {"date", "20260807"},
                {"zrgb", "25000000"}, {"zkx", "311250000"},
                {"zrgbzb", "7.5"}, {"zrf", "甲方"}, {"jsf", "乙方"}}), securities);
        const auto& transfer = transfers.as_array().front();
        require(close(transfer.at("transfer_price_yuan").as_number(), 12.45), "transfer price");
        require(transfer.at("event_date").as_string() == "20260807", "event date");

        const auto hk = tdx::normalize_company_change_rows(
            "list/func_zqbg105_1.jsn", one({
                {"$ZQDM", "00001"}, {"$SC", "31"}, {"gbrq", "20260101"},
                {"zqbg", "恒生指数"}, {"tzlj", "调入"}, {"tzrq", "20260115"}}));
        require(hk.as_array().front().at("security").at("market").as_string() == "hk", "hk market");
        require(hk.as_array().front().at("index_name").as_string() == "恒生指数", "hk index");

        std::cout << "company changes tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
