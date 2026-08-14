#include "tdx/leverage.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        const auto market = tdx::normalize_margin_market_rows(tdx::Json::parse(
            "[{\"date\":\"20260804\",\"rzye1\":\"25845.22\","
            "\"rzye4\":\"2.72\",\"rzzj\":\"-367.91\","
            "\"rqye1\":\"220.06\"}]"));
        const auto& market_row = market.as_array()[0];
        require(std::abs(market_row.at("financing_balance_yuan").as_number() -
                         2584522000000.0) < 1.0 &&
                    std::abs(market_row.at("short_balance_yuan").as_number() -
                             22006000000.0) < 1.0,
                "market margin hundred-million-yuan conversion failed");

        const auto transfer = tdx::normalize_margin_transfer_rows(tdx::Json::parse(
            "[{\"$ZQDM\":\"20260807\",\"zrz1\":\"\"," 
            "\"zrz2\":\"1400000000\",\"zrz3\":\"\"," 
            "\"zrz7\":\"147580000000\",\"zrq1\":\"\"," 
            "\"zrq2\":\"0\",\"zrq3\":\"\",\"zrq4\":\"\",\"zrq5\":\"\"}]") );
        const auto& transfer_row = transfer.as_array()[0];
        require(transfer_row.at("transfer_financing_lent_yuan").is_null() &&
                    transfer_row.at("transfer_financing_repaid_yuan").as_number() ==
                        1400000000.0 &&
                    transfer_row.at("transfer_financing_net_change_yuan").is_null() &&
                    transfer_row.at("transfer_financing_balance_yuan").as_number() ==
                        147580000000.0 &&
                    transfer_row.at("securities_lending_repaid_shares").as_number() == 0.0 &&
                    transfer_row.at("securities_lending_balance_shares").is_null() &&
                    transfer_row.at("raw").at("zrz1").as_string().empty(),
                "transfer-financing units or null preservation failed");

        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "000001"}] = tdx::Security{0, "SZ", "深圳", "000001", "平安银行"};
        const auto details = tdx::normalize_margin_security_rows(tdx::Json::parse(
            "[{\"$ZQDM\":\"000001\",\"$SC\":\"0\",\"date\":\"20260804\","
            "\"J_LTSZ\":\"2220.00\",\"J_LTGB\":\"194.06\","
            "\"drjme\":\"-3691981\",\"rzye\":\"4692093552\","
            "\"rzzltsz\":\"2.11\",\"rqyl\":\"1954300\","
            "\"zqzltgf\":\"0.0101\",\"rzrqce\":\"4669736360\"}]"),
            securities);
        const auto& detail = details.as_array()[0];
        require(detail.at("security").at("name").as_string() == "平安银行" &&
                    detail.at("float_market_cap_yuan").as_number() == 222000000000.0 &&
                    detail.at("financing_balance_yuan").as_number() == 4692093552.0 &&
                    detail.at("raw").at("rzye").as_string() == "4692093552",
                "security margin normalization failed");

        const auto classifications = tdx::normalize_margin_classification_rows(
            tdx::Json::parse(
                "[{\"$ZQDM\":\"880446\",\"hy\":\"电气设备\","
                "\"date\":\"20260730\",\"hyrzmr\":\"100\","
                "\"hyrzmc\":\"40\",\"hyrzjm\":\"60\","
                "\"hyrzye\":\"1000\",\"hyrqye\":\"25\","
                "\"hylrce\":\"975\"}]") , "industry");
        require(classifications.as_array()[0].at("classification_name").as_string() ==
                    "电气设备" &&
                    classifications.as_array()[0].at("financing_net_buy_yuan").as_number() == 60.0,
                "margin classification normalization failed");

        const auto classification_history =
            tdx::normalize_margin_classification_history_rows(tdx::Json::parse(
                "[{\"date\":\"20260730\",\"hyrzye\":\"194.5\","
                "\"hyrqye\":\"11.25\",\"hylrce\":\"193.375\"}]") );
        require(classification_history.as_array()[0].at("financing_balance_yuan").as_number() ==
                    19450000000.0 &&
                    classification_history.as_array()[0].at("short_balance_yuan").as_number() ==
                    112500000.0 &&
                    classification_history.as_array()[0].at("financing_short_difference_yuan").as_number() ==
                    19337500000.0,
                "margin classification chart-unit conversion failed");

        const auto flows = tdx::normalize_stock_connect_flow_rows(tdx::Json::parse(
            "[{\"rq\":\"20260804\",\"bxcje\":\"4074.10\","
            "\"drzjlr\":\"12.5\",\"mrcje\":\"200\",\"mccje\":\"180\","
            "\"szzs\":\"4057.74\",\"zdf\":\"-0.27\"}]"));
        require(flows.as_array()[0].at("turnover_yuan").as_number() == 407410000000.0 &&
                    flows.as_array()[0].at("net_buy_turnover_yuan").as_number() == 2000000000.0,
                "stock-connect flow conversion failed");

        const auto holdings = tdx::normalize_stock_connect_holding_rows(tdx::Json::parse(
            "[{\"$ZQDM1\":\"000001\",\"$SC1\":\"0\",\"N001\":\"20260630\","
            "\"N002\":\"736878410\",\"N003\":\"3.79\","
            "\"N004\":\"7405628020.5\",\"N005\":\"570772048\","
            "\"N006\":\"2.94\",\"date\":\"20161205\"}]"), securities);
        const auto& holding = holdings.as_array()[0];
        require(holding.at("security").at("security_id").as_string() == "SZ000001" &&
                    holding.at("holding_share_change").as_number() == 166106362.0 &&
                    holding.at("channel").as_string() == "深股通",
                "quarterly mainland holding normalization failed");

        const auto history = tdx::normalize_stock_connect_history_rows(tdx::Json::parse(
            "[{\"date\":\"20260630\",\"cgsl\":\"736878410\","
            "\"cgbd\":\"166106362\",\"cgsz\":\"7405628020.5\","
            "\"cgzb\":\"3.79\",\"jme\":\"125000000\",\"jmb\":\"1.7\"}]"));
        require(history.as_array()[0].at("holding_ratio_pct").as_number() == 3.79 &&
                    history.as_array()[0].at("market_value_change_yuan").as_number() == 125000000.0,
                "stock-connect history normalization failed");

        const auto holding_chart = tdx::normalize_stock_connect_chart_rows(tdx::Json::parse(
            "[{\"date\":\"20260630\",\"jme\":\"12500\",\"cgzb\":\"3.79\"}]") );
        require(holding_chart.as_array()[0].at("market_value_change_yuan").as_number() ==
                    125000000.0,
                "stock-connect chart ten-thousand-yuan conversion failed");

        securities[{31, "00272"}] = tdx::Security{31, "HK", "香港", "00272", "瑞安房地产"};
        const auto southbound_members = tdx::normalize_stock_connect_southbound_member_rows(
            tdx::Json::parse(
                "[{\"$ZQDM\":\"00272\",\"$SC\":\"31\","
                "\"drjlr\":\"100\",\"wrjlr\":\"500\",\"yyjlr\":\"900\","
                "\"price1\":\"0.54\",\"price2\":\"0.49\",\"price3\":\"0.45\"}]") ,
            securities);
        require(southbound_members.as_array()[0].at("security").at("name").as_string() ==
                    "瑞安房地产" &&
                    southbound_members.as_array()[0].at("daily_net_inflow_yuan").as_number() == 100.0,
                "southbound industry member normalization failed");
        const auto southbound_trend = tdx::normalize_stock_connect_southbound_trend_rows(
            tdx::Json::parse("[{\"date\":\"20260730\",\"drjlr\":\"1.6\"}]") );
        require(southbound_trend.as_array()[0].at("daily_net_inflow_yuan").as_number() ==
                    160000000.0,
                "southbound industry trend hundred-million-yuan conversion failed");

        const auto activity = tdx::normalize_stock_connect_activity_rows(
            tdx::Json::parse(
                "[{\"$ZQDM\":\"000001\",\"$SC\":\"0\","
                "\"tjrq\":\"20240816\",\"jmr\":\"1000\","
                "\"cgsl\":\"120\",\"srsl\":\"100\","
                "\"sjzc\":\"20\",\"zcfd\":\"20\"}]"),
            "daily-increase", securities);
        const auto& activity_row = activity.as_array()[0];
        require(activity_row.at("security").at("name").as_string() == "平安银行" &&
                    activity_row.at("holding_share_change").as_number() == 20.0 &&
                    activity_row.at("snapshot_freshness").as_string() ==
                        "historical-snapshot" &&
                    activity_row.at("raw").at("srsl").as_string() == "100",
                "stock-connect activity snapshot normalization failed");

        const auto industries = tdx::normalize_stock_connect_industry_rows(
            tdx::Json::parse(
                "[{\"$ZQDM\":\"HK0201\",\"$SC\":\"70\","
                "\"date\":\"20260318\",\"drjlr\":\"10\","
                "\"wrjlr\":\"50\",\"yyjlr\":\"100\","
                "\"price1\":\"110\",\"price2\":\"100\","
                "\"price3\":\"88\",\"LTSZ\":\"1000000\"}]"),
            "southbound-industry");
        const auto& industry = industries.as_array()[0];
        require(industry.at("classification_code").as_string() == "HK0201" &&
                    std::abs(industry.at("five_day_change_pct").as_number() - 10.0) < 1e-12 &&
                    std::abs(industry.at("one_month_change_pct").as_number() - 25.0) < 1e-12,
                "stock-connect industry flow normalization failed");

        const auto active = tdx::normalize_stock_connect_active_rows(
            tdx::Json::parse(
                "[{\"$ZQDM\":\"000001\",\"$SC\":\"0\","
                "\"price\":\"10\",\"zf\":\"2\",\"cje\":\"1000\","
                "\"bje\":\"100\",\"sje\":\"40\","
                "\"cjje\":\"2000000\",\"date\":\"20260731\"}]"),
            "sz-northbound", securities);
        const auto& active_row = active.as_array()[0];
        require(active_row.at("net_buy_10k_yuan").as_number() == 60.0 &&
                    std::abs(active_row.at("net_buy_total_turnover_pct").as_number() - 6.0) < 1e-12 &&
                    std::abs(active_row.at("connect_total_turnover_pct").as_number() - 20.0) < 1e-12,
                "stock-connect active-stock derived amounts failed");
        std::cout << "leverage tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "leverage test failed: " << error.what() << '\n';
        return 1;
    }
}
