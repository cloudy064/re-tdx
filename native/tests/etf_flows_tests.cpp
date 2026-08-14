#include "tdx/etf_flows.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{1, "600521"}] =
            tdx::Security{1, "SH", "上海", "600521", "华海药业"};
        std::map<std::string, std::string> industries{{"881230", "医药医疗"}};
        const auto rows = tdx::Json::parse(
            "[{\"$ZQDM\":\"600521\",\"$SC\":\"1\",\"jzrq\":\"20260731\"," 
            "\"etfzs\":\"89\",\"etfzcg\":\"58236050.71\"," 
            "\"etfcgzsz\":\"973706767.92\",\"kjjlr\":\"3057812.36\"," 
            "\"drsghz\":\"-3538872.69\",\"cje\":\"615684544.00\"," 
            "\"jyzjlr\":\"3007328.19\"}]" );
        const auto normalized = tdx::normalize_etf_flow_rows(
            rows, false, securities, industries);
        const auto& stock = normalized.as_array().front();
        require(stock.at("entity").at("name").as_string() == "华海药业" &&
                stock.at("entity").at("security_id").as_string() == "SH600521",
                "ETF stock identity normalization");
        require(std::fabs(stock.at("theme_net_inflow_yuan").as_number() +
                          6596685.05) < 0.01,
                "ETF theme flow equals total minus broad flow");
        require(std::fabs(stock.at("net_inflow_share_turnover_pct").as_number() +
                          0.574771) < 0.0001,
                "ETF total flow share of turnover");

        const auto industry_rows = tdx::Json::parse(
            "[{\"$ZQDM\":\"881230\",\"$SC\":\"1\",\"jzrq\":\"20260731\"," 
            "\"etfzs\":\"16686\",\"etfzcg\":\"6446200485.60\"," 
            "\"etfcgzsz\":\"201092905877.76\",\"kjjlr\":\"-424353703.07\"," 
            "\"drsghz\":\"-1262457279.60\",\"cje\":\"105405952108.00\"," 
            "\"jyzjlr\":\"3758653210.12\"}]" );
        const auto normalized_industries = tdx::normalize_etf_flow_rows(
            industry_rows, true, securities, industries);
        require(normalized_industries.as_array().front().at("entity")
                    .at("name").as_string() == "医药医疗" &&
                normalized_industries.as_array().front().at("entity")
                    .at("type").as_string() == "industry",
                "ETF industry identity normalization");
        std::cout << "etf flow tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "etf flow tests failed: " << error.what() << '\n';
        return 1;
    }
}
