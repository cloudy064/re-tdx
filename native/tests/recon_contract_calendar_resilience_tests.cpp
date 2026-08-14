#include "recon_contract_test_support.hpp"

namespace recon_contract_test {

void run_calendar_resilience_web_contracts() {
    const auto futures_calendar = tdx::evaluate_api_contract_response(
        "futures-calendar-live", 200, "application/json",
        R"json({"schema":"tdx-market-calendar-native-v1","view":"futures","match_count":32,"rows":[{"kind":"futures-calendar","exchange_code":"29","content":"交易提示","raw":{}}],"sources":[{"resource":"list/func_qhrl400_1.jsn"}]})json");
    require(futures_calendar.at("passed").as_bool(), "futures calendar contract failed");

    const auto social_security = tdx::evaluate_api_contract_response(
        "institution-social-security-summary-live", 200, "application/json",
        R"json({"schema":"tdx-institution-analysis-native-v1","view":"social-security-summary","counts":{"matched":18},"sections":[{"layout":"social-security-summary","records":[{"data":{"holding_shares":20367700,"total_share_pct":3.2412},"raw":{}}]}],"sources":[{"resource":"list/func_sbltgd101_1.jsn"}]})json");
    require(social_security.at("passed").as_bool(), "social-security summary contract failed");

    auto development_bank_body = tdx::Json::parse(
        R"json({"schema":"tdx-institution-analysis-native-v1","view":"development-bank-holdings","counts":{"matched":5},"sections":[{"layout":"development-bank-holdings","records":[{"data":{"report_date":"20260331","shareholder_position":"国开金融在十大股东中排第2名","holding_detail":"持有2564.27万股，占总股本8.55%"},"raw":{"cgxx":"原始披露"}}]}],"sources":[{"resource":"list/func_tzcg105_1.jsn"}]})json");
    for (int i = 1; i < 5; ++i)
        development_bank_body["sections"].as_array().front()["records"].push_back(
            development_bank_body["sections"].as_array().front()["records"].as_array().front());
    require(tdx::evaluate_api_contract_response("institution-development-bank-holdings-live", 200,
                                                "application/json", development_bank_body.dump(-1))
                .at("passed")
                .as_bool(),
            "development-bank holdings contract failed");
    development_bank_body["sections"]
        .as_array()
        .front()["records"]
        .as_array()
        .front()["data"]["holding_detail"] = "";
    require(!tdx::evaluate_api_contract_response("institution-development-bank-holdings-live", 200,
                                                 "application/json", development_bank_body.dump(-1))
                 .at("passed")
                 .as_bool(),
            "development-bank contract must require auditable disclosure text");

    const auto notable_investor = tdx::evaluate_api_contract_response(
        "notable-investor-holdings-live", 200, "application/json", [&]() {
            tdx::Json body = tdx::Json::object();
            body["schema"] = "tdx-market-shareholder-signals-native-v1";
            body["view"] = "investor-directory";
            body["mode"] = "investor";
            body["selected_investor"] = tdx::Json::object();
            body["selected_investor"]["investor_id"] = "GD012270";
            body["investor_directory"] = tdx::Json::array();
            for (int index = 0; index < 1755; ++index) {
                tdx::Json row = tdx::Json::object();
                row["investor_id"] =
                    index == 0 ? "GD012270" : "GD" + std::to_string(100000 + index);
                body["investor_directory"].push_back(std::move(row));
            }
            body["investor_holdings"] = tdx::Json::array();
            for (int index = 0; index < 35; ++index) {
                tdx::Json row = tdx::Json::object();
                row["kind"] = "investor-holding";
                row["investor_id"] = "GD012270";
                row["source_resource"] = "nscg/GD012270.jsn";
                row["security"] = tdx::Json::object();
                row["security"]["market"] = "sz";
                row["security"]["code"] = "000825";
                row["holding_change_shares"] = 17524800.0;
                row["holding_value_change_yuan"] = -22376530.0;
                row["holding_pct_change"] = 0.3076;
                row["raw"] = tdx::Json::object();
                row["raw"]["bqcg"] = "40684600";
                row["raw"]["sqcg"] = "23159800";
                row["raw"]["bqje"] = "176571164";
                row["raw"]["sqje"] = "198947694";
                row["raw"]["bqbl"] = "0.7142";
                row["raw"]["sqbl"] = "0.4066";
                body["investor_holdings"].push_back(std::move(row));
            }
            body["investor_reconciliation"] = tdx::Json::object();
            body["investor_reconciliation"]["company_count_matches"] = true;
            body["investor_reconciliation"]["holding_shares_match"] = true;
            body["investor_reconciliation"]["holding_value_matches"] = true;
            body["investor_reconciliation"]["actual_company_count"] = 35;
            body["investor_detail_source"] = tdx::Json::object();
            body["investor_detail_source"]["resource"] = "nscg/GD012270.jsn";
            body["investor_detail_source"]["normalized_row_count"] = 35;
            body["sources"] = tdx::Json::array();
            tdx::Json source = tdx::Json::object();
            source["resource"] = "list/func_nscg101_1.jsn";
            body["sources"].push_back(std::move(source));
            return body.dump(-1);
        }());
    require(notable_investor.at("passed").as_bool(), "notable-investor holdings contract failed");

    auto shareholder_signals_body = tdx::Json::parse(
        R"json({"schema":"tdx-market-shareholder-signals-native-v1","view":"all","sort":"signal","order":"desc","mode":"catalog","availability":"live","match_count":1039,"summary":{"notable-investors":912,"institution-accumulation":45,"research-growth":82},"records":[{"kind":"institution-accumulation","signal_value":595.410813030444,"institution_holding_growth_pct":595.410813030444,"shareholder_count_change_pct":-43.7014136927,"source_resource":"list/func_jgxc101_1.jsn","raw":{"cgzz":"5.95410813030444","rsjs":"-0.437014136927"}},{"kind":"research-growth","signal_value":6,"net_profit_growth_pct":127.18,"return_6m_pct":76.3804,"source_resource":"list/func_jgzd101_1.jsn","raw":{"jlrzf":"127.18","bnzaf":"76.3804"}},{"kind":"notable-investors","signal_value":3,"holding_pct":1.97558962005258,"holding_value_yuan":229365113.2,"source_resource":"list/func_cwnscg101_1.jsn","raw":{"N003":"26670362","N006":"1349995046","N007":"8.6"}}],"sources":[{"resource":"list/func_cwnscg101_1.jsn"},{"resource":"list/func_jgxc101_1.jsn"},{"resource":"list/func_jgzd101_1.jsn"},{"resource":"list/func_nscg101_1.jsn"}]})json");
    shareholder_signals_body["match_count"] = 1085;
    shareholder_signals_body["summary"]["small-cap-institution"] = 46;
    shareholder_signals_body["records"].push_back(tdx::Json::parse(
        R"json({"kind":"small-cap-institution","signal_value":2,"institution_holding_growth_pct":2,"institution_holding_value_change_pct":4,"institution_count_change_pct":25,"source_resource":"list/func_xszgp101_1.jsn","raw":{"zxcg":"100","cgbd":"2","ccsz":"100","zcsz":"4","jgsl":"8","jgbhl":"2"}})json"));
    tdx::Json small_cap_source = tdx::Json::object();
    small_cap_source["resource"] = "list/func_xszgp101_1.jsn";
    shareholder_signals_body["sources"].push_back(std::move(small_cap_source));
    require(tdx::evaluate_api_contract_response("shareholder-signals-live", 200, "application/json",
                                                shareholder_signals_body.dump(-1))
                .at("passed")
                .as_bool(),
            "shareholder signals contract failed");
    shareholder_signals_body["records"].as_array()[0]["institution_holding_growth_pct"] =
        5.95410813030444;
    require(!tdx::evaluate_api_contract_response("shareholder-signals-live", 200,
                                                 "application/json",
                                                 shareholder_signals_body.dump(-1))
                 .at("passed")
                 .as_bool(),
            "shareholder signals contract must enforce ratio-to-percent conversion");

    auto
        financial_insights_body =
            tdx::
                Json::
                    parse(R"json({"schema":"tdx-market-financial-insights-native-v1","view":"all","mode":"catalog","availability":"live","match_count":1151,"summary":{"buffett-quality":6,"high-bonus-potential":39,"investment-property":451,"low-price-sales":20,"dividend-shortfall":123,"equity-investment":200,"cash-above-market-cap":15,"high-receivables":297},"records":[{"kind":"high-bonus-potential","latest_share_capital_shares":139067031,"source_resource":"list/func_cbpl106_1.jsn","raw":{"ZX1":"13906.7031"}},{"kind":"investment-property","investment_property_qoq_pct":100,"source_resource":"list/func_cbpl107_1.jsn","raw":{"cyje1":"200","cyje2":"100"}},{"kind":"dividend-shortfall","latest_dividend_yuan":100000,"source_resource":"list/func_fhbdb101.jsn","raw":{"N001":"10"}},{"kind":"equity-investment","investment_total_yuan":200000,"source_resource":"list/func_gqtz101_1.jsn","raw":{"ZJE":"20"}},{"kind":"cash-above-market-cap","debt_ratio_pct":60,"cumulative_dividend_yuan":1100000000,"source_resource":"list/func_gxjcb101.jsn","raw":{"zcfzl":"0.6","ljfh":"11"}},{"kind":"high-receivables","receivables_to_market_cap_pct":42.5,"source_resource":"list/func_gyszk101_1.jsn","raw":{"zb":"42.5"}}],"sources":[{"resource":"list/func_cbpl101_8.jsn"},{"resource":"list/func_cbpl106_1.jsn"},{"resource":"list/func_cbpl107_1.jsn"},{"resource":"list/func_dpsgp_1.jsn"},{"resource":"list/func_fhbdb101.jsn"},{"resource":"list/func_gqtz101_1.jsn"},{"resource":"list/func_gxjcb101.jsn"},{"resource":"list/func_gyszk101_1.jsn"}]})json");
    financial_insights_body["match_count"] = 1464;
    financial_insights_body["summary"]["profit-warning"] = 29;
    financial_insights_body["summary"]["cash-flow-quality"] = 46;
    financial_insights_body["summary"]["earnings-reversal"] = 238;
    financial_insights_body["records"].push_back(tdx::Json::parse(
        R"json({"kind":"profit-warning","forecast_profit_midpoint_yuan":-156000000,"actual_profit_yuan":-26115409.54,"source_resource":"list/func_knzx101_1.jsn","raw":{"N005":"-26115409.54","N007":"-181000000","N008":"-131000000"}})json"));
    financial_insights_body["records"].push_back(tdx::Json::parse(
        R"json({"kind":"cash-flow-quality","screen_criteria_complete":true,"source_resource":"list/func_xjl101_1.jsn","raw":{"xjl":"3.504","jxjbl":"57.29","jlrbl":"131.319","chzcbl":"32.0677482679586925"}})json"));
    financial_insights_body["records"].push_back(tdx::Json::parse(
        R"json({"kind":"earnings-reversal","profit_growth_pct":10053.28,"profit_growth_recalculated_pct":10053.2833901726,"profit_reversal":true,"source_resource":"list/func_yjfz101_1.jsn","raw":{"JLR1":"-5716279.52","JLR2":"568957500"}})json"));
    for (const auto *resource :
         {"list/func_knzx101_1.jsn", "list/func_xjl101_1.jsn", "list/func_yjfz101_1.jsn"}) {
        tdx::Json source = tdx::Json::object();
        source["resource"] = resource;
        financial_insights_body["sources"].push_back(std::move(source));
    }
    financial_insights_body["match_count"] = 2480;
    financial_insights_body["summary"]["steady-growth"] = 363;
    financial_insights_body["summary"]["quality-growth"] = 108;
    financial_insights_body["summary"]["profit-breakout"] = 104;
    financial_insights_body["summary"]["dividend-plan"] = 441;
    financial_insights_body["records"].push_back(tdx::Json::parse(
        R"json({"kind":"steady-growth","cumulative_dividend_yuan":200000000,"dividend_payout_pct":50,"source_resource":"list/func_wjcg101_1.jsn","raw":{"ljfh":"2","zjlr":"400000000"}})json"));
    financial_insights_body["records"].push_back(tdx::Json::parse(
        R"json({"kind":"quality-growth","revenue_growth_t_pct":20,"screen_criteria_complete":true,"source_resource":"list/func_lxsnzz101_1.jsn","raw":{"denyysr":"100","dynyysr":"120"}})json"));
    financial_insights_body["records"].push_back(tdx::Json::parse(
        R"json({"kind":"profit-breakout","profit_breakout_growth_pct":50,"source_resource":"list/func_tqwclr101.jsn","raw":{"bqlr":"150","sqlr":"100"}})json"));
    financial_insights_body["records"].push_back(tdx::Json::parse(
        R"json({"kind":"dividend-plan","cash_dividend_per_share_yuan":0.3,"stock_transfer_per_share":0.5,"source_resource":"list/func_qxfa101_1.jsn","raw":{"xjfh":"3","szbl":"5"}})json"));
    for (const auto *resource : {"list/func_wjcg101_1.jsn", "list/func_lxsnzz101_1.jsn",
                                 "list/func_tqwclr101.jsn", "list/func_qxfa101_1.jsn"}) {
        tdx::Json source = tdx::Json::object();
        source["resource"] = resource;
        financial_insights_body["sources"].push_back(std::move(source));
    }
    require(tdx::evaluate_api_contract_response("financial-insights-live", 200, "application/json",
                                                financial_insights_body.dump(-1))
                .at("passed")
                .as_bool(),
            "financial insights contract failed");
    financial_insights_body["records"].as_array()[4]["debt_ratio_pct"] = 0.6;
    require(!tdx::evaluate_api_contract_response("financial-insights-live", 200, "application/json",
                                                 financial_insights_body.dump(-1))
                 .at("passed")
                 .as_bool(),
            "financial insights contract must enforce decimal debt ratio conversion");

    auto gdr_body = tdx::Json::parse(
        R"json({"schema":"tdx-market-gdr-native-v1","mode":"catalog","availability":"live","match_count":23,"summary":{"unique_underlyings":23},"records":[{"gdr_code":"CPIC","premium_pct":-2.24,"underlying":{"security_id":"SH601601","market":"sh","code":"601601"},"source_resource":"list/func_gdr101.jsn","raw":{"yjl":"-2.24"}}],"sources":[{"resource":"list/func_gdr101.jsn"}]})json");
    require(
        tdx::evaluate_api_contract_response("gdr-live", 200, "application/json", gdr_body.dump(-1))
            .at("passed")
            .as_bool(),
        "GDR contract failed");
    gdr_body["records"].as_array()[0]["premium_pct"] = -0.0224;
    require(
        !tdx::evaluate_api_contract_response("gdr-live", 200, "application/json", gdr_body.dump(-1))
             .at("passed")
             .as_bool(),
        "GDR contract must preserve source premium points");

    const auto fund_calendar = tdx::evaluate_api_contract_response(
        "fund-calendar-live", 200, "application/json",
        R"json({"schema":"tdx-market-fund-calendar-native-v1","mode":"catalog","availability":"live","match_count":315,"summary":{"unique_funds":261,"event_type_count":7,"category_count":5},"records":[{"security":{"market":"fund","market_id":33,"code":"027099"},"event_type":"基金开放申购","event_date":"20260814","content":"开放申购，起购金额1元","fund_category":"FOF","source_resource":"list/func_jjrl301_1.jsn","raw":{"$SC":"33","$ZQDM":"027099"}}],"sources":[{"resource":"list/func_jjrl301_1.jsn"}]})json");
    require(fund_calendar.at("passed").as_bool(), "fund calendar contract failed");

    auto recent_watch_body = tdx::Json::parse(
        R"json({"schema":"tdx-market-recent-watch-native-v1","view":"all","mode":"catalog","availability":"live","match_count":1530,"summary":{"earnings-divergence":110,"foreign-business":1405,"st-turnaround":15},"records":[{"kind":"earnings-divergence","return_since_announcement_pct":-38.87,"source_resource":"list/func_jqgz101_1.jsn","raw":{"zf_gj":"-38.87"}},{"kind":"foreign-business","foreign_revenue_yuan":3567000000,"source_resource":"list/func_jqgz104_1.jsn","raw":{"srje":"35.67"}},{"kind":"st-turnaround","profit_lower_yuan":1600000,"source_resource":"list/func_jqgz111_1.jsn","raw":{"ygjl1":"1600000"}}],"sources":[{"resource":"list/func_jqgz101_1.jsn"},{"resource":"list/func_jqgz104_1.jsn"},{"resource":"list/func_jqgz111_1.jsn"}]})json");
    require(tdx::evaluate_api_contract_response("recent-watch-live", 200, "application/json",
                                                recent_watch_body.dump(-1))
                .at("passed")
                .as_bool(),
            "recent watch contract failed");
    recent_watch_body["records"].as_array()[1]["foreign_revenue_yuan"] = 35.67;
    require(!tdx::evaluate_api_contract_response("recent-watch-live", 200, "application/json",
                                                 recent_watch_body.dump(-1))
                 .at("passed")
                 .as_bool(),
            "recent watch contract must enforce hundred-million-yuan unit");

    auto patent_body = tdx::Json::parse(
        R"json({"schema":"tdx-market-patent-statistics-native-v1","mode":"catalog","availability":"live","match_count":3970,"summary":{"unique_securities":3970,"sz":2104,"sh":1538,"bj":328},"records":[{"security":{"market":"sh","code":"601668"},"report_date":"20251231","cumulative_grant_invention":14107,"cumulative_grant_utility_model":null,"cumulative_grant_design":null,"cumulative_grant_classified_total":14107,"cumulative_grant_total":72800,"cumulative_grant_total_delta":58693,"source_resource":"list/func_gszl101_1.jsn","raw":{"ljfm":"14107","ljhj":"72800"}}],"sources":[{"resource":"list/func_gszl101_1.jsn"}]})json");
    require(tdx::evaluate_api_contract_response("patent-statistics-live", 200, "application/json",
                                                patent_body.dump(-1))
                .at("passed")
                .as_bool(),
            "patent statistics contract failed");
    patent_body["records"].as_array()[0]["cumulative_grant_total_delta"] = 58692;
    require(!tdx::evaluate_api_contract_response("patent-statistics-live", 200, "application/json",
                                                 patent_body.dump(-1))
                 .at("passed")
                 .as_bool(),
            "patent statistics contract must enforce total delta");

    auto overview_body = tdx::Json::parse(
        R"json({"schema":"tdx-market-overview-factors-native-v1","signal":"all","availability":"live","match_count":17,"returned":17,"summary":{"positive":2,"neutral":3,"negative":6,"unrated":6},"records":[{"factor_id":"zb0","name":"央行近期放水/回笼","description":"客户端日期快照","signal":"negative","signal_label":"利空","chart_indicator":"当日央行净投放(亿)","source_rank":1,"source_resource":"list/func_dpfx101_1.jsn","raw":{"lx":"利空"}}],"sources":[{"resource":"list/func_dpfx101_1.jsn"}]})json");
    overview_body["records"].as_array().front()["signal"] = "positive";
    for (int i = 1; i < 17; ++i) {
        auto row = overview_body.at("records").as_array().front();
        row["factor_id"] = "zb" + std::to_string(i);
        row["source_rank"] = i + 1;
        row["signal"] = i < 2 ? "positive"
                         : i < 5 ? "neutral"
                         : i < 11 ? "negative"
                                  : "unrated";
        overview_body["records"].push_back(std::move(row));
    }
    require(tdx::evaluate_api_contract_response("overview-factors-live", 200, "application/json",
                                                overview_body.dump(-1))
                .at("passed")
                .as_bool(),
            "overview factors contract failed");
    overview_body["summary"]["negative"] = 5;
    require(!tdx::evaluate_api_contract_response("overview-factors-live", 200, "application/json",
                                                 overview_body.dump(-1))
                 .at("passed")
                 .as_bool(),
            "overview factors contract must enforce exact signal distribution");

    const auto benchmark_analysis = tdx::evaluate_api_contract_response(
        "benchmark-analysis-live", 200, "application/json",
        R"json({"schema":"tdx-market-benchmark-analysis-native-v1","view":"stocks","stage":"216","mode":"catalog","availability":"live","match_count":5297,"summary":{"stocks":5297,"stages":13},"records":[{"kind":"stocks","stage_id":"216","stage_start_date":"20240918","stage_open":true,"security_return_pct":53.46,"source_resource":"list/func_jzfx216_1.jsn","raw":{"zaf1":"53.46"}}],"sources":[{"resource":"list/func_jzfx216_1.jsn"}]})json");
    require(benchmark_analysis.at("passed").as_bool(), "benchmark analysis contract failed");

    const auto calendar_expanded = tdx::evaluate_api_contract_response(
        "calendar-expanded-live", 200, "application/json",
        R"json({"schema":"tdx-market-calendar-native-v1","view":"all","mode":"security","availability":"live","summary":{"macro":159,"meeting":8,"company":2363,"listing":167,"major-event":279,"ipo-announcement":100,"recent-ipo":146,"star-news":100,"chinext-news":100,"neeq-news":100},"rows":[{"kind":"major-event","security":{"market":"sh","code":"688783"},"related_securities":[{"market":"sh","code":"688783"}],"content":"重大事项正文","raw":{"$SC1":"1","$ZQDM1":"688783"}},{"kind":"recent-ipo","security":{"market":"sh","code":"688783"},"related_securities":[{"market":"sh","code":"688783"}],"raised_yuan":1234567890,"overfunding_yuan":-1000000,"snapshot_date":"20260806","raw":{"mjzj":"1234567890.0000"}},{"kind":"star-news","security":{"market":"sh","code":"688783"},"related_securities":[{"market":"sh","code":"688783"}],"content":"已经清洗的科创板资讯正文","source_resource":"list/func_xgrl103_1.jsn","raw":{}}],"sources":[{"resource":"list/func_cjrl101_1.jsn","endpoint":"local-jsn:a"},{"resource":"list/func_cjrl105_1.jsn","endpoint":"local-jsn:b"},{"resource":"list/func_ggrl101_1.jsn","endpoint":"local-jsn:c"},{"resource":"list/func_gsrl206_1.jsn","endpoint":"local-jsn:d"},{"resource":"list/func_dsjtx101_1.jsn","endpoint":"local-jsn:e"},{"resource":"list/func_xgrl101_1.jsn","endpoint":"local-jsn:f"},{"resource":"list/func_xgrl102_1.jsn","endpoint":"local-jsn:g"},{"resource":"list/func_xgrl103_1.jsn","endpoint":"local-jsn:h"},{"resource":"list/func_xgrl104_1.jsn","endpoint":"local-jsn:i"},{"resource":"list/func_xgrl105_1.jsn","endpoint":"local-jsn:j"}]})json");
    require(calendar_expanded.at("passed").as_bool(), "expanded calendar contract failed");

    const auto calendar_bad_html = tdx::evaluate_api_contract_response(
        "calendar-expanded-live", 200, "application/json",
        R"json({"schema":"tdx-market-calendar-native-v1","view":"all","mode":"security","availability":"live","summary":{"macro":159,"meeting":8,"company":2363,"listing":167,"major-event":279,"ipo-announcement":100,"recent-ipo":146,"star-news":100,"chinext-news":100,"neeq-news":100},"rows":[{"kind":"major-event","security":{"market":"sh","code":"688783"},"related_securities":[{"market":"sh","code":"688783"}],"content":"重大事项正文","raw":{"$SC1":"1","$ZQDM1":"688783"}},{"kind":"recent-ipo","security":{"market":"sh","code":"688783"},"related_securities":[{"market":"sh","code":"688783"}],"raised_yuan":1234567890,"overfunding_yuan":-1000000,"snapshot_date":"20260806","raw":{"mjzj":"1234567890.0000"}},{"kind":"star-news","security":{"market":"sh","code":"688783"},"related_securities":[{"market":"sh","code":"688783"}],"content":"<p>未清洗正文</p>","source_resource":"list/func_xgrl103_1.jsn","raw":{}}],"sources":[{"resource":"list/func_cjrl101_1.jsn","endpoint":"local-jsn:a"},{"resource":"list/func_cjrl105_1.jsn","endpoint":"local-jsn:b"},{"resource":"list/func_ggrl101_1.jsn","endpoint":"local-jsn:c"},{"resource":"list/func_gsrl206_1.jsn","endpoint":"local-jsn:d"},{"resource":"list/func_dsjtx101_1.jsn","endpoint":"local-jsn:e"},{"resource":"list/func_xgrl101_1.jsn","endpoint":"local-jsn:f"},{"resource":"list/func_xgrl102_1.jsn","endpoint":"local-jsn:g"},{"resource":"list/func_xgrl103_1.jsn","endpoint":"local-jsn:h"},{"resource":"list/func_xgrl104_1.jsn","endpoint":"local-jsn:i"},{"resource":"list/func_xgrl105_1.jsn","endpoint":"local-jsn:j"}]})json");
    require(!calendar_bad_html.at("passed").as_bool(),
            "calendar contract must reject unclean display HTML");

    const auto value_attention = tdx::evaluate_api_contract_response(
        "intelligence-value-attention-live", 200, "application/json",
        R"json({"schema":"tdx-market-intelligence-native-v1","view":"value-attention","selected_value_attention":{"category_id":"3109"},"value_attention_summary":{"categories":8,"relationships":1256,"unique_securities":1015},"records":[{"category_id":"3109","security":{"code":"000001"},"anchor_price_yuan":10.5,"adjusted_anchor_price_yuan":11.2,"three_month_adjusted_close_yuan":9.8,"breach_depth_pct":null,"raw":{"aqjg":"10.5"}}],"value_attention_reconciliation":{"inline_member_count":1,"dynamic_member_count":1,"counts_match":true,"exact_match":true,"inline_only":[],"dynamic_only":[]},"sources":[{"resource":"list/func_jzgz101_1.jsn"},{"resource":"jzgz1/3109.jsn"}]})json");
    require(value_attention.at("passed").as_bool(), "value-attention contract failed");

    const auto stake_building = tdx::evaluate_api_contract_response(
        "institution-stake-building-live", 200, "application/json",
        R"json({"schema":"tdx-institution-analysis-native-v1","view":"stake-building","sections":[{"layout":"stake-building","resource":"list/func_tzcg108_1.jsn","summary":{"further_increase_yes":1,"insurance_capital_yes":0},"records":[{"data":{"announcement_date":"20260731","shareholder":"示例股东","start_adjusted_close":10,"end_adjusted_close":12,"period_return_pct":20,"increase_shares":1000000,"post_holding_total_pct":5.25,"further_increase":"是","insurance_capital":"否"},"raw":{"price1":"10","price2":"12"}}]}],"sources":[{"resource":"list/func_tzcg108_1.jsn"}]})json");
    require(stake_building.at("passed").as_bool(), "stake-building contract failed");

    const auto policy_financial = tdx::evaluate_api_contract_response(
        "bond-reference-policy-financial-live", 200, "application/json",
        R"json({"schema":"tdx-market-bond-reference-native-v1","summary":{"source_group":"category","source_bucket":"policy-financial"},"match_count":1,"returned":1,"projection_reconciliation":{"master_count":1,"projection_union_count":1,"counts_match":true,"exact_match":true,"projections":[{"market":"sh","count":1},{"market":"sz","count":0}]},"records":[{"security":{"code":"018015"},"issue_size_source_100m":20,"issue_size_yuan":2000000000,"raw":{"GM":"20"}}],"sources":[{"resource":"list/zqjrz201.jsn"},{"resource":"list/zq_jrz201_1.jsn"},{"resource":"list/zq_jrz201_2.jsn"}]})json");
    require(policy_financial.at("passed").as_bool(), "policy-financial bond contract failed");

    const std::vector<std::tuple<std::string, std::string, std::string>> jsn_cases{
        {"stock-research-live", "tdx-market-research-native-v1", "security"},
        {"stock-consensus-live", "tdx-market-consensus-native-v1", "security"},
        {"stock-industry-profile-live", "tdx-industry-profile-native-v1", "security"},
        {"stock-ownership-live", "tdx-market-ownership-native-v1", "security"},
        {"stock-repurchases-live", "tdx-market-repurchases-native-v1", "security"},
        {"stock-institution-lhb-live", "tdx-market-institution-lhb-native-v1", "security"},
        {"stock-ratings-live", "tdx-market-ratings-native-v1", "selection"},
        {"stock-foreign-alerts-live", "tdx-market-foreign-alerts-native-v1", "security"},
        {"stock-unlocks-live", "tdx-market-unlocks-native-v1", "security"},
        {"stock-block-trades-live", "tdx-market-block-trades-native-v1", "security"},
        {"stock-lhb-live", "tdx-lhb-native-v1", "security"},
    };
    for (const auto &[id, schema, mode] : jsn_cases) {
        const auto body =
            std::string(R"({"schema":")") + schema + R"(","availability":"live","mode":")" + mode +
            R"(","sources":[{"resource":"list/test.jsn","endpoint":"test:7709","attempts":1,"stale":false,"age_seconds":0,"upstream_error":null}],"cache":{"stale":false,"upstream":{"stale":false,"max_attempts":1,"upstream_errors":[]}}})";
        const auto evaluated =
            tdx::evaluate_api_contract_response(id, 200, "application/json", body);
        require(evaluated.at("passed").as_bool(), "stock JSN resilience contract failed");
    }

    const auto malformed_jsn = tdx::evaluate_api_contract_response(
        "stock-research-live", 200, "application/json",
        R"({"schema":"tdx-market-research-native-v1","availability":"live","mode":"security","sources":[{"resource":"list/test.jsn"}],"cache":{"stale":false,"upstream":{"stale":false,"max_attempts":1,"upstream_errors":[]}}})");
    require(!malformed_jsn.at("passed").as_bool(),
            "stock JSN contract must reject incomplete source health metadata");

    const auto consensus_stage = tdx::evaluate_api_contract_response(
        "consensus-stage-rankings-live", 200, "application/json",
        R"({"schema":"tdx-market-consensus-native-v1","mode":"master","category":"year-low-rise","availability":"live","categories":[{},{},{},{},{},{},{},{},{}],"records":[{"year_low_price":"3.95","latest_close":"4.86","change_from_year_low_pct":"23.03797"}],"sources":[{"resource":"list/func_yzyq109_1.jsn"}]})");
    require(consensus_stage.at("passed").as_bool(),
            "consensus price-stage ranking contract failed");

    const auto consensus_stage_missing = tdx::evaluate_api_contract_response(
        "consensus-stage-rankings-live", 200, "application/json",
        R"({"schema":"tdx-market-consensus-native-v1","mode":"master","category":"year-low-rise","availability":"live","categories":[{},{},{},{},{},{},{},{},{}],"records":[{"year_low_price":"3.95","latest_close":"4.86","change_from_year_low_pct":null}],"sources":[{"resource":"list/func_yzyq109_1.jsn"}]})");
    require(!consensus_stage_missing.at("passed").as_bool(),
            "consensus stage contract must reject missing typed percentage");

    const auto roadshows = tdx::evaluate_api_contract_response(
        "stock-roadshows-live", 200, "application/json",
        R"({"schema":"tdx-roadshows-native-v1","availability":"live","mode":"security","security":{"code":"000001"},"counts":{"returned":5},"source":{"entry":"CWSearch.tzx_rcache","request_id":null,"key":"ly:0_000001","attempts":1},"cache":{"stale":false}})");
    require(roadshows.at("passed").as_bool(), "stock roadshows no-ReqId contract failed");

    const auto variants = tdx::evaluate_api_contract_response(
        "cloud-variants-fixed", 200, "application/json",
        R"({"schema":"tdx-cloud-variant-coverage-native-v1","summary":{"fully_fixed":true,"generic_only_variant_count":0,"parse_error_count":0,"tqlex_request_id_count":34,"fixed_tqlex_request_id_count":34,"pbrpc_request_id_count":29,"fixed_pbrpc_request_id_count":29}})");
    require(variants.at("passed").as_bool(), "cloud variant fixed-coverage contract failed");

    const auto variants_gap = tdx::evaluate_api_contract_response(
        "cloud-variants-fixed", 200, "application/json",
        R"({"schema":"tdx-cloud-variant-coverage-native-v1","summary":{"fully_fixed":false,"generic_only_variant_count":1,"parse_error_count":0,"tqlex_request_id_count":35,"fixed_tqlex_request_id_count":34,"pbrpc_request_id_count":29,"fixed_pbrpc_request_id_count":29}})");
    require(!variants_gap.at("passed").as_bool(),
            "new generic-only cloud variant must fail the contract");

    const auto homepage = tdx::evaluate_api_contract_response(
        "homepage", 200, "text/html; charset=utf-8",
        "<!doctype html><html><body><div id=\"app\"></div></body></html>");
    require(homepage.at("passed").as_bool(), "homepage contract failed");

    const auto formula_route = tdx::evaluate_api_contract_response(
        "formula-workbench-route", 200, "text/html; charset=utf-8",
        "<!doctype html><html><head><script type=\"module\" "
        "src=\"/assets/index.js\"></script><link rel=\"stylesheet\" "
        "href=\"/assets/index.css\"></head><body><div id=\"app\"></div></body></html>");
    require(formula_route.at("passed").as_bool(),
            "formula workbench nested-route asset contract failed");

    const auto formula_route_relative = tdx::evaluate_api_contract_response(
        "formula-workbench-route", 200, "text/html; charset=utf-8",
        "<!doctype html><html><head><script type=\"module\" "
        "src=\"./assets/index.js\"></script><link rel=\"stylesheet\" "
        "href=\"./assets/index.css\"></head><body><div id=\"app\"></div></body></html>");
    require(!formula_route_relative.at("passed").as_bool(),
            "nested-route contract must reject route-relative assets");
}

} // namespace recon_contract_test
