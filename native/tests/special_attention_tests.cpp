#include "tdx/special_attention.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool close(double left, double right, double epsilon = 1e-8) {
    return std::abs(left - right) <= epsilon;
}

tdx::Json row(std::initializer_list<std::pair<const std::string, tdx::Json>> fields) {
    tdx::Json result = tdx::Json::object();
    for (const auto& [key, value] : fields) result[key] = value;
    return result;
}

tdx::Json rows(tdx::Json value) {
    tdx::Json result = tdx::Json::array();
    result.push_back(std::move(value));
    return result;
}

}  // namespace

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "000010"}] = {0, "sz", "深圳", "000010", "美丽生态"};
        securities[{0, "000430"}] = {0, "sz", "深圳", "000430", "张家界"};
        securities[{0, "300299"}] = {0, "sz", "深圳", "300299", "富春股份"};
        securities[{1, "600363"}] = {1, "sh", "上海", "600363", "联创光电"};

        const auto dispersion = tdx::normalize_special_attention_rows(
            "list/func_tbgz102_1.jsn", rows(row({
                {"$ZQDM", "000010"}, {"$SC", "0"}, {"hy", "建筑"},
                {"dq", "深圳"}, {"dgd", "第一大股东"}, {"cgbl", "8.7"},
                {"jzrq", "20260331"}})), securities);
        const auto& dispersion_row = dispersion.as_array().front();
        require(dispersion_row.at("kind").as_string() == "equity-dispersion",
                "equity-dispersion kind");
        require(dispersion_row.at("security").at("name").as_string() ==
                    "美丽生态", "security name resolution");
        require(close(dispersion_row.at("largest_shareholder_pct").as_number(), 8.7),
                "shareholder ratio stays percent");

        const auto st = tdx::normalize_special_attention_rows(
            "list/func_tbgz103_1.jsn", rows(row({
                {"$ZQDM", "000010"}, {"$SC", "0"},
                {"fxlx", "可能终止上市"}, {"blyy", "净资产为负"},
                {"price1", "3.08"}, {"bgq", "20260331"},
                {"zxgdhs", "126753"}, {"jlr", "-183826414.05"},
                {"jlr1", "-12582399856.80"}, {"jzc", "-6072305713.21"},
                {"yysr", "1931598532.06"}, {"kfjlr", "-179811962.98"},
                {"ID", "230"}})), securities);
        const auto& st_row = st.as_array().front();
        require(st_row.at("risk_reason").as_string() == "净资产为负",
                "ST risk reason");
        require(close(st_row.at("shareholder_households_10k").as_number(),
                      12.6753), "shareholder households display conversion");
        require(close(st_row.at("net_assets_yuan").as_number(),
                      -6072305713.21), "financial source remains yuan");

        const auto removal = tdx::normalize_special_attention_rows(
            "list/func_tbgz104_1.jsn", rows(row({
                {"$ZQDM", "000430"}, {"$SC", "0"}, {"bgq", "20260331"},
                {"jlr1", "30596.54"}, {"mgjzc", "2.24"},
                {"jlr2", "-54941.34"}, {"jlr3", "-58209.18"},
                {"zxzm", "摘帽"}, {"date", "20260518"},
                {"bkcs", "撤销其他风险警示，证券简称恢复。"}})), securities);
        const auto& removal_row = removal.as_array().front();
        require(close(removal_row.at("current_net_profit_yuan").as_number(),
                      305965400.0), "ten-thousand yuan net profit conversion");
        require(removal_row.at("status_or_forecast").as_string() == "摘帽",
                "star/cap removal status");

        const auto investigation = tdx::normalize_special_attention_rows(
            "list/func_tbgz106_1.jsn", rows(row({
                {"$ZQDM1", "600363"}, {"$SC1", "1"},
                {"blarq", "20260805"}, {"price", "27.38"},
                {"YY", "信披违法违规"}, {"fxts", "风险提示正文"},
                {"aqjz", "立案调查中"}, {"cfplr", ""}, {"cs", "1"},
                {"$ZQDM", "4600363"}})), securities);
        const auto& investigation_row = investigation.as_array().front();
        require(investigation_row.at("security").at("security_id").as_string() ==
                    "SH600363", "investigation uses referenced security fields");
        require(investigation_row.at("active").as_bool(),
                "active investigation state");
        require(investigation_row.at("source_event_key").as_string() == "4600363",
                "source event key is not confused with security code");

        const auto goodwill = tdx::normalize_special_attention_rows(
            "list/func_tbgz110_1.jsn", rows(row({
                {"$ZQDM", "300299"}, {"$SC", "0"}, {"bgq", "20260630"},
                {"sy1", "201194861.82"}, {"sy2", "202356372.74"},
                {"syzjl", "603.07"}, {"syzgd", "23.43"},
                {"jlr1", "33361834.69"}, {"jlr2", "-9705274.15"},
                {"jlrzs", "443.75"}, {"ys1", "216063218.03"},
                {"ys2", "181534071.92"}, {"yszs", "19.02"},
                {"hy", "互联网"}})), securities);
        const auto& goodwill_row = goodwill.as_array().front();
        require(close(goodwill_row.at("goodwill_change_yuan").as_number(),
                      -1161510.92, 1e-4), "goodwill change formula");
        require(close(goodwill_row.at("goodwill_to_net_profit_pct").as_number(),
                      603.07), "goodwill ratio remains percent");

        std::cout << "special attention tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
