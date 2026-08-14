#include "recon_contract_test_support.hpp"

namespace recon_contract_test {

void run_realtime_corporate_contracts() {
    const auto intelligence_highlights = tdx::evaluate_api_contract_response(
        "intelligence-highlights-live", 200, "application/json",
        R"({"schema":"tdx-market-intelligence-native-v1","view":"highlights","availability":"live","highlight_summary":{"securities":300,"names_resolved":300,"type_count":12},"records":[{"security":{"market":"sh","code":"603986","name":"兆易创新","name_resolved":true},"highlight_count":16,"primary_highlight_type":"净利率较高","highlight_detail":"净利率高达35.16%","raw":{"$SC":"1","$ZQDM":"603986"}},{"security":{"market":"sh","code":"601899","name":"紫金矿业","name_resolved":true},"highlight_count":15,"primary_highlight_type":"高ROE","highlight_detail":"平均ROE大于20%持续5年以上","raw":{"$SC":"1","$ZQDM":"601899"}}],"sources":[{"resource":"list/func_ldph101_1.jsn","attempts":1,"stale":false}]})");
    require(intelligence_highlights.at("passed").as_bool(),
            "intelligence highlights contract failed");

    const auto intelligence_highlights_unsorted = tdx::evaluate_api_contract_response(
        "intelligence-highlights-live", 200, "application/json",
        R"({"schema":"tdx-market-intelligence-native-v1","view":"highlights","availability":"live","highlight_summary":{"securities":300,"names_resolved":300,"type_count":12},"records":[{"security":{"code":"603986","name_resolved":true},"highlight_count":10,"primary_highlight_type":"高ROE","highlight_detail":"详情","raw":{}},{"security":{"code":"601899","name_resolved":true},"highlight_count":15,"primary_highlight_type":"高ROE","highlight_detail":"详情","raw":{}}],"sources":[{"resource":"list/func_ldph101_1.jsn","attempts":1,"stale":false}]})");
    require(!intelligence_highlights_unsorted.at("passed").as_bool(),
            "intelligence highlights contract must reject broken ordering");

    const auto stock_intelligence_highlight = tdx::evaluate_api_contract_response(
        "stock-intelligence-highlight-live", 200, "application/json",
        R"({"schema":"tdx-market-intelligence-native-v1","view":"security","security":{"market":"sh","code":"600519","name":"贵州茅台"},"highlights":[{"primary_highlight_type":"高ROE","highlight_detail":"平均ROE大于20%持续5年以上","raw":{}}],"sources":[{"resource":"list/func_ldph101_1.jsn","attempts":1,"stale":false}]})");
    require(stock_intelligence_highlight.at("passed").as_bool(),
            "stock intelligence highlight contract failed");

    const auto stream_live = tdx::evaluate_api_contract_response(
        "market-stream-live", 200, "text/event-stream; charset=utf-8",
        "retry: 2000\n\nid: 1\nevent: snapshot\ndata: "
        "{\"schema\":\"tdx-market-l1-stream-event-v1\",\"type\":\"snapshot\",\"delivery\":\"local-"
        "sse\",\"record\":{\"code\":\"000001\"},\"source\":{\"session\":{\"persistent\":true,"
        "\"endpoint_source\":\"connect.cfg:hqhost-primary-first\",\"primary_configured\":true,"
        "\"endpoint_pool_size\":3}},\"fast_hq_boundary\":{\"fast_hq_subscribe_used\":false}}\n\n");
    require(stream_live.at("passed").as_bool(), "market stream SSE contract failed");

    const auto recent_large_unlocks = tdx::evaluate_api_contract_response(
        "recent-large-unlocks-live", 200, "application/json",
        R"({"schema":"tdx-market-unlocks-native-v1","view":"recent-large","mode":"recent-large","availability":"live","summary":{"events":48,"unique_securities":48,"implemented_events":40,"pending_events":8,"first_date":"20260706","last_date":"20260812"},"events":[{"date":"20260812","progress":"待解禁","source_kind":"recent-large-window","security":{"market":"sz","code":"000001"},"unlock_to_total_ratio":0.2442,"unlock_to_total_pct":24.42,"raw":{"jjgzb":0.2442}},{"date":"20260811","progress":"待解禁","source_kind":"recent-large-window","security":{"market":"sh","code":"600000"},"unlock_to_total_ratio":0.2,"unlock_to_total_pct":20,"raw":{}}],"details":[],"detail_errors":[],"sources":[{"resource":"list/func_jqgz103_1.jsn","endpoint":"test:7709","attempts":1,"stale":false}]})");
    require(recent_large_unlocks.at("passed").as_bool(), "recent large-unlocks contract failed");

    const auto recent_large_unlocks_bad_ratio = tdx::evaluate_api_contract_response(
        "recent-large-unlocks-live", 200, "application/json",
        R"({"schema":"tdx-market-unlocks-native-v1","view":"recent-large","mode":"recent-large","availability":"live","summary":{"events":48,"unique_securities":48,"implemented_events":40,"pending_events":8},"events":[{"date":"20260812","source_kind":"recent-large-window","security":{},"unlock_to_total_ratio":0.2442,"unlock_to_total_pct":0.2442,"raw":{}},{"date":"20260811"}],"details":[],"detail_errors":[],"sources":[{"resource":"list/func_jqgz103_1.jsn"}]})");
    require(!recent_large_unlocks_bad_ratio.at("passed").as_bool(),
            "recent large-unlocks contract must reject ratio/percent unit confusion");

    auto monthly_unlocks_body = tdx::Json::parse(
        R"json({"schema":"tdx-market-unlocks-native-v1","view":"monthly-pressure","mode":"monthly-pressure","availability":"live","summary":{"months":25,"formula_checked":25,"formula_mismatches":0},"months":[{"month":"202608","unlock_market_value_yuan":121292277690,"unlock_shares":7007545800,"source_resource":"list/func_dxfjj101_1.jsn","raw":{"jjsz":"1212.9227769","jjsl":"70.075458"}}],"events":[],"details":[],"detail_errors":[],"sources":[{"resource":"list/func_dxfjj101_1.jsn"}]})json");
    for (int i = 1; i < 25; ++i) {
        auto row = monthly_unlocks_body.at("months").as_array().front();
        row["month"] = std::string("2027") + (i < 10 ? "0" : "") + std::to_string(i + 1);
        monthly_unlocks_body["months"].push_back(std::move(row));
    }
    require(tdx::evaluate_api_contract_response("unlock-monthly-pressure-live", 200,
                                                "application/json", monthly_unlocks_body.dump(-1))
                .at("passed")
                .as_bool(),
            "monthly unlock-pressure contract failed");
    monthly_unlocks_body["months"].as_array()[0]["unlock_market_value_yuan"] = 1212.9227769;
    require(!tdx::evaluate_api_contract_response("unlock-monthly-pressure-live", 200,
                                                 "application/json", monthly_unlocks_body.dump(-1))
                 .at("passed")
                 .as_bool(),
            "monthly unlock-pressure contract must enforce hundred-million units");

    const auto private_placements = tdx::evaluate_api_contract_response(
        "private-placements-live", 200, "application/json",
        R"json({"schema":"tdx-futures-issuance-native-v1","section":"placements","placement_summary":{"records":1339,"unique_securities":1008,"actual_gross_10k_yuan":198679514.95,"expected_raise_10k_yuan":90321735.22,"lifecycle_counts":[{"label":"implemented","count":288},{"label":"implemented-locked","count":267},{"label":"implemented-unlocked","count":215},{"label":"plan-active","count":411},{"label":"plan-stopped","count":80},{"label":"registered","count":78}]},"private_placements":[{"sort_date":"20260801","source_resource":"list/func_qxfa601_1.jsn","security":{"market":"sh","code":"603076"},"dates":{"registered":"20260801"},"raw":{"fajd":"注册生效"}},{"sort_date":"20260731","source_resource":"list/func_qxfa601_1.jsn","security":{"market":"sh","code":"688501"},"dates":{},"raw":{}}],"sources":[{"resource":"list/func_qxfa201_1.jsn"},{"resource":"list/func_qxfa202_1.jsn"},{"resource":"list/func_qxfa301_1.jsn"},{"resource":"list/func_qxfa302_1.jsn"},{"resource":"list/func_qxfa402_1.jsn"},{"resource":"list/func_qxfa601_1.jsn"}]})json");
    require(private_placements.at("passed").as_bool(),
            "private placements lifecycle contract failed");

    const auto private_placements_missing_source = tdx::evaluate_api_contract_response(
        "private-placements-live", 200, "application/json",
        R"json({"schema":"tdx-futures-issuance-native-v1","section":"placements","placement_summary":{"records":1339,"unique_securities":1008,"actual_gross_10k_yuan":198679514.95,"expected_raise_10k_yuan":90321735.22,"lifecycle_counts":[{"label":"implemented","count":288},{"label":"implemented-locked","count":267},{"label":"implemented-unlocked","count":215},{"label":"plan-active","count":411},{"label":"plan-stopped","count":80},{"label":"registered","count":78}]},"private_placements":[{"sort_date":"20260801","source_resource":"list/func_qxfa601_1.jsn","security":{},"dates":{},"raw":{}}],"sources":[]})json");
    require(!private_placements_missing_source.at("passed").as_bool(),
            "private placements contract must enforce six exact sources");

    const auto rights_offerings = tdx::evaluate_api_contract_response(
        "rights-offerings-live", 200, "application/json", [&]() {
            tdx::Json body = tdx::Json::object();
            body["schema"] = "tdx-futures-issuance-native-v1";
            body["section"] = "rights";
            body["rights_summary"] = tdx::Json::object();
            body["rights_summary"]["records"] = 105;
            body["rights_summary"]["unique_securities"] = 100;
            body["rights_summary"]["implemented_raised_yuan"] = 242130257567.0;
            body["rights_summary"]["planned_raised_yuan"] = 169898434200.0;
            body["rights_summary"]["phase_counts"] = tdx::Json::object();
            body["rights_summary"]["phase_counts"]["implemented"] = 70;
            body["rights_summary"]["phase_counts"]["deliberating"] = 6;
            body["rights_summary"]["phase_counts"]["abnormal"] = 29;
            body["rights_summary"]["status_counts"] = tdx::Json::object();
            body["rights_summary"]["status_counts"]["配股实施"] = 70;
            body["rights_summary"]["status_counts"]["股东大会通过"] = 6;
            body["rights_summary"]["status_counts"]["已终止"] = 27;
            body["rights_summary"]["status_counts"]["已延期"] = 1;
            body["rights_summary"]["status_counts"]["未获准"] = 1;
            body["rights_offerings"] = tdx::Json::array();
            for (int index = 0; index < 105; ++index) {
                const auto phase = index < 70   ? "implemented"
                                   : index < 76 ? "deliberating"
                                                : "abnormal";
                const auto stage = index < 70     ? "配股实施"
                                   : index < 76   ? "股东大会通过"
                                   : index < 103  ? "已终止"
                                   : index == 103 ? "已延期"
                                                  : "未获准";
                tdx::Json row = tdx::Json::object();
                row["kind"] = "rights-offering";
                row["security"] = tdx::Json::object();
                row["security"]["code"] = "600153";
                row["phase"] = phase;
                row["phase_label"] = phase == std::string("implemented")    ? "配股实施"
                                     : phase == std::string("deliberating") ? "配股审议中"
                                                                            : "配股进度异常";
                row["stage"] = stage;
                row["announcement_date"] = "20230429";
                row["sort_date"] = "20230523";
                row["amount_semantics"] =
                    phase == std::string("implemented") ? "actual" : "planned";
                row["event_id"] = "rights:" + std::to_string(index);
                row["rights_per_10_shares"] = 3.5;
                row["offered_shares"] = 1051424968.0;
                row["raised_yuan"] = 4980000000.0;
                row["raw"] = tdx::Json::object();
                row["raw"]["bl"] = "3.5";
                row["raw"]["sl"] = "1051424968";
                row["raw"]["zj"] = "4980000000";
                body["rights_offerings"].push_back(std::move(row));
            }
            body["sources"] = tdx::Json::array();
            for (const auto *resource : {"list/func_qxfa107_1.jsn", "list/func_qxfa108_1.jsn",
                                         "list/func_qxfa109_1.jsn"}) {
                tdx::Json source = tdx::Json::object();
                source["resource"] = resource;
                body["sources"].push_back(std::move(source));
            }
            return body.dump(-1);
        }());
    require(rights_offerings.at("passed").as_bool(), "rights offerings contract failed");

    const auto preferred_shares = tdx::evaluate_api_contract_response(
        "preferred-shares-live", 200, "application/json", [&]() {
            tdx::Json body = tdx::Json::object();
            body["schema"] = "tdx-futures-issuance-native-v1";
            body["section"] = "preferred-shares";
            body["preferred_share_summary"] = tdx::Json::object();
            body["preferred_share_summary"]["records"] = 55;
            body["preferred_share_summary"]["unique_underlying_securities"] = 33;
            body["preferred_share_summary"]["unique_preferred_codes"] = 55;
            body["preferred_share_summary"]["total_issue_size_yuan"] = 921551000000.0;
            body["preferred_shares"] = tdx::Json::array();
            for (int index = 0; index < 55; ++index) {
                tdx::Json row = tdx::Json::object();
                row["kind"] = "preferred-share";
                row["record_id"] = "preferred-share:1:600998:" + std::to_string(360000 + index);
                row["underlying_security"] = tdx::Json::object();
                row["underlying_security"]["code"] = "600998";
                row["preferred_code"] = std::to_string(360000 + index);
                row["listing_date"] = "20240924";
                row["source_resource"] = "list/func_yxg101_3.jsn";
                row["issue_shares"] = 17900000.0;
                row["issue_size_yuan"] = 1790000000.0;
                row["raw"] = tdx::Json::object();
                row["raw"]["fxsl"] = "1790.00";
                row["raw"]["fxgm"] = "17.90";
                body["preferred_shares"].push_back(std::move(row));
            }
            body["sources"] = tdx::Json::array();
            tdx::Json source = tdx::Json::object();
            source["resource"] = "list/func_yxg101_3.jsn";
            body["sources"].push_back(std::move(source));
            return body.dump(-1);
        }());
    require(preferred_shares.at("passed").as_bool(), "preferred-shares contract failed");

    const auto employee_share_plans = tdx::evaluate_api_contract_response(
        "employee-share-plans-live", 200, "application/json",
        R"json({"schema":"tdx-market-employees-native-v1","view":"share-plans","share_plan_summary":{"plans":1177,"unique_securities":840,"active_plans":37,"completed_plans":1140,"purchase_shares":14108054632,"purchase_amount_yuan":84350689304.62},"share_plans":[{"sort_date":"20260804","active":true,"source_resource":"list/func_qxfa501_1.jsn","security":{"market":"sz","code":"002203"},"dates":{"implementation_start":"20260804"},"purchase_average_price":10,"purchase_shares":1000000,"purchase_amount_yuan":10000000,"raw":{"gmgs":"100"}},{"sort_date":"20260722","active":true,"source_resource":"list/func_qxfa501_1.jsn","security":{"market":"sh","code":"600000"},"dates":{},"purchase_shares":500000,"purchase_amount_yuan":4000000,"raw":{}}],"sources":[{"resource":"list/func_ygxc101_1.jsn"},{"resource":"list/func_qxfa501_1.jsn"}]})json");
    require(employee_share_plans.at("passed").as_bool(), "employee share-plans contract failed");

    const auto employee_share_plans_bad_units = tdx::evaluate_api_contract_response(
        "employee-share-plans-live", 200, "application/json",
        R"json({"schema":"tdx-market-employees-native-v1","view":"share-plans","share_plan_summary":{"plans":1177,"unique_securities":840,"active_plans":37,"completed_plans":1140,"purchase_shares":1410805.4632,"purchase_amount_yuan":8435068.93},"share_plans":[{"sort_date":"20260804","active":true,"source_resource":"list/func_qxfa501_1.jsn","security":{},"dates":{},"purchase_shares":100,"purchase_amount_yuan":1000,"raw":{}}],"sources":[{"resource":"list/func_qxfa501_1.jsn"}]})json");
    require(!employee_share_plans_bad_units.at("passed").as_bool(),
            "employee share-plans contract must enforce single-share and yuan units");

    const auto hk_events = tdx::evaluate_api_contract_response(
        "hk-events-live", 200, "application/json",
        R"json({"schema":"tdx-market-hk-events-native-v1","view":"all","match_count":5674,"summary":{"dividends":1355,"holding_disclosures":1554,"short_selling":2565,"listing_applications":200},"rows":[{"kind":"dividend","date":"20260806","security":{"market":"hk","code":"00002"},"raw":{}},{"kind":"short-selling","date":"20260806","security":{"market":"hk","code":"00001"},"short_shares_10k":136.35,"short_shares":1363500,"short_amount_10k_currency_units":10082.1225,"short_amount_currency_units":100821225,"turnover_currency_units":441471392,"short_turnover_pct":22.8375443635,"raw":{}}],"sources":[{"resource":"list/func_ggrl102_1.jsn"},{"resource":"list/func_ggrl103_1.jsn"},{"resource":"list/func_ggrl104_1.jsn"},{"resource":"list/func_ggrl105_1.jsn"}]})json");
    require(hk_events.at("passed").as_bool(), "HK event family contract failed");

    const auto hk_events_bad_units = tdx::evaluate_api_contract_response(
        "hk-events-live", 200, "application/json",
        R"json({"schema":"tdx-market-hk-events-native-v1","view":"all","match_count":5674,"summary":{"dividends":1355,"holding_disclosures":1554,"short_selling":2565,"listing_applications":200},"rows":[{"kind":"short-selling","date":"20260806","short_shares_10k":136.35,"short_shares":136.35,"short_amount_10k_currency_units":10082.1225,"short_amount_currency_units":10082.1225,"turnover_currency_units":44147.1392,"short_turnover_pct":22.8375443635}],"sources":[{"resource":"list/func_ggrl102_1.jsn"},{"resource":"list/func_ggrl103_1.jsn"},{"resource":"list/func_ggrl104_1.jsn"},{"resource":"list/func_ggrl105_1.jsn"}]})json");
    require(!hk_events_bad_units.at("passed").as_bool(),
            "HK event contract must reject unconverted ten-thousand units");

    auto hk_short_history_body = tdx::Json::parse(
        R"json({"schema":"tdx-market-hk-short-history-native-v1","security":{"market":"hk","market_id":31,"code":"00700","security_id":"HK00700"},"summary":{"history_count":30},"reconciliation":{"overlap_day_count":1,"exact_match_count":1,"mismatch_count":0,"all_overlaps_exact":true},"history":[],"source":{"auxiliary_field":"hk_short_volume","transport":"tdx-7727-0x23ff","volume_lot_size_shares":100},"sources":[{"resource":"list/func_ggrl104_1.jsn"}]})json");
    for (int i = 0; i < 30; ++i) {
        auto row = tdx::Json::object();
        row["date"] = "2026-07-01";
        row["short_shares"] = 1000000 + i;
        row["volume_lots"] = 20000 + i;
        row["volume_shares"] = (20000 + i) * 100;
        row["short_share_volume_pct"] = 50.0;
        row["event_exact_match"] = i == 29 ? tdx::Json(true) : tdx::Json(nullptr);
        hk_short_history_body["history"].push_back(std::move(row));
    }
    const auto hk_short_history = tdx::evaluate_api_contract_response(
        "hk-short-history-live", 200, "application/json", hk_short_history_body.dump(-1));
    require(hk_short_history.at("passed").as_bool(), "HK short-history contract failed");

    hk_short_history_body["history"].as_array().front()["volume_shares"] = 20000;
    const auto hk_short_history_bad_lot = tdx::evaluate_api_contract_response(
        "hk-short-history-live", 200, "application/json", hk_short_history_body.dump(-1));
    require(!hk_short_history_bad_lot.at("passed").as_bool(),
            "HK short-history contract must enforce 100-share lots");

    const auto special_situations = tdx::evaluate_api_contract_response(
        "special-situations-live", 200, "application/json",
        R"json({"schema":"tdx-market-special-situations-native-v1","view":"all","mode":"catalog","match_count":179,"quote_source":null,"quote_errors":[],"summary":{"mergers":4,"b_to_h":3,"market_cap_warnings":172,"market_cap_unique_securities":151,"twenty_day_only":90,"one_year_only":62,"both_triggers":20},"records":[{"kind":"b-to-h","currency":"港币","cash_option_price":12.68,"primary_security":{"market":"sz","code":"200581"},"related_security":{"market":"sz","code":"000581"},"source_resource":"list/func_agtl102_1.jsn","raw":{}},{"kind":"market-cap-risk","twenty_day_triggered":true,"one_year_triggered":true,"sample_indexes":["创业板指"],"primary_security":{"market":"sz","code":"300672"},"related_security":null,"source_resource":"list/func_cdgc101_1.jsn","raw":{}},{"kind":"merger","absorber_exchange_price":13.21,"absorbed_cash_option_price":15.36,"primary_security":{"market":"sh","code":"600449"},"related_security":{"market":"bj","code":"834082","security_id":"BJ834082"},"source_resource":"list/func_agtl101_1.jsn","raw":{"$SC1":""}}],"sources":[{"resource":"list/func_agtl101_1.jsn"},{"resource":"list/func_agtl102_1.jsn"},{"resource":"list/func_cdgc101_1.jsn"}]})json");
    require(special_situations.at("passed").as_bool(), "special situations contract failed");

    const auto special_situations_bad_market = tdx::evaluate_api_contract_response(
        "special-situations-live", 200, "application/json",
        R"json({"schema":"tdx-market-special-situations-native-v1","view":"all","mode":"catalog","match_count":179,"quote_source":null,"quote_errors":[],"summary":{"mergers":4,"b_to_h":3,"market_cap_warnings":172,"market_cap_unique_securities":151,"twenty_day_only":90,"one_year_only":62,"both_triggers":20},"records":[{"kind":"b-to-h","currency":"港币","cash_option_price":12.68,"primary_security":{"market":"sz","code":"200581"},"related_security":{"market":"sz","code":"000581"},"source_resource":"list/func_agtl102_1.jsn","raw":{}},{"kind":"market-cap-risk","twenty_day_triggered":true,"one_year_triggered":true,"sample_indexes":["创业板指"],"primary_security":{"market":"sz","code":"300672"},"source_resource":"list/func_cdgc101_1.jsn","raw":{}},{"kind":"merger","absorber_exchange_price":13.21,"absorbed_cash_option_price":15.36,"primary_security":{"market":"sh","code":"600449"},"related_security":{"market":"sh","code":"834082","security_id":"SH834082"},"source_resource":"list/func_agtl101_1.jsn","raw":{"$SC1":""}}],"sources":[{"resource":"list/func_agtl101_1.jsn"},{"resource":"list/func_agtl102_1.jsn"},{"resource":"list/func_cdgc101_1.jsn"}]})json");
    require(!special_situations_bad_market.at("passed").as_bool(),
            "special situations contract must enforce blank-market recovery");

    const auto corporate_transitions = tdx::evaluate_api_contract_response(
        "corporate-transitions-live", 200, "application/json",
        R"json({"schema":"tdx-market-special-situations-native-v1","view":"all","match_count":4036,"quote_source":null,"summary":{"mergers":4,"b_to_h":3,"market_cap_warnings":172,"major_restructuring_plans":311,"major_restructuring_reviews":11,"major_restructuring_completed":142,"ordinary_merger_plans":2501,"neeq_transfer_plans":206,"neeq_regulation_events":683,"neeq_transfer_completed":3},"records":[{"kind":"major-restructuring-completed","transaction_amount_yuan":40600910000,"transaction_amount_100m_yuan":406.0091},{"kind":"neeq-transfer-plan","report_period":"20260331","net_assets_yuan":2945756938.18,"revenue_yuan":199293848.55},{"kind":"neeq-regulation","regulation_reason":"信息披露违规","regulation_measure":"出具警示函"},{"kind":"neeq-transfer-completed","acceptance_date":"20220101","listing_date":"20220818","listing_venue_after":"深交所创业板"}],"sources":[{"resource":"list/func_qxfa105_1.jsn"},{"resource":"list/func_qxfa106_1.jsn"},{"resource":"list/func_qxfa110_1.jsn"},{"resource":"list/func_qxfa111_1.jsn"},{"resource":"list/func_xsbtj101_1.jsn"},{"resource":"list/func_xsbtj102_1.jsn"},{"resource":"list/func_yzb101_1.jsn"}]})json");
    require(corporate_transitions.at("passed").as_bool(), "corporate transitions contract failed");

    const auto
        exchange_funds =
            tdx::
                evaluate_api_contract_response("exchange-funds-live",
                                               200, "application/json", R"json({"schema":"tdx-market-exchange-funds-native-v1","view":"all","mode":"catalog","match_count":1752,"quote_source":null,"quote_errors":[],"summary":{"etf_performance":1619,"cash_arbitrage":27,"cash_yield":13,"reits_issued":93,"reits_pipeline":0,"configured_empty_sources":1},"records":[{"kind":"etf-performance","security":{"market":"sh","code":"510300"},"close_price":4.2,"reference_close_5d":4.0,"change_5d_pct":5.0,"source_resource":"list/func_etfhq101.jsn","raw":{}},{"kind":"cash-arbitrage","security":{"market":"sh","code":"511600"},"seven_day_annualized_pct":0.908,"buy_redeem_interest_days":3,"theoretical_nav":100.00746301369863,"source_resource":"list/gxjty_etfjj103.jsn","raw":{}},{"kind":"cash-yield","source_market_id":34,"security":{"market":"sz","code":"159002"},"source_resource":"list/gxjty_etfjj104.jsn","raw":{}},{"kind":"cash-yield","source_market_id":34,"security":{"market":"sh","code":"519800"},"source_resource":"list/gxjty_etfjj104.jsn","raw":{}},{"kind":"reit-issued","security":{"market":"sz","code":"180101"},"subscription_price":2.31,"offering_total_units":900000000,"dates":{"inquiry":"20210525"},"source_resource":"list/func_reits101_1.jsn","raw":{}}],"sources":[{"resource":"list/func_etfhq101.jsn"},{"resource":"list/gxjty_etfjj103.jsn"},{"resource":"list/gxjty_etfjj104.jsn"},{"resource":"list/func_reits101_1.jsn"},{"resource":"list/func_reits102_1.jsn","row_count":1,"normalized_row_count":0,"blank_row_count":1}]})json");
    require(exchange_funds.at("passed").as_bool(), "exchange funds contract failed");

    const auto etf_share_ranking = tdx::evaluate_api_contract_response(
        "etf-share-ranking-live", 200, "application/json",
        R"json({"schema":"tdx-market-exchange-funds-native-v1","view":"etf-share-ranking","mode":"catalog","match_count":458,"quote_source":null,"summary":{"etf_share_ranking":458,"etf_share_ranking_nonzero_net_inflow":0},"records":[{"kind":"etf-share-ranking","latest_shares":25128487700,"net_inflow_yuan":0,"prior_week_scale_yuan":123149963168.1,"prior_month_scale_yuan":82496549840.2,"subscription_unit_10k_shares":90,"subscription_unit_shares":900000,"reference_instrument":{"market":"sh","code":"000300"},"source_resource":"list/func_tlfeyxetf101_1.jsn","raw":{"ZXSSDW":"90.00"}}],"sources":[{"resource":"list/func_tlfeyxetf101_1.jsn"}]})json");
    require(etf_share_ranking.at("passed").as_bool(), "ETF share ranking contract failed");

    const auto etf_share_bad_units = tdx::evaluate_api_contract_response(
        "etf-share-ranking-live", 200, "application/json",
        R"json({"schema":"tdx-market-exchange-funds-native-v1","view":"etf-share-ranking","mode":"catalog","match_count":458,"quote_source":null,"summary":{"etf_share_ranking":458,"etf_share_ranking_nonzero_net_inflow":0},"records":[{"kind":"etf-share-ranking","latest_shares":25128487700,"net_inflow_yuan":0,"prior_week_scale_yuan":123149963168.1,"prior_month_scale_yuan":82496549840.2,"subscription_unit_10k_shares":90,"subscription_unit_shares":90,"reference_instrument":{"market":"sh","code":"000300"},"source_resource":"list/func_tlfeyxetf101_1.jsn","raw":{}}],"sources":[{"resource":"list/func_tlfeyxetf101_1.jsn"}]})json");
    require(!etf_share_bad_units.at("passed").as_bool(),
            "ETF share ranking contract must enforce 10k-share conversion");

    const auto
        exchange_funds_bad_market = tdx::evaluate_api_contract_response("exchange-funds-live", 200,
                                                                        "application/json",
                                                                        R"json({"schema":"tdx-market-exchange-funds-native-v1","view":"all","mode":"catalog","match_count":1752,"quote_source":null,"quote_errors":[],"summary":{"etf_performance":1619,"cash_arbitrage":27,"cash_yield":13,"reits_issued":93,"reits_pipeline":0,"configured_empty_sources":1},"records":[{"kind":"etf-performance","security":{"market":"sh","code":"510300"},"close_price":4.2,"reference_close_5d":4.0,"change_5d_pct":5.0,"source_resource":"list/func_etfhq101.jsn","raw":{}},{"kind":"cash-arbitrage","security":{"market":"sh","code":"511600"},"seven_day_annualized_pct":0.908,"buy_redeem_interest_days":3,"theoretical_nav":100.00746301369863,"source_resource":"list/gxjty_etfjj103.jsn","raw":{}},{"kind":"cash-yield","source_market_id":34,"security":{"market":"sh","code":"159002"},"source_resource":"list/gxjty_etfjj104.jsn","raw":{}},{"kind":"cash-yield","source_market_id":34,"security":{"market":"sh","code":"519800"},"source_resource":"list/gxjty_etfjj104.jsn","raw":{}},{"kind":"reit-issued","security":{"market":"sz","code":"180101"},"subscription_price":2.31,"offering_total_units":900000000,"dates":{"inquiry":"20210525"},"source_resource":"list/func_reits101_1.jsn","raw":{}}],"sources":[{"resource":"list/func_etfhq101.jsn"},{"resource":"list/gxjty_etfjj103.jsn"},{"resource":"list/gxjty_etfjj104.jsn"},{"resource":"list/func_reits101_1.jsn"},{"resource":"list/func_reits102_1.jsn","row_count":1,"normalized_row_count":0,"blank_row_count":1}]})json");
    require(!exchange_funds_bad_market.at("passed").as_bool(),
            "exchange funds contract must enforce source market 34 routing");
}

} // namespace recon_contract_test
