#include "recon_contract_test_support.hpp"

namespace recon_contract_test {

void run_research_signal_contracts() {
    const auto forecast_latest = tdx::evaluate_api_contract_response(
        "forecast-latest-live", 200, "application/json",
        R"({"schema":"tdx-market-forecasts-native-v1","view":"latest","summary":{"latest_forecasts":1805,"current_report_rows":1796,"current_report_expected":1846,"current_report_gap":50,"future_report_rows":9},"securities":[{"security":{"market":"sz","code":"002303"},"forecast_date":"20260807","report_period":"20260630","forecast_type":"业绩预降","profit_lower_yuan":211323608.93,"growth_lower_pct":20,"source_variant":"all-market-static","raw":{}},{},{},{},{}],"sources":[{"resource":"list/func_yjygtj101_1.jsn"},{"resource":"list/func_ggyjyg101_1.jsn"},{"resource":"list/func_cbpl101_1.jsn"}]})");
    require(forecast_latest.at("passed").as_bool(), "latest all-market forecast contract failed");

    const auto forecast_latest_bad_gap = tdx::evaluate_api_contract_response(
        "forecast-latest-live", 200, "application/json",
        R"({"schema":"tdx-market-forecasts-native-v1","view":"latest","summary":{"latest_forecasts":1805,"current_report_rows":1796,"current_report_expected":1846,"current_report_gap":0,"future_report_rows":9},"securities":[{"security":{"code":"002303"},"forecast_date":"20260807","report_period":"20260630","forecast_type":"业绩预降","profit_lower_yuan":1,"growth_lower_pct":20,"source_variant":"all-market-static","raw":{}},{},{},{},{}],"sources":[{"resource":"list/func_yjygtj101_1.jsn"},{"resource":"list/func_ggyjyg101_1.jsn"},{"resource":"list/func_cbpl101_1.jsn"}]})");
    require(!forecast_latest_bad_gap.at("passed").as_bool(),
            "latest forecast contract must reject an inconsistent reconciliation gap");

    const auto economic_catalog = tdx::evaluate_api_contract_response(
        "economic-indicators-catalog-live", 200, "application/json",
        R"({"schema":"tdx-market-economic-indicators-native-v1","view":"catalog","availability":"live","summary":{"indicator_count":72},"indicators":[{"indicator_id":"M2800000005","update_date":"2026-08-06","current_value":107170,"month_on_month_pct":1.2,"year_on_year_pct":35,"raw":{}},{"indicator_id":"M2400000011","update_date":"2026-08-05","current_value":16705,"month_on_month_pct":0.69,"year_on_year_pct":8.62,"raw":{}}],"sources":[{"resource":"list/func_jjzb101_1.jsn"}]})");
    require(economic_catalog.at("passed").as_bool(), "economic indicator catalog contract failed");

    auto economic_detail_body = tdx::Json::parse(
        R"({"schema":"tdx-market-economic-indicators-native-v1","view":"indicator","availability":"live","summary":{"indicator_count":72,"related_security_count":139},"selected_indicator":{"indicator_id":"M2800000005","name":"阴极铜:期货结算价","unit":"元/吨","current_value":107170,"month_on_month_pct":1.2,"year_on_year_pct":35},"history":[],"related_securities":[],"quote_source":{"received":100},"sources":[{"resource":"list/func_jjzb101_1.jsn"},{"resource":"jjzb1/M2800000005.jsn"},{"resource":"jjzb2/M2800000005.jsn"}]})");
    for (int i = 0; i < 100; ++i) {
        auto point = tdx::Json::object();
        point["date"] = "2026-08-" + std::string(i < 10 ? "0" : "") + std::to_string(i);
        point["value"] = 100000.0 + i;
        economic_detail_body["history"].push_back(std::move(point));
        auto row = tdx::Json::object();
        row["security"] = tdx::Json::object();
        row["security"]["code"] = "601600";
        row["security"]["name_resolved"] = true;
        row["quote_available"] = true;
        row["raw"] = tdx::Json::object();
        economic_detail_body["related_securities"].push_back(std::move(row));
    }
    const auto economic_detail = tdx::evaluate_api_contract_response(
        "economic-indicator-detail-live", 200, "application/json", economic_detail_body.dump(-1));
    require(economic_detail.at("passed").as_bool(), "economic indicator detail contract failed");

    auto strategic_catalog_body = tdx::Json::parse(
        R"({"schema":"tdx-market-strategic-themes-native-v1","view":"catalog","availability":"live","summary":{"category_count":26,"theme_count":598,"category_theme_memberships":655,"count_mismatch_count":0,"duplicate_master_memberships":9},"categories":[],"themes":[],"sources":[]})");
    for (int i = 0; i < 26; ++i) {
        auto category = tdx::Json::object();
        category["block_id"] = "Z" + std::to_string(i + 1);
        category["name"] = "Category " + std::to_string(i + 1);
        strategic_catalog_body["categories"].push_back(std::move(category));
        auto source = tdx::Json::object();
        source["resource"] = i == 0   ? "list/func_5G101_1.jsn"
                             : i == 1 ? "list/func_gfjg101_1.jsn"
                             : i == 2 ? "list/func_gfjg102_1.jsn"
                             : i == 3 ? "list/func_hlw101_1.jsn"
                                      : "list/func_theme" + std::to_string(i) + "_1.jsn";
        strategic_catalog_body["sources"].push_back(std::move(source));
    }
    for (int i = 0; i < 598; ++i) {
        auto theme = tdx::Json::object();
        theme["theme_id"] = std::to_string(i);
        const auto digits = std::to_string(i);
        theme["name"] = "Theme " + std::string(3 - digits.size(), '0') + digits;
        theme["categories"] = tdx::Json::array();
        theme["categories"].push_back("Category 1");
        theme["master_member_count"] = 10;
        theme["detail_resource"] = "zttzty/" + std::to_string(i) + ".jsn";
        strategic_catalog_body["themes"].push_back(std::move(theme));
    }
    const auto strategic_catalog = tdx::evaluate_api_contract_response(
        "strategic-themes-catalog-live", 200, "application/json", strategic_catalog_body.dump(-1));
    require(strategic_catalog.at("passed").as_bool(), "strategic theme catalog contract failed");

    auto strategic_detail_body = tdx::Json::parse(
        R"({"schema":"tdx-market-strategic-themes-native-v1","view":"theme","availability":"live","summary":{"category_count":26,"theme_count":598,"category_theme_memberships":655,"count_mismatch_count":0,"duplicate_master_memberships":9},"categories":[],"themes":[],"selected_theme":{"theme_id":"657","name":"5G概念","detail_available":true,"master_member_count":441,"member_count":452},"members":[],"details":[],"sources":[]})");
    for (int i = 0; i < 26; ++i) {
        auto category = tdx::Json::object();
        category["block_id"] = "Z" + std::to_string(i + 1);
        category["name"] = "Category " + std::to_string(i + 1);
        strategic_detail_body["categories"].push_back(std::move(category));
        auto source = tdx::Json::object();
        source["resource"] = i == 0   ? "list/func_5G101_1.jsn"
                             : i == 1 ? "list/func_gfjg101_1.jsn"
                             : i == 2 ? "list/func_gfjg102_1.jsn"
                             : i == 3 ? "list/func_hlw101_1.jsn"
                                      : "list/func_theme" + std::to_string(i) + "_1.jsn";
        strategic_detail_body["sources"].push_back(std::move(source));
    }
    auto detail_source = tdx::Json::object();
    detail_source["resource"] = "zttzty/657.jsn";
    strategic_detail_body["sources"].push_back(std::move(detail_source));
    for (int i = 0; i < 452; ++i) {
        auto security = tdx::Json::object();
        security["market"] = "sz";
        security["code"] = "000063";
        security["name"] = "中兴通讯";
        security["name_resolved"] = true;
        strategic_detail_body["members"].push_back(security);
        auto detail = tdx::Json::object();
        detail["security"] = std::move(security);
        detail["logic"] = "5G equipment and network supplier";
        detail["raw"] = tdx::Json::object();
        strategic_detail_body["details"].push_back(std::move(detail));
    }
    const auto strategic_detail = tdx::evaluate_api_contract_response(
        "strategic-theme-detail-live", 200, "application/json", strategic_detail_body.dump(-1));
    require(strategic_detail.at("passed").as_bool(), "strategic theme detail contract failed");

    strategic_detail_body["selected_theme"]["detail_available"] = false;
    const auto strategic_detail_missing = tdx::evaluate_api_contract_response(
        "strategic-theme-detail-live", 200, "application/json", strategic_detail_body.dump(-1));
    require(!strategic_detail_missing.at("passed").as_bool(),
            "strategic theme detail contract must require the live detail resource");

    auto theme_library_catalog_body = tdx::Json::parse(
        R"({"schema":"tdx-market-theme-library-native-v1","view":"catalog","availability":"live","summary":{"source_count":5,"snapshot_count":1102,"unique_theme_id_count":927,"count_mismatch_count":0},"themes":[]})");
    for (int i = 0; i < 850; ++i) {
        auto theme = tdx::Json::object();
        theme["theme_id"] = i == 0 ? "3137" : std::to_string(i);
        theme["name"] = i == 0 ? "实验猴" : "Theme " + std::to_string(i);
        theme["created_date"] = i == 0 ? "20260716" : "20200101";
        theme["member_count"] = i == 0 ? 4 : 1;
        theme_library_catalog_body["themes"].push_back(std::move(theme));
    }
    const auto theme_library_catalog = tdx::evaluate_api_contract_response(
        "theme-library-catalog-live", 200, "application/json", theme_library_catalog_body.dump(-1));
    require(theme_library_catalog.at("passed").as_bool(), "theme library catalog contract failed");

    auto theme_library_detail_body = tdx::Json::parse(
        R"({"schema":"tdx-market-theme-library-native-v1","view":"theme","availability":"live","summary":{"source_count":5,"snapshot_count":1102,"unique_theme_id_count":927,"count_mismatch_count":0},"selected_theme":{"theme_id":"3137","name":"实验猴","active_member_count":4},"members":[],"details":[],"chart":[]})");
    for (int i = 0; i < 4; ++i) {
        auto security = tdx::Json::object();
        security["market"] = i < 1 ? "sz" : "sh";
        security["code"] = i < 1 ? "300759" : "688710";
        theme_library_detail_body["members"].push_back(security);
        auto detail = tdx::Json::object();
        detail["security"] = std::move(security);
        detail["description"] = "实验猴主题纳入原因";
        theme_library_detail_body["details"].push_back(std::move(detail));
    }
    for (int i = 0; i < 18; ++i) {
        auto point = tdx::Json::object();
        point["date"] = i == 0 ? "20260715" : "20260716";
        point["value"] = i == 0 ? 1000.0 : 1000.0 + i;
        theme_library_detail_body["chart"].push_back(std::move(point));
    }
    const auto theme_library_detail = tdx::evaluate_api_contract_response(
        "theme-library-detail-live", 200, "application/json", theme_library_detail_body.dump(-1));
    require(theme_library_detail.at("passed").as_bool(), "theme library detail contract failed");

    auto thematic_catalog_body = tdx::Json::parse(
        R"({"schema":"tdx-market-thematic-opportunities-native-v1","view":"catalog","availability":"live","summary":{"group_count":15,"industry_group_count":6,"region_group_count":8,"legacy_client_theme_count":1,"semantic_mismatch_count":1,"group_memberships":732,"security_count":559},"groups":[],"sources":[{"resource":"list/func_ydyl101_1.jsn"},{"resource":"list/func_ydyl102_1.jsn"},{"resource":"list/func_rdhs101_1.jsn"},{"resource":"list/func_rdhs102_1.jsn"},{"resource":"list/func_xnxs101_1.jsn"}]})");
    for (int i = 0; i < 15; ++i) {
        auto group = tdx::Json::object();
        group["group_id"] = i == 14 ? "299" : (i < 6 ? "hy" : "qy") + std::to_string(i);
        group["type"] = i < 6 ? "industry" : i < 14 ? "region" : "legacy-client-theme";
        group["name"] = "Group " + std::to_string(i);
        group["member_count"] = 40 + i;
        group["detail_resource"] =
            i == 14 ? "xnxs/299.jsn" : "ydyl1/" + group.at("group_id").as_string() + ".jsn";
        thematic_catalog_body["groups"].push_back(std::move(group));
    }
    const auto thematic_catalog =
        tdx::evaluate_api_contract_response("thematic-opportunities-catalog-live", 200,
                                            "application/json", thematic_catalog_body.dump(-1));
    require(thematic_catalog.at("passed").as_bool(),
            "thematic opportunities catalog contract failed");

    auto thematic_detail_body = tdx::Json::parse(
        R"({"schema":"tdx-market-thematic-opportunities-native-v1","view":"group","availability":"live","summary":{"group_count":15,"industry_group_count":6,"region_group_count":8,"legacy_client_theme_count":1,"semantic_mismatch_count":1,"group_memberships":732,"security_count":559},"selected_group":{"group_id":"hy57","name":"IGCC技术","type":"industry","detail_available":true},"details":[],"sources":[{"resource":"list/func_ydyl101_1.jsn"},{"resource":"list/func_ydyl102_1.jsn"},{"resource":"list/func_rdhs101_1.jsn"},{"resource":"list/func_rdhs102_1.jsn"},{"resource":"list/func_xnxs101_1.jsn"},{"resource":"ydyl1/hy57.jsn"}]})");
    for (int i = 0; i < 10; ++i) {
        auto detail = tdx::Json::object();
        detail["security"] = tdx::Json::parse(
            R"({"market":"sz","code":"000777","name":"中核科技","name_resolved":true})");
        detail["logic"] = "工业用阀门与清洁煤技术";
        detail["three_month_adjusted_close"] = 19.63;
        detail["year_start_adjusted_close"] = 25.74;
        detail["raw"] = tdx::Json::object();
        thematic_detail_body["details"].push_back(std::move(detail));
    }
    const auto thematic_detail = tdx::evaluate_api_contract_response(
        "thematic-opportunity-detail-live", 200, "application/json", thematic_detail_body.dump(-1));
    require(thematic_detail.at("passed").as_bool(), "thematic opportunity detail contract failed");

    auto legacy_theme_body = tdx::Json::parse(
        R"({"schema":"tdx-market-thematic-opportunities-native-v1","view":"group","availability":"live","summary":{"group_count":15,"industry_group_count":6,"region_group_count":8,"legacy_client_theme_count":1,"semantic_mismatch_count":1,"group_memberships":732,"security_count":559},"selected_group":{"group_id":"299","name":"快中子反应堆","category":"内容应用","type":"legacy-client-theme","page_declared_name":"虚拟现实","semantic_mismatch":true,"detail_available":true,"detail_resource":"xnxs/299.jsn"},"details":[],"sources":[{"resource":"list/func_ydyl101_1.jsn"},{"resource":"list/func_ydyl102_1.jsn"},{"resource":"list/func_rdhs101_1.jsn"},{"resource":"list/func_rdhs102_1.jsn"},{"resource":"list/func_xnxs101_1.jsn"},{"resource":"xnxs/299.jsn"}]})");
    for (int i = 0; i < 3; ++i) {
        auto detail = tdx::Json::object();
        detail["security"] = tdx::Json::parse(
            R"({"market":"sh","code":"601727","name":"上海电气","name_resolved":true})");
        detail["logic"] = "突破快堆复杂系统设备集成技术瓶颈";
        detail["reference_close_3d"] = 6.97;
        detail["reference_close_5d"] = 6.90;
        detail["reference_close_20d"] = 6.81;
        detail["three_month_adjusted_close"] = 8.14;
        detail["detail_variant"] = "legacy-client-theme";
        detail["raw"] = tdx::Json::object();
        legacy_theme_body["details"].push_back(std::move(detail));
    }
    const auto legacy_theme = tdx::evaluate_api_contract_response(
        "thematic-legacy-client-theme-live", 200, "application/json", legacy_theme_body.dump(-1));
    require(legacy_theme.at("passed").as_bool(), "legacy client-theme contract failed");

    const auto thematic_completed = tdx::evaluate_api_contract_response(
        "thematic-hype-completed-live", 200, "application/json",
        R"({"schema":"tdx-market-thematic-opportunities-native-v1","view":"hype-completed","availability":"live","summary":{"group_count":15,"industry_group_count":6,"region_group_count":8,"legacy_client_theme_count":1,"semantic_mismatch_count":1,"group_memberships":732,"security_count":559},"completed_hype":[{"record_id":"chip:001210","status":"completed","block_name":"芯片","start_date":"20260601","end_date":"20260731","limit_pattern":"42天11板","leader":{"market":"sz","code":"001210","name":"圣晖集成","name_resolved":true},"interval_return_pct":148.36,"analysis":"并购与半导体设备行情驱动","raw":{}}],"sources":[{"resource":"list/func_ydyl101_1.jsn"},{"resource":"list/func_ydyl102_1.jsn"},{"resource":"list/func_rdhs101_1.jsn"},{"resource":"list/func_rdhs102_1.jsn"},{"resource":"list/func_xnxs101_1.jsn"}]})");
    require(thematic_completed.at("passed").as_bool(), "completed hype review contract failed");

    auto thematic_active_body = tdx::Json::parse(
        R"({"schema":"tdx-market-thematic-opportunities-native-v1","view":"hype-active","availability":"live","summary":{"group_count":15,"industry_group_count":6,"region_group_count":8,"legacy_client_theme_count":1,"semantic_mismatch_count":1,"group_memberships":732,"security_count":559},"active_hype":[],"sources":[{"resource":"list/func_ydyl101_1.jsn"},{"resource":"list/func_ydyl102_1.jsn"},{"resource":"list/func_rdhs101_1.jsn"},{"resource":"list/func_rdhs102_1.jsn"},{"resource":"list/func_xnxs101_1.jsn"}]})");
    for (int i = 0; i < 3; ++i) {
        auto active = tdx::Json::object();
        active["record_id"] = "SH60000" + std::to_string(i);
        active["status"] = "active";
        active["security"] = tdx::Json::parse(
            R"({"market":"sh","code":"600000","name":"浦发银行","name_resolved":true})");
        active["interval_stat"] = "15天5板";
        active["stock_return_pct"] = 68.5 - i;
        active["shanghai_index_return_pct"] = 3.5;
        active["relative_return_pct"] = 65.0 - i;
        active["raw"] = tdx::Json::object();
        thematic_active_body["active_hype"].push_back(std::move(active));
    }
    const auto thematic_active = tdx::evaluate_api_contract_response(
        "thematic-hype-active-live", 200, "application/json", thematic_active_body.dump(-1));
    require(thematic_active.at("passed").as_bool(), "active hype review contract failed");
    thematic_active_body["active_hype"].as_array()[0]["relative_return_pct"] = 68.5;
    const auto thematic_active_bad_formula = tdx::evaluate_api_contract_response(
        "thematic-hype-active-live", 200, "application/json", thematic_active_body.dump(-1));
    require(!thematic_active_bad_formula.at("passed").as_bool(),
            "active hype contract must reject a broken relative-return formula");

    const auto block_rotation = tdx::evaluate_api_contract_response(
        "block-rotation-live", 200, "application/json",
        R"({"schema":"tdx-market-block-rotation-native-v1","availability":"live","filters":{"category":"all","period":"1w"},"counts":{"source_rows":515},"summary":{"by_category":[{},{},{},{}]},"records":[{"block":{"code":"880207","name":"北京板块","name_resolved":true},"periods":{"1w":{"anomaly_count":5}},"raw":{"$SC":"1","$ZQDM":"880207"}},{"block":{"code":"880217","name":"山西板块","name_resolved":true},"periods":{"1w":{"anomaly_count":4}},"raw":{"$SC":"1","$ZQDM":"880217"}}],"sources":[{"resource":"list/func_bkld101_1.jsn","attempts":1,"stale":false},{"resource":"list/func_bkld102_1.jsn","attempts":1,"stale":false},{"resource":"list/func_bkld103_1.jsn","attempts":1,"stale":false},{"resource":"list/func_bkld104_1.jsn","attempts":1,"stale":false}]})");
    require(block_rotation.at("passed").as_bool(), "block-rotation contract failed");

    const auto block_rotation_unsorted = tdx::evaluate_api_contract_response(
        "block-rotation-live", 200, "application/json",
        R"({"schema":"tdx-market-block-rotation-native-v1","availability":"live","filters":{"category":"all","period":"1w"},"counts":{"source_rows":515},"summary":{"by_category":[{},{},{},{}]},"records":[{"block":{"code":"880207","name":"北京板块","name_resolved":true},"periods":{"1w":{"anomaly_count":3}},"raw":{}},{"block":{"code":"880217","name":"山西板块","name_resolved":true},"periods":{"1w":{"anomaly_count":4}},"raw":{}}],"sources":[{"resource":"list/func_bkld101_1.jsn","attempts":1,"stale":false},{"resource":"list/func_bkld102_1.jsn","attempts":1,"stale":false},{"resource":"list/func_bkld103_1.jsn","attempts":1,"stale":false},{"resource":"list/func_bkld104_1.jsn","attempts":1,"stale":false}]})");
    require(!block_rotation_unsorted.at("passed").as_bool(),
            "block-rotation contract must reject a broken primary sort");

    const auto limit_ladder = tdx::evaluate_api_contract_response(
        "limit-ladder-live", 200, "application/json",
        R"({"schema":"tdx-market-limit-ladder-native-v1","availability":"live","filters":{"category":"all","sort":"total-height"},"counts":{"source_rows":246,"category_rows":246},"summary":{"advancement_formula_checked_blocks":200,"advancement_formula_mismatch_blocks":0,"by_category":[{"category":"industry"},{"category":"concept"}]},"records":[{"block":{"code":"880952","name":"芯片","name_resolved":true,"members_available":true},"sum_streak_heights":37,"advancement_rate_formula_matches":true,"raw":{"$SC":"1","$ZQDM":"880952"}},{"block":{"code":"880506","name":"5G概念","name_resolved":true,"members_available":true},"sum_streak_heights":26,"advancement_rate_formula_matches":true,"raw":{"$SC":"1","$ZQDM":"880506"}}],"sources":[{"resource":"list/func_lbtt101_1.jsn","attempts":1,"stale":false}]})");
    require(limit_ladder.at("passed").as_bool(), "limit-ladder contract failed");

    const auto limit_ladder_unsorted = tdx::evaluate_api_contract_response(
        "limit-ladder-live", 200, "application/json",
        R"({"schema":"tdx-market-limit-ladder-native-v1","availability":"live","filters":{"category":"all","sort":"total-height"},"counts":{"source_rows":246,"category_rows":246},"summary":{"advancement_formula_checked_blocks":200,"advancement_formula_mismatch_blocks":0,"by_category":[{},{}]},"records":[{"block":{"code":"880952","name":"芯片","name_resolved":true,"members_available":true},"sum_streak_heights":20,"advancement_rate_formula_matches":true,"raw":{}},{"block":{"code":"880506","name":"5G概念","name_resolved":true,"members_available":true},"sum_streak_heights":26,"advancement_rate_formula_matches":true,"raw":{}}],"sources":[{"resource":"list/func_lbtt101_1.jsn","attempts":1,"stale":false}]})");
    require(!limit_ladder_unsorted.at("passed").as_bool(),
            "limit-ladder contract must reject a broken primary sort");

    auto threshold_body = tdx::Json::parse(
        R"({"schema":"tdx-market-threshold-stocks-native-v1","view":"members","universe":"high-price","availability":"live","selected_period":{"date":"20260806","total_count":212},"summary":{"members":{"active_count_matches":true,"entered_count_matches":true,"exited_count_matches":true},"trend_check":{"selected_count_matches":true}},"records":[{"security":{"market":"sh","code":"688808","name":"奥特维","name_resolved":true},"close_price_yuan":2095.01,"raw":{"$SC":"1","$ZQDM":"688808"}},{"security":{"market":"sh","code":"688498","name":"源杰科技","name_resolved":true},"close_price_yuan":1333.0,"raw":{"$SC":"1","$ZQDM":"688498"}}],"trend":[],"sources":[{"resource":"list/func_bygtj102_1.jsn","attempts":1,"stale":false},{"resource":"bygtj3/220260806.jsn","attempts":1,"stale":false},{"resource":"bygtj1/220260806.jsn","attempts":1,"stale":false}]})");
    for (int index = 0; index < 100; ++index)
        threshold_body["trend"].push_back(tdx::Json::object());
    const auto threshold_stocks = tdx::evaluate_api_contract_response(
        "threshold-stocks-high-price-live", 200, "application/json", threshold_body.dump(-1));
    require(threshold_stocks.at("passed").as_bool(), "threshold-stocks contract failed");

    threshold_body["records"].as_array()[1]["close_price_yuan"] = 3000;
    const auto threshold_stocks_unsorted = tdx::evaluate_api_contract_response(
        "threshold-stocks-high-price-live", 200, "application/json", threshold_body.dump(-1));
    require(!threshold_stocks_unsorted.at("passed").as_bool(),
            "threshold-stocks contract must reject broken ordering");

    auto capital_ranking_body = tdx::Json::parse(
        R"({"schema":"tdx-market-capital-strength-native-v1","view":"ranking","period":"5d","availability":"live","summary":{"source_rows":100,"names_resolved":100,"source_ddx_descending":true,"statistics_dates":["20260806"]},"records":[{"security":{"market":"sz","code":"001232","name":"C嘉立创","name_resolved":true},"ddx_float_share_pct":22.2583,"total_net_inflow_yuan":189889792,"main_net_inflow_yuan":2139516576,"raw":{"$SC":"0","$ZQDM":"001232"}},{"security":{"market":"sh","code":"603468","name":"N津富","name_resolved":true},"ddx_float_share_pct":17.4282,"total_net_inflow_yuan":81616896,"main_net_inflow_yuan":277010112,"raw":{"$SC":"1","$ZQDM":"603468"}}],"sources":[{"resource":"list/func_qszj101_1.jsn","attempts":1,"stale":false}]})");
    const auto capital_ranking = tdx::evaluate_api_contract_response(
        "capital-strength-ranking-live", 200, "application/json", capital_ranking_body.dump(-1));
    require(capital_ranking.at("passed").as_bool(), "capital-strength ranking contract failed");

    capital_ranking_body["records"].as_array()[1]["ddx_float_share_pct"] = 30;
    const auto capital_ranking_unsorted = tdx::evaluate_api_contract_response(
        "capital-strength-ranking-live", 200, "application/json", capital_ranking_body.dump(-1));
    require(!capital_ranking_unsorted.at("passed").as_bool(),
            "capital-strength ranking contract must reject broken ordering");

    const auto capital_confluence = tdx::evaluate_api_contract_response(
        "capital-strength-confluence-live", 200, "application/json",
        R"({"schema":"tdx-market-capital-strength-native-v1","view":"confluence","period":null,"availability":"live","summary":{"source_rows":500,"unique_securities":276,"at_least_two_periods":128,"all_five_periods":9},"records":[{"security":{"market":"sz","code":"001232","name":"C嘉立创","name_resolved":true},"period_count":5,"average_ddx_float_share_pct":22.2583,"observations":[{},{},{},{},{}]},{"security":{"market":"sh","code":"603468","name":"N津富","name_resolved":true},"period_count":5,"average_ddx_float_share_pct":17.4282,"observations":[{},{},{},{},{}]}],"sources":[{"resource":"list/func_qszj101_1.jsn","attempts":1,"stale":false},{"resource":"list/func_qszj102_1.jsn","attempts":1,"stale":false},{"resource":"list/func_qszj103_1.jsn","attempts":1,"stale":false},{"resource":"list/func_qszj104_1.jsn","attempts":1,"stale":false},{"resource":"list/func_qszj105_1.jsn","attempts":1,"stale":false}]})");
    require(capital_confluence.at("passed").as_bool(),
            "capital-strength confluence contract failed");

    auto strong_intervals_body = tdx::Json::parse(
        R"({"schema":"tdx-market-strong-stocks-native-v1","view":"intervals","availability":"live","summary":{"source_rows":615,"unique_securities":562,"names_resolved":615,"first_start_date":"2023-08-08","latest_end_date":"2026-08-06"},"records":[{"interval_id":"002827260729260806","security":{"market":"sz","code":"002827","name":"高争民爆","name_resolved":true},"start_date":"2026-07-29","end_date":"2026-08-06","trading_days":6,"limit_up_days":4,"raw":{"$SC1":"0","$ZQDM1":"002827"}},{"interval_id":"003032260727260805","security":{"market":"sz","code":"003032","name":"传智教育","name_resolved":true},"start_date":"2026-07-27","end_date":"2026-08-05","trading_days":8,"limit_up_days":8,"raw":{"$SC1":"0","$ZQDM1":"003032"}}],"sources":[{"resource":"list/func_ygzl101_1.jsn","attempts":1,"stale":false}]})");
    const auto strong_intervals = tdx::evaluate_api_contract_response(
        "strong-stocks-intervals-live", 200, "application/json", strong_intervals_body.dump(-1));
    require(strong_intervals.at("passed").as_bool(), "strong-stocks intervals contract failed");

    strong_intervals_body["records"].as_array()[1]["end_date"] = "2026-08-07";
    const auto strong_intervals_unsorted = tdx::evaluate_api_contract_response(
        "strong-stocks-intervals-live", 200, "application/json", strong_intervals_body.dump(-1));
    require(!strong_intervals_unsorted.at("passed").as_bool(),
            "strong-stocks contract must reject broken interval ordering");

    const auto strong_detail = tdx::evaluate_api_contract_response(
        "strong-stocks-detail-live", 200, "application/json",
        R"({"schema":"tdx-market-strong-stocks-native-v1","view":"detail","availability":"live","summary":{"days":2,"expected_trading_days":2,"complete":true,"reason_days":2,"interval":{"interval_id":"002827260729260806"}},"records":[{"interval_id":"002827260729260806","date":"2026-07-29","security":{"market":"sz","code":"002827","name":"高争民爆","name_resolved":true},"stock_return_pct":10.0,"turnover_amount_yuan":65006112,"limit_up_reason":"矿山资产重组","market_limit_up_count":86,"market_broken_limit_count":18,"market_seal_success_pct":82.69,"raw":{"sj":"20260729"}},{"interval_id":"002827260729260806","date":"2026-07-30","security":{"market":"sz","code":"002827","name":"高争民爆","name_resolved":true},"stock_return_pct":10.0,"turnover_amount_yuan":52276192,"limit_up_reason":"矿山资产重组","market_limit_up_count":56,"market_broken_limit_count":23,"market_seal_success_pct":70.89,"raw":{"sj":"20260730"}}],"sources":[{"resource":"list/func_ygzl101_1.jsn","attempts":1,"stale":false},{"resource":"ygzl/002827260729260806.jsn","attempts":1,"stale":false}]})");
    require(strong_detail.at("passed").as_bool(), "strong-stocks detail contract failed");

    const auto commodity_list = tdx::evaluate_api_contract_response(
        "commodity-links-commodities-live", 200, "application/json",
        R"({"schema":"tdx-market-commodity-links-native-v1","view":"commodities","availability":"live","summary":{"quote_rows":221,"unique_commodity_ids":219},"records":[{"commodity_id":"X200101007","quote_id":"X200101007:2","name":"A 豆一","latest_price":4791,"quote_date":"2026-08-06","raw":{"$ZQDM":"X200101007"}},{"commodity_id":"X100102003","quote_id":"X100102003:1","name":"1/3焦煤","latest_price":1900,"quote_date":"2026-08-05","raw":{"$ZQDM":"X100102003"}}],"sources":[{"resource":"list/func_zjtc103_1.jsn","attempts":1,"stale":false}]})");
    require(commodity_list.at("passed").as_bool(),
            "commodity-links commodity list contract failed");

    const auto commodity_detail = tdx::evaluate_api_contract_response(
        "commodity-links-commodity-live", 200, "application/json",
        R"({"schema":"tdx-market-commodity-links-native-v1","view":"commodity","availability":"live","summary":{"quote_rows":221,"unique_commodity_ids":219},"records":[{"commodity_id":"X100102003","quotes":[{"name":"1/3焦煤"}],"stocks":[{"security":{"code":"600397"},"reference_price_3m":14.35,"return_since_reference_pct":null,"current_quote_available":false}],"related_securities":[{"security":{"code":"515220"}}]}],"sources":[{"resource":"list/func_zjtc103_1.jsn","attempts":1,"stale":false},{"resource":"zjtc4/X100102003.jsn","attempts":1,"stale":false},{"resource":"zjtc5/X100102003.jsn","attempts":1,"stale":false}]})");
    require(commodity_detail.at("passed").as_bool(),
            "commodity-links commodity detail contract failed");

    const auto theme_detail = tdx::evaluate_api_contract_response(
        "commodity-links-theme-live", 200, "application/json",
        R"({"schema":"tdx-market-commodity-links-native-v1","view":"theme","availability":"live","summary":{"themes":21},"records":[{"theme_id":"70","latest_driver_date":"2026-06-22","stocks":[{},{},{},{},{},{},{},{},{},{},{},{},{}],"drivers":[{},{},{},{},{},{},{},{},{}]}],"sources":[{"resource":"list/func_zjtc101_1.jsn","attempts":1,"stale":false},{"resource":"zjtc1/70.jsn","attempts":1,"stale":false},{"resource":"zjtc2/70.jsn","attempts":1,"stale":false}]})");
    require(theme_detail.at("passed").as_bool(), "commodity-links theme detail contract failed");

    const auto announcement_selected = tdx::evaluate_api_contract_response(
        "announcement-signals-selected-live", 200, "application/json",
        R"({"schema":"tdx-market-announcement-signals-native-v1","view":"selected","availability":"live","records":[{"security":{"market":"sz","code":"000921","name":"海信家电"},"date":"2026-08-07","title":"2025年度A股权益分派实施公告","pdf_url":"http://static.cninfo.com.cn/a.PDF","direction":"bullish","announcement_type":"权益分派预案及实施","recent_3d_return_pct":-2.04,"recent_10d_return_pct":5.54,"pre_3d_return_pct":null,"post_3d_return_pct":null,"raw":{"$SC":"0","$ZQDM":"000921"}}],"sources":[{"resource":"list/func_zxjx101_1.jsn","attempts":1,"stale":false}]})");
    require(announcement_selected.at("passed").as_bool(), "announcement selected contract failed");

    const auto announcement_risks = tdx::evaluate_api_contract_response(
        "announcement-signals-risks-live", 200, "application/json",
        R"({"schema":"tdx-market-announcement-signals-native-v1","view":"risks","availability":"live","records":[{"security":{"market":"sh","code":"603124","name":"江南新材"},"date":"2026-08-07","title":"股票交易异常波动公告","pdf_url":"http://dataclouds.cninfo.com.cn/a.PDF","direction":"bearish","announcement_type":"股票交易异常波动风险提示","recent_3d_return_pct":28.57,"recent_10d_return_pct":5.99,"pre_3d_return_pct":null,"post_3d_return_pct":null,"raw":{"$SC":"1","$ZQDM":"603124"}}],"sources":[{"resource":"list/func_zxjx103_1.jsn","attempts":1,"stale":false}]})");
    require(announcement_risks.at("passed").as_bool(), "announcement risks contract failed");

    const auto announcement_history = tdx::evaluate_api_contract_response(
        "announcement-signals-history-live", 200, "application/json",
        R"({"schema":"tdx-market-announcement-signals-native-v1","view":"history","availability":"live","records":[{"security":{"market":"sz","code":"000921","name":"海信家电"},"date":"2026-07-22","title":"股权激励公告","pdf_url":"http://static.cninfo.com.cn/b.PDF","direction":"bullish","announcement_type":"股权激励","recent_3d_return_pct":null,"recent_10d_return_pct":null,"pre_3d_return_pct":1.91,"post_3d_return_pct":-6.61,"raw":{"DK":"利好"}}],"sources":[{"resource":"ggjx/0000921.jsn","attempts":1,"stale":false}]})");
    require(announcement_history.at("passed").as_bool(), "announcement history contract failed");

    const auto reverse_repo_rates = tdx::evaluate_api_contract_response(
        "reverse-repo-rates-live", 200, "application/json",
        R"({"schema":"tdx-market-reverse-repo-native-v1","view":"rates","availability":"live","summary":{"rows":18,"quoted_rows":18,"schedule_date":"2026-08-07"},"records":[{"security":{"market":"sh","code":"204182","name":"GC182","category":"repo"},"term_days":182,"interest_days":192,"fee_yuan":30,"funds_available_date":"2027-02-04","funds_withdrawable_date":"2027-02-15","annualized_rate_pct":1.405,"gross_interest_yuan":739.07,"net_interest_yuan":709.07,"net_annualized_rate_pct":1.348},{"security":{"market":"sz","code":"131806","name":"Ｒ-182","category":"repo"},"term_days":182,"interest_days":192,"fee_yuan":30,"funds_available_date":"2027-02-04","funds_withdrawable_date":"2027-02-15","annualized_rate_pct":1.4,"gross_interest_yuan":736.44,"net_interest_yuan":706.44,"net_annualized_rate_pct":1.343}],"quote_source":{"command":"0x054C","received":18,"transport":{"connection_attempts":2,"transient_retries":1,"endpoints_attempted":1,"max_attempts_per_endpoint":3,"recovered_after_retry":true,"endpoint_source":"connect.cfg:hqhost-primary-first","primary_configured":true}},"sources":[{"resource":"list/func_gznhg100_1.jsn","attempts":1,"stale":false}]})");
    require(reverse_repo_rates.at("passed").as_bool(), "reverse repo rates contract failed");

    const auto reverse_repo_calendar = tdx::evaluate_api_contract_response(
        "reverse-repo-calendar-live", 200, "application/json",
        R"({"schema":"tdx-market-reverse-repo-native-v1","view":"rates","availability":"schedule-only","summary":{"rows":18,"quoted_rows":0,"schedule_date":"2026-08-07"},"records":[{"security":{"market":"sh","code":"204001","name":"GC001","category":"repo"},"term_days":1,"interest_days":3,"fee_yuan":1,"funds_available_date":"2026-08-07","funds_withdrawable_date":"2026-08-10","annualized_rate_pct":null}],"quote_source":null,"sources":[{"resource":"list/func_gznhg100_1.jsn","attempts":1,"stale":false}]})");
    require(reverse_repo_calendar.at("passed").as_bool(), "reverse repo calendar contract failed");

    const auto supervision_current = tdx::evaluate_api_contract_response(
        "exchange-supervision-current-live", 200, "application/json",
        R"({"schema":"tdx-market-exchange-supervision-native-v1","view":"current","availability":"live","records":[{"security":{"market":"sh","code":"603221","name":"爱丽家居"},"start_date":"2026-08-06","end_date":"2026-08-19","start_price":11.58,"end_price":null,"last_price":24.79,"period_return_pct":null,"since_start_return_pct":114.08,"announcement_url":"http://dataclouds.cninfo.com.cn/a.PDF","raw":{"$SC":"1","$ZQDM":"603221"}}],"quote_source":{"command":"0x054C","received":22},"sources":[{"resource":"list/func_jysjk101_1.jsn","attempts":1,"stale":false}]})");
    require(supervision_current.at("passed").as_bool(),
            "exchange supervision current contract failed");

    const auto supervision_history = tdx::evaluate_api_contract_response(
        "exchange-supervision-history-live", 200, "application/json",
        R"({"schema":"tdx-market-exchange-supervision-native-v1","view":"history","availability":"live","records":[{"security":{"market":"sh","code":"688635","name":"誉辰智能"},"start_date":"2026-07-22","end_date":"2026-08-04","start_price":395.5,"end_price":320.5,"last_price":null,"period_return_pct":-18.96,"since_start_return_pct":null,"announcement_url":"http://dataclouds.cninfo.com.cn/b.PDF","raw":{"$SC":"1","$ZQDM":"688635"}}],"quote_source":null,"sources":[{"resource":"list/func_jysjk102_1.jsn","attempts":1,"stale":false}]})");
    require(supervision_history.at("passed").as_bool(),
            "exchange supervision history contract failed");

    const auto tender_offers = tdx::evaluate_api_contract_response(
        "tender-offers-live", 200, "application/json",
        R"({"schema":"tdx-market-tender-offers-native-v1","availability":"live","filters":{"market":null,"code":null},"summary":{"rows":169,"unique_securities":137,"by_status":{"completed":144,"failed":19}},"records":[{"offer_id":"SH600491:20260805:169","security":{"market":"sh","code":"600491","name":"ST龙元"},"announcement_date":"2026-08-05","status":"要约进行中","status_category":"active","planned_shares_10k":9178.6,"planned_shares":91786000,"planned_funds_10k_yuan":11473.25,"planned_funds_yuan":114732500,"delisting_flag":false,"purpose":"维护投资者利益","raw":{"$SC":"1","$ZQDM":"600491","ngs":"9178.60","nzzj":"11473.25"}},{"offer_id":"SZ300955:20260731:168","security":{"market":"sz","code":"300955","name":"嘉亨家化"},"announcement_date":"2026-07-31","status":"要约完成","status_category":"completed","planned_shares_10k":2126.88,"planned_shares":21268800,"planned_funds_10k_yuan":70633.68,"planned_funds_yuan":706336800,"delisting_flag":false,"purpose":"巩固控制权","raw":{}}],"sources":[{"resource":"list/func_yysg101_1.jsn","attempts":1,"stale":false}]})");
    require(tender_offers.at("passed").as_bool(), "tender-offer market contract failed");

    const auto tender_offers_bad_units = tdx::evaluate_api_contract_response(
        "tender-offers-live", 200, "application/json",
        R"({"schema":"tdx-market-tender-offers-native-v1","availability":"live","filters":{},"summary":{"rows":169,"unique_securities":137,"by_status":{"completed":144,"failed":19}},"records":[{"offer_id":"SH600491","security":{"market":"sh","code":"600491"},"announcement_date":"2026-08-05","status":"要约进行中","status_category":"active","planned_shares_10k":9178.6,"planned_shares":9178.6,"planned_funds_10k_yuan":11473.25,"planned_funds_yuan":11473.25,"delisting_flag":false,"purpose":"目的","raw":{}}],"sources":[{"resource":"list/func_yysg101_1.jsn","attempts":1,"stale":false}]})");
    require(!tender_offers_bad_units.at("passed").as_bool(),
            "tender-offer contract must reject unconverted ten-thousand units");

    const auto stock_tender_offers = tdx::evaluate_api_contract_response(
        "stock-tender-offers-live", 200, "application/json",
        R"({"schema":"tdx-market-tender-offers-native-v1","availability":"live","filters":{"market":"sh","code":"600491"},"summary":{"rows":1,"unique_securities":1,"by_status":{"active":1}},"records":[{"offer_id":"SH600491:20260805:169","security":{"market":"sh","code":"600491","name":"ST龙元"},"announcement_date":"2026-08-05","status":"要约进行中","status_category":"active","planned_shares_10k":9178.6,"planned_shares":91786000,"planned_funds_10k_yuan":11473.25,"planned_funds_yuan":114732500,"delisting_flag":false,"purpose":"维护投资者利益","raw":{"$SC":"1","$ZQDM":"600491"}}],"sources":[{"resource":"list/func_yysg101_1.jsn","attempts":1,"stale":false}]})");
    require(stock_tender_offers.at("passed").as_bool(), "stock tender-offer contract failed");

    const auto active_lhb_ranking = tdx::evaluate_api_contract_response(
        "active-lhb-ranking-live", 200, "application/json",
        R"({"schema":"tdx-market-active-lhb-native-v1","mode":"ranking","period":"5d","availability":"live","period_summaries":{"5d":{},"month":{},"half-year":{}},"rankings":[{"security":{"market":"sz","code":"000820"},"event_count":7,"buy_amount_yuan":222045798,"sell_amount_yuan":254709336,"net_buy_amount_yuan":-32663538,"raw":{"cs":"7"}},{"security":{"market":"bj","code":"920176"},"event_count":6,"buy_amount_yuan":195330306.16,"sell_amount_yuan":154070452.87,"net_buy_amount_yuan":41259853.29,"raw":{"cs":"6"}}],"sources":[{"resource":"list/func_hylhb101_1.jsn"},{"resource":"list/func_hylhb102_1.jsn"},{"resource":"list/func_hylhb104_1.jsn"}]})");
    require(active_lhb_ranking.at("passed").as_bool(), "active LHB ranking contract failed");

    const auto active_lhb_unsorted = tdx::evaluate_api_contract_response(
        "active-lhb-ranking-live", 200, "application/json",
        R"({"schema":"tdx-market-active-lhb-native-v1","mode":"ranking","period":"5d","availability":"live","period_summaries":{"5d":{},"month":{},"half-year":{}},"rankings":[{"security":{"market":"sz","code":"000820"},"event_count":6,"buy_amount_yuan":1,"sell_amount_yuan":2,"net_buy_amount_yuan":-1,"raw":{}},{"security":{"market":"bj","code":"920176"},"event_count":7,"buy_amount_yuan":2,"sell_amount_yuan":1,"net_buy_amount_yuan":1,"raw":{}}],"sources":[{"resource":"list/func_hylhb101_1.jsn"},{"resource":"list/func_hylhb102_1.jsn"},{"resource":"list/func_hylhb104_1.jsn"}]})");
    require(!active_lhb_unsorted.at("passed").as_bool(),
            "active LHB ranking contract must reject broken ordering");

    const auto active_lhb_security = tdx::evaluate_api_contract_response(
        "active-lhb-security-live", 200, "application/json",
        R"({"schema":"tdx-market-active-lhb-native-v1","mode":"security","period":"half-year","availability":"live","period_summaries":{"5d":{},"month":{},"half-year":{}},"selected_ranking":{"security":{"market":"sh","code":"603459"},"unit_id":"14001"},"rankings":[{"security":{"market":"sh","code":"603459"},"event_count":64,"buy_amount_yuan":100,"sell_amount_yuan":90,"net_buy_amount_yuan":10,"raw":{}}],"events":[{"event_date":"2026-08-06","event_type":"日换手率达到20%","change_pct":10,"turnover_rate_pct":20,"raw":{}},{"event_date":"2026-08-05","event_type":"日价格涨幅偏离值达到7%","change_pct":9.9,"turnover_rate_pct":18,"raw":{}}],"sources":[{"resource":"list/func_hylhb101_1.jsn"},{"resource":"list/func_hylhb102_1.jsn"},{"resource":"list/func_hylhb104_1.jsn"},{"resource":"hylhb14001/1603459.jsn"}]})");
    require(active_lhb_security.at("passed").as_bool(), "active LHB security contract failed");

    const auto state_owned_groups = tdx::evaluate_api_contract_response(
        "state-owned-groups-live", 200, "application/json",
        R"({"schema":"tdx-market-state-owned-reform-native-v1","view":"groups","dimension":"integration","availability":"live","summary":{"group_count":111,"group_relationships":1968,"unique_grouped_securities":899,"member_count_mismatches":0,"restructuring_rows":49},"groups":[{"group_id":"gl2111","name":"央企","member_count":2,"members":[{"code":"000021"},{"code":"000028"}]},{"group_id":"gl197","name":"央企整合","member_count":1,"members":[{"code":"000050"}]}],"sources":[{"resource":"list/func_gqgg101_1.jsn"},{"resource":"list/func_gqgg102_1.jsn"},{"resource":"list/func_gqgg103_1.jsn"},{"resource":"list/func_gqgg104_1.jsn"},{"resource":"list/func_gqgg106_1.jsn"}]})");
    require(state_owned_groups.at("passed").as_bool(), "state-owned groups contract failed");

    const auto state_owned_group_detail = tdx::evaluate_api_contract_response(
        "state-owned-group-detail-live", 200, "application/json",
        R"({"schema":"tdx-market-state-owned-reform-native-v1","view":"group","dimension":"industry","availability":"live","summary":{"group_count":111,"group_relationships":1968,"unique_grouped_securities":899,"member_count_mismatches":0,"restructuring_rows":49},"groups":[{"group_id":"gl197","name":"央企整合"}],"details":[{"security":{"market":"sz","code":"000050"},"actual_controller":"中国航空工业集团有限公司","logic":"科研院所类央企及其下属上市公司","returns_pct":{"3d":20}}],"quote_source":{"command":"0x054C","received":1},"sources":[{"resource":"list/func_gqgg101_1.jsn"},{"resource":"list/func_gqgg102_1.jsn"},{"resource":"list/func_gqgg103_1.jsn"},{"resource":"list/func_gqgg104_1.jsn"},{"resource":"list/func_gqgg106_1.jsn"},{"resource":"gqgg/gl197.jsn"}]})");
    require(!state_owned_group_detail.at("passed").as_bool(),
            "state-owned group detail fixture must enforce 30-row coverage");

    auto state_owned_group_body = tdx::Json::parse(
        R"({"schema":"tdx-market-state-owned-reform-native-v1","view":"group","dimension":"industry","availability":"live","summary":{"group_count":111,"group_relationships":1968,"unique_grouped_securities":899,"member_count_mismatches":0,"restructuring_rows":49},"groups":[{"group_id":"gl197","name":"央企整合"}],"details":[],"quote_source":{"command":"0x054C","received":30},"sources":[{"resource":"list/func_gqgg101_1.jsn"},{"resource":"list/func_gqgg102_1.jsn"},{"resource":"list/func_gqgg103_1.jsn"},{"resource":"list/func_gqgg104_1.jsn"},{"resource":"list/func_gqgg106_1.jsn"},{"resource":"gqgg/gl197.jsn"}]})");
    for (int i = 0; i < 30; ++i) {
        auto row = tdx::Json::object();
        row["security"] = tdx::Json::object();
        row["security"]["market"] = "sz";
        row["security"]["code"] = "000050";
        row["actual_controller"] = "中国航空工业集团有限公司";
        row["logic"] = "科研院所类央企及其下属上市公司";
        row["returns_pct"] = tdx::Json::object();
        row["returns_pct"]["3d"] = 20.0;
        state_owned_group_body["details"].push_back(std::move(row));
    }
    const auto state_owned_group_ok = tdx::evaluate_api_contract_response(
        "state-owned-group-detail-live", 200, "application/json", state_owned_group_body.dump(-1));
    require(state_owned_group_ok.at("passed").as_bool(),
            "state-owned group detail contract failed");

    const auto state_owned_restructuring = tdx::evaluate_api_contract_response(
        "state-owned-restructuring-live", 200, "application/json",
        R"json({"schema":"tdx-market-state-owned-reform-native-v1","view":"restructuring","dimension":"industry","availability":"live","summary":{"group_count":111,"group_relationships":1968,"unique_grouped_securities":899,"member_count_mismatches":0,"restructuring_rows":49},"restructuring":[{"security":{"market":"sh","code":"600963"},"capital_operation":"定增募资","explanation":"剥离亏损资产，注入盈利资产","as_of_date":"2026-03-31","raw":{"DATE":"20260331"}},{"security":{"market":"sz","code":"002106"},"capital_operation":"重组(可能)","explanation":"产业资产整合预期","as_of_date":null,"raw":{}}],"sources":[{"resource":"list/func_gqgg101_1.jsn"},{"resource":"list/func_gqgg102_1.jsn"},{"resource":"list/func_gqgg103_1.jsn"},{"resource":"list/func_gqgg104_1.jsn"},{"resource":"list/func_gqgg106_1.jsn"}]})json");
    require(state_owned_restructuring.at("passed").as_bool(),
            "state-owned restructuring contract failed");
}

} // namespace recon_contract_test
