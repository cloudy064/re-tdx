#include "tdx/equity_valuation.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

tdx::Json rows(std::initializer_list<tdx::Json> values) {
    tdx::Json result = tdx::Json::array();
    for (const auto& value : values) result.push_back(value);
    return result;
}

}  // namespace

int main() {
    try {
        tdx::BlockData blocks;
        tdx::Block coal;
        coal.block_id = "research-industry:1";
        coal.family = "research-industry";
        coal.block_code = "881001";
        coal.name = "煤炭";
        blocks.blocks.push_back(coal);
        blocks.securities[{0, "000001"}] =
            tdx::Security{0, "SZ", "深圳", "000001", "平安银行"};
        blocks.securities[{1, "601918"}] =
            tdx::Security{1, "SH", "上海", "601918", "新集能源"};

        const auto industries = tdx::normalize_equity_valuation_rows(
            "pe-industries", rows({tdx::Json::parse(
                "{\"code_hy\":\"881001\",\"market_hy\":\"1\","
                "\"name_hy\":\"煤炭\",\"PE_TTM_industry\":\"16.97\"}")}), blocks);
        require(industries.size() == 1 &&
                    industries.as_array()[0].at("industry").at("block_id").as_string() ==
                        "research-industry:1" &&
                    std::abs(industries.as_array()[0].at("pe").as_number() - 16.97) < 1e-9,
                "PE industry overview resolves research industries and numeric PE");

        const auto history = tdx::normalize_equity_valuation_rows(
            "pe-security-history", rows({
                tdx::Json::parse(
                    "{\"Date\":\"20260805\",\"stock_code\":\"000001\","
                    "\"stock_market\":\"0\",\"Stockprice\":\"11.25\","
                    "\"PE_TTM_stock\":\"5.07\",\"PE_25\":\"4.36\","
                    "\"PE_median\":\"4.83\",\"PE_75\":\"5.08\","
                    "\"PE_max\":\"5.86\",\"PE_min\":\"3.61\"}"),
                tdx::Json::parse(
                    "{\"Date\":\"20260804\",\"stock_code\":\"000001\","
                    "\"stock_market\":\"0\",\"Stockprice\":\"11.10\","
                    "\"PE_TTM_stock\":\"5.00\"}")}), blocks);
        require(history.size() == 2 &&
                    history.as_array()[0].at("date").as_string() == "2026-08-04" &&
                    history.as_array()[1].at("security").at("name").as_string() == "平安银行" &&
                    std::abs(history.as_array()[1].at("pe_percentiles").at("p50").as_number() -
                             4.83) < 1e-9,
                "PE history sorts dates, resolves security and preserves percentiles");

        const auto pe_members = tdx::normalize_equity_valuation_rows(
            "pe-industry-members", rows({tdx::Json::parse(
                "{\"Date\":\"20260805\",\"stock_code\":\"601918\","
                "\"stock_market\":\"1\",\"stock_name\":\"新集能源\","
                "\"PE_TTM_stock\":\"11.35\",\"PE_stock_max\":\"13.58\","
                "\"PE_stock_min\":\"5.07\",\"T_forecast\":\"0.89\","
                "\"price_T_forecast\":\"10.07\",\"gjpd_T\":\"2\","
                "\"pe_T_forecast\":\"10.66\",\"gzpd_T\":\"3\","
                "\"T1_forecast\":\"0.95\",\"gzpd_T1\":\"1\"}")}), blocks);
        require(pe_members.as_array()[0].at("current_year").at("price_judgment")
                    .at("label").as_string() == "undervalued" &&
                    pe_members.as_array()[0].at("current_year").at("valuation_judgment")
                    .at("label").as_string() == "reasonable" &&
                    pe_members.as_array()[0].at("next_year").at("valuation_judgment")
                    .at("label").as_string() == "overvalued",
                "PE member forecast fields and upstream judgment enums are explicit");

        const auto pb = tdx::normalize_equity_valuation_rows(
            "pb-roe-members", rows({tdx::Json::parse(
                "{\"code_hy\":\"881001\",\"code\":\"601918\","
                "\"market\":\"1\",\"name\":\"新集能源\",\"close\":\"9.43\","
                "\"ROE\":\"3.14\",\"PB\":\"1.40\","
                "\"forcast_PB\":\"1.73\",\"gzpd\":\"2\"}")}), blocks);
        require(std::abs(pb.as_array()[0].at("roe_pct").as_number() - 3.14) < 1e-9 &&
                    pb.as_array()[0].at("valuation_judgment").at("label").as_string() ==
                        "undervalued",
                "PB-ROE member rows retain forecast PB and valuation label");

        bool duplicate_rejected = false;
        try {
            (void)tdx::normalize_equity_valuation_rows(
                "pb-roe-industries", rows({
                    tdx::Json::parse(
                        "{\"code_hy\":\"881001\",\"market_hy\":\"1\",\"PB_hy\":\"1\"}"),
                    tdx::Json::parse(
                        "{\"code_hy\":\"881001\",\"market_hy\":\"1\",\"PB_hy\":\"2\"}")}),
                blocks);
        } catch (const tdx::Error&) { duplicate_rejected = true; }
        require(duplicate_rejected, "duplicate industry identities are rejected");

        std::cout << "equity valuation tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
