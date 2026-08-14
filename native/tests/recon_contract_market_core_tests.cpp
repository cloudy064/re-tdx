#include "recon_contract_test_support.hpp"

namespace recon_contract_test {

void run_market_core_contracts() {
    const auto historical = tdx::evaluate_api_contract_response(
        "historical-securities-local", 200, "application/json",
        R"({"schema":"tdx-market-historical-securities-native-v1","availability":"local","filters":{"presence":"absent"},"records":[{"security":{"code":"000003"},"compatibility_name":"深金田A","current_directory_present":false}],"sources":[{"file":"pttab.dat"}],"transport":{"kind":"local-files","network_requests":0}})");
    require(historical.at("passed").as_bool(),
            "historical-securities local contract failed");

    const auto historical_network = tdx::evaluate_api_contract_response(
        "historical-securities-local", 200, "application/json",
        R"({"schema":"tdx-market-historical-securities-native-v1","availability":"local","filters":{"presence":"absent"},"records":[{"security":{"code":"000003"},"compatibility_name":"深金田A","current_directory_present":false}],"sources":[{"file":"pttab.dat"}],"transport":{"kind":"http","network_requests":1}})");
    require(!historical_network.at("passed").as_bool(),
            "historical-securities contract must reject network transport");

    const auto active_empty = tdx::evaluate_api_contract_response(
        "empty-active-funds", 200, "application/json",
        R"({"selected_holding":null,"counts":{"funds":0,"securities":0},"securities":[]})");
    require(active_empty.at("passed").as_bool(), "active-fund empty contract failed");

    const auto former_error = tdx::evaluate_api_contract_response(
        "empty-active-funds", 400, "application/json",
        R"({"error":"bad_request","message":"selected security is absent"})");
    require(!former_error.at("passed").as_bool(),
            "former missing-relation error must fail the empty contract");

    const auto invalid = tdx::evaluate_api_contract_response(
        "invalid-market", 400, "application/json",
        R"({"error":"bad_request","message":"market must be sz, sh, or bj"})");
    require(invalid.at("passed").as_bool(), "invalid argument contract failed");

    tdx::Json context = tdx::Json::object();
    context["requested_report_date"] = "2025-12-31";
    const auto explicit_date = tdx::evaluate_api_contract_response(
        "report-cache-explicit", 200, "application/json",
        R"({"parameters":{"requested_report_date":"2025-12-31","resolved_report_date":"2025-12-31","latest_report_fallback_used":false}})",
        context);
    require(explicit_date.at("passed").as_bool(), "explicit report-date isolation contract failed");

    const auto polluted = tdx::evaluate_api_contract_response(
        "report-cache-explicit", 200, "application/json",
        R"({"parameters":{"requested_report_date":"2025-12-31","resolved_report_date":"2025-06-30","latest_report_fallback_used":true}})",
        context);
    require(!polluted.at("passed").as_bool(), "fallback-polluted explicit report date must fail");

    const auto funds_live = tdx::evaluate_api_contract_response(
        "intraday-funds-live", 200, "application/json",
        R"({"availability":"live","mode":"security","security":{"code":"000001"},"found":true,"cache":{"stale":false},"source":{"master_request_id":"200340","detail_request_id":"200341"}})");
    require(funds_live.at("passed").as_bool(), "intraday-funds live contract failed");

    const auto funds_stale = tdx::evaluate_api_contract_response(
        "intraday-funds-live", 200, "application/json",
        R"({"availability":"stale-cache","mode":"security","security":{"code":"000001"},"found":true,"cache":{"stale":true},"source":{"master_request_id":"200340","detail_request_id":"200341"}})");
    require(funds_stale.at("passed").as_bool(), "intraday-funds stale-cache contract failed");

    const auto funds_bad = tdx::evaluate_api_contract_response(
        "intraday-funds-live", 400, "application/json",
        R"({"error":"bad_request","message":"PBRPC server rejected request with RpcID -1"})");
    require(!funds_bad.at("passed").as_bool(),
            "former intraday-funds upstream error must fail the contract");

    const auto panorama = tdx::evaluate_api_contract_response(
        "stock-panorama-live", 200, "application/json",
        R"({"schema":"tdx-market-panorama-native-v1","view":"capital-flow","market":"sz","code":"000001","counts":{"sections":1,"matched":1,"returned":1},"sections":[{"resource":"list/func_gx_zjlx101_1.jsn","records":[{"code":"000001","data":{"statistics_date":"20260806"},"raw":{"$SC":"0","$ZQDM":"000001"}}]}],"sources":[{"resource":"list/func_gx_zjlx101_1.jsn","endpoint":"test:7709","attempts":1,"stale":false,"upstream_error":null}],"upstream_health":{"stale":false,"max_attempts":1}})");
    require(panorama.at("passed").as_bool(), "stock panorama contract failed");

    const auto panorama_missing_raw = tdx::evaluate_api_contract_response(
        "stock-panorama-live", 200, "application/json",
        R"({"schema":"tdx-market-panorama-native-v1","view":"capital-flow","market":"sz","code":"000001","counts":{"sections":1,"matched":1},"sections":[{"resource":"list/func_gx_zjlx101_1.jsn","records":[{"code":"000001","data":{"statistics_date":"20260806"}}]}],"sources":[{"resource":"list/func_gx_zjlx101_1.jsn","endpoint":"test:7709","attempts":1,"stale":false,"upstream_error":null}],"upstream_health":{"stale":false,"max_attempts":1}})");
    require(!panorama_missing_raw.at("passed").as_bool(),
            "stock panorama contract must require the auditable raw row");

    const auto institution_analysis = tdx::evaluate_api_contract_response(
        "stock-institution-analysis-live", 200, "application/json",
        R"({"schema":"tdx-institution-analysis-native-v1","view":"all","market":"sz","code":"000001","counts":{"sections":1,"matched":1,"returned":1},"sections":[{"resource":"list/func_cgfx101_1.jsn","records":[{"code":"000001","data":{"report_date":"20260331","institution_count":95},"raw":{"$SC":"0","$ZQDM":"000001"}}]}],"sources":[{"resource":"list/func_cgfx101_1.jsn","endpoint":"test:7709","attempts":1,"stale":false,"upstream_error":null}],"upstream_health":{"stale":false,"max_attempts":1}})");
    require(institution_analysis.at("passed").as_bool(),
            "stock institution-analysis contract failed");

    const auto institution_analysis_missing_raw = tdx::evaluate_api_contract_response(
        "stock-institution-analysis-live", 200, "application/json",
        R"({"schema":"tdx-institution-analysis-native-v1","view":"all","market":"sz","code":"000001","counts":{"sections":1,"matched":1},"sections":[{"resource":"list/func_cgfx101_1.jsn","records":[{"code":"000001","data":{"report_date":"20260331","institution_count":95}}]}],"sources":[{"resource":"list/func_cgfx101_1.jsn","endpoint":"test:7709","attempts":1,"stale":false,"upstream_error":null}],"upstream_health":{"stale":false,"max_attempts":1}})");
    require(!institution_analysis_missing_raw.at("passed").as_bool(),
            "institution-analysis contract must require the auditable raw row");

    const auto exclusive_funds = tdx::evaluate_api_contract_response(
        "institution-exclusive-funds-live", 200, "application/json",
        R"({"schema":"tdx-institution-analysis-native-v1","view":"exclusive-funds","counts":{"matched":188},"sections":[{"layout":"exclusive-funds","source_rows":188,"summary":{"rows":188,"unique_securities":100,"names_resolved":188,"fund_management_companies":71,"duplicate_company_security_rows":88},"records":[{"name_resolved":true,"data":{"float_share_pct":10.67,"fund_management_company":"新华基金","holding_shares_10k":806.83,"holding_shares":8068300},"raw":{"$SC":"0","$ZQDM":"301251"}},{"name_resolved":true,"data":{"float_share_pct":7.79},"raw":{}}]}],"sources":[{"resource":"list/func_tbgz108_1.jsn","endpoint":"test:7709","attempts":1,"stale":false}],"upstream_health":{"stale":false,"max_attempts":1}})");
    require(exclusive_funds.at("passed").as_bool(), "exclusive-funds contract failed");

    const auto national_team = tdx::evaluate_api_contract_response(
        "institution-national-team-live", 200, "application/json",
        R"({"schema":"tdx-institution-analysis-native-v1","view":"national-team","counts":{"matched":191},"sections":[{"layout":"national-team","source_rows":191,"summary":{"rows":191,"unique_securities":191,"names_resolved":191,"combined_formula_checked":191,"combined_formula_mismatches":0},"records":[{"name_resolved":true,"data":{"combined_ratio_pct":61.61,"combined_ratio_formula_matches":true},"raw":{"$SC":"1","$ZQDM":"601988"}},{"name_resolved":true,"data":{"combined_ratio_pct":55.64,"combined_ratio_formula_matches":true},"raw":{}}]}],"sources":[{"resource":"list/func_tzcg104_1.jsn","endpoint":"test:7709","attempts":1,"stale":false}],"upstream_health":{"stale":false,"max_attempts":1}})");
    require(national_team.at("passed").as_bool(), "national-team contract failed");

    const auto national_team_bad_formula = tdx::evaluate_api_contract_response(
        "institution-national-team-live", 200, "application/json",
        R"({"schema":"tdx-institution-analysis-native-v1","view":"national-team","counts":{"matched":191},"sections":[{"layout":"national-team","source_rows":191,"summary":{"rows":191,"unique_securities":191,"names_resolved":191,"combined_formula_checked":191,"combined_formula_mismatches":1},"records":[{"name_resolved":true,"data":{"combined_ratio_pct":61.61,"combined_ratio_formula_matches":true},"raw":{}},{"name_resolved":true,"data":{"combined_ratio_pct":55.64,"combined_ratio_formula_matches":true},"raw":{}}]}],"sources":[{"resource":"list/func_tzcg104_1.jsn","endpoint":"test:7709","attempts":1,"stale":false}],"upstream_health":{"stale":false,"max_attempts":1}})");
    require(!national_team_bad_formula.at("passed").as_bool(),
            "national-team contract must reject formula mismatches");

    const auto notable_private_funds = tdx::evaluate_api_contract_response(
        "institution-notable-private-funds-live", 200, "application/json",
        R"({"schema":"tdx-institution-analysis-native-v1","view":"notable-private-funds","counts":{"matched":70},"sections":[{"layout":"notable-private-funds","source_rows":70,"summary":{"rows":70,"unique_securities":68,"names_resolved":70,"private_fund_managers":16,"industries":37,"holding_market_value":23605585400,"duplicate_manager_security_rows":2},"records":[{"name_resolved":true,"data":{"private_fund_manager":"景林资产","report_date":"20260331","holding_market_value_10k":88000,"holding_market_value":880000000,"float_share_pct":4.2,"industry":"银行"},"raw":{"$SC":"0","$ZQDM":"000001"}},{"name_resolved":true,"data":{"private_fund_manager":"淡水泉","report_date":"20260331","holding_market_value_10k":65000,"holding_market_value":650000000,"float_share_pct":3.1,"industry":"医药"},"raw":{}}]}],"sources":[{"resource":"list/func_jgcg108_1.jsn","endpoint":"test:7709","attempts":1,"stale":false}],"upstream_health":{"stale":false,"max_attempts":1}})");
    require(notable_private_funds.at("passed").as_bool(), "notable private-funds contract failed");

    const auto notable_private_funds_bad_units = tdx::evaluate_api_contract_response(
        "institution-notable-private-funds-live", 200, "application/json",
        R"({"schema":"tdx-institution-analysis-native-v1","view":"notable-private-funds","counts":{"matched":70},"sections":[{"layout":"notable-private-funds","summary":{"rows":70,"unique_securities":68,"private_fund_managers":16,"industries":37,"holding_market_value":23605585400},"records":[{"name_resolved":true,"data":{"private_fund_manager":"景林资产","report_date":"20260331","holding_market_value_10k":88000,"holding_market_value":88000},"raw":{}},{"name_resolved":true,"data":{"holding_market_value":650000000},"raw":{}}]}],"sources":[{"resource":"list/func_jgcg108_1.jsn","endpoint":"test:7709","attempts":1,"stale":false}]})");
    require(!notable_private_funds_bad_units.at("passed").as_bool(),
            "notable private-funds contract must reject a broken 10k-yuan conversion");

    const auto stock_connect_activity = tdx::evaluate_api_contract_response(
        "stock-connect-activity-live", 200, "application/json",
        R"({"schema":"tdx-market-stock-connect-native-v1","view":"activity","category":"daily-increase","availability":"live","count":1,"records":[{"date":"20240816","snapshot_freshness":"historical-snapshot","raw":{"$SC":"0","$ZQDM":"000001"}}],"sources":[{"resource":"list/func_hsgt205_1.jsn","endpoint":"test:7709","attempts":1,"stale":false,"upstream_error":null}]})");
    require(stock_connect_activity.at("passed").as_bool(),
            "stock-connect activity contract failed");

    const auto stock_connect_empty = tdx::evaluate_api_contract_response(
        "empty-stock-connect-industry", 200, "application/json",
        R"({"schema":"tdx-market-stock-connect-native-v1","view":"industry","category":"northbound-industry","availability":"empty","count":0,"records":[],"sources":[{"resource":"list/func_hsgt107_1.jsn","size":0,"missing":true}]})");
    require(stock_connect_empty.at("passed").as_bool(),
            "stock-connect configured empty contract failed");

    const auto stock_connect_empty_error = tdx::evaluate_api_contract_response(
        "empty-stock-connect-industry", 503, "application/json",
        R"({"error":"upstream_unavailable","message":"server reports a zero-length JSN resource"})");
    require(!stock_connect_empty_error.at("passed").as_bool(),
            "configured zero-length stock-connect table must not become an HTTP error");

    const auto intelligence_topics = tdx::evaluate_api_contract_response(
        "intelligence-topics-live", 200, "application/json",
        R"({"schema":"tdx-market-intelligence-native-v1","view":"topics","availability":"live","records":[{"topic_id":"1039","name":"半年报","updated_date":"20260730","raw":{"$ZQDM":"1039"}}],"sources":[{"resource":"list/func_ztxx101_1.jsn"}]})");
    require(intelligence_topics.at("passed").as_bool(), "intelligence topics contract failed");

    const auto intelligence_topic = tdx::evaluate_api_contract_response(
        "intelligence-topic-detail-live", 200, "application/json",
        R"({"schema":"tdx-market-intelligence-native-v1","view":"topic","availability":"live","selected_topic":{"topic_id":"1039"},"records":[{"date":"20260730","headline":"券商中期业绩","content":"纯文本正文\n查看原文","source_url":"https://example.com","raw":{"title":"标题TXT:<p>正文</p>"}}],"sources":[{"resource":"list/func_ztxx101_1.jsn"},{"resource":"ztxx/1039.jsn"}]})");
    require(intelligence_topic.at("passed").as_bool(), "intelligence topic detail contract failed");

    const auto intelligence_topic_unsafe = tdx::evaluate_api_contract_response(
        "intelligence-topic-detail-live", 200, "application/json",
        R"({"schema":"tdx-market-intelligence-native-v1","view":"topic","availability":"live","selected_topic":{"topic_id":"1039"},"records":[{"date":"20260730","headline":"标题","content":"<p>未清洗正文</p>","raw":{}}],"sources":[{"resource":"list/func_ztxx101_1.jsn"},{"resource":"ztxx/1039.jsn"}]})");
    require(!intelligence_topic_unsafe.at("passed").as_bool(),
            "topic contract must reject unclean HTML content");

    const auto intelligence_news = tdx::evaluate_api_contract_response(
        "intelligence-news-live", 200, "application/json",
        R"({"schema":"tdx-market-intelligence-native-v1","view":"news","availability":"live","records":[{"date":"20260806","headline":"新闻联播要闻","content":"今日主要内容","source_url":null,"raw":{}}],"sources":[{"resource":"list/func_xwlb101_1.jsn"}]})");
    require(intelligence_news.at("passed").as_bool(), "intelligence news contract failed");

    const auto market_anomalies = tdx::evaluate_api_contract_response(
        "market-anomalies-live", 200, "application/json",
        R"({"schema":"tdx-market-intelligence-native-v1","view":"market-anomalies","availability":"live","records":[{"date":"20260114","previous_close":100,"close":102,"change_points":2,"same_day_change_pct":2,"sh_turnover_yuan":400,"sz_turnover_yuan":600,"market_turnover_yuan":1000,"raw":{}}],"sources":[{"resource":"list/func_dpyd101_1.jsn"}]})");
    require(market_anomalies.at("passed").as_bool(), "market anomalies formula contract failed");

    const auto market_anomalies_bad = tdx::evaluate_api_contract_response(
        "market-anomalies-live", 200, "application/json",
        R"({"schema":"tdx-market-intelligence-native-v1","view":"market-anomalies","availability":"live","records":[{"date":"20260114","previous_close":100,"close":102,"change_points":2,"same_day_change_pct":0.02,"sh_turnover_yuan":400,"sz_turnover_yuan":600,"market_turnover_yuan":1000,"raw":{}}],"sources":[{"resource":"list/func_dpyd101_1.jsn"}]})");
    require(!market_anomalies_bad.at("passed").as_bool(),
            "market anomaly contract must reject ratio/percent confusion");

    const auto intelligence_event = tdx::evaluate_api_contract_response(
        "intelligence-event-detail-live", 200, "application/json",
        R"({"schema":"tdx-market-intelligence-native-v1","view":"event","availability":"live","event_reconciliation":{"exact_match":true},"records":[{"member_source":"dynamic-detail","members":[{"code":"300223"}],"raw":{}}],"sources":[{"resource":"list/func_sjqd101_1.jsn"},{"resource":"sjqd/20115.jsn"}]})");
    require(intelligence_event.at("passed").as_bool(), "intelligence event detail contract failed");

    const auto intelligence_discredited = tdx::evaluate_api_contract_response(
        "intelligence-discredited-live", 200, "application/json", [&]() {
            tdx::Json body = tdx::Json::object();
            body["schema"] = "tdx-market-intelligence-native-v1";
            body["view"] = "risks";
            body["category"] = "discredited";
            body["records"] = tdx::Json::array();
            for (int index = 0; index < 9; ++index) {
                tdx::Json row = tdx::Json::object();
                row["security"] = tdx::Json::object();
                row["security"]["code"] = "000545";
                row["category"] = "discredited";
                row["category_label"] = "失信被执行";
                row["announcement_date"] = "2026042" + std::to_string(9 - index);
                row["involved_subject"] = "郭金东";
                row["object_type"] = "实际控制人";
                row["occurrences_past_year"] = 1;
                row["safety_score"] = nullptr;
                row["raw"] = tdx::Json::object();
                body["records"].push_back(std::move(row));
            }
            body["sources"] = tdx::Json::array();
            tdx::Json source = tdx::Json::object();
            source["resource"] = "list/func_sxbzx101_1.jsn";
            body["sources"].push_back(std::move(source));
            return body.dump(-1);
        }());
    require(intelligence_discredited.at("passed").as_bool(),
            "intelligence discredited-subject contract failed");

    const auto margin_classifications = tdx::evaluate_api_contract_response(
        "margin-classifications-live", 200, "application/json",
        R"({"schema":"tdx-market-margin-native-v1","view":"classifications","availability":"live","records":[{"classification":"industry","classification_code":"880491","classification_name":"半导体","date":"20260805","raw":{}}],"sources":[{"resource":"list/func_rzrq101_1.jsn"},{"resource":"rzrq6/20260805.jsn"}]})");
    require(margin_classifications.at("passed").as_bool(),
            "margin classifications contract failed");

    const auto margin_history = tdx::evaluate_api_contract_response(
        "margin-classification-history-live", 200, "application/json",
        R"({"schema":"tdx-market-margin-native-v1","view":"classification-history","availability":"live","group_id":"880868","records":[{"date":"20260805","financing_balance_yuan":146200000000,"short_balance_yuan":730000000,"raw":{"hyrzye":"1462","hyrqye":"73"}}],"sources":[{"resource":"rzrq5/880868.jsn"}]})");
    require(margin_history.at("passed").as_bool(), "margin classification history contract failed");

    const auto margin_history_bad_units = tdx::evaluate_api_contract_response(
        "margin-classification-history-live", 200, "application/json",
        R"({"schema":"tdx-market-margin-native-v1","view":"classification-history","availability":"live","group_id":"880868","records":[{"date":"20260805","financing_balance_yuan":1462,"short_balance_yuan":73,"raw":{"hyrzye":"1462","hyrqye":"73"}}],"sources":[{"resource":"rzrq5/880868.jsn"}]})");
    require(!margin_history_bad_units.at("passed").as_bool(),
            "margin history contract must reject broken source-unit conversion");

    const auto margin_etf = tdx::evaluate_api_contract_response(
        "margin-etf-history-live", 200, "application/json",
        R"({"schema":"tdx-market-margin-native-v1","view":"security","availability":"live","records":[{"date":"20260805","raw":{}}],"trend":[{"date":"20260805"}],"sources":[{"resource":"rzrq3/1510900.jsn"},{"resource":"rzrq4/1510900.jsn"}]})");
    require(margin_etf.at("passed").as_bool(), "margin ETF history contract failed");

    const auto margin_transfer = tdx::evaluate_api_contract_response(
        "margin-transfer-live", 200, "application/json",
        R"({"schema":"tdx-market-margin-native-v1","view":"transfer","availability":"live","records":[{"date":"20260807","transfer_financing_repaid_yuan":1400000000,"transfer_financing_balance_yuan":147580000000,"raw":{"zrz2":"1400000000","zrz7":"147580000000"}}],"sources":[{"resource":"list/func_rzt101_1.jsn"}]})");
    require(margin_transfer.at("passed").as_bool(), "margin transfer-financing contract failed");

    const auto margin_transfer_bad_units = tdx::evaluate_api_contract_response(
        "margin-transfer-live", 200, "application/json",
        R"({"schema":"tdx-market-margin-native-v1","view":"transfer","availability":"live","records":[{"date":"20260807","transfer_financing_repaid_yuan":14,"transfer_financing_balance_yuan":1475.8,"raw":{"zrz2":"1400000000","zrz7":"147580000000"}}],"sources":[{"resource":"list/func_rzt101_1.jsn"}]})");
    require(!margin_transfer_bad_units.at("passed").as_bool(),
            "margin transfer contract must reject invented unit conversion");

    const auto stock_connect_chart = tdx::evaluate_api_contract_response(
        "stock-connect-chart-live", 200, "application/json",
        R"({"schema":"tdx-market-stock-connect-native-v1","view":"security","records":[{"date":"20260630"}],"trend":[{"date":"20260630"}],"reconciliation":{"all_matched":true},"sources":[{"resource":"hsgtcg1/jd000001.jsn"},{"resource":"hsgtcg2/jd000001.jsn"}]})");
    require(stock_connect_chart.at("passed").as_bool(),
            "stock-connect chart reconciliation contract failed");

    const auto stock_connect_industry = tdx::evaluate_api_contract_response(
        "stock-connect-industry-detail-live", 200, "application/json",
        R"({"schema":"tdx-market-stock-connect-native-v1","view":"industry-detail","group_id":"70HK0201","records":[{"security":{"code":"02007"},"raw":{}}],"trend":[{"date":"20260805","daily_net_inflow_yuan":35000000,"raw":{"drjlr":"0.35"}}],"sources":[{"resource":"ggthy/70HK0201.jsn"},{"resource":"ggthy1/70HK0201.jsn"}]})");
    require(stock_connect_industry.at("passed").as_bool(),
            "stock-connect industry detail contract failed");

    const auto ownership_change_chart = tdx::evaluate_api_contract_response(
        "ownership-change-chart-live", 200, "application/json",
        R"({"schema":"tdx-market-ownership-native-v1","view":"statistics","change_count_trend":[{"period":"2026-08","increase_companies":16,"decrease_companies":48,"raw":{}}],"change_count_reconciliation":{"chart_rows":1,"matched_rows":0,"mismatch_rows":1,"all_overlaps_match":false},"sources":[{"resource":"gdzjc1/2026-08.jsn"}]})");
    require(ownership_change_chart.at("passed").as_bool(),
            "ownership change-count chart contract failed");

    const auto ownership_rankings = tdx::evaluate_api_contract_response(
        "ownership-rankings-live", 200, "application/json",
        R"({"schema":"tdx-market-ownership-native-v1","view":"rankings","category":"decrease-ratio","availability":"live","summary":{"ranking_views":6,"ranking_rows":600},"rankings":[{"direction":"decrease","signed_float_change_pct":-22.44,"raw":{"$SC":"0","$ZQDM":"301029"}}],"sources":[{"resource":"list/func_zcjc108_1.jsn","attempts":1,"stale":false,"upstream_error":null}]})");
    require(ownership_rankings.at("passed").as_bool(),
            "ownership aggregate ranking contract failed");

    const auto ownership_rankings_unsigned = tdx::evaluate_api_contract_response(
        "ownership-rankings-live", 200, "application/json",
        R"({"schema":"tdx-market-ownership-native-v1","view":"rankings","category":"decrease-ratio","availability":"live","summary":{"ranking_views":6,"ranking_rows":600},"rankings":[{"direction":"decrease","signed_float_change_pct":22.44,"raw":{"$SC":"0","$ZQDM":"301029"}}],"sources":[{"resource":"list/func_zcjc108_1.jsn","attempts":1,"stale":false,"upstream_error":null}]})");
    require(!ownership_rankings_unsigned.at("passed").as_bool(),
            "decrease ranking must retain a negative signed float change");

    const auto shareholder_counts = tdx::evaluate_api_contract_response(
        "shareholder-counts-live", 200, "application/json",
        R"({"schema":"tdx-market-ownership-native-v1","view":"shareholder-counts","category":"bj","availability":"live","summary":{"shareholder_rows":5417},"shareholder_counts":[{"board":"bj","security":{"code":"920001"},"daily_household_change_pct":-0.2,"raw":{"$SC":"2","$ZQDM":"920001"}}],"sources":[{"resource":"list/func_gdrs107_1.jsn","attempts":1,"stale":false}]})");
    require(shareholder_counts.at("passed").as_bool(), "shareholder-count market contract failed");

    const auto shareholder_legacy = tdx::evaluate_api_contract_response(
        "empty-shareholder-counts-legacy", 200, "application/json",
        R"({"schema":"tdx-market-ownership-native-v1","view":"shareholder-counts","category":"sz-sme-legacy","availability":"empty","shareholder_counts":[],"sources":[{"resource":"list/func_gdrs103_1.jsn","missing":true,"size":0}]})");
    require(shareholder_legacy.at("passed").as_bool(),
            "legacy SME shareholder-count empty contract failed");

    const auto shareholder_legacy_error = tdx::evaluate_api_contract_response(
        "empty-shareholder-counts-legacy", 503, "application/json",
        R"({"error":"upstream_unavailable","message":"zero-length JSN resource"})");
    require(!shareholder_legacy_error.at("passed").as_bool(),
            "legacy SME empty relation must not become an HTTP error");

    const auto limit_review_current = tdx::evaluate_api_contract_response(
        "limit-review-current-live", 200, "application/json",
        R"({"schema":"tdx-market-limit-review-native-v1","view":"current","availability":"live","counts":{"source_rows":177},"records":[{"category":"limit-up","raw":{"$SC":"0","$ZQDM":"000692"}}],"sources":[{"resource":"list/func_zdtfx101_1.jsn"}]})");
    require(limit_review_current.at("passed").as_bool(), "current limit-review contract failed");

    const auto limit_review_history = tdx::evaluate_api_contract_response(
        "limit-review-history-live", 200, "application/json",
        R"({"schema":"tdx-market-limit-review-native-v1","view":"history","availability":"live","counts":{"source_rows":485},"records":[{"date":"20260806","limit_up":{"all_count":106},"raw":{"$ZQDM":"20260806"}}],"sources":[{"resource":"list/func_zdtfx107_1.jsn"}]})");
    require(limit_review_history.at("passed").as_bool(),
            "market-history limit-review contract failed");

    const auto limit_review_daily = tdx::evaluate_api_contract_response(
        "limit-review-daily-live", 200, "application/json",
        R"({"schema":"tdx-market-limit-review-native-v1","view":"daily","date":"20260731","availability":"live","counts":{"source_rows":244},"records":[{"date":"20260731","raw":{"$SC":"0","$ZQDM":"000001"}}],"sources":[{"resource":"zdtfx2/20260731.jsn"},{"resource":"zdtfx3/20260731.jsn"}]})");
    require(limit_review_daily.at("passed").as_bool(), "daily limit-review contract failed");

    const auto limit_review_security = tdx::evaluate_api_contract_response(
        "limit-review-security-live", 200, "application/json",
        R"({"schema":"tdx-market-limit-review-native-v1","view":"security","code":"000009","availability":"live","history":[{"date":"20250806","raw":{"date":"20250806"}}],"sources":[{"resource":"zdtfx1/0000009.jsn"}]})");
    require(limit_review_security.at("passed").as_bool(),
            "security-history limit-review contract failed");

    const auto limit_review_missing_raw = tdx::evaluate_api_contract_response(
        "limit-review-current-live", 200, "application/json",
        R"({"schema":"tdx-market-limit-review-native-v1","view":"current","availability":"live","counts":{"source_rows":1},"records":[{"category":"limit-up"}],"sources":[{"resource":"list/func_zdtfx101_1.jsn"}]})");
    require(!limit_review_missing_raw.at("passed").as_bool(),
            "limit-review contract must require auditable raw rows");

    const auto session_turnover_a = tdx::evaluate_api_contract_response(
        "session-turnover-a-live", 200, "application/json",
        R"({"schema":"tdx-market-session-turnover-native-v1","universe":"a","availability":"live","statistics_date":"20260806","counts":{"source_rows":5505},"records":[{"after_hours_turnover_yuan":100,"raw":{"$SC":"1","$ZQDM":"688825"}},{"after_hours_turnover_yuan":50,"raw":{"$SC":"0","$ZQDM":"002272"}}],"source":{"resource":"list/func_phcje101_1.jsn","attempts":1,"stale":false}})");
    require(session_turnover_a.at("passed").as_bool(), "A-share session-turnover contract failed");

    const auto session_turnover_etf = tdx::evaluate_api_contract_response(
        "session-turnover-etf-live", 200, "application/json",
        R"({"schema":"tdx-market-session-turnover-native-v1","universe":"etf","availability":"live","statistics_date":"20260806","counts":{"source_rows":1619},"records":[{"opening_turnover_yuan":200,"raw":{"$SC":"1","$ZQDM":"511990"}},{"opening_turnover_yuan":100,"raw":{"$SC":"1","$ZQDM":"511360"}}],"source":{"resource":"list/func_phcje104_1.jsn","attempts":1,"stale":false}})");
    require(session_turnover_etf.at("passed").as_bool(), "ETF session-turnover contract failed");

    const auto session_turnover_unsorted = tdx::evaluate_api_contract_response(
        "session-turnover-a-live", 200, "application/json",
        R"({"schema":"tdx-market-session-turnover-native-v1","universe":"a","availability":"live","statistics_date":"20260806","counts":{"source_rows":5505},"records":[{"after_hours_turnover_yuan":50,"raw":{}},{"after_hours_turnover_yuan":100,"raw":{}}],"source":{"resource":"list/func_phcje101_1.jsn","attempts":1,"stale":false}})");
    require(!session_turnover_unsorted.at("passed").as_bool(),
            "session-turnover contract must reject a broken primary sort");

    const auto pending_bonds = tdx::evaluate_api_contract_response(
        "pending-convertible-bonds-live", 200, "application/json",
        R"({"schema":"tdx-market-convertible-bonds-native-v1","view":"pending","availability":"live","summary":{"pending_issues":157},"pending_issues":[{"underlying":{"market":"sz","code":"301565","name":"中仑新材"},"raw":{"$SC":"0","$ZQDM":"301565"}}],"projection_reconciliation":[{"primary_resource":"list/dfkzz201_1.jsn","projection_resource":"list/func_kzz102_1.jsn","primary_rows":157,"projection_rows":138,"common_securities":137,"primary_only_security_ids":["SZ301565"],"projection_only_security_ids":["SZ300495"]},{"primary_resource":"list/dfkzz201_1.jsn","projection_resource":"list/gxjty_zq_dfkzz102_1.jsn","primary_rows":157,"projection_rows":156,"common_securities":155,"primary_only_security_ids":["SZ301565"],"projection_only_security_ids":["SZ300495"]}],"sources":[{"resource":"list/dfkzz201_1.jsn","attempts":1,"stale":false},{"resource":"list/func_kzz102_1.jsn"},{"resource":"list/gxjty_zq_dfkzz102_1.jsn"}]})");
    require(pending_bonds.at("passed").as_bool(), "pending convertible-bonds contract failed");

    const auto pending_bonds_missing_raw = tdx::evaluate_api_contract_response(
        "pending-convertible-bonds-live", 200, "application/json",
        R"({"schema":"tdx-market-convertible-bonds-native-v1","view":"pending","availability":"live","summary":{"pending_issues":157},"pending_issues":[{"underlying":{"market":"sz","code":"301565"}}],"sources":[{"resource":"list/dfkzz201_1.jsn","attempts":1,"stale":false}]})");
    require(!pending_bonds_missing_raw.at("passed").as_bool(),
            "pending convertible-bonds contract must require auditable raw rows");

    const auto bond_subscriptions = tdx::evaluate_api_contract_response(
        "convertible-bond-subscriptions-live", 200, "application/json", [&]() {
            tdx::Json body = tdx::Json::object();
            body["schema"] = "tdx-market-convertible-bonds-native-v1";
            body["view"] = "subscriptions";
            body["availability"] = "live";
            body["match_count"] = 323;
            body["summary"] = tdx::Json::object();
            body["summary"]["subscriptions"] = 323;
            body["summary"]["listed"] = 310;
            body["summary"]["not_listed"] = 13;
            body["summary"]["formula_complete"] = 323;
            body["summary"]["issue_size_100m_yuan"] = 5457.4;
            body["summary"]["new_bond_projection_rows"] = 20;
            body["subscriptions"] = tdx::Json::array();
            for (int index = 0; index < 323; ++index) {
                const double stock_close = 19.62 + index * 0.001;
                const double conversion_price = 20.28;
                const double conversion_value = stock_close * 100.0 / conversion_price;
                const double premium = (100.0 - conversion_value) * 100.0 / conversion_value;
                tdx::Json row = tdx::Json::object();
                row["kind"] = "convertible-bond-subscription";
                row["bond"] = tdx::Json::object();
                row["bond"]["code"] = "123281";
                row["underlying"] = tdx::Json::object();
                row["underlying"]["code"] = "301565";
                row["subscription_date"] = "20260806";
                row["subscription_code"] = "371565";
                row["conversion_start_date"] = "20270212";
                row["underlying_close_yuan"] = stock_close;
                row["conversion_price_yuan"] = conversion_price;
                row["conversion_value_yuan"] = conversion_value;
                row["bond_close_yuan"] = 100.0;
                row["conversion_premium_pct"] = premium;
                row["issue_size_100m_yuan"] = 10.68;
                row["lottery_date"] = "20260810";
                row["lottery_rate_pct"] = 0.00103202;
                row["event_id"] = "subscription:" + std::to_string(index);
                row["source_resource"] = "list/func_kkzss101_1.jsn";
                row["raw"] = tdx::Json::object();
                row["raw"]["zql"] = "0.00103202";
                row["raw"]["fxzs"] = "10.68";
                if (index == 0) {
                    row["lottery_rate_pct"] = tdx::Json(nullptr);
                    row["raw"]["zql"] = "";
                }
                body["subscriptions"].push_back(std::move(row));
            }
            body["new_bond_projection"] = tdx::Json::array();
            for (int index = 0; index < 20; ++index) {
                tdx::Json row = tdx::Json::object();
                row["kind"] = "new-convertible-bond-projection";
                row["underlying"] = tdx::Json::object();
                row["underlying"]["code"] = "301565";
                row["subscription_code"] = "371565";
                row["subscription_date"] = "20260806";
                row["source_resource"] = "list/gxjty_zq_xkzz102_1.jsn";
                row["raw"] = tdx::Json::object();
                row["subscription_match"] = tdx::Json::object();
                row["subscription_match"]["matched"] = true;
                row["subscription_match"]["match_method"] = "subscription-code";
                body["new_bond_projection"].push_back(std::move(row));
            }
            body["new_bond_reconciliation"] = tdx::Json::object();
            body["new_bond_reconciliation"]["exact_subscription_code_matches"] = 20;
            body["new_bond_reconciliation"]["hybrid_or_stale_count"] = 3;
            body["sources"] = tdx::Json::array();
            tdx::Json source = tdx::Json::object();
            source["resource"] = "list/func_kkzss101_1.jsn";
            body["sources"].push_back(std::move(source));
            tdx::Json projection_source = tdx::Json::object();
            projection_source["resource"] = "list/gxjty_zq_xkzz102_1.jsn";
            body["sources"].push_back(std::move(projection_source));
            return body.dump(-1);
        }());
    require(bond_subscriptions.at("passed").as_bool(),
            "convertible-bond subscriptions contract failed");

    const auto exchangeable_bond = tdx::evaluate_api_contract_response(
        "exchangeable-bond-supplement-live", 200, "application/json",
        R"({"schema":"tdx-market-convertible-bonds-native-v1","view":"listed","availability":"live","summary":{"bonds":321,"exchangeable_bonds":2,"exchangeable_bonds_supplemented":2,"exchangeable_bonds_projection_verified":2,"core_terms_complete":317,"core_terms_missing":4},"bonds":[{"instrument_type":"exchangeable-bond","exchangeable_supplemented":true,"exchangeable_projection_verified":true,"bond":{"market":"sh","code":"132024","name":"26江铜EB"},"underlying":{"market":"sh","code":"600362","name":"江西铜业"},"overview":{"face_value":100,"conversion_price":52.4,"maturity_date":"20310409","maturity_redemption_price":105,"unpaid_coupon_sum":0.04,"issuer_rating":"AAA","core_terms_complete":true,"source_resource":"list/kjhz_kjhzsy201_1.jsn","projection_resource":"list/func_kzz103_1.jsn"}}],"exchangeable_projection_reconciliation":{"exact_security_set":true,"common_securities":2},"master_errors":[],"sources":[{"resource":"list/kzz_kzzsy201_1.jsn"},{"resource":"list/func_kzz_tkjd201.jsn"},{"resource":"list/func_kzz_lltk201.jsn"},{"resource":"list/func_kzz_hstk201.jsn"},{"resource":"list/func_kzz_shtk201.jsn"},{"resource":"list/func_kzz_xztk201.jsn"},{"resource":"list/kjhz_kjhzsy201_1.jsn"},{"resource":"list/func_kzz103_1.jsn"}]})");
    require(exchangeable_bond.at("passed").as_bool(),
            "exchangeable-bond supplement contract failed");

    const auto exchangeable_bond_missing = tdx::evaluate_api_contract_response(
        "exchangeable-bond-supplement-live", 200, "application/json",
        R"({"schema":"tdx-market-convertible-bonds-native-v1","view":"listed","availability":"partial","summary":{"exchangeable_bonds":2,"exchangeable_bonds_supplemented":0,"core_terms_complete":319,"core_terms_missing":6},"bonds":[],"master_errors":[{"resource":"list/kjhz_kjhzsy201_1.jsn"}],"sources":[]})");
    require(!exchangeable_bond_missing.at("passed").as_bool(),
            "exchangeable-bond contract must reject a missing supplement");

    const auto convertible_bond_pricing = tdx::evaluate_api_contract_response(
        "convertible-bond-pricing-live", 200, "application/json",
        R"({"schema":"tdx-market-convertible-bonds-native-v1","view":"pricing","availability":"live","summary":{"active_bonds":309,"complete_valuations":308},"pricing":[{"bond":{"market":"sh","code":"110076","name":"华海转债"},"underlying":{"market":"sh","code":"600521","name":"华海药业"},"quote":{"bond_last_price":114.98},"valuation":{"accrued_interest_source":"coupon-schedule-derived-actual-365","full_price":122.596438356164,"conversion_value":99.757575757576,"conversion_premium_pct":22.894364087285,"maturity_yield_pct":-36.546033180448,"double_low_score":137.874364087285},"raw":{"$SC":"1","$ZQDM":"110076"}}],"sources":[{"resource":"list/gxjty_zq_kzzsy101_1.jsn"}]})");
    require(convertible_bond_pricing.at("passed").as_bool(),
            "convertible-bond pricing contract failed");

    const auto convertible_bond_pricing_missing_raw = tdx::evaluate_api_contract_response(
        "convertible-bond-pricing-live", 200, "application/json",
        R"({"schema":"tdx-market-convertible-bonds-native-v1","view":"pricing","availability":"live","summary":{"active_bonds":309,"complete_valuations":308},"pricing":[{"bond":{"code":"110076"},"underlying":{"code":"600521"},"quote":{"bond_last_price":114.98},"valuation":{"accrued_interest_source":"coupon-schedule-derived-actual-365","full_price":122.596438356164,"conversion_value":99.757575757576,"conversion_premium_pct":22.894364087285,"maturity_yield_pct":-36.546033180448,"double_low_score":137.874364087285}}],"sources":[{"resource":"list/gxjty_zq_kzzsy101_1.jsn"}]})");
    require(!convertible_bond_pricing_missing_raw.at("passed").as_bool(),
            "convertible-bond pricing contract must require the auditable raw row");

    auto bond_reference_body = tdx::Json::parse(
        R"({"schema":"tdx-market-bond-reference-native-v1","availability":"live","summary":{"source_group":"rating","source_bucket":"aa-plus","source_row_count":941},"match_count":941,"returned":5,"records":[],"sources":[{"resource":"list/zq_aaj201.jsn"}]})");
    for (int index = 0; index < 5; ++index) {
        auto row = tdx::Json::parse(
            R"({"security":{"market":"sh","code":"115455","name":"23海旅01"},"bond_credit_rating":"AA+","maturity_date":"20280626","current_coupon_rate_pct":1.85,"coupon_schedule":[{"date":"20280626","rate_pct":1.85}],"remaining_coupon_schedule":[{"date":"20280626","rate_pct":1.85}],"source_resource":"list/zq_aaj201.jsn","raw":{"ZQXY":"AA+"}})");
        row["security"]["code"] = "11545" + std::to_string(index);
        bond_reference_body["records"].push_back(std::move(row));
    }
    const auto bond_reference = tdx::evaluate_api_contract_response(
        "bond-reference-aa-plus-live", 200, "application/json", bond_reference_body.dump(-1));
    require(bond_reference.at("passed").as_bool(), "bond reference AA+ contract failed");

    auto all_bonds_body = tdx::Json::parse(
        R"({"schema":"tdx-market-bond-reference-native-v1","availability":"live","summary":{"source_group":"category","source_bucket":"all","source_row_count":42479},"match_count":42479,"returned":5,"records":[],"sources":[{"resource":"list/zq_zqqb201.jsn","endpoint":"local-jsn:output/tdx-jsn/list/zq_zqqb201.jsn"}]})");
    for (int index = 0; index < 5; ++index) {
        auto row = tdx::Json::parse(
            R"({"security":{"market":"sh","security_id":"SH010107","code":"010107","name":"21国债⑺"},"bond_type":"国债","maturity_date":"20400718","current_coupon_rate_pct":2.13,"coupon_schedule":[],"remaining_coupon_schedule":[],"source_resource":"list/zq_zqqb201.jsn","raw":{"ZQLX":"国债"}})");
        row["security"]["code"] = "01010" + std::to_string(index);
        row["security"]["security_id"] = "SH01010" + std::to_string(index);
        if (index == 0)
            row["coupon_schedule"].push_back(
                tdx::Json::parse(R"({"date":"20400718","rate_pct":2.13})"));
        all_bonds_body["records"].push_back(std::move(row));
    }
    const auto all_bonds = tdx::evaluate_api_contract_response(
        "bond-reference-universe-live", 200, "application/json", all_bonds_body.dump(-1));
    require(all_bonds.at("passed").as_bool(), "all-bond universe contract failed");

    all_bonds_body["summary"]["source_row_count"] = 39999;
    const auto incomplete_all_bonds = tdx::evaluate_api_contract_response(
        "bond-reference-universe-live", 200, "application/json", all_bonds_body.dump(-1));
    require(!incomplete_all_bonds.at("passed").as_bool(),
            "all-bond universe contract must reject an incomplete master");

    auto corporate_bonds_body = tdx::Json::parse(
        R"({"schema":"tdx-market-bond-reference-native-v1","availability":"live","summary":{"source_group":"category","source_bucket":"corporate","source_row_count":7289,"matched_unique_security_count":7289},"match_count":7289,"returned":5,"projection_reconciliation":{"master_resource":"list/zqgsz201.jsn","master_count":7289,"projection_union_count":7289,"exact_match":true,"projections":[{"resource":"list/zq_gsz201_1.jsn","market":"sh","count":5755},{"resource":"list/zq_gsz201_2.jsn","market":"sz","count":1534}]},"records":[],"sources":[{"resource":"list/zqgsz201.jsn"},{"resource":"list/zq_gsz201_1.jsn"},{"resource":"list/zq_gsz201_2.jsn"}]})");
    for (int index = 0; index < 5; ++index) {
        auto row = tdx::Json::parse(
            R"({"security":{"security_id":"SH115028","code":"115028","name":"23安租04"},"client_instrument_id":"9900031871","source_scale_raw":6.3,"source_scale_semantics":"client-master-hidden-unit","source_resource":"list/zqgsz201.jsn","raw":{"$ZQDM1":"115028"}})");
        row["security"]["code"] = "11502" + std::to_string(index);
        row["security"]["security_id"] = "SH11502" + std::to_string(index);
        corporate_bonds_body["records"].push_back(std::move(row));
    }
    const auto corporate_bonds =
        tdx::evaluate_api_contract_response("bond-reference-corporate-projections-live", 200,
                                            "application/json", corporate_bonds_body.dump(-1));
    require(corporate_bonds.at("passed").as_bool(), "corporate bond projection contract failed");
    corporate_bonds_body["projection_reconciliation"]["exact_match"] = false;
    const auto corporate_bonds_mismatch =
        tdx::evaluate_api_contract_response("bond-reference-corporate-projections-live", 200,
                                            "application/json", corporate_bonds_body.dump(-1));
    require(!corporate_bonds_mismatch.at("passed").as_bool(),
            "corporate bond projection contract must reject a set mismatch");

    // Preserve the real cardinalities without embedding every security id.
    auto private_body = tdx::Json::parse(
        R"({"schema":"tdx-market-bond-reference-native-v1","summary":{"source_row_count":11268,"matched_unique_security_count":11263},"projection_reconciliation":{"master_count":11263,"master_row_count":11268,"projection_union_count":11216,"exact_match":false,"master_only":[],"projection_only":[],"projections":[{"resource":"list/zq_smz201_1.jsn","market":"sh","count":9633},{"resource":"list/zq_smz201_2.jsn","market":"sz","count":1583}],"client_master_comparison":{"resource":"list/zqsmz201.jsn","row_count":1583,"security_count":1583,"matches_single_market_projection":true,"matching_projection_resources":["list/zq_smz201_2.jsn"]}},"sources":[{"resource":"list/gxjty_zq_smz101_1.jsn"},{"resource":"list/zq_smz201_1.jsn"},{"resource":"list/zq_smz201_2.jsn"},{"resource":"list/zqsmz201.jsn"}]})");
    for (int index = 0; index < 77; ++index)
        private_body["projection_reconciliation"]["master_only"].push_back("SHM" +
                                                                           std::to_string(index));
    for (int index = 0; index < 30; ++index)
        private_body["projection_reconciliation"]["projection_only"].push_back(
            "SZP" + std::to_string(index));
    const auto private_boundary = tdx::evaluate_api_contract_response(
        "bond-reference-private-boundary-live", 200, "application/json", private_body.dump(-1));
    require(private_boundary.at("passed").as_bool(), "private bond boundary contract failed");

    auto government_bond_body = tdx::Json::parse(
        R"({"schema":"tdx-market-bond-reference-native-v1","availability":"live","summary":{"source_group":"category","source_bucket":"government","source_row_count":450},"match_count":450,"returned":5,"records":[],"sources":[{"resource":"list/zqgz201.jsn"}]})");
    for (int index = 0; index < 5; ++index) {
        auto row = tdx::Json::parse(
            R"({"security":{"market":"sh","code":"019707","name":"23国债14"},"maturity_date":"20300625","current_coupon_rate_pct":2.62,"source_scale_raw":null,"source_scale_semantics":"client-master-hidden-unit","issue_size_yuan":null,"underlying":null,"coupon_schedule":[{"date":"20300625","rate_pct":2.62}],"source_resource":"list/zqgz201.jsn","raw":{"ZQLX":"国债"}})");
        row["security"]["code"] = "01970" + std::to_string(index);
        government_bond_body["records"].push_back(std::move(row));
    }
    const auto government_bond = tdx::evaluate_api_contract_response(
        "bond-reference-government-live", 200, "application/json", government_bond_body.dump(-1));
    require(government_bond.at("passed").as_bool(), "government bond reference contract failed");
}

} // namespace recon_contract_test
