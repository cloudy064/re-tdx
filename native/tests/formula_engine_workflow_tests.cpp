#include "formula_engine_test_support.hpp"

namespace formula_engine_test {

void run_workflow_tests() {
    const auto scan_formula =
        tdx::make_formula_source_definition("RESULT:CLOSE>REF(CLOSE,1);", "RISING", "selection");
    require(scan_formula.at("kind_key").as_string() == "selection" &&
                scan_formula.at("analysis").at("executable").as_bool() &&
                scan_formula.at("analysis").at("numeric_signal_safe").as_bool(),
            "custom selection definition carries native safety analysis");
    const auto scan = tdx::scan_formula_documents({sample(10)}, scan_formula);
    require(scan.at("evaluated").as_number() == 1 && scan.at("match_count").as_number() == 1,
            "custom selection source scanner latest match");
    const auto initial_diff = tdx::diff_formula_scan_results(tdx::Json::object(), scan);
    require(initial_diff.at("entered_count").as_number() == 1 &&
                initial_diff.at("active_count").as_number() == 1 &&
                initial_diff.at("changed").as_bool(),
            "formula watch initial snapshot enters every active match");
    auto expanded_scan = scan;
    expanded_scan["matches"].as_array()[0]["trigger_date"] = "2026-02-01";
    auto second_match = expanded_scan.at("matches").as_array()[0];
    second_match["market"] = "sh";
    second_match["code"] = "600000";
    second_match["security_id"] = "sh600000";
    expanded_scan["matches"].push_back(std::move(second_match));
    expanded_scan["match_count"] = 2;
    const auto changed_diff = tdx::diff_formula_scan_results(scan, expanded_scan);
    require(changed_diff.at("entered_count").as_number() == 1 &&
                changed_diff.at("updated_count").as_number() == 1 &&
                changed_diff.at("exited_count").as_number() == 0,
            "formula watch distinguishes new members from updated trigger payloads");
    const auto state =
        tdx::make_formula_scan_watch_state(expanded_scan, "fixture-configuration", 7);
    require(state.at("schema").as_string() == "tdx-formula-watch-state-v1" &&
                state.at("iteration").as_number() == 7 &&
                state.at("active_count").as_number() == 2 &&
                state.as_object().count("source_text") == 0,
            "formula watch state persists compact membership without source text");
    require(tdx::formula_scan_block_text(expanded_scan) == "0000001\r\n1600000\r\n",
            "formula watch exports deterministic sz/sh TDX block membership");
    auto sh_only_state = state;
    sh_only_state["active_matches"] = tdx::Json::array();
    sh_only_state["active_matches"].push_back(expanded_scan.at("matches").as_array()[1]);
    const auto exit_diff = tdx::diff_formula_scan_results(state, sh_only_state);
    require(exit_diff.at("exited_count").as_number() == 1 &&
                exit_diff.at("entered_count").as_number() == 0,
            "formula watch detects members leaving a persisted strategy pool");

    const auto expert = tdx::make_formula_source_definition("ENTERLONG:CROSS(CLOSE,MA(CLOSE,N));"
                                                            "EXITLONG:CROSS(MA(CLOSE,N),CLOSE);",
                                                            "TREND", "expert", {{"N", 3}});
    require(expert.at("kind_key").as_string() == "expert" &&
                expert.at("parameters").as_array().size() == 1 &&
                expert.at("analysis").at("numeric_signal_safe").as_bool(),
            "custom expert definition retains defaults and signal safety");
    const auto backtest = tdx::backtest_formula_document(sample(20), expert);
    require(backtest.at("engine").as_string() == "tdx-source-backtest-v1", "backtest engine");
    require(backtest.at("final_equity").is_number() && backtest.at("max_drawdown_pct").is_number(),
            "custom expert source backtest metrics");

    auto adjusted_sample = sample(20);
    adjusted_sample["adjustment_mode"] = "qfq";
    adjusted_sample["adjustment"] = tdx::Json::object();
    adjusted_sample["adjustment"]["mode"] = "qfq";
    adjusted_sample["adjustment"]["source_command"] = "0x000F";
    adjusted_sample["adjustment"]["method"] = "local-corporate-action-factor-v1";
    const auto adjusted_scan_formula =
        tdx::make_formula_source_definition("RESULT:TQFLAG=1;", "QFQ_SCAN", "selection");
    const auto adjusted_scan =
        tdx::scan_formula_documents({adjusted_sample}, adjusted_scan_formula);
    require(adjusted_scan.at("match_count").as_number() == 1 &&
                adjusted_scan.at("matches").as_array()[0].at("adjustment_mode").as_string() ==
                    "qfq",
            "scan match preserves the per-security adjustment mode");
    const auto adjusted_expert =
        tdx::make_formula_source_definition("ENTERLONG:TQFLAG=1 AND CROSS(CLOSE,MA(CLOSE,3));"
                                            "EXITLONG:TQFLAG()=1 AND CROSS(MA(CLOSE,3),CLOSE);",
                                            "QFQ_EXPERT", "expert");
    const auto adjusted_backtest = tdx::backtest_formula_document(adjusted_sample, adjusted_expert);
    require(adjusted_backtest.at("adjustment_mode").as_string() == "qfq" &&
                adjusted_backtest.at("adjustment").at("source_command").as_string() == "0x000F",
            "backtest preserves the complete adjustment audit metadata");

    auto finance_kline = sample(6);
    tdx::Json finance_context = tdx::Json::object();
    finance_context["series"] = tdx::Json::object();
    finance_context["series"]["FINVALUE#0"] = tdx::Json::object();
    for (const auto &bar : finance_kline.at("bars").as_array()) {
        const auto date = bar.at("date").as_string();
        if (date >= "2026-01-03")
            finance_context["series"]["FINVALUE#0"][date + "|" + bar.at("time").as_string()] =
                date >= "2026-01-05" ? 20250630 : 20250331;
    }
    finance_context["finance_point_in_time_mode"] =
        "archived-actual-full-report-next-bar-no-current-fallback";
    tdx::Json finance_expert = tdx::Json::object();
    finance_expert["code"] = "PITEXPERT";
    finance_expert["kind_key"] = "expert";
    finance_expert["source_text"] =
        "ENTERLONG:FINVALUE(0)=20250331; EXITLONG:FINVALUE(0)=20250630;";
    finance_expert["parameters"] = tdx::Json::array();
    const auto finance_backtest = tdx::backtest_formula_document(
        finance_kline, finance_expert, {}, 100000.0, 0.0, 0.0, &finance_context);
    require(
        finance_backtest.at("trade_count").as_number() == 1 &&
            finance_backtest.at("context_bindings").size() == 1 &&
            finance_backtest.at("context_metadata").at("finance_point_in_time_mode").as_string() ==
                "archived-actual-full-report-next-bar-no-current-fallback",
        "backtest must consume and expose strict point-in-time finance series");

    auto finance_scan_kline = finance_kline;
    finance_scan_kline["formula_context"] = finance_context;
    tdx::Json finance_scan_formula = tdx::Json::object();
    finance_scan_formula["code"] = "PITSCAN";
    finance_scan_formula["kind_key"] = "selection";
    finance_scan_formula["source_text"] = "FINVALUE(0)=20250630;";
    finance_scan_formula["parameters"] = tdx::Json::array();
    const auto finance_scan =
        tdx::scan_formula_documents({finance_scan_kline}, finance_scan_formula, {}, 1);
    require(finance_scan.at("match_count").as_number() == 1 &&
                finance_scan.at("matches")
                        .as_array()[0]
                        .at("context_metadata")
                        .at("finance_point_in_time_mode")
                        .as_string() == "archived-actual-full-report-next-bar-no-current-fallback",
            "scan matches must retain point-in-time finance audit metadata");

    const auto tcalc_analysis =
        tdx::analyze_formula_source("X:TRADENUM;");
    tdx::Json generic_missing_context = tdx::Json::object();
    generic_missing_context["series"] = tdx::Json::object();
    generic_missing_context["series"]["TRADENUM"] = tdx::Json::object();
    for (const auto* stamp : {"2026-08-10|14:57", "2026-08-11|14:58",
                              "2026-08-12|14:59"})
        generic_missing_context["series"]["TRADENUM"][stamp] = nullptr;
    require(!tdx::formula_explicit_context_ready(
                tcalc_analysis, &generic_missing_context),
            "generic all-null explicit context must remain caller-unfilled");

    auto trusted_missing_context = generic_missing_context;
    trusted_missing_context["schema"] = "tdx-level2-tcalc-order-flow-v1";
    trusted_missing_context["tdx_callback_type"] = 31;
    auto& trusted_metadata = trusted_missing_context["_formula_context"];
    trusted_metadata["schema"] =
        "tdx-formula-explicit-context-from-tcalc-l2-v1";
    trusted_metadata["materialization_schema"] =
        "tdx-formula-tcalc-l2-kline-context-v1";
    trusted_metadata["materialized"] = true;
    trusted_metadata["context_complete"] = true;
    trusted_metadata["materialized_bar_count"] = 3;
    require(tdx::formula_explicit_context_ready(
                tcalc_analysis, &trusted_missing_context),
            "complete materialized type-31 native-missing series is ready");

    auto incomplete_missing_context = trusted_missing_context;
    incomplete_missing_context["_formula_context"]["materialized_bar_count"] = 4;
    require(!tdx::formula_explicit_context_ready(
                tcalc_analysis, &incomplete_missing_context),
            "materialized type-31 missing series must cover every declared bar");
    auto absent_missing_context = trusted_missing_context;
    absent_missing_context["series"].as_object().erase("TRADENUM");
    require(!tdx::formula_explicit_context_ready(
                tcalc_analysis, &absent_missing_context),
            "materialized type-31 context must contain every required binding");
    auto malformed_missing_context = trusted_missing_context;
    malformed_missing_context["series"]["TRADENUM"]["2026-08-11|14:58"] = "missing";
    require(!tdx::formula_explicit_context_ready(
                tcalc_analysis, &malformed_missing_context),
            "materialized type-31 missing series accepts only number or null values");
    auto untrusted_missing_context = trusted_missing_context;
    untrusted_missing_context["schema"] = "tdx-formula-explicit-context-v1";
    require(!tdx::formula_explicit_context_ready(
                tcalc_analysis, &untrusted_missing_context),
            "type-31 native-missing exception requires the exact decoder schema");
    auto numeric_generic_context = generic_missing_context;
    numeric_generic_context["series"]["TRADENUM"]["2026-08-11|14:58"] = 7;
    require(tdx::formula_explicit_context_ready(
                tcalc_analysis, &numeric_generic_context),
            "ordinary explicit series with a numeric point remains ready");

    tdx::Json audit_kline = tdx::Json::object();
    audit_kline["market"] = "sz";
    audit_kline["code"] = "000001";
    audit_kline["name"] = "fixture";
    audit_kline["period"] = "day";
    audit_kline["period_id"] = 4;
    audit_kline["bars"] = tdx::Json::array();
    for (const auto& [date, time, close] :
         std::array<std::tuple<const char*, const char*, double>, 3>{{
             {"2026-08-10", "14:57", 10.0},
             {"2026-08-11", "14:58", 10.1},
             {"2026-08-12", "14:59", 10.2},
         }}) {
        tdx::Json bar = tdx::Json::object();
        bar["date"] = date;
        bar["time"] = time;
        bar["open"] = close;
        bar["high"] = close;
        bar["low"] = close;
        bar["close"] = close;
        bar["amount"] = 100000.0;
        bar["volume"] = 10000.0;
        audit_kline["bars"].push_back(std::move(bar));
    }
    audit_kline["count"] = 3;

    tdx::Json audit_library = tdx::Json::object();
    audit_library["formulas"] = tdx::Json::array();
    audit_library["formulas"].push_back(
        tdx::make_formula_source_definition(
            "X:TRADENUM;Y:BIDORDERVOL;", "TCALC_AUDIT", "technical"));

    tdx::Json decoded_context = tdx::Json::object();
    decoded_context["schema"] = "tdx-level2-tcalc-order-flow-v1";
    decoded_context["tdx_callback_type"] = 31;
    decoded_context["records"] = tdx::Json::array();
    decoded_context["series"] = tdx::Json::object();
    for (const auto& [stamp, bid_volume] :
         std::array<std::pair<const char*, double>, 2>{{
             {"2026-08-09|15:00", 5.0},
             {"2026-08-11|15:00", 12.0},
         }}) {
        tdx::Json bindings = tdx::Json::object();
        bindings["TRADENUM"] = nullptr;
        bindings["BIDORDERVOL"] = bid_volume;
        tdx::Json record = tdx::Json::object();
        record["formula_context_stamp"] = stamp;
        record["formula_bindings"] = bindings;
        decoded_context["records"].push_back(std::move(record));
        decoded_context["series"]["TRADENUM"][stamp] = nullptr;
        decoded_context["series"]["BIDORDERVOL"][stamp] = bid_volume;
    }
    auto& decoded_metadata = decoded_context["_formula_context"];
    decoded_metadata["schema"] =
        "tdx-formula-explicit-context-from-tcalc-l2-v1";
    decoded_metadata["context_complete"] = true;
    decoded_metadata["invalid_date_record_count"] = 0;

    const auto audit_root = fs::temp_directory_path() /
        "tdx-formula-tcalc-audit-materializer-test";
    fs::remove_all(audit_root);
    fs::create_directories(audit_root);
    const auto kline_path = audit_root / "kline.json";
    const auto library_path = audit_root / "library.json";
    const auto context_path = audit_root / "context.json";
    const auto output_path = audit_root / "audit.json";
    tdx::atomic_write_text(kline_path, audit_kline.dump(-1));
    tdx::atomic_write_text(library_path, audit_library.dump(-1));
    tdx::atomic_write_text(context_path, decoded_context.dump(-1));
    require(tdx::command_formulas_audit({
                "--library", tdx::path_utf8(library_path),
                "--input", tdx::path_utf8(kline_path),
                "--context-file", tdx::path_utf8(context_path),
                "--output", tdx::path_utf8(output_path), "--compact"}) == 0,
            "formula audit accepts a decoded type-31 context file");
    const auto materialized_audit =
        tdx::Json::parse(tdx::read_text_utf8(output_path));
    fs::remove_all(audit_root);
    const auto& materialized_row =
        materialized_audit.at("formulas").as_array().front();
    require(
        materialized_row.at("status").as_string() == "passed" &&
            materialized_row.at("explicit_context").as_bool() &&
            materialized_row.at("numeric_outputs").as_array().size() == 1 &&
            materialized_row.at("numeric_outputs").as_array().front().as_string() == "Y" &&
            materialized_row.at("latest_numeric_outputs").as_array().front().as_string() == "Y" &&
            materialized_audit.at("audit").at("with_numeric_output").as_number() == 1.0 &&
            materialized_audit.at("audit").at("with_latest_numeric_output").as_number() == 1.0,
        "audit materializes cropped type-31 records onto non-15:00 daily bars and accepts a complete native-missing binding");
}

} // namespace formula_engine_test
