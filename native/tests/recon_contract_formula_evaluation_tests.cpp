#include "recon_contract_test_support.hpp"

namespace recon_contract_test {

void run_formula_evaluation_contracts() {
    const auto health = tdx::evaluate_api_contract_response(
        "health", 200, "application/json; charset=utf-8",
        R"({"ok":true,"native_cpp":true,"python_runtime":false,"service":"tdx-tool"})");
    require(health.at("passed").as_bool(), "health contract failed");

    const auto holder_history = tdx::evaluate_api_contract_response(
        "holder-cross-stock-live", 200, "application/json",
        R"json({"schema":"tdx-holder-history-native-v1","availability":"live","holder":{"holder_id":"GD011907"},"counts":{"records":3503,"stock_periods":35},"pagination":{"returned":5},"cache":{"history_attempts":1,"detail_attempts":1,"history_stale":false,"detail_stale":false,"max_attempts":3}})json");
    require(holder_history.at("passed").as_bool(), "holder cross-stock resilience contract failed");

    const auto generic_tqlex = tdx::evaluate_api_contract_response(
        "generic-tqlex-live", 200, "application/json",
        R"json({"schema":"tdx-tqlex-native-v1","request_id":"200626","attempts":1,"max_attempts":3,"response":{"ErrorCode":0,"ResultSets":[{"RowNum":33}]}})json");
    require(generic_tqlex.at("passed").as_bool(), "generic TQLEX resilience contract failed");

    const auto generic_pbrpc = tdx::evaluate_api_contract_response(
        "generic-pbrpc-live", 200, "application/json",
        R"json({"schema":"tdx-pbrpc-native-v1","request_id":"200340","attempts":2,"max_attempts":3,"rounds":2,"response":{"ErrorCode":0,"ResultSets":[{"RowNum":39}]}})json");
    require(generic_pbrpc.at("passed").as_bool(), "generic PBRPC resilience contract failed");

    const auto generic_pbrpc_exhausted = tdx::evaluate_api_contract_response(
        "generic-pbrpc-live", 200, "application/json",
        R"json({"schema":"tdx-pbrpc-native-v1","request_id":"200340","attempts":4,"max_attempts":3,"rounds":0,"response":{"ErrorCode":4,"ResultSets":[]}})json");
    require(!generic_pbrpc_exhausted.at("passed").as_bool(),
            "generic PBRPC contract must reject invalid retry metadata");

    const auto inline_formula = tdx::evaluate_api_contract_response(
        "formula-inline-post", 200, "application/json; charset=utf-8",
        R"({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula_source_mode":"inline-post","request_body_retained":false,"count":120,"outputs":["FAST","SIGNAL","HIST"],"points":[)" +
            [&] {
                std::string points;
                for (int index = 0; index < 120; ++index) {
                    if (index)
                        points += ',';
                    points += "{}";
                }
                return points;
            }() +
            R"(],"analysis":{"syntax_supported":true,"executable":true}})");
    require(inline_formula.at("passed").as_bool(), "inline formula POST contract failed");

    tdx::Json custom_core_document = tdx::Json::object();
    custom_core_document["engine"] = "tdx-source-interpreter-v1";
    custom_core_document["execution_mode"] = "native-cpp";
    custom_core_document["formula_source_mode"] = "inline-post";
    custom_core_document["formula"] = "CONTRACT_CUSTOM_CORE";
    custom_core_document["analysis"] = tdx::Json::object();
    custom_core_document["analysis"]["syntax_supported"] = true;
    custom_core_document["analysis"]["executable"] = true;
    custom_core_document["analysis"]["has_machine_clock_dependency"] = true;
    custom_core_document["analysis"]["pure_ohlcv"] = false;
    custom_core_document["points"] = tdx::Json::array();
    const auto clock_stamp = std::time(nullptr);
    std::tm clock_local{};
#ifdef _WIN32
    localtime_s(&clock_local, &clock_stamp);
#else
    localtime_r(&clock_stamp, &clock_local);
#endif
    const int clock_date =
        clock_local.tm_year * 10000 + (clock_local.tm_mon + 1) * 100 + clock_local.tm_mday;
    const int clock_time =
        clock_local.tm_hour * 10000 + clock_local.tm_min * 100 + clock_local.tm_sec;
    for (int index = 0; index < 120; ++index) {
        tdx::Json point = tdx::Json::object();
        point["date"] = index == 119 ? "2026-08-07" : "2026-01-01";
        point["values"] = tdx::Json::object();
        auto &values = point["values"];
        values["YR"] = 2026;
        values["MO"] = index == 119 ? 8 : 1;
        values["DY"] = index == 119 ? 7 : 1;
        values["WD"] = index == 119 ? 5 : 4;
        values["BS"] = index == 0 ? 1 : (index == 119 ? 2 : 0);
        values["TB"] = 120;
        values["RAW"] = index == 119 ? 12.34 : 10.0;
        values["FIXED"] = 12.34;
        values["BACK2"] = index == 119 ? 10.34 : 10.0;
        values["CA"] = 10.34;
        values["R2"] = 1.24;
        values["EV"] = index == 119 ? 1 : 0;
        values["IN"] = 1;
        values["FR"] = 0.34;
        values["SG"] = 1;
        values["TRIG"] = 3.3561944901923448;
        values["MD"] = clock_date;
        values["MT"] = clock_time;
        values["MW"] = clock_local.tm_wday;
        custom_core_document["points"].push_back(std::move(point));
    }
    const auto custom_core = tdx::evaluate_api_contract_response(
        "formula-custom-core-inline-post", 200, "application/json", custom_core_document.dump(-1));
    require(custom_core.at("passed").as_bool(), "custom formula native core contract failed");

    tdx::Json sequence_statistics_document = tdx::Json::object();
    sequence_statistics_document["engine"] = "tdx-source-interpreter-v1";
    sequence_statistics_document["execution_mode"] = "native-cpp";
    sequence_statistics_document["formula_source_mode"] = "inline-post";
    sequence_statistics_document["formula"] = "CONTRACT_SEQUENCE_STATS";
    sequence_statistics_document["analysis"] = tdx::Json::object();
    sequence_statistics_document["analysis"]["syntax_supported"] = true;
    sequence_statistics_document["analysis"]["has_future_function"] = true;
    sequence_statistics_document["analysis"]["read_only_future_executable"] = true;
    sequence_statistics_document["context_metadata"] = tdx::Json::object();
    sequence_statistics_document["context_metadata"]["capital_series_mode"] =
        "tdx-0x000f-historical";
    sequence_statistics_document["points"] = tdx::Json::array();
    for (int index = 0; index < 120; ++index) {
        tdx::Json point = tdx::Json::object();
        point["values"] = tdx::Json::object();
        auto &values = point["values"];
        values["FX"] = index == 118 ? 1 : 0;
        if (index >= 9)
            values["NS"] = 10.0 + static_cast<double>(index) / 100.0;
        if (index == 116)
            values["FT"] = 474;
        if (index == 117)
            values["FT"] = -4;
        if (index == 118)
            values["FT"] = -2;
        if (index == 119)
            values["FT"] = 0;
        if (index == 119) {
            values["BL"] = 3;
            values["BSN"] = 3;
            values["TM"] = 119;
            values["XM"] = 119.5;
            values["CV"] = 5;
            values["RL"] = 1;
            values["BE"] = 0.5;
            values["FH"] = 118;
            values["FHB"] = 2;
            values["FL"] = 116;
            values["FLB"] = 4;
            values["LF"] = 12.5;
        }
        sequence_statistics_document["points"].push_back(std::move(point));
    }
    const auto sequence_statistics = tdx::evaluate_api_contract_response(
        "formula-sequence-statistics-inline-post", 200, "application/json",
        sequence_statistics_document.dump(-1));
    require(sequence_statistics.at("passed").as_bool(),
            "sequence/statistics native formula contract failed");

    tdx::Json rolling_variance_document = tdx::Json::object();
    rolling_variance_document["engine"] = "tdx-source-interpreter-v1";
    rolling_variance_document["execution_mode"] = "native-cpp";
    rolling_variance_document["formula_source_mode"] = "inline-post";
    rolling_variance_document["formula"] = "CONTRACT_ROLLING_VARIANCE";
    rolling_variance_document["analysis"] = tdx::Json::object();
    rolling_variance_document["analysis"]["syntax_supported"] = true;
    rolling_variance_document["analysis"]["executable"] = true;
    rolling_variance_document["analysis"]["has_future_function"] = false;
    rolling_variance_document["points"] = tdx::Json::array();
    for (int index = 0; index < 120; ++index) {
        tdx::Json point = tdx::Json::object();
        point["values"] = tdx::Json::object();
        auto &values = point["values"];
        if (index <= 3)
            values["RV"] = index == 3 ? 8 : 1;
        if (index == 0) {
            values["IV"] = 0;
            values["IT"] = 20;
            values["IN"] = 10;
            values["N"] = 1;
        } else if (index == 2) {
            values["IV"] = 1;
            values["IT"] = 10;
            values["IN"] = 20;
            values["N"] = 0;
            values["MX"] = 6;
            values["MN"] = 1;
            values["DS"] = 86.0 / 3.0;
            values["VS"] = 43.0 / 3.0;
            values["SS"] = std::sqrt(43.0 / 3.0);
            values["VP"] = 0;
            values["SP"] = 0;
            values["DV"] = nullptr;
        } else if (index == 3) {
            values["MU"] = 128;
            values["AM"] = 6.203125;
            values["VP"] = 296.0 / 9.0;
            values["SP"] = std::sqrt(296.0) / 3.0;
            values["DV"] = std::log(2.0) / 2.0;
        } else if (index == 4) {
            values["HR"] = 3;
            values["LR"] = 1;
            values["HV"] = 16;
            values["LV"] = 8;
        }
        rolling_variance_document["points"].push_back(std::move(point));
    }
    const auto rolling_variance =
        tdx::evaluate_api_contract_response("formula-rolling-variance-inline-post", 200,
                                            "application/json", rolling_variance_document.dump(-1));
    require(rolling_variance.at("passed").as_bool(),
            "rolling/variance native formula contract failed");

    tdx::Json benchmark_cumulative_document = tdx::Json::object();
    benchmark_cumulative_document["engine"] = "tdx-source-interpreter-v1";
    benchmark_cumulative_document["execution_mode"] = "native-cpp";
    benchmark_cumulative_document["formula_source_mode"] = "inline-post";
    benchmark_cumulative_document["formula"] = "CONTRACT_BENCHMARK_CUMULATIVE";
    benchmark_cumulative_document["analysis"] = tdx::Json::object();
    benchmark_cumulative_document["analysis"]["syntax_supported"] = true;
    benchmark_cumulative_document["analysis"]["executable"] = false;
    benchmark_cumulative_document["analysis"]["executable_with_context"] = true;
    benchmark_cumulative_document["analysis"]["requires_automatic_market_context"] = true;
    benchmark_cumulative_document["analysis"]["automatic_context_dependencies"] =
        tdx::Json::array();
    benchmark_cumulative_document["analysis"]["automatic_context_dependencies"].push_back("BETA");
    benchmark_cumulative_document["context_bindings"] = tdx::Json::array();
    benchmark_cumulative_document["context_bindings"].push_back("__BETA_BENCHMARK_CLOSE");
    benchmark_cumulative_document["context_metadata"] = tdx::Json::object();
    benchmark_cumulative_document["context_metadata"]["beta_benchmark"] = tdx::Json::object();
    benchmark_cumulative_document["context_metadata"]["beta_benchmark"]["security"] = "sz:399001";
    benchmark_cumulative_document["context_metadata"]["beta_benchmark"]["mode"] =
        "TCalc-sub_10023F40-native-security-market-benchmark-selection";
    benchmark_cumulative_document["points"] = tdx::Json::array();
    for (int index = 0; index < 120; ++index) {
        tdx::Json point = tdx::Json::object();
        point["values"] = tdx::Json::object();
        auto &values = point["values"];
        if (index == 0) {
            values["SB"] = 0;
            values["SX"] = -1;
            values["NB"] = 0;
            values["NX"] = nullptr;
        } else if (index == 1) {
            values["SB"] = 1;
            values["SX"] = 1;
        } else if (index == 119) {
            values["SB"] = 5;
            values["SX"] = 4;
            values["NB"] = 119;
            values["NX"] = nullptr;
            values["BT"] = 0.8;
        }
        benchmark_cumulative_document["points"].push_back(std::move(point));
    }
    const auto benchmark_cumulative = tdx::evaluate_api_contract_response(
        "formula-benchmark-cumulative-inline-post", 200, "application/json",
        benchmark_cumulative_document.dump(-1));
    require(benchmark_cumulative.at("passed").as_bool(),
            "benchmark/cumulative native formula contract failed");

    tdx::Json calendar_filter_document = tdx::Json::object();
    calendar_filter_document["engine"] = "tdx-source-interpreter-v1";
    calendar_filter_document["execution_mode"] = "native-cpp";
    calendar_filter_document["formula_source_mode"] = "inline-post";
    calendar_filter_document["formula"] = "CONTRACT_CALENDAR_FILTER";
    calendar_filter_document["analysis"] = tdx::Json::object();
    calendar_filter_document["analysis"]["syntax_supported"] = true;
    calendar_filter_document["analysis"]["executable"] = false;
    calendar_filter_document["analysis"]["has_external_dependency"] = false;
    calendar_filter_document["analysis"]["has_future_function"] = true;
    calendar_filter_document["analysis"]["read_only_future_executable"] = true;
    calendar_filter_document["analysis"]["pure_ohlcv"] = false;
    calendar_filter_document["points"] = tdx::Json::array();
    constexpr double f0[]{1, 0, 2, 0, 1, 2, 1, 0, 2, 0};
    constexpr double f1[]{1, 0, 0, 0, 1, 0, 1, 0, 0, 0};
    constexpr double f2[]{0, 0, 1, 0, 1, 1, 0, 0, 1, 0};
    constexpr double t0[]{1, 0, 2, 4, 1, 2, 0, 4, 1, 0};
    constexpr double t1[]{1, 0, 0, 0, 1, 0, 0, 0, 1, 0};
    constexpr double t2[]{0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
    constexpr double t3[]{0, 0, 1, 0, 0, 1, 0, 0, 0, 0};
    constexpr double t4[]{0, 0, 0, 1, 0, 0, 0, 1, 0, 0};
    for (int index = 0; index < 120; ++index) {
        tdx::Json point = tdx::Json::object();
        point["values"] = tdx::Json::object();
        auto &values = point["values"];
        values["WOY"] = 32;
        values["T2"] = 150000;
        values["DC"] = index + 1;
        values["DL"] = index + 1;
        values["DN"] = index + 1;
        values["DF"] = 0;
        if (index == 0) {
            values["D0"] = 0;
            values["D1"] = 1;
            values["DT"] = 910101;
            values["TS"] = 34200;
            values["ST"] = 93000;
        }
        if (index < 10) {
            values["F0"] = f0[index];
            values["F1"] = f1[index];
            values["F2"] = f2[index];
            values["TT0"] = t0[index];
            values["TT1"] = t1[index];
            values["TT2"] = t2[index];
            values["TT3"] = t3[index];
            values["TT4"] = t4[index];
        }
        if (index == 117)
            values["AR"] = 2;
        if (index == 118) {
            values["AR"] = 4;
            values["LATEST"] = nullptr;
        }
        if (index == 119) {
            values["AR"] = 5;
            values["LATEST"] = 120;
            values["BD"] = 1260807;
            values["RT"] = 1260807;
        }
        calendar_filter_document["points"].push_back(std::move(point));
    }
    const auto calendar_filter =
        tdx::evaluate_api_contract_response("formula-calendar-filter-inline-post", 200,
                                            "application/json", calendar_filter_document.dump(-1));
    require(calendar_filter.at("passed").as_bool(),
            "calendar/filter native formula contract failed");

    tdx::Json directional_document = tdx::Json::object();
    directional_document["engine"] = "tdx-source-interpreter-v1";
    directional_document["execution_mode"] = "native-cpp";
    directional_document["formula_source_mode"] = "inline-post";
    directional_document["formula"] = "CONTRACT_DIRECTIONAL_BARS";
    directional_document["analysis"] = tdx::Json::object();
    directional_document["analysis"]["syntax_supported"] = true;
    directional_document["analysis"]["executable"] = false;
    directional_document["analysis"]["has_external_dependency"] = false;
    directional_document["analysis"]["has_future_function"] = true;
    directional_document["analysis"]["read_only_future_executable"] = true;
    directional_document["analysis"]["future_functions"] = tdx::Json::array();
    directional_document["analysis"]["market_dependencies"] = tdx::Json::array();
    for (const auto *name : {"DCLOSE", "DHIGH", "DLOW", "DOPEN", "DVOL"})
        directional_document["analysis"]["future_functions"].push_back(name);
    for (const auto *name : {"CLOSE", "HIGH", "LOW", "OPEN", "VOL"})
        directional_document["analysis"]["market_dependencies"].push_back(name);
    directional_document["points"] = tdx::Json::array();
    for (int index = 0; index < 120; ++index) {
        tdx::Json point = tdx::Json::object();
        point["values"] = tdx::Json::object();
        auto &values = point["values"];
        values["DH"] = 13;
        values["DHF"] = 13;
        values["DO"] = 10;
        values["DL"] = 9;
        values["DC"] = 12;
        values["DV"] = 30;
        values["R"] = 10;
        directional_document["points"].push_back(std::move(point));
    }
    const auto directional =
        tdx::evaluate_api_contract_response("formula-directional-bars-inline-post", 200,
                                            "application/json", directional_document.dump(-1));
    require(directional.at("passed").as_bool(), "directional-bar native formula contract failed");

    tdx::Json ziga_document = tdx::Json::object();
    ziga_document["engine"] = "tdx-source-interpreter-v1";
    ziga_document["execution_mode"] = "native-cpp";
    ziga_document["formula_source_mode"] = "inline-post";
    ziga_document["formula"] = "CONTRACT_ZIGA";
    ziga_document["market"] = "sz";
    ziga_document["code"] = "000001";
    ziga_document["analysis"] = tdx::Json::object();
    ziga_document["analysis"]["syntax_supported"] = true;
    ziga_document["analysis"]["executable"] = false;
    ziga_document["analysis"]["has_external_dependency"] = false;
    ziga_document["analysis"]["has_future_function"] = true;
    ziga_document["analysis"]["read_only_future_executable"] = true;
    ziga_document["analysis"]["future_functions"] = tdx::Json::array();
    ziga_document["analysis"]["future_functions"].push_back("ZIGA");
    ziga_document["points"] = tdx::Json::array();
    for (int index = 0; index < 120; ++index) {
        const double close = 10.0 + static_cast<double>(index % 3);
        const double path = index == 1 ? 10.5 : close;
        tdx::Json point = tdx::Json::object();
        point["close"] = close;
        point["values"] = tdx::Json::object();
        point["values"]["R"] = close;
        point["values"]["Z"] = path;
        point["values"]["S"] = path;
        ziga_document["points"].push_back(std::move(point));
    }
    const auto ziga = tdx::evaluate_api_contract_response(
        "formula-ziga-inline-post", 200, "application/json", ziga_document.dump(-1));
    require(ziga.at("passed").as_bool(), "absolute-threshold ZIGA native formula contract failed");

    tdx::Json random_score_document = tdx::Json::object();
    random_score_document["engine"] = "tdx-source-interpreter-v1";
    random_score_document["execution_mode"] = "native-cpp";
    random_score_document["formula_source_mode"] = "inline-post";
    random_score_document["formula"] = "CONTRACT_RANDOM_SECURITY_SCORE";
    random_score_document["market"] = "sz";
    random_score_document["code"] = "000001";
    random_score_document["analysis"] = tdx::Json::object();
    auto &random_analysis = random_score_document["analysis"];
    random_analysis["syntax_supported"] = true;
    random_analysis["executable"] = false;
    random_analysis["executable_with_context"] = true;
    random_analysis["has_external_dependency"] = true;
    random_analysis["has_future_function"] = false;
    random_analysis["has_random_dependency"] = true;
    random_analysis["random_seed_bindable"] = true;
    random_analysis["automatic_context_dependencies"] = tdx::Json::array();
    random_analysis["automatic_context_dependencies"].push_back("SAFESCORE");
    random_analysis["automatic_context_dependencies"].push_back("SHINESCORE");
    random_analysis["random_functions"] = tdx::Json::array();
    random_analysis["random_functions"].push_back("RAND");
    random_score_document["random"] = tdx::Json::object();
    random_score_document["random"]["algorithm"] = "msvc-crt-rand-lcg-v1";
    random_score_document["random"]["seed"] = 1;
    random_score_document["random"]["seed_mode"] = "explicit-context";
    random_score_document["random"]["invalid_input_advances_state"] = false;
    random_score_document["context_metadata"] = tdx::Json::object();
    auto &score_metadata = random_score_document["context_metadata"];
    score_metadata["security_score_source"] = "C:/new_tdx/T0002/hq_cache/specgpext.txt";
    score_metadata["security_score_mode"] = "tdxw-type167-specgpext-fields4-5-local-reconstruction";
    score_metadata["security_score_values"] = tdx::Json::object();
    score_metadata["security_score_values"]["record_found"] = true;
    score_metadata["security_score_values"]["safety_score"] = 92;
    score_metadata["security_score_values"]["shine_score"] = 7;
    random_score_document["points"] = tdx::Json::array();
    const std::array<int, 4> random_prefix{2, 8, 5, 1};
    for (int index = 0; index < 120; ++index) {
        tdx::Json point = tdx::Json::object();
        point["values"] = tdx::Json::object();
        point["values"]["R"] = index < 4 ? random_prefix[index] : 1;
        point["values"]["SAFE"] = 92;
        point["values"]["SHINE"] = 7;
        random_score_document["points"].push_back(std::move(point));
    }
    const auto random_score =
        tdx::evaluate_api_contract_response("formula-random-security-score-inline-post", 200,
                                            "application/json", random_score_document.dump(-1));
    require(random_score.at("passed").as_bool(),
            "RAND and local security-score native formula contract failed");

    tdx::Json adjustment_document = tdx::Json::object();
    adjustment_document["engine"] = "tdx-source-interpreter-v1";
    adjustment_document["execution_mode"] = "native-cpp";
    adjustment_document["formula_source_mode"] = "inline-post";
    adjustment_document["formula"] = "CONTRACT_ADJUSTMENT_FLAG";
    adjustment_document["analysis"] = tdx::Json::object();
    adjustment_document["analysis"]["syntax_supported"] = true;
    adjustment_document["analysis"]["executable"] = true;
    adjustment_document["analysis"]["pure_ohlcv"] = false;
    adjustment_document["analysis"]["has_adjustment_mode_dependency"] = true;
    adjustment_document["analysis"]["has_external_dependency"] = false;
    adjustment_document["analysis"]["has_future_function"] = false;
    adjustment_document["adjustment_mode"] = "qfq";
    adjustment_document["adjustment"] = tdx::Json::object();
    adjustment_document["adjustment"]["mode"] = "qfq";
    adjustment_document["adjustment"]["source_command"] = "0x000F";
    adjustment_document["adjustment"]["method"] = "local-corporate-action-factor-v1";
    adjustment_document["points"] = tdx::Json::array();
    for (int index = 0; index < 120; ++index) {
        tdx::Json point = tdx::Json::object();
        point["values"] = tdx::Json::object();
        point["values"]["T"] = 1;
        point["values"]["TF"] = 1;
        adjustment_document["points"].push_back(std::move(point));
    }
    const auto adjustment =
        tdx::evaluate_api_contract_response("formula-adjustment-flag-inline-post", 200,
                                            "application/json", adjustment_document.dump(-1));
    require(adjustment.at("passed").as_bool(), "adjustment-flag native formula contract failed");

    tdx::Json adjustment_get_document = tdx::Json::object();
    adjustment_get_document["engine"] = "tdx-source-interpreter-v1";
    adjustment_get_document["execution_mode"] = "native-cpp";
    adjustment_get_document["formula"] = "MACD";
    adjustment_get_document["adjustment_mode"] = "qfq";
    adjustment_get_document["adjustment"] = tdx::Json::object();
    adjustment_get_document["adjustment"]["mode"] = "qfq";
    adjustment_get_document["adjustment"]["source_command"] = "0x000F";
    adjustment_get_document["adjustment"]["method"] = "local-corporate-action-factor-v1";
    adjustment_get_document["adjustment"]["applied_event_count"] = 3;
    adjustment_get_document["points"] = tdx::Json::array();
    for (int index = 0; index < 120; ++index) {
        tdx::Json point = tdx::Json::object();
        point["values"] = tdx::Json::object();
        point["values"]["DIF"] = index < 25 ? tdx::Json(nullptr) : tdx::Json(1);
        point["values"]["DEA"] = index < 33 ? tdx::Json(nullptr) : tdx::Json(1);
        point["values"]["MACD"] = index < 33 ? tdx::Json(nullptr) : tdx::Json(0);
        adjustment_get_document["points"].push_back(std::move(point));
    }
    const auto adjustment_get = tdx::evaluate_api_contract_response(
        "formula-adjustment-qfq-get", 200, "application/json", adjustment_get_document.dump(-1));
    require(adjustment_get.at("passed").as_bool(), "adjustment qfq GET formula contract failed");

    tdx::Json security_string_document = tdx::Json::object();
    security_string_document["engine"] = "tdx-source-interpreter-v1";
    security_string_document["execution_mode"] = "native-cpp";
    security_string_document["formula_source_mode"] = "inline-post";
    security_string_document["formula"] = "CONTRACT_SECURITY_STRING";
    security_string_document["analysis"] = tdx::Json::object();
    security_string_document["analysis"]["syntax_supported"] = true;
    security_string_document["analysis"]["executable"] = true;
    security_string_document["analysis"]["numeric_signal_safe"] = true;
    security_string_document["analysis"]["has_external_dependency"] = false;
    security_string_document["analysis"]["has_future_function"] = false;
    security_string_document["analysis"]["unsupported"] = tdx::Json::array();
    security_string_document["points"] = tdx::Json::array();
    for (int index = 0; index < 120; ++index) {
        tdx::Json point = tdx::Json::object();
        point["values"] = tdx::Json::object();
        auto &values = point["values"];
        values["N0"] = 1;
        values["N1"] = 0;
        values["C0"] = 1;
        values["C1"] = 0;
        values["I0"] = 1;
        values["I1"] = 0;
        values["F0"] = 1;
        values["F1"] = 0;
        values["S0"] = 2365.02001953125;
        values["S1"] = 0;
        values["U"] = index == 0 ? tdx::Json(nullptr) : tdx::Json(1);
        values["Z"] = 0;
        security_string_document["points"].push_back(std::move(point));
    }
    security_string_document["render_ir"] = tdx::Json::object();
    security_string_document["render_ir"]["primitives"] = tdx::Json::array();
    tdx::Json security_text = tdx::Json::object();
    security_text["function"] = "DRAWTEXT_FIX";
    security_text["events"] = tdx::Json::array();
    tdx::Json security_text_event = tdx::Json::object();
    security_text_event["annotation_text"] = "平安银行";
    security_text["events"].push_back(std::move(security_text_event));
    security_string_document["render_ir"]["primitives"].push_back(std::move(security_text));
    const auto security_string =
        tdx::evaluate_api_contract_response("formula-security-string-inline-post", 200,
                                            "application/json", security_string_document.dump(-1));
    require(security_string.at("passed").as_bool(),
            "security/string native formula contract failed");

    tdx::Json block_metadata_document = tdx::Json::object();
    block_metadata_document["engine"] = "tdx-source-interpreter-v1";
    block_metadata_document["execution_mode"] = "native-cpp";
    block_metadata_document["formula_source_mode"] = "inline-post";
    block_metadata_document["formula"] = "CONTRACT_BLOCK_METADATA";
    block_metadata_document["market"] = "sz";
    block_metadata_document["code"] = "000001";
    block_metadata_document["analysis"] = tdx::Json::object();
    block_metadata_document["analysis"]["syntax_supported"] = true;
    block_metadata_document["analysis"]["executable"] = false;
    block_metadata_document["analysis"]["executable_with_context"] = true;
    block_metadata_document["analysis"]["automatic_context_dependencies"] = tdx::Json::array();
    for (const char *dependency :
         {"FGBLOCK", "FGBLOCKNUM", "GNBLOCKNUM", "HYZSCODE", "INBLOCK", "ZSBLOCK", "ZSBLOCKNUM"})
        block_metadata_document["analysis"]["automatic_context_dependencies"].push_back(dependency);
    block_metadata_document["context_metadata"] = tdx::Json::object();
    block_metadata_document["context_metadata"]["formula_text_symbols"] = tdx::Json::object();
    auto &block_texts = block_metadata_document["context_metadata"]["formula_text_symbols"];
    block_texts["FGBLOCK"] = "大盘股 高股息股";
    block_texts["ZSBLOCK"] = "沪深300 深证100";
    block_texts["HYZSCODE"] = "880471";
    block_metadata_document["points"] = tdx::Json::array();
    for (int index = 0; index < 120; ++index) {
        tdx::Json point = tdx::Json::object();
        point["values"] = tdx::Json::object();
        point["values"]["M"] = 1;
        point["values"]["MISS"] = 0;
        point["values"]["N"] = 29;
        block_metadata_document["points"].push_back(std::move(point));
    }
    const auto block_metadata =
        tdx::evaluate_api_contract_response("formula-block-metadata-inline-post", 200,
                                            "application/json", block_metadata_document.dump(-1));
    require(block_metadata.at("passed").as_bool(),
            "local block metadata native formula contract failed");

    tdx::Json block_code_document = tdx::Json::object();
    block_code_document["engine"] = "tdx-source-interpreter-v1";
    block_code_document["execution_mode"] = "native-cpp";
    block_code_document["formula_source_mode"] = "inline-post";
    block_code_document["formula"] = "CONTRACT_BLOCK_CODE_NAME";
    block_code_document["market"] = "sz";
    block_code_document["code"] = "000001";
    block_code_document["analysis"] = tdx::Json::object();
    block_code_document["analysis"]["syntax_supported"] = true;
    block_code_document["analysis"]["executable"] = false;
    block_code_document["analysis"]["executable_with_context"] = true;
    block_code_document["analysis"]["automatic_context_dependencies"] = tdx::Json::array();
    for (const char *dependency : {"BLOCKSETNUM", "FGBKZSCODE", "GETNAMEOFCODE", "GNBKZSCODE",
                                   "HORCALC", "HYZSCODE", "INSORT", "INSUM"})
        block_code_document["analysis"]["automatic_context_dependencies"].push_back(dependency);
    block_code_document["analysis"]["context_bindings_required"] = tdx::Json::array();
    for (const char *binding :
         {"BLOCKSETNUM#GN.跨境支付", "BLOCKSETNUM#HY.银行", "BLOCKSETNUM#不存在",
          "HORCALC#HY.银行#103#0#2", "HORCALC#HY.银行#103#1#2", "HORCALC#HY.银行#103#2#2",
          "INSORT#HY.银行#KDJ#3#0", "INSORT#HY.银行#KDJ#3#1", "INSUM#HY.银行#KDJ#3#0",
          "INSUM#HY.银行#KDJ#3#1", "INSUM#HY.银行#KDJ#3#2", "INSUM#HY.银行#KDJ#3#3",
          "INSUM#HY.银行#KDJ#3#4", "INSUM#HY.银行#KDJ#3#5"})
        block_code_document["analysis"]["context_bindings_required"].push_back(binding);
    block_code_document["context_metadata"] = tdx::Json::object();
    auto &block_code_metadata = block_code_document["context_metadata"];
    block_code_metadata["type167_block_code_mode"] =
        "tdxw-type167-direct-membership-dword-ascending-concept-then-style";
    block_code_metadata["type167_block_code_limit"] = 60;
    block_code_metadata["type167_concept_code_count"] = 1;
    block_code_metadata["type167_style_code_count"] = 10;
    block_code_metadata["type167_block_code_total"] = 11;
    block_code_metadata["type167_concept_codes"] = tdx::Json::array();
    block_code_metadata["type167_concept_codes"].push_back("880609");
    block_code_metadata["type167_style_codes"] = tdx::Json::array();
    for (const auto *value : {"880679", "880721", "880801", "880805", "880821", "880826", "880829",
                              "880845", "880846", "880883"})
        block_code_metadata["type167_style_codes"].push_back(value);
    block_code_metadata["code_name_lookup_mode"] =
        "tdxw-type120-security-directory-name-offset31-native-tnf-cache";
    block_code_metadata["code_name_security_catalog_source"] = "T0002/hq_cache/{szs,shs,bjs}.tnf";
    block_code_metadata["code_name_override_count"] = 1000;
    block_code_metadata["blocksetnum_mode"] =
        "tcalc-opcode1244-command8-type5-offset1004-local-catalog-first-match";
    block_code_metadata["blocksetnum_binding_count"] = 3;
    block_code_metadata["blocksetnum_industry_mode"] = 2;
    block_code_metadata["blocksetnum_resolutions"] = tdx::Json::array();
    for (const auto &[binding, found, family, code, count] :
         std::vector<std::tuple<const char *, bool, const char *, const char *, int>>{
             {"BLOCKSETNUM#HY.银行", true, "research-industry", "881385", 42},
             {"BLOCKSETNUM#GN.跨境支付", true, "concept", "880609", 75},
             {"BLOCKSETNUM#不存在", false, "", "", 0}}) {
        tdx::Json item = tdx::Json::object();
        item["binding"] = binding;
        item["found"] = found;
        item["member_count"] = count;
        item["family"] = found ? tdx::Json(family) : tdx::Json(nullptr);
        item["block_code"] = found ? tdx::Json(code) : tdx::Json(nullptr);
        block_code_metadata["blocksetnum_resolutions"].push_back(std::move(item));
    }
    block_code_metadata["horcalc_mode"] =
        "tcalc-opcode1245-command8-type7-local-day-date-aligned-horizontal-aggregate";
    block_code_metadata["horcalc_binding_count"] = 3;
    block_code_metadata["horcalc_member_file_count"] = 42;
    block_code_metadata["horcalc_member_series_count"] = 42;
    block_code_metadata["horcalc_industry_mode"] = 2;
    block_code_metadata["horcalc_resolutions"] = tdx::Json::array();
    for (const auto &[binding, calculation] :
         std::vector<std::pair<const char *, int>>{{"HORCALC#HY.银行#103#0#2", 0},
                                                   {"HORCALC#HY.银行#103#1#2", 1},
                                                   {"HORCALC#HY.银行#103#2#2", 2}}) {
        tdx::Json item = tdx::Json::object();
        item["binding"] = binding;
        item["found"] = true;
        item["family"] = "research-industry";
        item["block_code"] = "881385";
        item["member_count"] = 42;
        item["member_series_count"] = 42;
        item["calculation"] = calculation;
        block_code_metadata["horcalc_resolutions"].push_back(std::move(item));
    }
    block_code_metadata["indicator_aggregate_mode"] =
        "tcalc-opcodes1246-1247-active-technical-indicator-local-day-horizontal-evaluation";
    block_code_metadata["insort_binding_count"] = 2;
    block_code_metadata["insum_binding_count"] = 6;
    block_code_metadata["indicator_aggregate_member_file_count"] = 42;
    block_code_metadata["indicator_aggregate_member_series_count"] = 42;
    block_code_metadata["indicator_aggregate_formula_evaluation_count"] = 42;
    block_code_metadata["indicator_aggregate_warmup_bars"] = 100;
    block_code_metadata["indicator_aggregate_industry_mode"] = 2;
    block_code_metadata["indicator_aggregate_resolutions"] = tdx::Json::array();
    for (const auto &[binding, function, mode] :
         std::vector<std::tuple<const char *, const char *, int>>{
             {"INSORT#HY.银行#KDJ#3#0", "INSORT", 0},
             {"INSORT#HY.银行#KDJ#3#1", "INSORT", 1},
             {"INSUM#HY.银行#KDJ#3#0", "INSUM", 0},
             {"INSUM#HY.银行#KDJ#3#1", "INSUM", 1},
             {"INSUM#HY.银行#KDJ#3#2", "INSUM", 2},
             {"INSUM#HY.银行#KDJ#3#3", "INSUM", 3},
             {"INSUM#HY.银行#KDJ#3#4", "INSUM", 4},
             {"INSUM#HY.银行#KDJ#3#5", "INSUM", 5}}) {
        tdx::Json item = tdx::Json::object();
        item["binding"] = binding;
        item["function"] = function;
        item["found"] = true;
        item["family"] = "research-industry";
        item["block_code"] = "881385";
        item["formula_code"] = "KDJ";
        item["formula_output_name"] = "J";
        item["member_count"] = 42;
        item["member_series_count"] = std::string(function) == "INSORT" ? 41 : 42;
        item["mode"] = mode;
        block_code_metadata["indicator_aggregate_resolutions"].push_back(std::move(item));
    }
    block_code_document["points"] = tdx::Json::array();
    for (int index = 0; index < 120; ++index) {
        tdx::Json point = tdx::Json::object();
        point["values"] = tdx::Json::object();
        for (const auto *output : {"G", "F", "H", "CG", "CF", "MISS"})
            point["values"][output] = 1;
        point["values"]["BSH"] = 42;
        point["values"]["BSG"] = 75;
        point["values"]["BSM"] = 0;
        point["values"]["HS"] = 420;
        point["values"]["HR"] = 10;
        point["values"]["HA"] = 10;
        point["values"]["IRD"] = 10;
        point["values"]["IRA"] = 33;
        point["values"]["ISS"] = 420;
        point["values"]["IAV"] = 10;
        point["values"]["IMX"] = 30;
        point["values"]["IMN"] = -10;
        point["values"]["IMXI"] = 18;
        point["values"]["IMNI"] = 20;
        block_code_document["points"].push_back(std::move(point));
    }
    const auto block_code_name =
        tdx::evaluate_api_contract_response("formula-block-code-name-inline-post", 200,
                                            "application/json", block_code_document.dump(-1));
    require(block_code_name.at("passed").as_bool(),
            "type-167 block code and type-120 name native formula contract failed");

    tdx::Json one_point_document = tdx::Json::object();
    one_point_document["engine"] = "tdx-source-interpreter-v1";
    one_point_document["execution_mode"] = "native-cpp";
    one_point_document["formula_source_mode"] = "inline-post";
    one_point_document["formula"] = "CONTRACT_SINGLE_POINT_DATA";
    one_point_document["market"] = "sz";
    one_point_document["code"] = "000001";
    one_point_document["analysis"] = tdx::Json::object();
    one_point_document["analysis"]["syntax_supported"] = true;
    one_point_document["analysis"]["executable"] = false;
    one_point_document["analysis"]["executable_with_context"] = true;
    one_point_document["analysis"]["automatic_context_dependencies"] = tdx::Json::array();
    for (const auto *dependency : {"BKJYONE", "FINONE", "GPJYONE", "GPONEDAT", "SCJYONE"})
        one_point_document["analysis"]["automatic_context_dependencies"].push_back(dependency);
    one_point_document["analysis"]["context_bindings_required"] = tdx::Json::array();
    for (const auto *binding :
         {"BKJYONE#5#1#0#0", "FINONE#183#0#0", "GPJYONE#1#1#0#0", "GPONEDAT#7", "SCJYONE#1#1#0#0"})
        one_point_document["analysis"]["context_bindings_required"].push_back(binding);
    one_point_document["context_metadata"] = tdx::Json::object();
    auto &one_point_metadata = one_point_document["context_metadata"];
    one_point_metadata["finone_mode"] =
        "tcalc-type172-quarter-year-mmdd-single-point-official-gpcw";
    one_point_metadata["finone_period_count"] = 1;
    one_point_metadata["finone_relative_period_limit"] = 1;
    one_point_metadata["gpjyone_mode"] =
        "tdxw-type175-exact-date-or-reverse-ordinal-official-tdxgp";
    one_point_metadata["bkjyone_target_market"] = "sh";
    one_point_metadata["bkjyone_target_code"] = "880471";
    one_point_metadata["scjyone_mode"] =
        "tdxw-type175-sh999999-exact-date-or-reverse-ordinal-official-tdxgp";
    one_point_metadata["gponedat_mode"] = "tdxw-type170-local-10-byte-record-zero-when-absent";
    one_point_metadata["gponedat_source"] = "C:/new_tdx/T0002/hq_cache/gpszone.dat";
    one_point_document["points"] = tdx::Json::array();
    for (int index = 0; index < 120; ++index) {
        tdx::Json point = tdx::Json::object();
        point["values"] = tdx::Json::object();
        point["values"]["F"] = 4.65;
        point["values"]["G"] = 457610;
        point["values"]["B"] = 6.84;
        point["values"]["S"] = 261556640;
        point["values"]["P"] = 0;
        one_point_document["points"].push_back(std::move(point));
    }
    const auto one_point =
        tdx::evaluate_api_contract_response("formula-single-point-data-inline-post", 200,
                                            "application/json", one_point_document.dump(-1));
    require(one_point.at("passed").as_bool(),
            "FINONE/JYONE/GPONEDAT single-point native formula contract failed");

    tdx::Json type167_text_document = tdx::Json::object();
    type167_text_document["engine"] = "tdx-source-interpreter-v1";
    type167_text_document["execution_mode"] = "native-cpp";
    type167_text_document["formula_source_mode"] = "inline-post";
    type167_text_document["formula"] = "CONTRACT_TYPE167_TEXT";
    type167_text_document["market"] = "sz";
    type167_text_document["code"] = "000001";
    type167_text_document["analysis"] = tdx::Json::object();
    type167_text_document["analysis"]["syntax_supported"] = true;
    type167_text_document["analysis"]["executable"] = false;
    type167_text_document["analysis"]["executable_with_context"] = true;
    type167_text_document["analysis"]["automatic_context_dependencies"] = tdx::Json::array();
    for (const auto *dependency : {"LEVEL1HYBLOCK", "MAINBUSINESS", "MOREHYBLOCK", "SIMIBLOCK",
                                   "ZDBLOCK", "ZDBLOCKNUM", "ZHBLOCK", "ZHBLOCKNUM"})
        type167_text_document["analysis"]["automatic_context_dependencies"].push_back(dependency);
    type167_text_document["context_metadata"] = tdx::Json::object();
    auto &type167_metadata = type167_text_document["context_metadata"];
    type167_metadata["formula_text_symbols"] = tdx::Json::object();
    type167_metadata["formula_text_symbols"]["LEVEL1HYBLOCK"] = "银行";
    type167_metadata["formula_text_symbols"]["MAINBUSINESS"] = "零售金融业务";
    type167_metadata["formula_text_symbols"]["MOREHYBLOCK"] = "股份制银行";
    type167_metadata["formula_text_symbols"]["ZDBLOCK"] = " ";
    type167_metadata["formula_text_symbols"]["ZHBLOCK"] = " ";
    type167_metadata["formula_text_symbols"]["SIMIBLOCK"] = " ";
    type167_metadata["formula_text_symbol_sources"] = tdx::Json::object();
    type167_metadata["formula_text_symbol_sources"]["LEVEL1HYBLOCK"] =
        "tdxw-type167-research-industry-code-prefix3-local-hierarchy";
    type167_metadata["formula_text_symbol_sources"]["MAINBUSINESS"] =
        "tdxw-type167-specgpext-third-field";
    type167_metadata["formula_text_symbol_sources"]["MOREHYBLOCK"] =
        "tdxw-type167-user-ini-industry-mode-leaf-name";
    type167_metadata["formula_text_symbol_sources"]["ZDBLOCK"] =
        "tdxw-command8-category2-blocknew-membership-pipe-to-space";
    type167_metadata["formula_text_symbol_sources"]["ZHBLOCK"] =
        "tdxw-command8-category3-lcidx-cis-membership-pipe-to-space";
    type167_metadata["formula_text_symbol_sources"]["SIMIBLOCK"] =
        "tdxw-command8-category0-no-provider-branch-pipe-to-space";
    type167_metadata["level1_research_industry"] = tdx::Json::object();
    type167_metadata["level1_research_industry"]["family"] = "research-industry";
    type167_metadata["level1_research_industry"]["level"] = 1;
    type167_metadata["level1_research_industry"]["source_key"] = "X50";
    type167_metadata["more_industry"] = tdx::Json::object();
    type167_metadata["more_industry"]["configured_mode"] = 2;
    type167_metadata["more_industry"]["type167_mode_byte"] = 1;
    type167_metadata["more_industry"]["selected_family"] = "research-industry";
    type167_metadata["more_industry"]["source_key"] = "X500102";
    type167_metadata["custom_block_membership_count"] = 0;
    type167_metadata["custom_block_directory_count"] = 2;
    type167_metadata["custom_block_mode"] =
        "tdxw-command8-category2-zxg-tjg-blocknew-local-reconstruction";
    type167_metadata["combination_block_directory_source"] = "C:\\new_tdx\\T0002\\lc\\lcidx.lii";
    type167_metadata["combination_block_directory_count"] = 0;
    type167_metadata["combination_block_readable_member_file_count"] = 0;
    type167_metadata["combination_block_membership_count"] = 0;
    type167_metadata["combination_block_mode"] =
        "tdxw-command8-category3-lcidx-lii-cis-local-reconstruction";
    type167_metadata["similar_block_mode"] = "tdxw-command8-category0-empty-in-current-host-build";
    type167_metadata["main_business_source"] = "C:\\new_tdx\\T0002\\hq_cache\\specgpext.txt";
    type167_metadata["main_business_record_count"] = 5000;
    type167_metadata["main_business_mode"] =
        "tdxw-type167-specgpext-enhanced-function-local-reconstruction";
    type167_text_document["points"] = tdx::Json::array();
    for (int index = 0; index < 120; ++index) {
        tdx::Json point = tdx::Json::object();
        point["values"] = tdx::Json::object();
        point["values"]["L"] = 1;
        point["values"]["M"] = 1;
        point["values"]["H"] = 1;
        point["values"]["Z"] = 1;
        point["values"]["N"] = 0;
        point["values"]["Q"] = 1;
        point["values"]["R"] = 0;
        point["values"]["S"] = 1;
        type167_text_document["points"].push_back(std::move(point));
    }
    const auto type167_text =
        tdx::evaluate_api_contract_response("formula-type167-text-inline-post", 200,
                                            "application/json", type167_text_document.dump(-1));
    require(type167_text.at("passed").as_bool(),
            "type-167 local text native formula contract failed");

    tdx::Json security_status_document = tdx::Json::object();
    security_status_document["engine"] = "tdx-source-interpreter-v1";
    security_status_document["execution_mode"] = "native-cpp";
    security_status_document["formula_source_mode"] = "inline-post";
    security_status_document["formula"] = "CONTRACT_SECURITY_STATUS";
    security_status_document["market"] = "sz";
    security_status_document["code"] = "000001";
    security_status_document["analysis"] = tdx::Json::object();
    security_status_document["analysis"]["syntax_supported"] = true;
    security_status_document["analysis"]["context_bindable"] = true;
    security_status_document["analysis"]["executable_with_context"] = true;
    security_status_document["analysis"]["automatic_context_dependencies"] = tdx::Json::array();
    for (const char *dependency :
         {"IST0CODE", "ISSTCODE", "ISQUITCODE", "ISQHQQCODE", "ISJYDATE", "LOCALDAYNUM"})
        security_status_document["analysis"]["automatic_context_dependencies"].push_back(
            dependency);
    security_status_document["context_metadata"] = tdx::Json::object();
    auto &status_metadata = security_status_document["context_metadata"];
    status_metadata["security_status_mode"] =
        "tdxw-types167-120-105-exact-host-field-reconstruction";
    status_metadata["security_status_category_source"] = "local-tnf-offset-282";
    status_metadata["security_status_category"] = 1;
    status_metadata["security_status_spblock_source"] = "fixture/spblock.dat";
    status_metadata["security_status_quit_source"] = "fixture/infoharbor_spec.cfg";
    status_metadata["security_status_values"] = tdx::Json::object();
    for (const char *dependency : {"IST0CODE", "ISSTCODE", "ISQUITCODE", "ISQHQQCODE"})
        status_metadata["security_status_values"][dependency] = 0;
    status_metadata["host_calendar_mode"] = "tdxw-types122-168-exact-host-field-reconstruction";
    status_metadata["host_calendar_current_trading_date_source"] =
        "latest-request-kline-date-proxy-for-tdxw-global-trading-date";
    status_metadata["host_calendar_current_trading_date"] = 20260807;
    status_metadata["host_calendar_machine_date"] = 20260809;
    status_metadata["host_calendar_local_day_file"] = "fixture/vipdoc/sz/lday/sz000001.day";
    status_metadata["host_calendar_local_day_file_exists"] = true;
    status_metadata["host_calendar_local_day_record_count"] = 8386;
    status_metadata["host_calendar_local_day_last_date"] = 20260610;
    status_metadata["host_calendar_local_day_synthetic_current"] = true;
    status_metadata["host_calendar_local_day_effective_count"] = 8387;
    status_metadata["host_calendar_values"] = tdx::Json::object();
    status_metadata["host_calendar_values"]["ISJYDATE"] = 0;
    status_metadata["host_calendar_values"]["LOCALDAYNUM"] = 8387;
    security_status_document["points"] = tdx::Json::array();
    for (int index = 0; index < 120; ++index) {
        tdx::Json point = tdx::Json::object();
        point["values"] = tdx::Json::object();
        point["values"]["T0"] = 0;
        point["values"]["ST"] = 0;
        point["values"]["QUIT"] = 0;
        point["values"]["QHQQ"] = 0;
        point["values"]["JY"] = 0;
        point["values"]["LD"] = 8387;
        security_status_document["points"].push_back(std::move(point));
    }
    const auto security_status =
        tdx::evaluate_api_contract_response("formula-security-status-inline-post", 200,
                                            "application/json", security_status_document.dump(-1));
    require(security_status.at("passed").as_bool(),
            "security status native formula contract failed");

    tdx::Json host_summary_document = tdx::Json::object();
    host_summary_document["engine"] = "tdx-source-interpreter-v1";
    host_summary_document["execution_mode"] = "native-cpp";
    host_summary_document["formula_source_mode"] = "inline-post";
    host_summary_document["formula"] = "CONTRACT_HOST_SUMMARY";
    host_summary_document["market"] = "sz";
    host_summary_document["code"] = "000001";
    host_summary_document["analysis"] = tdx::Json::object();
    host_summary_document["analysis"]["syntax_supported"] = true;
    host_summary_document["analysis"]["context_bindable"] = true;
    host_summary_document["analysis"]["executable_with_context"] = true;
    host_summary_document["analysis"]["automatic_context_dependencies"] = tdx::Json::array();
    for (const auto *name : {"BETAVALUE", "HYSJL", "HYSYL", "MAINZSHQ", "SHAPE_LONG", "SHAPE_MID",
                             "SHAPE_SHORT", "TOTALHQINFO", "TOTALMMPAMO"})
        host_summary_document["analysis"]["automatic_context_dependencies"].push_back(name);
    host_summary_document["analysis"]["context_bindings_unavailable"] = tdx::Json::array();
    host_summary_document["context_metadata"] = tdx::Json::object();
    auto &summary_metadata = host_summary_document["context_metadata"];
    summary_metadata["security_stat_functions"] = tdx::Json::object();
    summary_metadata["security_stat_functions"]["security_found"] = true;
    summary_metadata["security_stat_functions"]["shape_packed"] = 50601;
    summary_metadata["security_stat_functions"]["mode"] =
        "TCalc-opcodes1333-1345-1347-TdxW-type163-tdxstat-columns3-23";
    summary_metadata["public_market_summary_functions"] = tdx::Json::object();
    summary_metadata["public_market_summary_functions"]["command"] = "0x0547";
    summary_metadata["public_market_summary_functions"]["received"] = 7;
    summary_metadata["public_market_summary_functions"]["mainzshq_mode"] =
        "TCalc-opcode1368-TdxW-type102-index-L1-broadcast";
    summary_metadata["public_market_summary_functions"]["totalhqinfo_mode"] =
        "TCalc-opcode1384-SH880005-public-L1-broadcast";
    summary_metadata["public_market_summary_functions"]["totalmmpamo_mode"] =
        "TCalc-opcode1376-TdxW-type168-SH999997-public-L1-selectors1-4";
    summary_metadata["public_market_summary_functions"]["totalmmpamo_unit"] = "100m-yuan";
    summary_metadata["industry_valuation"] = tdx::Json::object();
    auto &industry_valuation = summary_metadata["industry_valuation"];
    industry_valuation["schema"] = "tdx-formula-industry-valuation-v1";
    industry_valuation["selected_code"] = "880471";
    industry_valuation["availability"] = "public-hyzt-record";
    industry_valuation["source_resource"] = "list/func_gx_hyzt101_1.jsn";
    industry_valuation["bindings"] = tdx::Json::object();
    industry_valuation["bindings"]["HYSYL"] = tdx::Json::parse(
        R"({"value":5.2105,"available":true,"source_field":"hyPE","tcalc_opcode":1328,"tdxw_callback_type":120,"tdxw_return_offset":60})");
    industry_valuation["bindings"]["HYSJL"] = tdx::Json::parse(
        R"({"value":0.5302,"available":true,"source_field":"hyPB","tcalc_opcode":1344,"tdxw_callback_type":163,"tdxw_return_offset":380})");
    host_summary_document["points"] = tdx::Json::array();
    for (int index = 0; index < 120; ++index) {
        tdx::Json point = tdx::Json::object();
        point["values"] = tdx::Json::object();
        auto &values = point["values"];
        values["BETA"] = -0.2025;
        values["S1"] = 5;
        values["S2"] = 6;
        values["S3"] = 1;
        values["PE"] = 5.2105;
        values["PB"] = 0.5302;
        for (int field = 0; field <= 9; ++field)
            values["M" + std::to_string(field)] = field == 0   ? 3939.97
                                                  : field == 1 ? 3928.10
                                                               : field;
        for (int field = 1; field <= 6; ++field)
            values["T" + std::to_string(field)] = field * 100.0;
        for (int field = 1; field <= 4; ++field)
            values["A" + std::to_string(field)] = field * 10.0;
        host_summary_document["points"].push_back(std::move(point));
    }
    const auto host_summary =
        tdx::evaluate_api_contract_response("formula-host-summary-inline-post", 200,
                                            "application/json", host_summary_document.dump(-1));
    require(host_summary.at("passed").as_bool(), "host summary native formula contract failed");

    tdx::Json market_alias_document = tdx::Json::object();
    market_alias_document["engine"] = "tdx-source-interpreter-v1";
    market_alias_document["execution_mode"] = "native-cpp";
    market_alias_document["formula_source_mode"] = "inline-post";
    market_alias_document["formula"] = "CONTRACT_MARKET_BREADTH_DYNA";
    market_alias_document["market"] = "sz";
    market_alias_document["code"] = "000001";
    market_alias_document["analysis"] = tdx::Json::object();
    market_alias_document["analysis"]["syntax_supported"] = true;
    market_alias_document["analysis"]["context_bindable"] = true;
    market_alias_document["analysis"]["executable_with_context"] = true;
    market_alias_document["analysis"]["automatic_context_dependencies"] = tdx::Json::array();
    for (const auto *name : {"DYNA_LB", "DYNA_NOW", "DYNA_ZAF", "DYNA_ZAS", "INDEXADV", "INDEXDEC"})
        market_alias_document["analysis"]["automatic_context_dependencies"].push_back(name);
    market_alias_document["analysis"]["context_bindings_unavailable"] = tdx::Json::array();
    market_alias_document["context_metadata"] = tdx::Json::object();
    auto &market_alias_metadata = market_alias_document["context_metadata"];
    market_alias_metadata["benchmark"] = "sz:399001";
    market_alias_metadata["benchmark_mode"] =
        "TCalc-opcodes1201-1202-native-market-code-selection-offsets31-33";
    market_alias_metadata["dynamic_alias_mode"] =
        "TCalc-opcodes1380-1383-DYNAINFO-selectors7-14-17-24-public-L1-broadcast";
    market_alias_metadata["dynamic_quote_command"] = "0x054C";
    market_alias_metadata["dynamic_speed_command"] = "0x053E";
    market_alias_metadata["dynamic_aliases"] = tdx::Json::object();
    const auto append_alias = [&](const char *name, int selector, int opcode, const char *command,
                                  double value) {
        auto &alias = market_alias_metadata["dynamic_aliases"][name];
        alias = tdx::Json::object();
        alias["dynainfo_selector"] = selector;
        alias["tcalc_opcode"] = opcode;
        alias["broadcast"] = true;
        alias["source_command"] = command;
        alias["value"] = value;
    };
    append_alias("DYNA_NOW", 7, 1380, "0x054C", 11.35);
    append_alias("DYNA_ZAF", 14, 1381, "0x054C", 0.01429848);
    append_alias("DYNA_LB", 17, 1382, "0x054C", 1.0686987);
    append_alias("DYNA_ZAS", 24, 1383, "0x053E", -0.0008);
    market_alias_document["points"] = tdx::Json::array();
    for (int index = 0; index < 120; ++index) {
        tdx::Json point = tdx::Json::object();
        point["values"] = tdx::Json::object();
        auto &values = point["values"];
        values["IA"] = 1800 + index;
        values["IAC"] = 1800 + index;
        values["ID"] = 1000 - index;
        values["IDC"] = 1000 - index;
        values["N"] = 11.35;
        values["NC"] = 11.35;
        values["Z"] = 0.01429848;
        values["L"] = 1.0686987;
        values["S"] = -0.0008;
        market_alias_document["points"].push_back(std::move(point));
    }
    const auto market_alias_contract =
        tdx::evaluate_api_contract_response("formula-market-breadth-dyna-inline-post", 200,
                                            "application/json", market_alias_document.dump(-1));
    require(market_alias_contract.at("passed").as_bool(),
            "market breadth and dynamic quote alias contract failed");

    tdx::Json relation_document = tdx::Json::object();
    relation_document["engine"] = "tdx-source-interpreter-v1";
    relation_document["execution_mode"] = "native-cpp";
    relation_document["formula_source_mode"] = "inline-post";
    relation_document["formula"] = "CONTRACT_SECURITY_RELATION_DIVFACTOR";
    relation_document["market"] = "sh";
    relation_document["code"] = "110075";
    relation_document["analysis"] = tdx::Json::object();
    relation_document["analysis"]["syntax_supported"] = true;
    relation_document["analysis"]["context_bindable"] = true;
    relation_document["analysis"]["executable_with_context"] = true;
    relation_document["analysis"]["automatic_context_dependencies"] = tdx::Json::array();
    for (const auto *name : {"DIVFACTOR", "DPZSCODE", "DPZSNAME", "UNDERCODE", "UNDERLYC"})
        relation_document["analysis"]["automatic_context_dependencies"].push_back(name);
    relation_document["analysis"]["context_bindings_unavailable"] = tdx::Json::array();
    relation_document["context_metadata"] = tdx::Json::object();
    auto &relation_metadata = relation_document["context_metadata"];
    relation_metadata["main_index_identity"] = tdx::Json::object();
    relation_metadata["main_index_identity"]["security"] = "sh:999999";
    relation_metadata["main_index_identity"]["code"] = "999999";
    relation_metadata["main_index_identity"]["name"] = "上证指数";
    relation_metadata["main_index_identity"]["mode"] =
        "TCalc-sub_10044AA0-sub_10044C00-exact-market-code-selection";
    relation_metadata["underlying_identity"] = tdx::Json::object();
    relation_metadata["underlying_identity"]["available"] = true;
    relation_metadata["underlying_identity"]["market_id"] = 1;
    relation_metadata["underlying_identity"]["code"] = "600029";
    relation_metadata["underlying_identity"]["source"] =
        "T0002/hq_cache/speckzzdata.txt-client-convertible-master";
    relation_metadata["underlying_identity"]["host_layout"] =
        "type120-market-u16-at176-code-6bytes-at178";
    relation_metadata["underlying_identity"]["close_mode"] =
        "TCalc-opcode1320-date-time-aligned-35-byte-kline-close-offset19";
    relation_metadata["divfactor_mode"] =
        "TCalc-opcode1359-type164-category1-offset21-bonus-only-float32";
    relation_metadata["divfactor_event_date_count"] = 3;
    relation_metadata["divfactor_matched_bar_count"] = 1;
    relation_document["points"] = tdx::Json::array();
    for (int index = 0; index < 120; ++index) {
        tdx::Json point = tdx::Json::object();
        point["values"] = tdx::Json::object();
        auto &values = point["values"];
        values["DPC"] = 1;
        values["DPN"] = 1;
        values["UC"] = 1;
        values["UCC"] = 1;
        values["U"] = 10.0 + index / 100.0;
        values["U2"] = 10.0 + index / 100.0;
        values["F"] = 1.0;
        values["H"] = 1.0;
        relation_document["points"].push_back(std::move(point));
    }
    const auto relation_contract =
        tdx::evaluate_api_contract_response("formula-security-relation-divfactor-inline-post", 200,
                                            "application/json", relation_document.dump(-1));
    require(relation_contract.at("passed").as_bool(),
            "security relation and DIVFACTOR contract failed");

    auto string_builder_kline = formula_sample(120);
    string_builder_kline["name"] = "平安银行";
    auto string_builder_document = tdx::evaluate_formula_source_document(
        string_builder_kline,
        "SEQ:=TOTALBARSCOUNT-CURRBARSCOUNT+1;"
        "L:STRLEN('通达信');S0:STRCMP(SUBSTR('通达信',3,2),'达');"
        "S1:STRCMP(SUBSTR('ABC',99,1),'C');"
        "S2:STRCMP(STRSPACE('A'),'A ');"
        "S3:STRCMP(STRCAT6('A','B','C','D','E','F'),'ABCDEF');"
        "S4:STRCMP(VARCAT('A','B'),'AB');"
        "S5:STRCMP(VARCAT6('A','B','C','D','E','F'),'ABCDEF');"
        "N:STR2CON(SUBSTR('2365.02',1,7));"
        "DRAWTEXT(1,LOW,VARCAT6(VAR2STR(SEQ,0),'|',CON2STR(SEQ,0),'|',"
        "STRSPACE(SUBSTR('通达信',3,2)),STKNAME));"
        "DRAWTEXT(1,HIGH,STRCAT6(VAR2STR(SEQ,0),'|',CON2STR(SEQ,0),'|',"
        "STRSPACE('定'),'值'));",
        {}, "CONTRACT_STRING_BUILDERS");
    string_builder_document["formula_source_mode"] = "inline-post";
    const auto string_builders =
        tdx::evaluate_api_contract_response("formula-string-builders-inline-post", 200,
                                            "application/json", string_builder_document.dump(-1));
    require(string_builders.at("passed").as_bool(),
            "GBK and series/non-series string-builder contract failed");
}

} // namespace recon_contract_test
