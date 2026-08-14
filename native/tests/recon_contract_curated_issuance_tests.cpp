#include "recon_contract_test_support.hpp"

namespace recon_contract_test {

void run_curated_issuance_contracts() {
    auto curated_body = tdx::Json::parse(
        R"json({"schema":"tdx-market-curated-data-native-v1","view":"all","mode":"catalog","availability":"live","match_count":1934,"summary":{"media_entertainment":73,"low_valuation_smallcap":49,"dividend_fundraising":500,"buyback_statistics":14,"high_dividend":3,"hk_performance":651,"high_refinancing_lending":300,"below_book_soe":344,"valid_refinancing_lending_rows":3,"refinancing_lending_latest_date":"20240930"},"records":[{"kind":"media-entertainment","title":"《星河入梦》","background_excerpt":"影视项目背景","source_resource":"list/func_cmyl101_1.jsn","raw":{}},{"kind":"low-valuation-smallcap","estimated_peg":0.8,"source_resource":"list/func_dgzxz101.jsn","raw":{"PE":20,"YCEPS":25}},{"kind":"dividend-fundraising","cumulative_dividend_yuan":401145437200,"dividend_fundraising_ratio":178.73289885648,"source_resource":"list/func_fhmz101_1.jsn","raw":{"ljfh":4011.454372,"ljmj":22.44385}},{"kind":"buyback-statistics","planned_buyback_yuan":32802000000,"market_cap_ratio_pct":0.0290606338608782,"source_resource":"list/func_gfhgtj101_1.jsn","raw":{"hgsz":328.02,"J_ZSZ":112874344575665.03}},{"kind":"high-refinancing-lending","refinancing_lending_balance_yuan":95800,"source_resource":"list/func_ggzrt101_1.jsn","raw":{"zxye":9.58}},{"kind":"hk-performance","security":{"market":"hk","market_id":49,"code":"02800"},"source_resource":"list/func_ggthq101_1.jsn","raw":{}},{"kind":"below-book-soe","price_to_book_ratio":0.47,"controlling_shareholder":"财政部","source_resource":"list/func_gqpjg101_1.jsn","raw":{}}],"sources":[{"resource":"list/func_cmyl101_1.jsn","endpoint":"local-jsn:a"},{"resource":"list/func_dgzxz101.jsn","endpoint":"local-jsn:b"},{"resource":"list/func_fhmz101_1.jsn","endpoint":"local-jsn:c"},{"resource":"list/func_gfhgtj101_1.jsn","endpoint":"local-jsn:d"},{"resource":"list/func_gfhl101_1.jsn","endpoint":"local-jsn:e"},{"resource":"list/func_ggthq101_1.jsn","endpoint":"local-jsn:f"},{"resource":"list/func_ggzrt101_1.jsn","endpoint":"local-jsn:g"},{"resource":"list/func_gqpjg101_1.jsn","endpoint":"local-jsn:h"}]})json");
    const auto curated = tdx::evaluate_api_contract_response(
        "curated-data-live", 200, "application/json", curated_body.dump(-1));
    require(curated.at("passed").as_bool(), "curated data contract failed");

    curated_body["records"].as_array()[5]["security"]["market_id"] = 31;
    const auto curated_bad_market = tdx::evaluate_api_contract_response(
        "curated-data-live", 200, "application/json", curated_body.dump(-1));
    require(!curated_bad_market.at("passed").as_bool(),
            "curated data contract must retain Hong Kong market 49");

    auto special_attention_body = tdx::Json::parse(
        R"json({"schema":"tdx-market-special-attention-native-v1","view":"all","mode":"catalog","availability":"live","match_count":1146,"summary":{"equity_dispersion":170,"st_risk":136,"star_cap_removal":73,"investigations":467,"goodwill_risk":300},"records":[{"kind":"equity-dispersion","source_resource":"list/func_tbgz102_1.jsn","security":{"market":"sz","code":"000010"},"raw":{"$ZQDM":"000010","$SC":"0"}},{"kind":"st-risk","shareholder_households_10k":12.6753,"source_resource":"list/func_tbgz103_1.jsn","security":{"market":"sz","code":"000016"},"raw":{"zxgdhs":"126753"}},{"kind":"star-cap-removal","current_net_profit_yuan":305965400,"source_resource":"list/func_tbgz104_1.jsn","security":{"market":"sz","code":"000430"},"raw":{"jlr1":"30596.54"}},{"kind":"investigations","source_resource":"list/func_tbgz106_1.jsn","security":{"market":"sh","code":"600363"},"raw":{"$ZQDM1":"600363","$SC1":"1","$ZQDM":"4600363"}},{"kind":"goodwill-risk","goodwill_change_yuan":-1161510.92,"source_resource":"list/func_tbgz110_1.jsn","security":{"market":"sz","code":"300299"},"raw":{"sy1":"201194861.82","sy2":"202356372.74"}}],"sources":[{"resource":"list/func_tbgz102_1.jsn","endpoint":"local-jsn:a"},{"resource":"list/func_tbgz103_1.jsn","endpoint":"local-jsn:b"},{"resource":"list/func_tbgz104_1.jsn","endpoint":"local-jsn:c"},{"resource":"list/func_tbgz106_1.jsn","endpoint":"local-jsn:d"},{"resource":"list/func_tbgz110_1.jsn","endpoint":"local-jsn:e"}]})json");
    const auto special_attention = tdx::evaluate_api_contract_response(
        "special-attention-live", 200, "application/json", special_attention_body.dump(-1));
    require(special_attention.at("passed").as_bool(), "special attention contract failed");

    special_attention_body["records"].as_array()[1]["shareholder_households_10k"] = 126753;
    const auto special_attention_bad_units = tdx::evaluate_api_contract_response(
        "special-attention-live", 200, "application/json", special_attention_body.dump(-1));
    require(!special_attention_bad_units.at("passed").as_bool(),
            "special attention contract must enforce household unit conversion");

    auto fund_statistics_body = tdx::Json::parse(
        R"json({"schema":"tdx-market-fund-statistics-native-v1","view":"all","mode":"catalog","availability":"live","match_count":3811,"summary":{"new_funds":244,"fund_dividends":581,"equity_fund_performance":2723,"fund_market_size":20,"fund_market_size_chart":20,"etf_market_size":23,"etf_subscription_chart":23,"etf_weekly":51,"listed_funds":126},"records":[{"kind":"new-funds","security":{"market":"fund","market_id":33,"code":"023205"},"source_resource":"list/func_jjtj101_1.jsn","raw":{"$SC":"33","$ZQDM":"023205"}},{"kind":"fund-market-size","all_fund_nav_yuan":39284172000000,"source_resource":"list/func_jjtj104_1.jsn","raw":{"qbzc":"392841.72"}},{"kind":"etf-market-size","total_net_subscription_units":-10581000000,"source_resource":"list/func_jjtj105_1.jsn","raw":{"hjss":"-105.81"}},{"kind":"etf-weekly","turnover_yuan":772030332909.13,"source_resource":"list/func_jjtj108_1.jsn","raw":{"BZCJE":"7720.3033290913"}},{"kind":"listed-funds","raised_units":232256813,"source_resource":"list/func_jjtj109_1.jsn","raw":{"mjfe":"2.32256813"}}],"sources":[{"resource":"list/func_jjtj101_1.jsn","endpoint":"local-jsn:a"},{"resource":"list/func_jjtj102_1.jsn","endpoint":"local-jsn:b"},{"resource":"list/func_jjtj103_1.jsn","endpoint":"local-jsn:c"},{"resource":"list/func_jjtj104_1.jsn","endpoint":"local-jsn:d"},{"resource":"list/func_jjtj104_2.jsn","endpoint":"local-jsn:e"},{"resource":"list/func_jjtj105_1.jsn","endpoint":"local-jsn:f"},{"resource":"list/func_jjtj105_2.jsn","endpoint":"local-jsn:g"},{"resource":"list/func_jjtj108_1.jsn","endpoint":"local-jsn:h"},{"resource":"list/func_jjtj109_1.jsn","endpoint":"local-jsn:i"}]})json");
    const auto fund_statistics = tdx::evaluate_api_contract_response(
        "fund-statistics-live", 200, "application/json", fund_statistics_body.dump(-1));
    require(fund_statistics.at("passed").as_bool(), "fund statistics contract failed");

    fund_statistics_body["records"].as_array()[2]["total_net_subscription_units"] = -105.81;
    const auto fund_statistics_bad_units = tdx::evaluate_api_contract_response(
        "fund-statistics-live", 200, "application/json", fund_statistics_body.dump(-1));
    require(!fund_statistics_bad_units.at("passed").as_bool(),
            "fund statistics contract must enforce hundred-million units");

    auto specialized_metrics_body = tdx::Json::parse(
        R"json({"schema":"tdx-market-specialized-metrics-native-v1","view":"all","mode":"catalog","availability":"live","match_count":97,"summary":{"banks":42,"securities":49,"insurers":6},"records":[{"kind":"banks","capital_adequacy_ratio_pct":13.46,"source_resource":"list/func_hyjyfx101_1.jsn","raw":{"zbczl":"0.1346"}},{"kind":"securities","monthly_revenue_yoy_pct":30,"source_resource":"list/func_hyjyfx102_1.jsn","raw":{"yysr":"130","snyysr":"100"}},{"kind":"insurers","core_solvency_adequacy_ratio_pct":160.7,"source_resource":"list/func_hyjyfx103_1.jsn","raw":{"T012":"1.607"}}],"sources":[{"resource":"list/func_hyjyfx101_1.jsn"},{"resource":"list/func_hyjyfx102_1.jsn"},{"resource":"list/func_hyjyfx103_1.jsn"}]})json");
    const auto specialized_metrics = tdx::evaluate_api_contract_response(
        "specialized-metrics-live", 200, "application/json", specialized_metrics_body.dump(-1));
    require(specialized_metrics.at("passed").as_bool(), "specialized metrics contract failed");
    specialized_metrics_body["records"].as_array()[0]["capital_adequacy_ratio_pct"] = 0.1346;
    require(!tdx::evaluate_api_contract_response("specialized-metrics-live", 200,
                                                 "application/json",
                                                 specialized_metrics_body.dump(-1))
                 .at("passed")
                 .as_bool(),
            "specialized metrics contract must enforce ratio units");

    auto company_changes_body = tdx::Json::parse(
        R"json({"schema":"tdx-market-company-changes-native-v1","view":"all","mode":"catalog","availability":"live","match_count":5130,"summary":{"security-renames":300,"company-renames":80,"mainland-index":1000,"major-equity":200,"hk-index":100,"controllers":100,"industries":1000,"equity-transfers":600,"neeq-index":1750},"records":[{"kind":"major-equity","shares":1689599800,"source_resource":"list/func_zqbg104_1.jsn","raw":{"NBD3":"168959.98"}},{"kind":"equity-transfers","transfer_price_yuan":12.45,"source_resource":"list/func_zqbg109_1.jsn","raw":{"zkx":"311250000","zrgb":"25000000"}},{"kind":"neeq-index","security":{"market":"neeq"},"source_resource":"list/func_zqbg110_1.jsn","raw":{"$SC":""}}],"sources":[{"resource":"list/func_zqbg101_1.jsn"},{"resource":"list/func_zqbg102_1.jsn"},{"resource":"list/func_zqbg103_1.jsn"},{"resource":"list/func_zqbg104_1.jsn"},{"resource":"list/func_zqbg105_1.jsn"},{"resource":"list/func_zqbg107_1.jsn"},{"resource":"list/func_zqbg108_1.jsn"},{"resource":"list/func_zqbg109_1.jsn"},{"resource":"list/func_zqbg110_1.jsn"}]})json");
    require(tdx::evaluate_api_contract_response("company-changes-live", 200, "application/json",
                                                company_changes_body.dump(-1))
                .at("passed")
                .as_bool(),
            "company changes contract failed");
    company_changes_body["records"].as_array()[0]["shares"] = 168959.98;
    require(!tdx::evaluate_api_contract_response("company-changes-live", 200, "application/json",
                                                 company_changes_body.dump(-1))
                 .at("passed")
                 .as_bool(),
            "company changes contract must enforce ten-thousand-share unit");

    auto financial_screen_body = tdx::Json::parse(
        R"json({"schema":"tdx-market-financial-screen-native-v1","view":"all","sort":"market-cap","order":"desc","mode":"catalog","availability":"live","match_count":5539,"summary":{"sh-main":1699,"sz-main":1494,"chinext":1401,"star":612,"beijing":333},"records":[{"board":"sh-main","market_cap_yuan":309411200000,"debt_ratio_pct":91.84,"contract_liability_yoy_pct":30.604407884490946,"source_resource":"list/func_cwzb101_1.jsn","raw":{"SZ":"30941.12","ZCFZ":"91.84","htfzbq":"852638742.34","htfzsq":"652840708.94"}},{"board":"sz-main","market_cap_yuan":218704700000,"source_resource":"list/func_cwzb102_1.jsn","raw":{"SZ":"21870.47"}}],"sources":[{"resource":"list/func_cwzb101_1.jsn"},{"resource":"list/func_cwzb102_1.jsn"},{"resource":"list/func_cwzb104_1.jsn"},{"resource":"list/func_cwzb105_1.jsn"},{"resource":"list/func_cwzb107_1.jsn"}]})json");
    require(tdx::evaluate_api_contract_response("financial-screen-live", 200, "application/json",
                                                financial_screen_body.dump(-1))
                .at("passed")
                .as_bool(),
            "financial screen contract failed");
    financial_screen_body["records"].as_array()[0]["market_cap_yuan"] = 30941.12;
    require(!tdx::evaluate_api_contract_response("financial-screen-live", 200, "application/json",
                                                 financial_screen_body.dump(-1))
                 .at("passed")
                 .as_bool(),
            "financial screen contract must enforce ten-million-yuan market cap");

    const auto small_cap_growth = tdx::evaluate_api_contract_response(
        "small-cap-growth-live", 200, "application/json", [&]() {
            tdx::Json body = tdx::Json::object();
            body["schema"] = "tdx-market-financial-screen-native-v1";
            body["dataset"] = "small-cap-growth";
            body["match_count"] = 33;
            body["summary"] = tdx::Json::object();
            body["summary"]["sh-main"] = 10;
            body["summary"]["sz-main"] = 4;
            body["summary"]["chinext"] = 6;
            body["summary"]["star"] = 13;
            body["records"] = tdx::Json::array();
            body["projection_reconciliation"] = tdx::Json::object();
            body["projection_reconciliation"]["master_unique_count"] = 33;
            body["projection_reconciliation"]["chinext"] = tdx::Json::object();
            body["projection_reconciliation"]["chinext"]["exact_match"] = true;
            body["projection_reconciliation"]["star"] = tdx::Json::object();
            body["projection_reconciliation"]["star"]["exact_match"] = true;
            for (int index = 0; index < 33; ++index) {
                const double cagr = 100.0 - index;
                tdx::Json row = tdx::Json::object();
                row["dataset"] = "small-cap-growth";
                row["security"] = tdx::Json::object();
                row["security"]["code"] = "600137";
                row["report_period"] = "20260331";
                row["adjusted_net_profit_yoy_pct"] = 42.1;
                row["adjusted_net_profit_yoy_t_minus_1_pct"] = 30.0;
                row["adjusted_net_profit_yoy_t_minus_2_pct"] = 20.0;
                row["adjusted_net_profit_yoy_t_minus_3_pct"] = 10.0;
                row["adjusted_net_profit_cagr_3y_pct"] = cagr;
                row["revenue_yoy_pct"] = 18.0;
                row["revenue_cagr_3y_pct"] = 12.0;
                row["raw"] = tdx::Json::object();
                row["raw"]["jlzs1"] = "42.1";
                row["raw"]["jlfh"] = cagr;
                row["raw"]["yszs1"] = "18.0";
                row["raw"]["ysfh"] = "12.0";
                body["records"].push_back(std::move(row));
            }
            body["sources"] = tdx::Json::array();
            for (const auto *resource : {"list/func_xpcz101_1.jsn", "list/func_xpcz103_1.jsn",
                                         "list/func_xpcz104_1.jsn"}) {
                tdx::Json source = tdx::Json::object();
                source["resource"] = resource;
                body["sources"].push_back(std::move(source));
            }
            return body.dump(-1);
        }());
    require(small_cap_growth.at("passed").as_bool(), "small-cap growth contract failed");

    auto equity_performance_body = tdx::Json::parse(
        R"json({"schema":"tdx-market-equity-performance-native-v1","sort":"return-5d","order":"desc","mode":"catalog","availability":"live","match_count":5539,"summary":{"sh":2311,"sz":2895,"bj":333},"records":[{"security":{"market":"sz","code":"301707"},"quote_date":"20260807","close":122.19,"prior_month_close":24.59,"daily_turnover_yuan":986373760,"five_day_turnover_yuan":6491881472,"return_5d_pct":396.89,"month_to_date_pct":396.909312728752,"source_resource":"list/func_aghq101.jsn","raw":{"price0":"122.19","price1":"24.59","cje":"986373760","zdf_5d":"396.89"}},{"security":{"market":"sz","code":"000001"},"quote_date":"20260807","close":11.19,"prior_month_close":11.63,"daily_turnover_yuan":986373760,"return_5d_pct":-3.78,"month_to_date_pct":-3.78331900257954,"source_resource":"list/func_aghq101.jsn","raw":{"price0":"11.19","price1":"11.63","cje":"986373760","zdf_5d":"-3.78"}}],"sources":[{"resource":"list/func_aghq101.jsn"}]})json");
    require(tdx::evaluate_api_contract_response("equity-performance-live", 200, "application/json",
                                                equity_performance_body.dump(-1))
                .at("passed")
                .as_bool(),
            "equity performance contract failed");
    equity_performance_body["records"].as_array()[1]["month_to_date_pct"] = -0.44;
    require(!tdx::evaluate_api_contract_response("equity-performance-live", 200, "application/json",
                                                 equity_performance_body.dump(-1))
                 .at("passed")
                 .as_bool(),
            "equity performance contract must enforce CFG month formula");

    const auto corporate_orders = tdx::evaluate_api_contract_response(
        "corporate-orders-live", 200, "application/json",
        R"json({"schema":"tdx-market-corporate-orders-native-v1","match_count":5806,"summary":{"tenders":5767,"major_contracts":39,"formula_mismatches":0},"records":[{"security":{"market":"sz","code":"002090"},"source_url":"https://example.test/order.pdf","source_resource":"list/func_zb101_1.jsn","raw":{}}],"sources":[{"resource":"list/func_zb101_1.jsn"},{"resource":"list/func_zdht101_1.jsn"}]})json");
    require(corporate_orders.at("passed").as_bool(), "corporate orders contract failed");

    const auto event_impact = tdx::evaluate_api_contract_response(
        "event-impact-live", 200, "application/json",
        R"json({"schema":"tdx-market-event-impact-native-v1","match_count":583,"summary":{"shanghai_composite":116,"hang_seng":232,"nasdaq_composite":235},"records":[{"benchmark":"shanghai-composite","description":"样本事件","closes":{},"impacts":{},"raw":{}}],"sources":[{"resource":"list/func_zdsj101_1.jsn"},{"resource":"list/func_zdsj102_1.jsn"},{"resource":"list/func_zdsj103_1.jsn"}]})json");
    require(event_impact.at("passed").as_bool(), "event impact contract failed");

    const auto global_performance = tdx::evaluate_api_contract_response(
        "global-performance-live", 200, "application/json",
        R"json({"schema":"tdx-market-global-performance-native-v1","match_count":365,"summary":{"major_indices":13,"overseas_china":352},"records":[{"kind":"major-index","instrument":{"market_id":1,"code":"000300","name":"沪深300"},"return_5d_pct":5,"source_resource":"list/func_zyzh101.jsn","raw":{"price0":105,"price5":100}}],"sources":[{"resource":"list/func_zyzh101.jsn"},{"resource":"list/func_zgghq101.jsn"}]})json");
    require(global_performance.at("passed").as_bool(), "global performance contract failed");

    const auto ipo_guidance =
        tdx::evaluate_api_contract_response("ipo-guidance-live", 200, "application/json", [&]() {
            tdx::Json body = tdx::Json::object();
            body["schema"] = "tdx-market-calendar-native-v1";
            body["view"] = "ipo-guidance";
            body["availability"] = "live";
            body["match_count"] = 1259;
            body["summary"] = tdx::Json::object();
            body["summary"]["ipo_guidance"] = 1259;
            body["summary"]["ipo_guidance_boards"] = tdx::Json::object();
            body["summary"]["ipo_guidance_progress"] = tdx::Json::object();
            body["summary"]["ipo_guidance_regions"] = tdx::Json::object();
            const std::array<std::string, 4> boards{"主板", "创业板", "科创板", "北交所"};
            const std::array<std::string, 6> progresses{"辅导备案",     "撤回辅导备案", "辅导验收",
                                                        "辅导工作完成", "辅导中",       "终止辅导"};
            const std::array<std::string, 4> regions{"江苏", "浙江", "广东", "北京"};
            std::array<int, 4> board_counts{};
            std::array<int, 6> progress_counts{};
            std::array<int, 4> region_counts{};
            body["rows"] = tdx::Json::array();
            for (int index = 0; index < 1259; ++index) {
                ++board_counts[index % boards.size()];
                ++progress_counts[index % progresses.size()];
                ++region_counts[index % regions.size()];
                const auto record_id = "sample-" + std::to_string(index);
                tdx::Json row = tdx::Json::object();
                row["kind"] = "ipo-guidance";
                row["company_name"] = "样本企业" + std::to_string(index);
                row["date"] = "20260808";
                row["guidance_progress"] = progresses[index % progresses.size()];
                row["board_category"] = boards[index % boards.size()];
                row["guidance_institution"] = "样本辅导机构";
                row["region"] = regions[index % regions.size()];
                row["content"] = "样本公司简介";
                row["event_id"] = "ipo-guidance:" + record_id;
                row["source_record_id"] = record_id;
                row["source_resource"] = "list/func_zdgz_qzkcbd101_1.jsn";
                row["raw"] = tdx::Json::object();
                row["raw"]["$ZQDM"] = record_id;
                body["rows"].push_back(std::move(row));
            }
            for (std::size_t index = 0; index < boards.size(); ++index)
                body["summary"]["ipo_guidance_boards"][boards[index]] = board_counts[index];
            for (std::size_t index = 0; index < progresses.size(); ++index)
                body["summary"]["ipo_guidance_progress"][progresses[index]] =
                    progress_counts[index];
            for (std::size_t index = 0; index < regions.size(); ++index)
                body["summary"]["ipo_guidance_regions"][regions[index]] = region_counts[index];
            body["sources"] = tdx::Json::array();
            tdx::Json source = tdx::Json::object();
            source["resource"] = "list/func_zdgz_qzkcbd101_1.jsn";
            body["sources"].push_back(std::move(source));
            return body.dump(-1);
        }());
    require(ipo_guidance.at("passed").as_bool(), "IPO guidance contract failed");

    const auto ipo_review =
        tdx::evaluate_api_contract_response("ipo-review-live", 200, "application/json", [&]() {
            tdx::Json body = tdx::Json::object();
            body["schema"] = "tdx-market-calendar-native-v1";
            body["view"] = "ipo-review";
            body["availability"] = "live";
            body["match_count"] = 514;
            body["summary"] = tdx::Json::object();
            body["summary"]["ipo_reviews"] = 514;
            body["summary"]["ipo_review_boards"] = tdx::Json::object();
            body["summary"]["ipo_review_statuses"] = tdx::Json::object();
            const std::array<std::string, 4> boards{"主板", "创业板", "科创板", "北交所"};
            const std::array<std::string, 7> statuses{"已受理",   "已问询", "上会通过", "提交注册",
                                                      "注册生效", "中止",   "终止"};
            std::array<int, 4> board_counts{};
            std::array<int, 7> status_counts{};
            body["rows"] = tdx::Json::array();
            for (int index = 0; index < 514; ++index) {
                ++board_counts[index % boards.size()];
                ++status_counts[index % statuses.size()];
                tdx::Json row = tdx::Json::object();
                row["kind"] = "ipo-review";
                row["company_name"] = "样本企业" + std::to_string(index);
                row["review_status"] = statuses[index % statuses.size()];
                row["board_category"] = boards[index % boards.size()];
                row["sponsor"] = "样本保荐人";
                row["prospectus_url"] = "https://example.test/ipo.pdf";
                row["source_resource"] = "list/func_zdgz_kcbsq101_1.jsn";
                row["planned_financing_yuan"] = 250000000.0;
                row["pre_issue_total_shares"] = 75000000.0;
                row["planned_issue_shares"] = 25000000.0;
                row["post_issue_share_pct"] = 25.0;
                row["raw"] = tdx::Json::object();
                row["raw"]["rzje"] = "250000000.0";
                body["rows"].push_back(std::move(row));
            }
            for (std::size_t index = 0; index < boards.size(); ++index)
                body["summary"]["ipo_review_boards"][boards[index]] = board_counts[index];
            for (std::size_t index = 0; index < statuses.size(); ++index)
                body["summary"]["ipo_review_statuses"][statuses[index]] = status_counts[index];
            body["sources"] = tdx::Json::array();
            tdx::Json source = tdx::Json::object();
            source["resource"] = "list/func_zdgz_kcbsq101_1.jsn";
            body["sources"].push_back(std::move(source));
            return body.dump(-1);
        }());
    require(ipo_review.at("passed").as_bool(), "IPO review contract failed");

    const auto ipo_subscriptions = tdx::evaluate_api_contract_response(
        "ipo-subscriptions-live", 200, "application/json", [&]() {
            tdx::Json body = tdx::Json::object();
            body["schema"] = "tdx-market-calendar-native-v1";
            body["view"] = "ipo-subscriptions";
            body["availability"] = "live";
            body["match_count"] = 34;
            body["rows"] = tdx::Json::array();
            const std::array<std::string, 4> boards{"主板", "创业板", "科创板", "北交所"};
            for (int index = 0; index < 34; ++index) {
                tdx::Json row = tdx::Json::object();
                row["kind"] = "ipo-subscription";
                row["source_resource"] = "list/func_zdgz_kcbsq103_1.jsn";
                row["board_category"] = boards[index % boards.size()];
                row["issue_price"] = 10.0 + index;
                row["planned_financing_yuan"] = 250000000.0;
                row["pre_issue_total_shares"] = 75000000.0;
                row["planned_issue_shares"] = 25000000.0;
                row["post_issue_share_pct"] = 25.0;
                row["security"] = tdx::Json::object();
                row["security"]["market"] = boards[index % boards.size()] == "北交所" ? "bj" : "sz";
                row["security"]["code"] = "301688";
                row["raw"] = tdx::Json::object();
                row["raw"]["rzje"] = "250000000.0";
                body["rows"].push_back(std::move(row));
            }
            body["sources"] = tdx::Json::array();
            tdx::Json source = tdx::Json::object();
            source["resource"] = "list/func_zdgz_kcbsq103_1.jsn";
            body["sources"].push_back(std::move(source));
            return body.dump(-1);
        }());
    require(ipo_subscriptions.at("passed").as_bool(), "IPO subscriptions contract failed");

    const auto ipo_subscription_details = tdx::evaluate_api_contract_response(
        "ipo-subscription-details-live", 200, "application/json", [&]() {
            tdx::Json body = tdx::Json::object();
            body["schema"] = "tdx-market-calendar-native-v1";
            body["view"] = "ipo-subscription-details";
            body["availability"] = "live";
            body["match_count"] = 44;
            body["rows"] = tdx::Json::array();
            for (int index = 0; index < 44; ++index) {
                tdx::Json row = tdx::Json::object();
                row["kind"] = "ipo-subscription-detail";
                row["subscription_code"] = "889999";
                row["subscription_date"] = "20260810";
                row["listing_date"] = index < 40 ? "20260820" : "";
                row["issue_total_shares"] = 33998500.0;
                row["online_issue_shares"] = 30598650.0;
                row["raised_yuan"] = 271494700.0;
                row["winning_rate_pct"] = 0.02740647;
                row["security"] = tdx::Json::object();
                row["security"]["market"] = "bj";
                row["security"]["code"] = "920059";
                row["raw"] = tdx::Json::object();
                row["raw"]["fxzl"] = "33998500";
                row["raw"]["fxws"] = "30598650";
                row["raw"]["fxmz"] = "271494700";
                row["raw"]["zql"] = "0.02740647";
                if (index == 0) {
                    row["online_issue_shares"] = tdx::Json(nullptr);
                    row["winning_rate_pct"] = tdx::Json(nullptr);
                    row["raw"]["fxws"] = "";
                    row["raw"]["zql"] = "";
                }
                body["rows"].push_back(std::move(row));
            }
            body["sources"] = tdx::Json::array();
            tdx::Json source = tdx::Json::object();
            source["resource"] = "list/func_sbxg101_1.jsn";
            body["sources"].push_back(std::move(source));
            return body.dump(-1);
        }());
    require(ipo_subscription_details.at("passed").as_bool(),
            "BSE IPO subscription details contract failed");

    const auto us_ipo =
        tdx::evaluate_api_contract_response("us-ipo-live", 200, "application/json", [&]() {
            tdx::Json body = tdx::Json::object();
            body["schema"] = "tdx-market-calendar-native-v1";
            body["view"] = "us-ipo";
            body["availability"] = "live";
            body["match_count"] = 3355;
            body["summary"] = tdx::Json::object();
            body["summary"]["us_ipo_applications"] = 718;
            body["summary"]["us_ipo_calendar"] = 2240;
            body["summary"]["us_ipo_listed"] = 396;
            body["summary"]["us_ipo_pending"] = 1;
            body["summary"]["us_ipo_calendar_listed_code_overlap"] = 396;
            body["summary"]["us_ipo_calendar_listed_same_date"] = 382;
            body["summary"]["us_ipo_calendar_listed_date_changed"] = 14;
            body["rows"] = tdx::Json::array();
            for (int index = 0; index < 3355; ++index) {
                const bool application = index < 718;
                const bool scheduled = index >= 718 && index < 2958;
                const bool listed = index >= 2958 && index < 3354;
                const std::string kind = application ? "us-ipo-application"
                                         : scheduled ? "us-ipo-calendar"
                                         : listed    ? "us-ipo-listed"
                                                     : "us-ipo-pending";
                const bool million = application || scheduled;
                tdx::Json row = tdx::Json::object();
                row["kind"] = kind;
                row["source_resource"] = application ? "list/func_mgrl101_1.jsn"
                                         : scheduled ? "list/func_mgrl102_1.jsn"
                                         : listed    ? "list/func_mgxg101_1.jsn"
                                                     : "list/func_mgxg102_1.jsn";
                row["security"] = tdx::Json::object();
                row["security"]["market"] = "us";
                row["security"]["code"] = "TEST";
                row["issue_amount_usd"] = million ? 75000000.0 : 75000000.0;
                row["issue_shares"] = million ? 7500000.0 : 7500000.0;
                row["raw"] = tdx::Json::object();
                row["raw"]["fxze"] = million ? "75" : "75000000";
                row["raw"]["fxgfs"] = million ? "7.5" : "7500000";
                body["rows"].push_back(std::move(row));
            }
            body["sources"] = tdx::Json::array();
            for (const auto *resource : {"list/func_mgrl101_1.jsn", "list/func_mgrl102_1.jsn",
                                         "list/func_mgxg101_1.jsn", "list/func_mgxg102_1.jsn"}) {
                tdx::Json source = tdx::Json::object();
                source["resource"] = resource;
                body["sources"].push_back(std::move(source));
            }
            return body.dump(-1);
        }());
    require(us_ipo.at("passed").as_bool(), "US IPO lifecycle contract failed");
}

} // namespace recon_contract_test
