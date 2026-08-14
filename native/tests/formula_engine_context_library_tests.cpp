#include "formula_engine_test_support.hpp"

namespace formula_engine_test {

void run_context_and_library_tests() {
    const auto limit_price_analysis = tdx::analyze_formula_source(
        "BASE:=MA(CLOSE,2);Z:ZTPRICE(BASE,0.1);"
        "D:DTPRICE(CLOSE,0.1);SAFE:BASE;");
    const auto string_set = [](const tdx::Json& values) {
        std::set<std::string> result;
        for (const auto& value : values.as_array())
            result.insert(value.as_string());
        return result;
    };
    const std::set<std::string> limit_price_context{
        "HOST_EVALUATOR_MARKET_WORD_RAW",
        "HOST_TYPE120_SECURITY_CLASS_RAW"};
    require(!limit_price_analysis.at("executable").as_bool() &&
                !limit_price_analysis.at("executable_with_context").as_bool() &&
                limit_price_analysis.at("explicit_context_bindable").as_bool() &&
                limit_price_analysis.at("numeric_signal_safe").as_bool() &&
                !limit_price_analysis.at("has_degraded_numeric_output").as_bool() &&
                limit_price_analysis.at("semantic_fidelity").as_string() ==
                    "numeric-safe" &&
                !limit_price_analysis.at("pure_ohlcv").as_bool(),
            "ZTPRICE/DTPRICE are exact only through their explicit raw host context");
    require(string_set(limit_price_analysis.at("external_dependencies")) ==
                    limit_price_context &&
                string_set(limit_price_analysis.at("context_bindings_required")) ==
                    limit_price_context &&
                string_set(limit_price_analysis.at("context_bindings_unavailable")) ==
                    limit_price_context &&
                string_set(limit_price_analysis.at(
                    "explicit_context_bindings_required")) ==
                    limit_price_context &&
                limit_price_analysis.at("degraded_numeric_output_causes")
                    .as_object().empty() &&
                !limit_price_analysis.at("has_semantic_surrogate").as_bool() &&
                limit_price_analysis.at("presentation_return_surrogates")
                    .as_array()
                    .empty(),
            "limit-price analysis exposes both raw type-120 inputs without a surrogate");

    const auto limit_price_template =
        tdx::make_formula_explicit_context_template_document(
            [&] {
                auto library = tdx::Json::object();
                library["formulas"] = tdx::Json::array();
                auto formula = tdx::Json::object();
                formula["code"] = "LIMITCTX";
                formula["kind_key"] = "technical";
                formula["source_text"] = "Z:ZTPRICE(CLOSE,0.1);";
                formula["parameters"] = tdx::Json::array();
                library["formulas"].push_back(std::move(formula));
                return library;
            }(),
            {"LIMITCTX"},
            {"2026-08-12|15:00", "2026-08-13|15:00"});
    require(limit_price_template.at("formula_scalar_bindings").size() == 2 &&
                limit_price_template.at("formula_scalar_bindings")
                    .at("HOST_TYPE120_SECURITY_CLASS_RAW").is_null() &&
                limit_price_template.at("formula_scalar_bindings")
                    .at("HOST_EVALUATOR_MARKET_WORD_RAW").is_null() &&
                limit_price_template.at("series").as_object().empty() &&
                limit_price_template.at("_template")
                    .at("scalar_binding_count").as_number() == 2.0 &&
                limit_price_template.at("_template")
                    .at("series_binding_count").as_number() == 0.0 &&
                limit_price_template.at("_template")
                    .at("stamp_count").as_number() == 2.0,
            "limit-price template emits two scalar placeholders without per-bar duplication");
    for (const auto& row :
         limit_price_template.at("_template").at("bindings").as_array()) {
        require(row.at("source_kind").as_string() ==
                    "caller-host-raw-scalar" &&
                    row.at("value_shape").as_string() == "u16-scalar",
                "limit-price template labels caller-owned raw u16 scalars precisely");
    }
    require(limit_price_template.at("_template").at("fill_rule")
                    .as_string().find("formula_scalar_bindings") !=
                std::string::npos,
            "limit-price template explains scalar and series fill shapes");

    bool missing_limit_context_rejected = false;
    try {
        (void)tdx::evaluate_formula_source_document(
            sample(3), "Z:ZTPRICE(10.05,0.1);", {}, "LIMIT_NO_CONTEXT");
    } catch (const tdx::Error&) {
        missing_limit_context_rejected = true;
    }
    require(missing_limit_context_rejected,
            "limit-price execution rejects absent raw host context");

    tdx::Json ordinary_limit_context = tdx::Json::object();
    ordinary_limit_context["formula_scalar_bindings"] = tdx::Json::object();
    ordinary_limit_context["formula_scalar_bindings"]
                          ["HOST_TYPE120_SECURITY_CLASS_RAW"] = 2.0;
    ordinary_limit_context["formula_scalar_bindings"]
                          ["HOST_EVALUATOR_MARKET_WORD_RAW"] = 0.0;
    const auto ordinary_limit = tdx::evaluate_formula_source_document(
        sample(3),
        "Z:ZTPRICE(10.05,0.1);D:DTPRICE(10.05,0.1);",
        {}, "LIMIT_ORDINARY", &ordinary_limit_context);
    require(std::abs(point_value(ordinary_limit, 2, "Z").as_number() -
                     11.0600004196167) < 1e-12 &&
                std::abs(point_value(ordinary_limit, 2, "D").as_number() -
                         9.05000019073486) < 1e-12,
            "ordinary-market type-120 rounding preserves both native float landings");
    const auto final_rate_limit = tdx::evaluate_formula_source_document(
        sample(3),
        "P:=IF(BARSCOUNT(CLOSE)=0,DRAWNULL,CLOSE);"
        "R:=IF(ISLASTBAR,0.2,0.1);Z:ZTPRICE(P,R);D:DTPRICE(P,R);",
        {}, "LIMIT_FINAL_RATE", &ordinary_limit_context);
    require(point_value(final_rate_limit, 0, "Z").is_null() &&
                point_value(final_rate_limit, 0, "D").is_null() &&
                std::abs(point_value(final_rate_limit, 1, "Z").as_number() -
                         13.2) < 1e-6 &&
                std::abs(point_value(final_rate_limit, 1, "D").as_number() -
                         8.8) < 1e-6,
            "limit price reads only the final rate and preserves source sentinels");

    auto invalid_limit_context = ordinary_limit_context;
    invalid_limit_context["formula_scalar_bindings"]
                         ["HOST_TYPE120_SECURITY_CLASS_RAW"] = -1.0;
    bool invalid_limit_context_rejected = false;
    try {
        (void)tdx::evaluate_formula_source_document(
            sample(3), "Z:ZTPRICE(CLOSE,0.1);", {},
            "LIMIT_INVALID_CONTEXT", &invalid_limit_context);
    } catch (const tdx::Error&) {
        invalid_limit_context_rejected = true;
    }
    require(invalid_limit_context_rejected,
            "limit-price raw host context is constrained to exact u16 values");

    tdx::Json special_limit_context = tdx::Json::object();
    special_limit_context["formula_scalar_bindings"] = tdx::Json::object();
    special_limit_context["formula_scalar_bindings"]
                         ["HOST_TYPE120_SECURITY_CLASS_RAW"] = 3.0;
    special_limit_context["formula_scalar_bindings"]
                         ["HOST_EVALUATOR_MARKET_WORD_RAW"] = 44.0;
    const auto special_limit = tdx::evaluate_formula_source_document(
        sample(3),
        "Z:ZTPRICE(10.05,0.1);D:DTPRICE(10.05,0.1);",
        {}, "LIMIT_SPECIAL", &special_limit_context);
    require(std::abs(point_value(special_limit, 2, "Z").as_number() -
                     11.0550003051758) < 1e-12 &&
                std::abs(point_value(special_limit, 2, "D").as_number() -
                         9.04500007629395) < 1e-12,
            "class-3 special-market type-120 path uses thousandths and native biases");

    const auto nested_limit_price_analysis = tdx::analyze_formula_source(
        "NESTED:RGB(ZTPRICE(CLOSE,0.1),0,0);");
    const auto& nested_limit_price_causes =
        nested_limit_price_analysis.at("degraded_numeric_output_causes")
            .at("NESTED")
            .as_array();
    const auto has_nested_limit_price_cause =
        [&nested_limit_price_causes](const std::string& expected) {
            return std::any_of(
                nested_limit_price_causes.begin(),
                nested_limit_price_causes.end(),
                [&expected](const tdx::Json& cause) {
                    return cause.as_string() == expected;
                });
        };
    require(!nested_limit_price_analysis.at("numeric_signal_safe").as_bool() &&
                has_nested_limit_price_cause("RGB") &&
                nested_limit_price_causes.size() == 1,
            "numeric surrogate remains the only nested degradation after exact limit price");

    const auto ordinary_analysis = tdx::analyze_formula_source(
        "BASE:=MA(CLOSE,2);SAFE:BASE;");
    require(ordinary_analysis.at("executable").as_bool() &&
                ordinary_analysis.at("numeric_signal_safe").as_bool() &&
                !ordinary_analysis.at("has_degraded_numeric_output").as_bool() &&
                ordinary_analysis.at("semantic_fidelity").as_string() ==
                    "numeric-safe" &&
                ordinary_analysis.at("pure_ohlcv").as_bool(),
            "ordinary OHLCV analysis remains numeric-safe and pure");

    tdx::Json unsafe_limit_scan = tdx::Json::object();
    unsafe_limit_scan["code"] = "UNSAFE_LIMIT_SCAN";
    unsafe_limit_scan["kind_key"] = "selection";
    unsafe_limit_scan["source_text"] = "SIGNAL:ZTPRICE(CLOSE,0.1);";
    unsafe_limit_scan["parameters"] = tdx::Json::array();
    const auto rejected_limit_scan =
        tdx::scan_formula_documents({sample(10)}, unsafe_limit_scan);
    require(rejected_limit_scan.at("evaluated").as_number() == 0.0 &&
                rejected_limit_scan.at("error_count").as_number() == 1.0 &&
                rejected_limit_scan.at("errors").as_array()[0]
                    .at("error").as_string().find(
                        "HOST_TYPE120_SECURITY_CLASS_RAW") != std::string::npos,
            "scanner reports missing explicit ZTPRICE host context per input");

    tdx::Json unsafe_limit_backtest = tdx::Json::object();
    unsafe_limit_backtest["code"] = "UNSAFE_LIMIT_BACKTEST";
    unsafe_limit_backtest["kind_key"] = "expert";
    unsafe_limit_backtest["source_text"] =
        "ENTERLONG:DTPRICE(CLOSE,0.1)>0;EXITLONG:0;";
    unsafe_limit_backtest["parameters"] = tdx::Json::array();
    bool rejected_limit_backtest = false;
    try {
        (void)tdx::backtest_formula_document(
            sample(20), unsafe_limit_backtest);
    } catch (const tdx::Error&) {
        rejected_limit_backtest = true;
    }
    require(rejected_limit_backtest,
            "backtest rejects absent explicit DTPRICE host context");

    const auto degraded_analysis =
        tdx::analyze_formula_source("TEXT:=STRCAT('A','B'); BAD:TEXT+1;");
    require(!degraded_analysis.at("numeric_signal_safe").as_bool() &&
                degraded_analysis.at("has_degraded_numeric_output").as_bool() &&
                degraded_analysis.at("degraded_numeric_output_causes")
                        .at("BAD")
                        .as_array()[0]
                        .as_string() == "STRCAT",
            "string-handle placeholder must be traced into numeric outputs");

    tdx::Json unsafe_scan_formula = tdx::Json::object();
    unsafe_scan_formula["code"] = "UNSAFE";
    unsafe_scan_formula["kind_key"] = "selection";
    unsafe_scan_formula["source_text"] = "BAD:RGB(1,2,3)+1;";
    unsafe_scan_formula["parameters"] = tdx::Json::array();
    bool rejected_unsafe_scan = false;
    try {
        (void)tdx::scan_formula_documents({sample(10)}, unsafe_scan_formula);
    } catch (const tdx::Error &) {
        rejected_unsafe_scan = true;
    }
    require(rejected_unsafe_scan, "scanner must reject surrogate-tainted numeric outputs");

    tdx::Json unsafe_expert = tdx::Json::object();
    unsafe_expert["code"] = "UNSAFEEXPERT";
    unsafe_expert["kind_key"] = "expert";
    unsafe_expert["source_text"] = "TEXT:=STRCAT('A','B');ENTERLONG:TEXT;EXITLONG:0;";
    unsafe_expert["parameters"] = tdx::Json::array();
    bool rejected_unsafe_backtest = false;
    try {
        (void)tdx::backtest_formula_document(sample(20), unsafe_expert);
    } catch (const tdx::Error &) {
        rejected_unsafe_backtest = true;
    }
    require(rejected_unsafe_backtest, "backtest must reject surrogate-tainted numeric outputs");

    tdx::Json context = tdx::Json::object();
    context["finance"] = tdx::Json::object();
    context["finance"]["7"] = 1000000;
    context["dynainfo"] = tdx::Json::object();
    context["dynainfo"]["3"] = 10.0;
    context["symbols"] = tdx::Json::object();
    context["symbols"]["CAPITAL"] = 10000;
    const auto bound = tdx::evaluate_formula_source_document(
        sample(10), "X:FINANCE(7)/CAPITAL+DYNAINFO(3);", {}, "BOUND", &context);
    require(std::abs(point_value(bound, 9, "X").as_number() - 110.0) < 1e-9,
            "external context bindings");
    const auto bound_analysis = tdx::analyze_formula_source("X:FINANCE(7)/CAPITAL+DYNAINFO(3);");
    require(bound_analysis.at("executable_with_context").as_bool(), "context-bindable analysis");
    const auto added_finance_analysis = tdx::analyze_formula_source(
        "M:FINANCE(2);U:FINANCE(31);UP:FINANCE(32);"
        "RP:FINANCE(37);EP:FINANCE(38);D:FINANCE(53);");
    require(added_finance_analysis.at("executable_with_context").as_bool() &&
                added_finance_analysis.at("context_bindings_unavailable")
                    .as_array().empty(),
            "TCalc FINANCE 2/31/32/37/38/53 are context bindable");
    for (const auto& [selector, value] :
         std::array<std::pair<const char*, double>, 6>{{
             {"2", 31.0}, {"31", 0.0}, {"32", 0.0},
             {"37", 4.0}, {"38", 20.0}, {"53", 4.5}}})
        context["finance"][selector] = value;
    const auto added_finance_bound = tdx::evaluate_formula_source_document(
        sample(10),
        "M:FINANCE(2);U:FINANCE(31);UP:FINANCE(32);"
        "RP:FINANCE(37);EP:FINANCE(38);D:FINANCE(53);",
        {}, "ADDED_FINANCE", &context);
    require(point_value(added_finance_bound, 9, "M").as_number() == 31.0 &&
                point_value(added_finance_bound, 9, "RP").as_number() == 4.0 &&
                point_value(added_finance_bound, 9, "D").as_number() == 4.5,
            "new finance bindings reach the interpreter without remapping");
    const auto cage_analysis = tdx::analyze_formula_source("UP:DYNAINFO(28); DOWN:DYNAINFO(29);");
    require(cage_analysis.at("executable_with_context").as_bool(),
            "DYNAINFO(28/29) cage prices are depth-context bindable");
    context["dynainfo"]["28"] = 10.2;
    context["dynainfo"]["29"] = 9.8;
    const auto cage_bound = tdx::evaluate_formula_source_document(
        sample(10), "UP:DYNAINFO(28); DOWN:DYNAINFO(29);", {}, "CAGE", &context);
    require(point_value(cage_bound, 9, "UP").as_number() ==
                    static_cast<double>(static_cast<float>(10.2)) &&
                point_value(cage_bound, 9, "DOWN").as_number() ==
                    static_cast<double>(static_cast<float>(9.8)),
            "DYNAINFO(28/29) context values reach the interpreter");
    const auto display_analysis =
        tdx::analyze_formula_source("X:IF(HQCRBK=0,MA(C,2),DRAWNULL);"
                                    "DRAWTEXT_FIX(ISLASTBAR,0,0,0,STRCAT(CON2STR(2,0),' bars')); ");
    require(display_analysis.at("executable").as_bool(),
            "host display colour and non-series string functions execute headlessly");
    const auto display_bound = tdx::evaluate_formula_source_document(
        sample(10),
        "X:IF(HQCRBK=0,MA(C,2),DRAWNULL);"
        "DRAWTEXT_FIX(ISLASTBAR,0,0,0,STRCAT(CON2STR(2,0),' bars'));",
        {}, "DISPLAY");
    require(point_value(display_bound, 9, "X").is_number(),
            "HQCRBK dark-background default selects the numeric branch");
    const auto seal_analysis =
        tdx::analyze_formula_source("Z1:=STRCAT(HYBLOCK,' ');Z2:=STRCAT(Z1,DYBLOCK);"
                                    "DRAWTEXT_FIX(ISLASTBAR,0,0,0,STRCAT(Z2,GNBLOCK));"
                                    "X:HQCRBK=0 AND DYNAINFO(88)>0;");
    require(seal_analysis.at("executable_with_context").as_bool(),
            "DYNAINFO(88) and TDX block-text symbols are context bindable");
    tdx::Json seal_context = tdx::Json::object();
    seal_context["dynainfo"] = tdx::Json::object();
    seal_context["dynainfo"]["88"] = 10.0;
    seal_context["symbols"] = tdx::Json::object();
    for (const auto symbol : {"HYBLOCK", "DYBLOCK", "GNBLOCK"})
        seal_context["symbols"][symbol] = 0.0;
    seal_context["formula_text_symbols"] = tdx::Json::object();
    seal_context["formula_text_symbols"]["HYBLOCK"] = "银行";
    seal_context["formula_text_symbols"]["DYBLOCK"] = "深圳板块";
    seal_context["formula_text_symbols"]["GNBLOCK"] = "跨境支付";
    seal_context["dynainfo_seal_state_mode"] =
        "tcalc-dynainfo88-type163-offset384-limit-up-percent-when-sealed";
    const auto seal_bound =
        tdx::evaluate_formula_source_document(sample(10),
                                              "Z1:=STRCAT(HYBLOCK,' ');Z2:=STRCAT(Z1,DYBLOCK);"
                                              "DRAWTEXT_FIX(ISLASTBAR,0,0,0,STRCAT(Z2,GNBLOCK));"
                                              "X:HQCRBK=0 AND DYNAINFO(88)>0;",
                                              {}, "SEAL", &seal_context);
    require(point_value(seal_bound, 9, "X").as_number() == 1.0 &&
                seal_bound.at("context_metadata")
                        .at("formula_text_symbols")
                        .at("DYBLOCK")
                        .as_string() == "深圳板块",
            "seal state and materialized block text reach the interpreter");
    const auto block_metadata_analysis =
        tdx::analyze_formula_source("M:INBLOCK('银行');N:GNBLOCKNUM+FGBLOCKNUM+ZSBLOCKNUM;"
                                    "DRAWTEXT_FIX(ISLASTBAR,0,0,0,STRCAT(FGBLOCK,ZSBLOCK));"
                                    "DRAWTEXT_FIX(ISLASTBAR,0,0,0,HYZSCODE);");
    require(block_metadata_analysis.at("executable_with_context").as_bool() &&
                block_metadata_analysis.at("automatic_context_dependencies").as_array().size() == 7,
            "public block membership, count, text and industry-code metadata are context bindable");
    tdx::Json block_metadata_context = tdx::Json::object();
    block_metadata_context["symbols"] = tdx::Json::object();
    block_metadata_context["symbols"]["GNBLOCKNUM"] = 2.0;
    block_metadata_context["symbols"]["FGBLOCKNUM"] = 1.0;
    block_metadata_context["symbols"]["ZSBLOCKNUM"] = 3.0;
    block_metadata_context["formula_text_symbols"] = tdx::Json::object();
    block_metadata_context["formula_text_symbols"]["FGBLOCK"] = "大盘风格";
    block_metadata_context["formula_text_symbols"]["ZSBLOCK"] = "沪深300";
    block_metadata_context["formula_text_symbols"]["HYZSCODE"] = "880471";
    block_metadata_context["formula_private_text_symbols"] = tdx::Json::object();
    block_metadata_context["formula_private_text_symbols"]["__INBLOCK_MEMBERSHIPS"] =
        std::string("\x1F") + "银行\x1F跨境支付\x1F";
    const auto block_metadata_bound = tdx::evaluate_formula_source_document(
        sample(10),
        "M:INBLOCK('银行');MISS:INBLOCK('证券');"
        "N:GNBLOCKNUM+FGBLOCKNUM+ZSBLOCKNUM;"
        "DRAWTEXT_FIX(ISLASTBAR,0,0,0,STRCAT(FGBLOCK,ZSBLOCK));"
        "DRAWTEXT_FIX(ISLASTBAR,0,0,0,HYZSCODE);",
        {}, "BLOCKMETA", &block_metadata_context);
    require(point_value(block_metadata_bound, 9, "M").as_number() == 1.0 &&
                point_value(block_metadata_bound, 9, "MISS").as_number() == 0.0 &&
                point_value(block_metadata_bound, 9, "N").as_number() == 6.0,
            "INBLOCK performs an exact name match and block counts reach the interpreter");
    const auto blocksetnum_analysis =
        tdx::analyze_formula_source("H:BLOCKSETNUM('HY.银行');G:BLOCKSETNUM('GN.跨境支付');"
                                    "M:BLOCKSETNUM('不存在');");
    require(blocksetnum_analysis.at("executable_with_context").as_bool() &&
                blocksetnum_analysis.at("numeric_signal_safe").as_bool() &&
                blocksetnum_analysis.at("context_bindings_required").as_array().size() == 3 &&
                blocksetnum_analysis.at("automatic_context_dependencies").as_array().size() == 1,
            "literal BLOCKSETNUM calls are automatic local-catalog bindings");
    require(!tdx::analyze_formula_source("X:BLOCKSETNUM(HYBLOCK);")
                 .at("executable_with_context")
                 .as_bool(),
            "dynamic BLOCKSETNUM names remain unavailable until their string context is static");
    tdx::Json blocksetnum_context = tdx::Json::object();
    blocksetnum_context["formula_scalar_bindings"] = tdx::Json::object();
    blocksetnum_context["formula_scalar_bindings"]["BLOCKSETNUM#HY.银行"] = 42.0;
    blocksetnum_context["formula_scalar_bindings"]["BLOCKSETNUM#GN.跨境支付"] = 75.0;
    blocksetnum_context["formula_scalar_bindings"]["BLOCKSETNUM#不存在"] = 0.0;
    const auto blocksetnum_bound = tdx::evaluate_formula_source_document(
        sample(10),
        "H:BLOCKSETNUM('HY.银行');G:BLOCKSETNUM('GN.跨境支付');"
        "M:BLOCKSETNUM('不存在');",
        {}, "BLOCKSETNUM", &blocksetnum_context);
    require(point_value(blocksetnum_bound, 9, "H").as_number() == 42.0 &&
                point_value(blocksetnum_bound, 9, "G").as_number() == 75.0 &&
                point_value(blocksetnum_bound, 9, "M").as_number() == 0.0,
            "BLOCKSETNUM broadcasts exact local catalog member counts");
    const auto horcalc_analysis = tdx::analyze_formula_source("S:HORCALC('HY.银行',103,0,2);"
                                                              "R:HORCALC('GN.跨境支付',103,1,2);");
    require(horcalc_analysis.at("executable_with_context").as_bool() &&
                horcalc_analysis.at("numeric_signal_safe").as_bool() &&
                horcalc_analysis.at("context_bindings_required").as_array().size() == 2 &&
                horcalc_analysis.at("automatic_context_dependencies").as_array().size() == 1,
            "literal HORCALC calls are automatic horizontal-series bindings");
    require(!tdx::analyze_formula_source("X:HORCALC(HYBLOCK,103,0,2);")
                    .at("executable_with_context")
                    .as_bool() &&
                !tdx::analyze_formula_source("X:HORCALC('HY.银行',107,0,2);")
                     .at("executable_with_context")
                     .as_bool(),
            "dynamic and undocumented HORCALC argument surfaces remain unavailable");
    tdx::Json horcalc_context = tdx::Json::object();
    horcalc_context["series"] = tdx::Json::object();
    horcalc_context["series"]["HORCALC#HY.银行#103#0#2"] = tdx::Json::object();
    horcalc_context["series"]["HORCALC#GN.跨境支付#103#1#2"] = tdx::Json::object();
    const auto horcalc_sample = sample(10);
    for (std::size_t index = 0; index < horcalc_sample.at("bars").as_array().size(); ++index) {
        const auto &row = horcalc_sample.at("bars").as_array()[index];
        const auto key = row.at("date").as_string() + "|" + row.at("time").as_string();
        horcalc_context["series"]["HORCALC#HY.银行#103#0#2"][key] =
            static_cast<double>(100 + index);
        horcalc_context["series"]["HORCALC#GN.跨境支付#103#1#2"][key] =
            static_cast<double>(10 + index);
    }
    const auto horcalc_bound =
        tdx::evaluate_formula_source_document(horcalc_sample,
                                              "S:HORCALC('HY.银行',103,0,2);"
                                              "R:HORCALC('GN.跨境支付',103,1,2);",
                                              {}, "HORCALC", &horcalc_context);
    require(point_value(horcalc_bound, 0, "S").as_number() == 109.0 &&
                point_value(horcalc_bound, 9, "S").as_number() == 100.0 &&
                point_value(horcalc_bound, 9, "R").as_number() == 10.0,
            "HORCALC consumes exact date/time-aligned context series");
    const auto indicator_aggregate_analysis_static =
        tdx::analyze_formula_source("R:INSORT('HY.银行','KDJ',3,0);"
                                    "S:INSUM('GN.跨境支付','KDJ',3,0);");
    require(
        indicator_aggregate_analysis_static.at("executable_with_context").as_bool() &&
            indicator_aggregate_analysis_static.at("numeric_signal_safe").as_bool() &&
            indicator_aggregate_analysis_static.at("context_bindings_required").as_array().size() ==
                2 &&
            indicator_aggregate_analysis_static.at("automatic_context_dependencies")
                    .as_array()
                    .size() == 2,
        "literal INSORT/INSUM calls are automatic indicator-aggregate bindings");
    require(!tdx::analyze_formula_source("X:INSORT(HYBLOCK,'KDJ',3,0);")
                    .at("executable_with_context")
                    .as_bool() &&
                !tdx::analyze_formula_source("X:INSORT('HY.银行','KDJ',0,0);")
                     .at("executable_with_context")
                     .as_bool() &&
                !tdx::analyze_formula_source("X:INSUM('HY.银行','KDJ',3,6);")
                     .at("executable_with_context")
                     .as_bool(),
            "dynamic and out-of-range INSORT/INSUM arguments remain unavailable");
    tdx::Json indicator_aggregate_context_static = tdx::Json::object();
    indicator_aggregate_context_static["series"] = tdx::Json::object();
    indicator_aggregate_context_static["series"]["INSORT#HY.银行#KDJ#3#0"] = tdx::Json::object();
    indicator_aggregate_context_static["series"]["INSUM#GN.跨境支付#KDJ#3#0"] = tdx::Json::object();
    const auto indicator_aggregate_sample = sample(10);
    for (std::size_t index = 0; index < indicator_aggregate_sample.at("bars").as_array().size();
         ++index) {
        const auto &row = indicator_aggregate_sample.at("bars").as_array()[index];
        const auto key = row.at("date").as_string() + "|" + row.at("time").as_string();
        indicator_aggregate_context_static["series"]["INSORT#HY.银行#KDJ#3#0"][key] =
            static_cast<double>(index + 1);
        indicator_aggregate_context_static["series"]["INSUM#GN.跨境支付#KDJ#3#0"][key] =
            static_cast<double>(100 + index);
    }
    const auto indicator_aggregate_bound = tdx::evaluate_formula_source_document(
        indicator_aggregate_sample,
        "R:INSORT('HY.银行','KDJ',3,0);"
        "S:INSUM('GN.跨境支付','KDJ',3,0);",
        {}, "INDICATOR_AGGREGATE", &indicator_aggregate_context_static);
    require(point_value(indicator_aggregate_bound, 0, "R").as_number() == 10.0 &&
                point_value(indicator_aggregate_bound, 0, "S").as_number() == 109.0 &&
                point_value(indicator_aggregate_bound, 9, "R").as_number() == 1.0 &&
                point_value(indicator_aggregate_bound, 9, "S").as_number() == 100.0,
            "INSORT/INSUM consume exact date/time-aligned context series");
    const auto nested_indicator_analysis =
        tdx::analyze_formula_source("D:CALCSTOCKINDEX('SH000001','MACD',1);");
    require(
        nested_indicator_analysis.at("executable_with_context").as_bool() &&
            nested_indicator_analysis.at("numeric_signal_safe").as_bool() &&
            nested_indicator_analysis.at("context_bindings_required").as_array().size() == 1 &&
            nested_indicator_analysis.at("context_bindings_required").as_array()[0].as_string() ==
                "CALCSTOCKINDEX#SH000001#MACD#1" &&
            nested_indicator_analysis.at("automatic_context_dependencies").as_array().size() == 1,
        "literal CALCSTOCKINDEX is an automatic nested-indicator binding: " +
            nested_indicator_analysis.dump(-1));
    require(!tdx::analyze_formula_source("D:CALCSTOCKINDEX(CODE,'MACD',1);")
                    .at("executable_with_context")
                    .as_bool() &&
                !tdx::analyze_formula_source("D:CALCSTOCKINDEX('SH000001','MACD',0);")
                     .at("executable_with_context")
                     .as_bool(),
            "dynamic and invalid CALCSTOCKINDEX arguments remain unavailable");
    tdx::Json nested_indicator_context = tdx::Json::object();
    nested_indicator_context["series"] = tdx::Json::object();
    nested_indicator_context["series"]["CALCSTOCKINDEX#SH000001#MACD#1"] = tdx::Json::object();
    for (std::size_t index = 0; index < indicator_aggregate_sample.at("bars").as_array().size();
         ++index) {
        const auto &row = indicator_aggregate_sample.at("bars").as_array()[index];
        const auto key = row.at("date").as_string() + "|" + row.at("time").as_string();
        nested_indicator_context["series"]["CALCSTOCKINDEX#SH000001#MACD#1"][key] =
            static_cast<double>(200 + index);
    }
    const auto nested_indicator_bound = tdx::evaluate_formula_source_document(
        indicator_aggregate_sample, "D:CALCSTOCKINDEX('SH000001','MACD',1);", {}, "CALCSTOCKINDEX",
        &nested_indicator_context);
    require(point_value(nested_indicator_bound, 0, "D").as_number() == 209.0 &&
                point_value(nested_indicator_bound, 9, "D").as_number() == 200.0,
            "CALCSTOCKINDEX consumes its exact date/time-aligned context series");

    const auto formula_reference_analysis =
        tdx::analyze_formula_source("X:\"USERBASE.FAST\"-\"USERBASE.SLOW\";");
    require(formula_reference_analysis.at("executable_with_context").as_bool() &&
                formula_reference_analysis.at("external_dependencies")
                        .as_array()
                        .size() == 2 &&
                formula_reference_analysis.at("automatic_context_dependencies")
                        .as_array()
                        .size() == 2,
            "double-quoted indicator outputs are automatic library references");
    tdx::Json reference_library = tdx::Json::object();
    reference_library["formulas"] = tdx::Json::array();
    auto reference_base = tdx::make_formula_source_definition(
        "FAST:CLOSE;SLOW:MA(CLOSE,N);", "USERBASE", "technical", {{"N", 3}});
    reference_base["outputs"] = reference_base.at("analysis").at("outputs");
    reference_library["formulas"].push_back(reference_base);
    auto reference_child = tdx::make_formula_source_definition(
        "SPREAD:\"USERBASE.FAST\"-\"USERBASE.SLOW\";",
        "USERCHILD", "technical");
    reference_child["outputs"] = reference_child.at("analysis").at("outputs");
    reference_library["formulas"].push_back(reference_child);
    const auto reference_bars = sample(10);
    auto reference_context = tdx::build_formula_market_context_document(
        {}, "sz", "000001", reference_child.at("analysis"), 1000,
        nullptr, &reference_bars, false, &reference_library);
    reference_context["automatic_market_context"] = true;
    const auto reference_bound = tdx::evaluate_formula_document(
        reference_bars, reference_child, {}, &reference_context);
    const auto reference_base_bound = tdx::evaluate_formula_document(
        reference_bars, reference_base);
    require(
        std::abs(point_value(reference_bound, 9, "SPREAD").as_number() -
                 (point_value(reference_base_bound, 9, "FAST").as_number() -
                  point_value(reference_base_bound, 9, "SLOW").as_number())) < 1e-10 &&
            reference_bound.at("context_metadata")
                    .at("formula_reference_binding_count")
                    .as_number() == 2 &&
            reference_bound.at("context_metadata")
                    .at("formula_reference_evaluation_count")
                    .as_number() == 1,
        "same-security formula references inherit defaults, cache one child evaluation, and bind exact output names");

    auto parameterized_reference = tdx::make_formula_source_definition(
        "DELTA:\"USERBASE.SLOW\"(1+1)-\"USERBASE.SLOW\"(P);",
        "USERPARAMREF", "technical", {{"P", 4}});
    parameterized_reference["outputs"] =
        parameterized_reference.at("analysis").at("outputs");
    reference_library["formulas"].push_back(parameterized_reference);
    const auto& parameterized_analysis =
        parameterized_reference.at("analysis");
    require(
        parameterized_analysis.at("executable_with_context").as_bool() &&
            parameterized_analysis.at("parameterized_formula_reference_count")
                    .as_number() == 2.0 &&
            parameterized_analysis.at(
                "scalar_parameterized_formula_reference_count")
                    .as_number() == 2.0,
        "constant arithmetic and declared parent parameters are safe parameterized references");
    const std::map<std::string, double> parent_parameters{{"P", 5.0}};
    auto parameterized_context = tdx::build_formula_market_context_document(
        {}, "sz", "000001", parameterized_analysis, 1000, nullptr,
        &reference_bars, false, &reference_library, {}, nullptr,
        &parent_parameters);
    parameterized_context["automatic_market_context"] = true;
    const auto parameterized_bound = tdx::evaluate_formula_document(
        reference_bars, parameterized_reference, parent_parameters,
        &parameterized_context);
    const auto base_n2 = tdx::evaluate_formula_document(
        reference_bars, reference_base, {{"N", 2.0}});
    const auto base_n5 = tdx::evaluate_formula_document(
        reference_bars, reference_base, {{"N", 5.0}});
    require(
        std::abs(point_value(parameterized_bound, 9, "DELTA").as_number() -
                 (point_value(base_n2, 9, "SLOW").as_number() -
                  point_value(base_n5, 9, "SLOW").as_number())) < 1e-10 &&
            parameterized_bound.at("context_metadata")
                    .at("formula_reference_binding_count")
                    .as_number() == 2.0 &&
            parameterized_bound.at("context_metadata")
                    .at("formula_reference_evaluation_count")
                    .as_number() == 2.0,
        "parameterized references positionally override child defaults and honor parent overrides");
    const auto varying_reference_analysis =
        tdx::analyze_formula_source("X:\"USERBASE.SLOW\"(CLOSE);");
    require(
        !varying_reference_analysis.at("executable_with_context").as_bool() &&
            varying_reference_analysis.at(
                "parameterized_formula_reference_supported")
                    .as_bool() == false,
        "bar-varying formula reference parameters stay explicitly unavailable");

    tdx::Json cyclic_library = tdx::Json::object();
    cyclic_library["formulas"] = tdx::Json::array();
    auto cyclic_a = tdx::make_formula_source_definition(
        "X:\"CYCLEB.X\";", "CYCLEA", "technical");
    auto cyclic_b = tdx::make_formula_source_definition(
        "X:\"CYCLEA.X\";", "CYCLEB", "technical");
    cyclic_a["outputs"] = cyclic_a.at("analysis").at("outputs");
    cyclic_b["outputs"] = cyclic_b.at("analysis").at("outputs");
    cyclic_library["formulas"].push_back(cyclic_a);
    cyclic_library["formulas"].push_back(cyclic_b);
    bool cyclic_reference_rejected = false;
    try {
        (void)tdx::build_formula_market_context_document(
            {}, "sz", "000001", cyclic_a.at("analysis"), 1000,
            nullptr, &reference_bars, false, &cyclic_library);
    } catch (const tdx::Error& error) {
        cyclic_reference_rejected =
            std::string(error.what()).find("cycle") != std::string::npos;
    }
    require(cyclic_reference_rejected,
            "recursive formula references must fail explicitly instead of producing missing/zero data");
    tdx::Json reference_graph_library = cyclic_library;
    reference_graph_library["formulas"].push_back(reference_base);
    reference_graph_library["formulas"].push_back(reference_child);
    reference_graph_library["formulas"].push_back(
        tdx::make_formula_source_definition(
            "X:\"UNKNOWN.X\"+\"USERBASE.NOPE\";", "BROKENREF", "selection"));
    const auto reference_graph =
        tdx::analyze_formula_reference_graph(reference_graph_library);
    const auto& reference_summary = reference_graph.at("summary");
    require(reference_graph.at("schema").as_string() ==
                "tdx-formula-reference-graph-v1" &&
                reference_summary.at("binding_count").as_number() == 6 &&
                reference_summary.at("resolved_binding_count").as_number() == 4 &&
                reference_summary.at("missing_formula_count").as_number() == 1 &&
                reference_summary.at("missing_output_count").as_number() == 1 &&
                reference_summary.at("cycle_component_count").as_number() == 1 &&
                reference_summary.at("cyclic_formula_count").as_number() == 2 &&
                reference_summary.at("blocked_by_cycle_binding_count").as_number() == 2 &&
                reference_summary.at("executable_binding_count").as_number() == 2 &&
                !reference_summary.at("all_resolved").as_bool() &&
                !reference_summary.at("acyclic").as_bool(),
            "formula reference graph reports resolved, missing and cyclic dependencies");
    const auto block_code_analysis =
        tdx::analyze_formula_source("G:STRCMP(GNBKZSCODE(1),'880609');"
                                    "F:STRCMP(FGBKZSCODE(1),'880679');"
                                    "H:STRCMP(GETNAMEOFCODE(1,HYZSCODE),'银行');"
                                    "CG:STRCMP(GETNAMEOFCODE(1,GNBKZSCODE(1)),'跨境支付');"
                                    "CF:STRCMP(GETNAMEOFCODE(1,FGBKZSCODE(1)),'周期股');");
    require(block_code_analysis.at("executable_with_context").as_bool() &&
                block_code_analysis.at("automatic_context_dependencies").as_array().size() == 4,
            "type-167 block-code and type-120 code-name calls are context bindable");
    tdx::Json block_code_context = tdx::Json::object();
    block_code_context["formula_private_text_symbols"] = tdx::Json::object();
    block_code_context["formula_private_text_symbols"]["__GNBKZSCODE#1"] = "880609";
    block_code_context["formula_private_text_symbols"]["__FGBKZSCODE#1"] = "880679";
    block_code_context["formula_text_symbols"] = tdx::Json::object();
    block_code_context["formula_text_symbols"]["HYZSCODE"] = "880471";
    block_code_context["formula_code_name_overrides"] = tdx::Json::object();
    block_code_context["formula_code_name_overrides"]["0|000001"] = "平安银行";
    block_code_context["formula_code_name_overrides"]["1|880471"] = "银行";
    block_code_context["formula_code_name_overrides"]["1|880609"] = "跨境支付";
    block_code_context["formula_code_name_overrides"]["1|880679"] = "周期股";
    const auto block_code_bound = tdx::evaluate_formula_source_document(
        sample(10),
        "G:STRCMP(GNBKZSCODE(1),'880609');"
        "F:STRCMP(FGBKZSCODE(1),'880679');"
        "H:STRCMP(GETNAMEOFCODE(1,HYZSCODE),'银行');"
        "CG:STRCMP(GETNAMEOFCODE(1,GNBKZSCODE(1)),'跨境支付');"
        "CF:STRCMP(GETNAMEOFCODE(1,FGBKZSCODE(1)),'周期股');"
        "S:STRCMP(GETNAMEOFCODE(0,'000001'),'平安银行');"
        "MISS:STRCMP(GNBKZSCODE(99),'');",
        {}, "BLOCKCODE", &block_code_context);
    for (const auto *output : {"G", "F", "H", "CG", "CF", "S", "MISS"})
        require(point_value(block_code_bound, 9, output).as_number() == 1.0,
                std::string(output) + " exact block-code/name string semantics");
    const auto block_code_numeric_analysis = tdx::analyze_formula_source("BAD:GNBKZSCODE(1)+1;");
    require(!block_code_numeric_analysis.at("numeric_signal_safe").as_bool() &&
                block_code_numeric_analysis.at("has_degraded_numeric_output").as_bool(),
            "opaque GNBKZSCODE string handle is rejected in numeric output");
    const auto type167_text_analysis =
        tdx::analyze_formula_source("L:STRCMP(LEVEL1HYBLOCK,'银行');"
                                    "M:STRCMP(MAINBUSINESS,'零售金融业务');"
                                    "H:STRCMP(MOREHYBLOCK,'股份制银行');"
                                    "Z:STRCMP(ZDBLOCK,'自选股 ');N:ZDBLOCKNUM;"
                                    "Q:STRCMP(ZHBLOCK,'组合一 组合二 ');R:ZHBLOCKNUM;"
                                    "S:STRCMP(SIMIBLOCK,' ');");
    require(type167_text_analysis.at("executable_with_context").as_bool() &&
                type167_text_analysis.at("automatic_context_dependencies").as_array().size() == 8,
            "type-167, custom/combination and SIMIBLOCK text/count are context bindable");
    tdx::Json type167_text_context = tdx::Json::object();
    type167_text_context["symbols"] = tdx::Json::object();
    type167_text_context["symbols"]["LEVEL1HYBLOCK"] = 0.0;
    type167_text_context["symbols"]["MAINBUSINESS"] = 0.0;
    type167_text_context["symbols"]["MOREHYBLOCK"] = 0.0;
    type167_text_context["symbols"]["ZDBLOCK"] = 0.0;
    type167_text_context["symbols"]["ZDBLOCKNUM"] = 1.0;
    type167_text_context["symbols"]["ZHBLOCK"] = 0.0;
    type167_text_context["symbols"]["ZHBLOCKNUM"] = 2.0;
    type167_text_context["symbols"]["SIMIBLOCK"] = 0.0;
    type167_text_context["formula_text_symbols"] = tdx::Json::object();
    type167_text_context["formula_text_symbols"]["LEVEL1HYBLOCK"] = "银行";
    type167_text_context["formula_text_symbols"]["MAINBUSINESS"] = "零售金融业务";
    type167_text_context["formula_text_symbols"]["MOREHYBLOCK"] = "股份制银行";
    type167_text_context["formula_text_symbols"]["ZDBLOCK"] = "自选股 ";
    type167_text_context["formula_text_symbols"]["ZHBLOCK"] = "组合一 组合二 ";
    type167_text_context["formula_text_symbols"]["SIMIBLOCK"] = " ";
    const auto type167_text_bound =
        tdx::evaluate_formula_source_document(sample(10),
                                              "L:STRCMP(LEVEL1HYBLOCK,'银行');"
                                              "M:STRCMP(MAINBUSINESS,'零售金融业务');"
                                              "H:STRCMP(MOREHYBLOCK,'股份制银行');"
                                              "Z:STRCMP(ZDBLOCK,'自选股 ');N:ZDBLOCKNUM;"
                                              "Q:STRCMP(ZHBLOCK,'组合一 组合二 ');R:ZHBLOCKNUM;"
                                              "S:STRCMP(SIMIBLOCK,' ');",
                                              {}, "TYPE167TEXT", &type167_text_context);
    require(
        point_value(type167_text_bound, 9, "L").as_number() == 1.0 &&
            point_value(type167_text_bound, 9, "M").as_number() == 1.0 &&
            point_value(type167_text_bound, 9, "H").as_number() == 1.0 &&
            point_value(type167_text_bound, 9, "Z").as_number() == 1.0 &&
            point_value(type167_text_bound, 9, "N").as_number() == 1.0 &&
            point_value(type167_text_bound, 9, "Q").as_number() == 1.0 &&
            point_value(type167_text_bound, 9, "R").as_number() == 2.0 &&
            point_value(type167_text_bound, 9, "S").as_number() == 1.0,
        "materialized type-167/custom/combination block values participate in formula evaluation");
    const auto session_analysis =
        tdx::analyze_formula_source("X:TOTALFZNUM; CLOCK:HOUR*100+MINUTE;");
    require(session_analysis.at("executable").as_bool(),
            "TOTALFZNUM/HOUR/MINUTE are native bar/session symbols");
    const auto session_bound = tdx::evaluate_formula_source_document(
        sample(10), "X:TOTALFZNUM; CLOCK:HOUR*100+MINUTE;", {}, "SESSION");
    require(point_value(session_bound, 9, "X").as_number() == 240.0 &&
                point_value(session_bound, 9, "CLOCK").as_number() == 1500.0,
            "A-share session minutes and bar clock values");
    auto explicit_session_sample = sample(2);
    explicit_session_sample["min_tick"] = 0.2;
    explicit_session_sample["trading_sessions"] = tdx::Json::parse(
        R"([{"start":"09:00","end":"10:00"}])");
    tdx::Json explicit_session_context = tdx::Json::object();
    explicit_session_context["formula_scalar_bindings"] = tdx::Json::object();
    explicit_session_context["formula_scalar_bindings"]["totalfznum"] = 345.0;
    explicit_session_context["formula_scalar_bindings"]["mindiff"] = 0.125;
    const auto explicit_session_bound = tdx::evaluate_formula_source_document(
        std::move(explicit_session_sample), "TOTAL:TOTALFZNUM;TICK:MINDIFF;", {},
        "EXPLICITSESSIONSCALARS", &explicit_session_context);
    require(point_value(explicit_session_bound, 1, "TOTAL").as_number() == 345.0 &&
                point_value(explicit_session_bound, 1, "TICK").as_number() == 0.125,
            "finite caller-owned TOTALFZNUM/MINDIFF scalars take precedence over session constants");
    const auto calendar_bound =
        tdx::evaluate_formula_source_document(sample(10), "AGE:DAYSTOTODAY;", {}, "CALENDAR");
    require(point_value(calendar_bound, 0, "AGE").as_number() ==
                point_value(calendar_bound, 1, "AGE").as_number() + 1.0,
            "DAYSTOTODAY follows TCalc natural-calendar-day semantics");
    const auto refdate = tdx::evaluate_formula_source_document(
        sample(10), "LATEST:REFDATE(CLOSE,DATE); FIXED:REFDATE(CLOSE,1260105);", {}, "REFDATE");
    require(point_value(refdate, 0, "LATEST").as_number() == 19.0 &&
                point_value(refdate, 9, "LATEST").as_number() == 19.0 &&
                point_value(refdate, 9, "FIXED").as_number() == 14.0,
            "REFDATE broadcasts the latest value on or before the requested date");
    const auto external_analysis =
        tdx::analyze_formula_source("TOTAL:(\"SH999999$AMO\"+\"SZ399001$AMO\")/100000000;");
    require(external_analysis.at("executable_with_context").as_bool(),
            "cross-security OHLCVA references are context bindable");
    tdx::Json external_context = tdx::Json::object();
    external_context["series"] = tdx::Json::object();
    for (const auto name : {"EXTERNAL#SH999999$AMO", "EXTERNAL#SZ399001$AMO"}) {
        external_context["series"][name] = tdx::Json::object();
        for (int day = 1; day <= 10; ++day) {
            const auto date =
                "2026-01-" + std::string(day < 10 ? "0" : "") + std::to_string(day) + "|15:00";
            external_context["series"][name][date] = 100000000.0;
        }
    }
    const auto external_bound = tdx::evaluate_formula_source_document(
        sample(10), "TOTAL:(\"SH999999$AMO\"+\"SZ399001$AMO\")/100000000;", {}, "EXTERNAL",
        &external_context);
    require(point_value(external_bound, 9, "TOTAL").as_number() == 2.0,
            "cross-security amount series align by bar date and time");
    const auto industry_analysis =
        tdx::analyze_formula_source("A:=REF(HY_INDEXC,1);R:IF(A>0,(HY_INDEXC-A)*100/A,0);"
                                    "DRAWTEXT_FIX(ISLASTBAR,0,0,0,HYBLOCK);"
                                    "K:DRAWKLINE(HY_INDEXH,HY_INDEXO,HY_INDEXL,HY_INDEXC);");
    require(industry_analysis.at("executable_with_context").as_bool(),
            "industry-index and HYBLOCK symbols are context bindable");
    tdx::Json industry_context = tdx::Json::object();
    industry_context["series"] = tdx::Json::object();
    for (const auto symbol : {"HY_INDEXO", "HY_INDEXH", "HY_INDEXL", "HY_INDEXC"}) {
        industry_context["series"][symbol] = tdx::Json::object();
        for (const auto &bar : external_bound.at("points").as_array()) {
            const auto key = bar.at("date").as_string() + "|" + bar.at("time").as_string();
            industry_context["series"][symbol][key] = bar.at("close").as_number() * 2.0;
        }
    }
    industry_context["symbols"] = tdx::Json::object();
    industry_context["symbols"]["HYBLOCK"] = 0.0;
    industry_context["formula_text_symbols"] = tdx::Json::object();
    industry_context["formula_text_symbols"]["HYBLOCK"] = "银行";
    const auto industry_bound = tdx::evaluate_formula_source_document(
        sample(10),
        "A:=REF(HY_INDEXC,1);R:IF(A>0,(HY_INDEXC-A)*100/A,0);"
        "DRAWTEXT_FIX(ISLASTBAR,0,0,0,HYBLOCK);"
        "K:DRAWKLINE(HY_INDEXH,HY_INDEXO,HY_INDEXL,HY_INDEXC);",
        {}, "INDUSTRY", &industry_context);
    require(point_value(industry_bound, 9, "R").is_number() && industry_bound.at("context_metadata")
                                                                       .at("formula_text_symbols")
                                                                       .at("HYBLOCK")
                                                                       .as_string() == "银行",
            "industry series execute while HYBLOCK text is preserved as metadata");
    context["symbols"]["BUYVOL"] = 600.0;
    context["symbols"]["SELLVOL"] = 900.0;
    const auto inside_outside = tdx::evaluate_formula_source_document(
        sample(10), "RATIO:SELLVOL/BUYVOL;", {}, "INSIDEOUTSIDE", &context);
    require(std::abs(point_value(inside_outside, 9, "RATIO").as_number() - 1.5) < 1e-12,
            "BUYVOL/SELLVOL context maps outer/inside volume in hands");
    require(
        tdx::analyze_formula_source("X:SELLVOL/BUYVOL;").at("executable_with_context").as_bool(),
        "BUYVOL/SELLVOL analysis is snapshot-context bindable");
    const auto growth_analysis =
        tdx::analyze_formula_source("PEG:DYNAINFO(39)/FINANCE(43); GROWTH:FINANCE(44);");
    require(growth_analysis.at("executable_with_context").as_bool(),
            "latest-report FINANCE(43/44) are professional-context bindable");
    context["finance"]["43"] = -3.5;
    context["finance"]["44"] = 0.0;
    const auto growth_bound = tdx::evaluate_formula_source_document(
        sample(10), "PROFIT:FINANCE(43); REVENUE:FINANCE(44);", {}, "GROWTH", &context);
    require(point_value(growth_bound, 9, "PROFIT").as_number() == -3.5 &&
                point_value(growth_bound, 9, "REVENUE").as_number() == 0.0,
            "official negative and zero growth pass through without recomputation");
    const auto event_analysis = tdx::analyze_formula_source(
        "D1:=FINANCE(90);D2:=FINANCE(91);(D1>0&&D1<N)||(D2>0&&D2<N);", {"N"});
    require(event_analysis.at("executable_with_context").as_bool(),
            "FINANCE(90/91) event-day fields are context bindable");
    context["finance"]["90"] = 1;
    context["finance"]["91"] = 0;
    const auto event_bound = tdx::evaluate_formula_source_document(
        sample(10), "D1:=FINANCE(90);D2:=FINANCE(91);X:(D1>0&&D1<N)||(D2>0&&D2<N);", {{"N", 5.0}},
        "EVENTS", &context);
    require(point_value(event_bound, 9, "X").as_number() == 1.0,
            "inclusive day-one event must pass the built-in five-day screen");
    const auto northbound_analysis =
        tdx::analyze_formula_source("DNUM:=FINANCE(88);DNUM>0&&DNUM<N;", {"N"});
    require(northbound_analysis.at("executable_with_context").as_bool(),
            "FINANCE(88) tipinfo event-day field is context bindable");
    context["finance"]["88"] = 2;
    const auto northbound_bound = tdx::evaluate_formula_source_document(
        sample(10), "DNUM:=FINANCE(88);X:DNUM>0&&DNUM<N;", {{"N", 5.0}}, "NORTHBOUND", &context);
    require(point_value(northbound_bound, 9, "X").as_number() == 1.0,
            "FINANCE(88) inclusive day value must execute in the source interpreter");
    const auto eligibility_analysis =
        tdx::analyze_formula_source("CONNECT:FINANCE(48); MARGIN:FINANCE(52);");
    require(eligibility_analysis.at("executable_with_context").as_bool(),
            "FINANCE(48/52) security eligibility fields are context bindable");
    context["finance"]["48"] = 1;
    context["finance"]["52"] = 0;
    const auto eligibility_bound = tdx::evaluate_formula_source_document(
        sample(10), "CONNECT:FINANCE(48); MARGIN:FINANCE(52);", {}, "ELIGIBILITY", &context);
    require(point_value(eligibility_bound, 9, "CONNECT").as_number() == 1.0 &&
                point_value(eligibility_bound, 9, "MARGIN").as_number() == 0.0,
            "FINANCE(48/52) membership values must execute in the interpreter");
    tdx::Json index_context = tdx::Json::object();
    index_context["series"] = tdx::Json::object();
    index_context["series"]["INDEXC"] = tdx::Json::object();
    const auto index_sample = sample(10);
    for (const auto &bar : index_sample.at("bars").as_array())
        index_context["series"]["INDEXC"]
                     [bar.at("date").as_string() + "|" + bar.at("time").as_string()] =
                         bar.at("close").as_number() * 2.0;
    const auto index_bound = tdx::evaluate_formula_source_document(
        index_sample, "REL:CLOSE/INDEXC;", {}, "INDEXBOUND", &index_context);
    require(std::abs(point_value(index_bound, 9, "REL").as_number() - 0.5) < 1e-9,
            "benchmark index series binding");
    require(
        tdx::analyze_formula_source("REL:CLOSE/INDEXC;").at("executable_with_context").as_bool(),
        "index context analysis");
    require(tdx::analyze_formula_source("ADL:SUM(ADVANCE-DECLINE,0);")
                .at("executable_with_context")
                .as_bool(),
            "breadth context analysis");
    require(
        tdx::analyze_formula_source("ENERGY:EMA(HSL,13);").at("executable_with_context").as_bool(),
        "turnover context analysis");
    const auto professional_analysis =
        tdx::analyze_formula_source("A:GPJYVALUE(3,1,1); B:SCJYVALUE(31,2,0);");
    require(professional_analysis.at("executable_with_context").as_bool(),
            "professional trading context analysis");
    require(professional_analysis.at("context_bindings_required").size() == 2,
            "professional trading binding collection");
    const auto split_analysis =
        tdx::analyze_formula_source("R:SPLIT(0,0); B:SPLITBARS(0,0); D:DAYSTOTODAY;");
    require(split_analysis.at("executable_with_context").as_bool() &&
                split_analysis.at("context_bindings_required").size() == 2,
            "corporate-action SPLIT bindings are context executable");
    require(!tdx::analyze_formula_source("X:SPLIT(0,2);").at("executable_with_context").as_bool(),
            "SPLIT type 2 remains invalid while SPLITBARS type 2 is valid");
    require(!tdx::analyze_formula_source("X:GPJYVALUE(45,1,0);")
                 .at("executable_with_context")
                 .as_bool(),
            "undocumented professional stock ID must stay unavailable");
    tdx::Json professional_context = tdx::Json::object();
    professional_context["series"] = tdx::Json::object();
    professional_context["series"]["GPJYVALUE#3#1#1"] = tdx::Json::object();
    professional_context["series"]["SCJYVALUE#31#2#0"] = tdx::Json::object();
    const auto professional_sample = sample(10);
    for (const auto &bar : professional_sample.at("bars").as_array()) {
        const auto key = bar.at("date").as_string() + "|" + bar.at("time").as_string();
        professional_context["series"]["GPJYVALUE#3#1#1"][key] = 123.0;
        professional_context["series"]["SCJYVALUE#31#2#0"][key] = 456.0;
    }
    const auto professional_bound = tdx::evaluate_formula_source_document(
        professional_sample, "X:GPJYVALUE(3,1,1)+SCJYVALUE(31,2,0);", {}, "PROFESSIONAL",
        &professional_context);
    require(point_value(professional_bound, 9, "X").as_number() == 579.0,
            "professional trading series bindings");
    const auto one_point_analysis =
        tdx::analyze_formula_source("F:FINONE(183,25,630);G:GPJYONE(3,1,0,0);"
                                    "B:BKJYONE(5,2,2026,807);S:SCJYONE(31,1,0,1);P:GPONEDAT(7);");
    require(one_point_analysis.at("executable_with_context").as_bool() &&
                one_point_analysis.at("context_bindings_required").size() == 5,
            "single-point finance/trading calls are automatic-context bindable");
    require(!tdx::analyze_formula_source("X:GPJYONE(CLOSE,1,0,0);")
                    .at("executable_with_context")
                    .as_bool() &&
                !tdx::analyze_formula_source("X:BKJYONE(4,1,0,0);")
                     .at("executable_with_context")
                     .as_bool(),
            "dynamic or undocumented single-point bindings remain unavailable");
    tdx::Json one_point_context = tdx::Json::object();
    one_point_context["formula_scalar_bindings"] = tdx::Json::object();
    one_point_context["formula_scalar_bindings"]["FINONE#183#25#630"] = 11.0;
    one_point_context["formula_scalar_bindings"]["GPJYONE#3#1#0#0"] = 22.0;
    one_point_context["formula_scalar_bindings"]["BKJYONE#5#2#2026#807"] = 33.0;
    one_point_context["formula_scalar_bindings"]["SCJYONE#31#1#0#1"] = 44.0;
    one_point_context["formula_scalar_bindings"]["GPONEDAT#7"] = 55.0;
    const auto one_point_bound = tdx::evaluate_formula_source_document(
        professional_sample,
        "F:FINONE(183,25,630);G:GPJYONE(3,1,0,0);"
        "B:BKJYONE(5,2,2026,807);S:SCJYONE(31,1,0,1);P:GPONEDAT(7);",
        {}, "ONEPOINT", &one_point_context);
    require(point_value(one_point_bound, 9, "F").as_number() == 11.0 &&
                point_value(one_point_bound, 9, "G").as_number() == 22.0 &&
                point_value(one_point_bound, 9, "B").as_number() == 33.0 &&
                point_value(one_point_bound, 9, "S").as_number() == 44.0 &&
                point_value(one_point_bound, 9, "P").as_number() == 55.0,
            "single-point scalar bindings reach every output unchanged");
    const auto broker_signal_analysis =
        tdx::analyze_formula_source("ENTER:SIGNALS_QS(102,0); HOLDERS:SIGNALS_QS(103,0);");
    require(!broker_signal_analysis.at("executable_with_context").as_bool() &&
                broker_signal_analysis.at("explicit_context_bindable").as_bool() &&
                broker_signal_analysis.at("explicit_context_bindings_required").size() == 2,
            "broker-private signals must require explicit caller-owned bindings");
    require(!tdx::analyze_formula_source("X:SIGNALS_QS(CLOSE,0);")
                 .at("explicit_context_bindable")
                 .as_bool(),
            "dynamic broker-signal IDs must not be accepted as context keys");
    tdx::Json broker_context = tdx::Json::object();
    broker_context["series"] = tdx::Json::object();
    broker_context["series"]["SIGNALS_QS#102#0"] = tdx::Json::object();
    broker_context["series"]["SIGNALS_QS#103#0"] = tdx::Json::object();
    for (std::size_t i = 0; i < professional_sample.at("bars").size(); ++i) {
        const auto &bar = professional_sample.at("bars").as_array()[i];
        const auto key = bar.at("date").as_string() + "|" + bar.at("time").as_string();
        broker_context["series"]["SIGNALS_QS#102#0"][key] = static_cast<double>(i % 4) - 1.0;
        broker_context["series"]["SIGNALS_QS#103#0"][key] = i == 9 ? 1.0 : 0.0;
    }
    const auto broker_bound = tdx::evaluate_formula_source_document(
        professional_sample, "ENTER:SIGNALS_QS(102,0); HOLDERS:SIGNALS_QS(103,0);", {},
        "BROKERSIGNALS", &broker_context);
    require(point_value(broker_bound, 9, "ENTER").as_number() == -1.0 &&
                point_value(broker_bound, 9, "HOLDERS").as_number() == 0.0,
            "explicit broker signal series reaches the interpreter without a surrogate");

    auto signal_sample = sample(5);
    tdx::Json native_signal_context = tdx::Json::object();
    native_signal_context["series"] = tdx::Json::object();
    const auto bind_signal = [&](const std::string& binding,
                                 const std::vector<double>& values) {
        native_signal_context["series"][binding] = tdx::Json::object();
        const auto& bars = signal_sample.at("bars").as_array();
        for (std::size_t index = 0; index < values.size(); ++index) {
            const auto& bar = bars[bars.size() - 1 - index];
            const auto key = bar.at("date").as_string() + "|" +
                             bar.at("time").as_string();
            if (std::isfinite(values[index]))
                native_signal_context["series"][binding][key] = values[index];
            else
                native_signal_context["series"][binding][key] = nullptr;
        }
    };
    const double absent = std::numeric_limits<double>::quiet_NaN();
    bind_signal("SIGNALS_QS#102#0",
                {16777217.0, 0.1, -16777217.0, 3.0, 4.0});
    bind_signal("SIGNALS_QS#103#1", {absent, 5.0, absent, absent, 7.0});
    bind_signal("SIGNALS_QS#104#2", {absent, 5.0, absent, absent, 7.0});
    const auto native_signal = tdx::evaluate_formula_source_document(
        signal_sample,
        "D:SIGNALS_QS(IF(CURRBARSCOUNT=1,102,999),"
        "IF(CURRBARSCOUNT=1,0,2));C:SIGNALS_QS(103,1);"
        "Z:SIGNALS_QS(104,2);",
        {}, "NATIVESIGNALSQS", &native_signal_context);
    const double direct_values[]{
        16777216.0, static_cast<double>(static_cast<float>(0.1)),
        -16777216.0, 3.0, 4.0};
    const double carried_values[]{5.0, 5.0, 5.0, 7.0};
    const double zero_values[]{0.0, 5.0, 0.0, 0.0, 7.0};
    require(point_value(native_signal, 0, "C").is_null(),
            "SIGNALS_QS mode 1 preserves a leading absent record");
    for (std::size_t index = 0; index < 5; ++index) {
        require(point_value(native_signal, index, "D").as_number() ==
                    direct_values[index],
                "SIGNALS_QS uses final raw-f32 selectors and f32 output");
        require(point_value(native_signal, index, "Z").as_number() ==
                    zero_values[index],
                "SIGNALS_QS mode 2 fills absent records with zero");
        if (index > 0)
            require(point_value(native_signal, index, "C").as_number() ==
                        carried_values[index - 1],
                    "SIGNALS_QS mode 1 carries the prior output");
    }
    const auto authorized_l2_analysis =
        tdx::analyze_formula_source("AMO0:L2_AMO(0,2); FLOW:LARGEINTRDVOL-LARGEOUTTRDVOL; "
                                    "COUNT:TRADENUM;");
    require(!authorized_l2_analysis.at("executable_with_context").as_bool() &&
                authorized_l2_analysis.at("explicit_context_bindable").as_bool() &&
                authorized_l2_analysis.at("explicit_context_bindings_required").size() == 4 &&
                authorized_l2_analysis.at("unsupported").size() == 0,
            "authorized Level2 values require explicit caller-owned series");
    require(!tdx::analyze_formula_source("X:L2_AMO(CLOSE,2);")
                    .at("explicit_context_bindable")
                    .as_bool() &&
                !tdx::analyze_formula_source("X:L2_AMO(4,0);")
                     .at("explicit_context_bindable")
                     .as_bool(),
            "dynamic and out-of-range Level2 amount selectors remain unavailable");
    auto native_l2_sample = sample(5);
    tdx::Json native_l2_context = tdx::Json::object();
    native_l2_context["series"] = tdx::Json::object();
    native_l2_context["series"]["L2_AMO#3#2"] = tdx::Json::object();
    const double native_l2_values[]{16777217.0, 0.1, -16777217.0,
                                    std::numeric_limits<double>::quiet_NaN(), 4.0};
    const auto& native_l2_bars = native_l2_sample.at("bars").as_array();
    for (std::size_t index = 0; index < 5; ++index) {
        const auto& bar = native_l2_bars[native_l2_bars.size() - 1 - index];
        const auto key = bar.at("date").as_string() + "|" +
                         bar.at("time").as_string();
        if (std::isfinite(native_l2_values[index]))
            native_l2_context["series"]["L2_AMO#3#2"][key] =
                native_l2_values[index];
        else
            native_l2_context["series"]["L2_AMO#3#2"][key] = nullptr;
    }
    const auto native_l2 = tdx::evaluate_formula_source_document(
        native_l2_sample,
        "A:L2_AMO(IF(CURRBARSCOUNT=1,3,0),"
        "IF(CURRBARSCOUNT=1,2,0));",
        {}, "NATIVEL2AMO", &native_l2_context);
    const double expected_native_l2[]{
        16777216.0, static_cast<double>(static_cast<float>(0.1)),
        -16777216.0};
    for (std::size_t index = 0; index < 3; ++index)
        require(point_value(native_l2, index, "A").as_number() ==
                    expected_native_l2[index],
                "L2_AMO uses final raw-f32 selectors and f32 output");
    require(point_value(native_l2, 3, "A").is_null(),
            "L2_AMO preserves a missing caller-supplied field");
    require(point_value(native_l2, 4, "A").as_number() == 4.0,
            "L2_AMO resumes after a missing caller-supplied field");
    tdx::Json authorized_l2_context = tdx::Json::object();
    authorized_l2_context["series"] = tdx::Json::object();
    for (const auto *binding : {"L2_AMO#0#2", "LARGEINTRDVOL", "LARGEOUTTRDVOL", "TRADENUM"})
        authorized_l2_context["series"][binding] = tdx::Json::object();
    for (std::size_t i = 0; i < professional_sample.at("bars").size(); ++i) {
        const auto &bar = professional_sample.at("bars").as_array()[i];
        const auto key = bar.at("date").as_string() + "|" + bar.at("time").as_string();
        authorized_l2_context["series"]["L2_AMO#0#2"][key] = 10000.0 + i;
        authorized_l2_context["series"]["LARGEINTRDVOL"][key] = 300.0 + i;
        authorized_l2_context["series"]["LARGEOUTTRDVOL"][key] = 120.0 + i;
        authorized_l2_context["series"]["TRADENUM"][key] = 20.0 + i;
    }
    const auto authorized_l2_bound = tdx::evaluate_formula_source_document(
        professional_sample,
        "AMO0:L2_AMO(0,2); FLOW:LARGEINTRDVOL-LARGEOUTTRDVOL; "
        "COUNT:TRADENUM;",
        {}, "AUTHORIZEDL2", &authorized_l2_context);
    require(point_value(authorized_l2_bound, 9, "AMO0").as_number() == 10000.0,
            "authorized Level2 amount context reaches the interpreter");
    require(point_value(authorized_l2_bound, 9, "FLOW").as_number() == 180.0,
            "authorized Level2 large-volume context reaches the interpreter");
    require(point_value(authorized_l2_bound, 9, "COUNT").as_number() == 20.0,
            "authorized Level2 trade-count context reaches the interpreter");
    const auto order_flow_analysis = tdx::analyze_formula_source(
        "V:L2_VOL(3,2);N:L2_VOLNUM(1,0);A:ACTINVOL;"
        "C:BIDCANCELVOL();Q:CUR_BUYORDER;S:ISBUYORDER;");
    require(order_flow_analysis.at("explicit_context_bindable").as_bool() &&
                order_flow_analysis.at("explicit_context_bindings_required").size() == 6 &&
                order_flow_analysis.at("unsupported").size() == 0,
            "all recovered TCalc type-31 Level2 selectors are explicit-context capable");
    require(!tdx::analyze_formula_source("X:L2_VOLNUM(2,0);")
                     .at("explicit_context_bindable")
                     .as_bool(),
            "L2_VOLNUM preserves the original two-by-two selector boundary");
    for (const auto *binding : {"L2_VOL#3#2", "L2_VOLNUM#1#0", "ACTINVOL",
                                "BIDCANCELVOL", "CUR_BUYORDER", "ISBUYORDER"})
        authorized_l2_context["series"][binding] = tdx::Json::object();
    for (std::size_t i = 0; i < professional_sample.at("bars").size(); ++i) {
        const auto &bar = professional_sample.at("bars").as_array()[i];
        const auto key = bar.at("date").as_string() + "|" + bar.at("time").as_string();
        authorized_l2_context["series"]["L2_VOL#3#2"][key] = 40.0 + i;
        authorized_l2_context["series"]["L2_VOLNUM#1#0"][key] = 10.0 + i;
        authorized_l2_context["series"]["ACTINVOL"][key] = 30.0 + i;
        authorized_l2_context["series"]["BIDCANCELVOL"][key] = 4.0 + i;
        authorized_l2_context["series"]["CUR_BUYORDER"][key] = 3.0 + i;
        authorized_l2_context["series"]["ISBUYORDER"][key] = i == 0 ? 1.0 : 0.0;
    }
    const auto order_flow_bound = tdx::evaluate_formula_source_document(
        professional_sample,
        "V:L2_VOL(3,2);N:L2_VOLNUM(1,0);A:ACTINVOL;"
        "C:BIDCANCELVOL();Q:CUR_BUYORDER;S:ISBUYORDER;",
        {}, "AUTHORIZED_ORDER_FLOW", &authorized_l2_context);
    require(point_value(order_flow_bound, 9, "V").as_number() == 40.0 &&
                point_value(order_flow_bound, 9, "N").as_number() == 10.0 &&
                point_value(order_flow_bound, 9, "A").as_number() == 30.0 &&
                point_value(order_flow_bound, 9, "C").as_number() == 4.0 &&
                point_value(order_flow_bound, 9, "Q").as_number() == 3.0 &&
                point_value(order_flow_bound, 9, "S").as_number() == 1.0,
            "recovered TCalc Level2 order-flow bindings reach the interpreter exactly");
    const auto account_state_analysis = tdx::analyze_formula_source(
        "CASH:FREEMONEY;POS:TOTALPOSITION;LAST:ISLASTBUY;"
        "FEE:FEERATE;MARGIN:MARGINRATE;");
    require(!account_state_analysis.at("executable_with_context").as_bool() &&
                account_state_analysis.at("explicit_context_bindable").as_bool() &&
                account_state_analysis.at("explicit_context_bindings_required").size() == 5 &&
                account_state_analysis.at("unsupported").size() == 0,
            "account and strategy state requires explicit caller-owned context");
    tdx::Json account_state_context = tdx::Json::object();
    account_state_context["formula_scalar_bindings"] = tdx::Json::object();
    account_state_context["formula_scalar_bindings"]["TOTALPOSITION"] = 1200.0;
    account_state_context["formula_scalar_bindings"]["ISLASTBUY"] = 1.0;
    account_state_context["formula_scalar_bindings"]["FEERATE"] = 0.00025;
    account_state_context["formula_scalar_bindings"]["MARGINRATE"] = 0.12;
    account_state_context["series"] = tdx::Json::object();
    account_state_context["series"]["FREEMONEY"] = tdx::Json::object();
    for (std::size_t i = 0; i < professional_sample.at("bars").size(); ++i) {
        const auto& bar = professional_sample.at("bars").as_array()[i];
        const auto key = bar.at("date").as_string() + "|" +
                         bar.at("time").as_string();
        account_state_context["series"]["FREEMONEY"][key] = 50000.0 + i;
    }
    require(tdx::formula_explicit_context_ready(
                account_state_analysis, &account_state_context),
            "complete account-state context is ready for read-only evaluation");
    const auto account_state_bound = tdx::evaluate_formula_source_document(
        professional_sample,
        "CASH:FREEMONEY;POS:TOTALPOSITION;LAST:ISLASTBUY;"
        "FEE:FEERATE;MARGIN:MARGINRATE;",
        {}, "ACCOUNT_STATE", &account_state_context);
    require(point_value(account_state_bound, 9, "CASH").as_number() == 50000.0 &&
                point_value(account_state_bound, 9, "POS").as_number() == 1200.0 &&
                point_value(account_state_bound, 9, "LAST").as_number() == 1.0 &&
                point_value(account_state_bound, 9, "FEE").as_number() == 0.00025 &&
                point_value(account_state_bound, 9, "MARGIN").as_number() == 0.12,
            "caller-owned account state reaches the interpreter without account access");
    const auto account_template = tdx::make_formula_explicit_context_template_document(
        [&] {
            auto library = tdx::Json::object();
            library["formulas"] = tdx::Json::array();
            auto formula = tdx::Json::object();
            formula["code"] = "ACCOUNTCTX";
            formula["kind_key"] = "technical";
            formula["source_text"] = "X:FREEMONEY+TOTALPOSITION;";
            formula["parameters"] = tdx::Json::array();
            library["formulas"].push_back(std::move(formula));
            return library;
        }(), {"ACCOUNTCTX"}, {"2026-08-12|15:00"});
    require(account_template.at("_template").at("bindings").as_array()[0]
                    .at("source_kind").as_string() ==
                "caller-account-strategy-state" &&
                account_template.at("formula_scalar_bindings")
                    .as_object().empty() &&
                account_template.at("_template").at("authorization_boundary")
                    .as_string().find("no order action") != std::string::npos,
            "account-state template preserves the read-only authorization boundary");
    const auto action_analysis =
        tdx::analyze_formula_source("X:ORDERBUY(1,CLOSE);");
    require(!action_analysis.at("explicit_context_bindable").as_bool() &&
                action_analysis.at("unsupported").size() == 1,
            "ORDERBUY remains an unavailable action rather than a forged state value");
    professional_context["series"]["SPLIT#0#0"] = tdx::Json::object();
    professional_context["series"]["SPLITBARS#0#0"] = tdx::Json::object();
    for (const auto &bar : professional_sample.at("bars").as_array()) {
        const auto key = bar.at("date").as_string() + "|" + bar.at("time").as_string();
        professional_context["series"]["SPLIT#0#0"][key] = 0.5;
        professional_context["series"]["SPLITBARS#0#0"][key] = 0.0;
    }
    const auto split_bound = tdx::evaluate_formula_source_document(
        professional_sample, "R:SPLIT(0,0); B:SPLITBARS(0,0);", {}, "SPLITTEST",
        &professional_context);
    require(point_value(split_bound, 9, "R").as_number() == 0.5 &&
                point_value(split_bound, 9, "B").as_number() == 0.0,
            "SPLIT/SPLITBARS date-aligned context reaches the interpreter");
    const auto valuation_analysis =
        tdx::analyze_formula_source("X:IF(FINANCE(3)=0,BKJYVALUE(5,1,1),FINVALUE(308));");
    require(valuation_analysis.at("executable_with_context").as_bool(),
            "professional valuation context analysis");
    professional_context["finance"] = tdx::Json::object();
    professional_context["finance"]["3"] = 1;
    professional_context["finvalue"] = tdx::Json::object();
    professional_context["finvalue"]["308"] = 2.5;
    professional_context["series"]["BKJYVALUE#5#1#1"] = tdx::Json::object();
    for (const auto &bar : professional_sample.at("bars").as_array()) {
        const auto key = bar.at("date").as_string() + "|" + bar.at("time").as_string();
        professional_context["series"]["BKJYVALUE#5#1#1"][key] = 99.0;
    }
    const auto valuation_bound = tdx::evaluate_formula_source_document(
        professional_sample, "X:IF(FINANCE(3)=0,BKJYVALUE(5,1,1),FINVALUE(308));", {}, "VALUATION",
        &professional_context);
    require(point_value(valuation_bound, 9, "X").as_number() == 2.5,
            "professional finance/board bindings");
    const auto host_summary_analysis = tdx::analyze_formula_source(
        "B:BETAVALUE;BC:BETAVALUE();S:SHAPE_SHORT;M:SHAPE_MID();"
        "L:SHAPE_LONG;U:MAINZSHQ(3,6);D:MAINZSHQ(CLOSE-CLOSE+3,CLOSE-CLOSE+7);"
        "Z:MAINZSHQ(99,0);T:TOTALHQINFO(3);A:TOTALMMPAMO(1);"
        "AD:TOTALMMPAMO(CLOSE-CLOSE+4);PE:HYSYL;PEC:HYSYL();"
        "PB:HYSJL;PBC:HYSJL();");
    require(host_summary_analysis.at("executable_with_context").as_bool() &&
                host_summary_analysis.at("context_bindings_unavailable").as_array().empty() &&
                host_summary_analysis.at("automatic_context_dependencies").as_array().size() == 9,
            "TdxW statistics, industry valuation and public market-summary functions are automatic "
            "bindings");
    tdx::Json host_summary_context = tdx::Json::object();
    host_summary_context["symbols"] = tdx::Json::object();
    host_summary_context["symbols"]["BETAVALUE"] = -0.2025;
    host_summary_context["symbols"]["SHAPE_SHORT"] = 5.0;
    host_summary_context["symbols"]["SHAPE_MID"] = 6.0;
    host_summary_context["symbols"]["SHAPE_LONG"] = 1.0;
    host_summary_context["symbols"]["HYSYL"] = 5.2105;
    host_summary_context["symbols"]["HYSJL"] = 0.5302;
    host_summary_context["formula_scalar_bindings"] = tdx::Json::object();
    host_summary_context["formula_scalar_bindings"]["MAINZSHQ#3#6"] = 1492.0;
    host_summary_context["formula_scalar_bindings"]["MAINZSHQ#3#7"] = 1365.0;
    host_summary_context["formula_scalar_bindings"]["MAINZSHQ#DEFAULT#0"] = 3940.04;
    host_summary_context["formula_scalar_bindings"]["TOTALHQINFO#3"] = 74.0;
    host_summary_context["formula_scalar_bindings"]["TOTALMMPAMO#1"] = 110.0;
    host_summary_context["formula_scalar_bindings"]["TOTALMMPAMO#4"] = 130.0;
    const auto host_summary = tdx::evaluate_formula_source_document(
        sample(10),
        "B:BETAVALUE;BC:BETAVALUE();S:SHAPE_SHORT;M:SHAPE_MID();"
        "L:SHAPE_LONG;U:MAINZSHQ(3,6);D:MAINZSHQ(CLOSE-CLOSE+3,CLOSE-CLOSE+7);"
        "Z:MAINZSHQ(99,0);T:TOTALHQINFO(3);A:TOTALMMPAMO(1);"
        "AD:TOTALMMPAMO(CLOSE-CLOSE+4);PE:HYSYL;PEC:HYSYL();"
        "PB:HYSJL;PBC:HYSJL();",
        {}, "HOSTSUMMARY", &host_summary_context);
    require(std::abs(point_value(host_summary, 9, "B").as_number() + 0.2025) < 1e-9 &&
                std::abs(point_value(host_summary, 9, "BC").as_number() + 0.2025) < 1e-9 &&
                point_value(host_summary, 9, "S").as_number() == 5.0 &&
                point_value(host_summary, 9, "M").as_number() == 6.0 &&
                point_value(host_summary, 9, "L").as_number() == 1.0 &&
                point_value(host_summary, 0, "U").as_number() == 1492.0 &&
                point_value(host_summary, 0, "D").as_number() == 1365.0 &&
                std::abs(point_value(host_summary, 0, "Z").as_number() - 3940.04) < 1e-9 &&
                point_value(host_summary, 0, "T").as_number() == 74.0 &&
                point_value(host_summary, 0, "A").as_number() == 110.0 &&
                point_value(host_summary, 0, "AD").as_number() == 130.0 &&
                std::abs(point_value(host_summary, 0, "PE").as_number() - 5.2105) < 1e-9 &&
                std::abs(point_value(host_summary, 0, "PEC").as_number() - 5.2105) < 1e-9 &&
                std::abs(point_value(host_summary, 0, "PB").as_number() - 0.5302) < 1e-9 &&
                std::abs(point_value(host_summary, 0, "PBC").as_number() - 0.5302) < 1e-9,
            "statistics, industry valuation and market-summary host functions broadcast final-bar "
            "values");
    const auto market_alias_analysis =
        tdx::analyze_formula_source("IA:INDEXADV;IAC:INDEXADV();ID:INDEXDEC;IDC:INDEXDEC();"
                                    "N:DYNA_NOW;NC:DYNA_NOW();Z:DYNA_ZAF;L:DYNA_LB();S:DYNA_ZAS;");
    require(market_alias_analysis.at("syntax_supported").as_bool() &&
                market_alias_analysis.at("executable_with_context").as_bool() &&
                market_alias_analysis.at("context_bindings_unavailable").as_array().empty() &&
                market_alias_analysis.at("automatic_context_dependencies").as_array().size() == 6,
            "INDEXADV/INDEXDEC and four DYNA aliases are automatic zero-argument contexts");
    tdx::Json market_alias_context = tdx::Json::object();
    market_alias_context["series"] = tdx::Json::object();
    market_alias_context["series"]["INDEXADV"] = tdx::Json::object();
    market_alias_context["series"]["INDEXDEC"] = tdx::Json::object();
    const auto market_alias_bars = sample(10);
    for (std::size_t index = 0; index < market_alias_bars.at("bars").size(); ++index) {
        const auto &bar = market_alias_bars.at("bars").as_array()[index];
        const auto stamp = bar.at("date").as_string() + "|" + bar.at("time").as_string();
        market_alias_context["series"]["INDEXADV"][stamp] = 1800.0 + static_cast<double>(index);
        market_alias_context["series"]["INDEXDEC"][stamp] = 1000.0 - static_cast<double>(index);
    }
    market_alias_context["symbols"] = tdx::Json::object();
    market_alias_context["symbols"]["DYNA_NOW"] = 11.35;
    market_alias_context["symbols"]["DYNA_ZAF"] = 0.01429848;
    market_alias_context["symbols"]["DYNA_LB"] = 1.0686987;
    market_alias_context["symbols"]["DYNA_ZAS"] = -0.0008;
    const auto market_aliases = tdx::evaluate_formula_source_document(
        market_alias_bars,
        "IA:INDEXADV;IAC:INDEXADV();ID:INDEXDEC;IDC:INDEXDEC();"
        "N:DYNA_NOW;NC:DYNA_NOW();Z:DYNA_ZAF;L:DYNA_LB();S:DYNA_ZAS;",
        {}, "MARKETALIASES", &market_alias_context);
    const auto indexadv_bare_first = point_value(market_aliases, 0, "IA").as_number();
    require(indexadv_bare_first == 1809.0,
            "INDEXADV bare symbol first value, actual=" + std::to_string(indexadv_bare_first));
    require(point_value(market_aliases, 9, "IAC").as_number() == 1800.0,
            "INDEXADV empty call last value");
    require(point_value(market_aliases, 0, "ID").as_number() == 991.0,
            "INDEXDEC bare symbol first value");
    require(point_value(market_aliases, 9, "IDC").as_number() == 1000.0,
            "INDEXDEC empty call last value");
    require(std::abs(point_value(market_aliases, 0, "N").as_number() - 11.35) < 1e-12,
            "DYNA_NOW bare symbol value");
    require(point_value(market_aliases, 9, "NC").as_number() ==
                point_value(market_aliases, 0, "N").as_number(),
            "DYNA_NOW empty call broadcast");
    require(std::abs(point_value(market_aliases, 9, "S").as_number() + 0.0008) < 1e-12,
            "DYNA_ZAS fractional rise-speed value");
    const auto relation_analysis =
        tdx::analyze_formula_source("A:STRCMP(DPZSCODE,'399001');B:STRCMP(DPZSNAME(),'深圳成指');"
                                    "C:STRCMP(UNDERCODE,'600029');D:STRCMP(UNDERCODE(),'600029');"
                                    "U:UNDERLYC;UC:UNDERLYC();F:DIVFACTOR(1);H:DIVFACTOR(2);");
    require(relation_analysis.at("syntax_supported").as_bool() &&
                relation_analysis.at("executable_with_context").as_bool() &&
                relation_analysis.at("context_bindings_unavailable").as_array().empty() &&
                relation_analysis.at("automatic_context_dependencies").as_array().size() == 5,
            "main-index, underlying-security and DIVFACTOR dependencies are automatic contexts");
    const auto divfactor_kline = tdx::Json::parse(
        R"({"bars":[{"date":"2026-06-10","time":""},{"date":"2026-06-11","time":""},{"date":"2026-06-12","time":""},{"date":"2026-06-15","time":""}]})");
    const auto divfactor_capital = tdx::Json::parse(
        R"({"records":[{"date":"2026-06-11","category":1,"details":{"bonus_transfer_per_10_shares":2,"dividend_per_10_shares_yuan":99}},{"date":"2026-06-15","category":1,"details":{"bonus_transfer_per_10_shares":5,"dividend_per_10_shares_yuan":88}}]})");
    const auto divfactor = tdx::build_divfactor_series_document(divfactor_kline, divfactor_capital);
    require(std::abs(divfactor.at("front").at("2026-06-10|").as_number() -
                     static_cast<double>(static_cast<float>(1.0F / 1.2F / 1.5F))) < 1e-7 &&
                std::abs(divfactor.at("front").at("2026-06-11|").as_number() -
                         static_cast<double>(static_cast<float>(1.0F / 1.5F))) < 1e-7 &&
                divfactor.at("front").at("2026-06-15|").as_number() == 1.0 &&
                std::abs(divfactor.at("back").at("2026-06-11|").as_number() - 1.2) < 1e-6 &&
                std::abs(divfactor.at("back").at("2026-06-15|").as_number() - 1.8) < 1e-6 &&
                divfactor.at("event_date_count").as_number() == 2.0 &&
                divfactor.at("matched_bar_count").as_number() == 2.0,
            "DIVFACTOR fixed vector preserves strict ex-date boundaries, float32 order and "
            "bonus-only factors");
    tdx::Json relation_context = tdx::Json::object();
    relation_context["symbols"] = tdx::Json::object();
    relation_context["formula_text_symbols"] = tdx::Json::object();
    for (const auto &[name, value] : std::map<std::string, std::string>{
             {"DPZSCODE", "399001"}, {"DPZSNAME", "深圳成指"}, {"UNDERCODE", "600029"}}) {
        relation_context["symbols"][name] = 0.0;
        relation_context["formula_text_symbols"][name] = value;
    }
    relation_context["series"] = tdx::Json::object();
    relation_context["series"]["UNDERLYC"] = tdx::Json::object();
    relation_context["series"]["__DIVFACTOR_FRONT"] = tdx::Json::object();
    relation_context["series"]["__DIVFACTOR_BACK"] = tdx::Json::object();
    const auto relation_bars = sample(10);
    for (std::size_t index = 0; index < relation_bars.at("bars").size(); ++index) {
        const auto &bar = relation_bars.at("bars").as_array()[index];
        const auto stamp = bar.at("date").as_string() + "|" + bar.at("time").as_string();
        relation_context["series"]["UNDERLYC"][stamp] = 20.0 + static_cast<double>(index);
        relation_context["series"]["__DIVFACTOR_FRONT"][stamp] = 0.625;
        relation_context["series"]["__DIVFACTOR_BACK"][stamp] = 1.6;
    }
    auto adaptive_relation_bars = relation_bars;
    adaptive_relation_bars["adjustment_mode"] = "qfq";
    const auto relations = tdx::evaluate_formula_source_document(
        adaptive_relation_bars,
        "A:STRCMP(DPZSCODE,'399001');B:STRCMP(DPZSNAME(),'深圳成指');"
        "C:STRCMP(UNDERCODE,'600029');D:STRCMP(UNDERCODE(),'600029');"
        "U:UNDERLYC;UC:UNDERLYC();F:DIVFACTOR(1);H:DIVFACTOR(2);"
        "AD:DIVFACTOR(0);INVALID:DIVFACTOR(9);",
        {}, "SECURITYRELATION", &relation_context);
    require(point_value(relations, 9, "A").as_number() == 1.0 &&
                point_value(relations, 9, "B").as_number() == 1.0 &&
                point_value(relations, 9, "C").as_number() == 1.0 &&
                point_value(relations, 9, "D").as_number() == 1.0 &&
                point_value(relations, 9, "U").as_number() == 20.0 &&
                point_value(relations, 0, "UC").as_number() == 29.0 &&
                point_value(relations, 9, "F").as_number() == 0.625 &&
                point_value(relations, 9, "H").as_number() == 1.6 &&
                point_value(relations, 9, "AD").as_number() == 0.625 &&
                point_value(relations, 9, "INVALID").as_number() == 1.0,
            "security relation strings/series and adaptive/front/back DIVFACTOR execute exactly");
    const auto total_amount_l2_analysis =
        tdx::analyze_formula_source("BID:TOTALMMPAMO(5);ASK:TOTALMMPAMO(6);");
    require(!total_amount_l2_analysis.at("executable_with_context").as_bool() &&
                total_amount_l2_analysis.at("context_bindings_unavailable").as_array().size() == 2,
            "TOTALMMPAMO total-order selectors preserve the explicit Level2 boundary");
    require(tdx::analyze_formula_source("PS:SAR(4,2,20); TURN:SARTURN(4,2,20);")
                .at("executable")
                .as_bool(),
            "SAR functions must be executable");
    require(tdx::analyze_formula_source("X:\"KDJ.J\"; Y:SAR.SAR;").at("executable").as_bool(),
            "verified cross-indicator references must be intrinsic");
    const auto sar = tdx::evaluate_formula_source_document(
        sample(40), "PS:SAR(4,2,20); TURN:SARTURN(4,2,20);", {}, "SARTEST");
    require(point_value(sar, 39, "PS").is_number(), "SAR latest value");
    for (std::size_t index = 3; index < sar.at("points").as_array().size(); ++index) {
        const auto &point = sar.at("points").as_array()[index];
        const auto &turn = point.at("values").at("TURN");
        require(turn.is_number() &&
                    (index == 3 || turn.as_number() == -1.0 ||
                     turn.as_number() == 0.0 || turn.as_number() == 1.0),
                "SARTURN post-seed signal domain");
    }
    const auto embedded_kdj =
        tdx::evaluate_formula_source_document(sample(40), "X:\"KDJ.J\";", {}, "KDJREF");
    const auto explicit_kdj =
        tdx::evaluate_formula_source_document(sample(40),
                                              "R:=(CLOSE-LLV(LOW,9))/(HHV(HIGH,9)-LLV(LOW,9))*100;"
                                              "K:=SMA(R,3,1);D:=SMA(K,3,1);J:3*K-2*D;",
                                              {}, "KDJEXPLICIT");
    require(std::abs(point_value(embedded_kdj, 39, "X").as_number() -
                     point_value(explicit_kdj, 39, "J").as_number()) < 1e-10,
            "KDJ.J cross-indicator value");

    const auto expansion_analysis =
        tdx::analyze_formula_source("OI:VOLINSTK; DELTA:CCL-REF(CCL,1); SHORT:HKSHORTVOL/10000;");
    require(expansion_analysis.at("executable").as_bool(),
            "expansion-market symbols must be directly executable");
    const auto expansion = tdx::evaluate_formula_source_document(
        expansion_sample(), "OI:VOLINSTK; DELTA:CCL-REF(CCL,1); SHORT:HKSHORTVOL/10000;", {},
        "EXPANSION");
    require(point_value(expansion, 1, "OI").as_number() == 123457.0,
            "VOLINSTK open-interest binding");
    require(point_value(expansion, 1, "DELTA").as_number() == 1.0, "CCL open-interest alias");
    require(std::abs(point_value(expansion, 1, "SHORT").as_number() -
                     static_cast<double>(static_cast<float>(2.0001))) < 1e-9,
            "HKSHORTVOL auxiliary binding");
    require(expansion.at("formula_volume_unit").as_string() == "contract" &&
                expansion.at("formula_volume_divisor").as_number() == 1.0,
            "expansion formula volume unit");

    const auto ivolat_analysis =
        tdx::analyze_formula_source("HV:IVOLAT(N,0)*100; IV:IVOLAT(N,1)*100;", {"N"});
    require(ivolat_analysis.at("executable_with_context").as_bool() &&
                ivolat_analysis.at("has_external_dependency").as_bool(),
            "IVOLAT is recognized as a cross-security context function");
    auto option_bars = expansion_sample();
    option_bars["market"] = "5";
    option_bars["code"] = "TESTOPT";
    option_bars["bars"].as_array()[0]["date"] = "2026-01-03";
    option_bars["bars"].as_array()[1]["date"] = "2026-01-04";
    option_bars["bars"].as_array()[0]["close"] = 8.0;
    option_bars["bars"].as_array()[1]["close"] = 9.0;
    tdx::Json ivolat_context = tdx::Json::object();
    ivolat_context["ivolat"] = tdx::Json::parse(
        R"({"strike":100,"call":true,"american":true,"futures_model":true,)"
        R"("divisor":1,"risk_free":0.0187,"expiry":"2026-06-30",)"
        R"("history":[{"date":"2026-01-01","close":98},{"date":"2026-01-02","close":100},)"
        R"({"date":"2026-01-03","close":102},{"date":"2026-01-04","close":104}]})");
    const auto ivolat = tdx::evaluate_formula_source_document(
        std::move(option_bars), "HV:IVOLAT(N,0); IV:IVOLAT(N,1);", {{"N", 3}}, "IVOLATTEST",
        &ivolat_context);
    require(point_value(ivolat, 1, "HV").is_number() &&
                point_value(ivolat, 1, "HV").as_number() > 0.0 &&
                point_value(ivolat, 1, "IV").is_number() &&
                point_value(ivolat, 1, "IV").as_number() > 0.0,
            "IVOLAT computes historical and implied volatility from underlying context");

    // Context dates come from callers, so the environment builder skips rows it
    // cannot shape-check instead of raising.  It has to drop the day and the close
    // together or the two IVOLAT history series lose index alignment, so
    // interleaving rejected rows among the good ones must leave HV exactly where it
    // was.  Only the length and separator guard rejects: the day-number arithmetic
    // itself does no range validation, so out-of-range fields like month 13 parse
    // leniently and do contribute a point.
    const auto ivolat_option_bars = [] {
        auto bars = expansion_sample();
        bars["market"] = "5";
        bars["code"] = "TESTOPT";
        bars["bars"].as_array()[0]["date"] = "2026-01-03";
        bars["bars"].as_array()[1]["date"] = "2026-01-04";
        bars["bars"].as_array()[0]["close"] = 8.0;
        bars["bars"].as_array()[1]["close"] = 9.0;
        return bars;
    };
    tdx::Json ivolat_dirty_context = tdx::Json::object();
    ivolat_dirty_context["ivolat"] = tdx::Json::parse(
        R"({"strike":100,"call":true,"american":true,"futures_model":true,)"
        R"("divisor":1,"risk_free":0.0187,"expiry":"2026-06-30",)"
        R"("history":[{"date":"2026-01-01","close":98},{"date":"bad","close":1},)"
        R"({"date":"2026-01-02","close":100},{"date":"2026/01/02","close":2},)"
        R"({"date":"2026-01-03","close":102},{"date":"20260103","close":3},)"
        R"({"date":42,"close":4},{"date":"2026-01-04","close":104},)"
        R"({"date":"2026-01-05","close":"x"}]})");
    const auto ivolat_dirty = tdx::evaluate_formula_source_document(
        ivolat_option_bars(), "HV:IVOLAT(N,0); IV:IVOLAT(N,1);", {{"N", 3}},
        "IVOLATTEST", &ivolat_dirty_context);
    require(point_value(ivolat_dirty, 1, "HV").as_number() ==
                point_value(ivolat, 1, "HV").as_number(),
            "IVOLAT skips malformed history dates without shifting the close series");

    // An unparseable expiry is likewise skipped rather than raised, leaving the
    // historical branch intact.
    tdx::Json ivolat_expiry_context = tdx::Json::object();
    ivolat_expiry_context["ivolat"] = tdx::Json::parse(
        R"({"strike":100,"call":true,"american":true,"futures_model":true,)"
        R"("divisor":1,"risk_free":0.0187,"expiry":"2026/06/30",)"
        R"("history":[{"date":"2026-01-01","close":98},{"date":"2026-01-02","close":100},)"
        R"({"date":"2026-01-03","close":102},{"date":"2026-01-04","close":104}]})");
    const auto ivolat_bad_expiry = tdx::evaluate_formula_source_document(
        ivolat_option_bars(), "HV:IVOLAT(N,0); IV:IVOLAT(N,1);", {{"N", 3}},
        "IVOLATTEST", &ivolat_expiry_context);
    require(point_value(ivolat_bad_expiry, 1, "HV").as_number() ==
                point_value(ivolat, 1, "HV").as_number(),
            "a malformed IVOLAT expiry leaves the historical branch intact");

    const auto library = tdx::load_bundled_formula_library_document();
    const auto native_kdj = tdx::evaluate_formula_document(sample(40), formula(library, "KDJ-TDX"));
    require(point_value(native_kdj, 6, "K").as_number() == 0.0 &&
                point_value(native_kdj, 7, "K").as_number() == 50.0 &&
                point_value(native_kdj, 39, "J").is_number(),
            "recovered KDJ-TDX native seed and outputs");
    const auto native_boll_m =
        tdx::evaluate_formula_document(sample(40), formula(library, "BOLL-M"));
    require(point_value(native_boll_m, 39, "BOLL").is_number() &&
                point_value(native_boll_m, 39, "UB").is_number() &&
                point_value(native_boll_m, 39, "UB").as_number() >
                    point_value(native_boll_m, 39, "BOLL").as_number() &&
                point_value(native_boll_m, 39, "LB").as_number() <
                    point_value(native_boll_m, 39, "BOLL").as_number(),
            "recovered BOLL-M delayed center and deviation bands");
    const auto native_bb = tdx::evaluate_formula_document(sample(45), formula(library, "BB"));
    const auto native_width = tdx::evaluate_formula_document(sample(45), formula(library, "WIDTH"));
    require(point_value(native_bb, 38, "BB").as_number() == 0.0 &&
                point_value(native_bb, 39, "BB").is_number() &&
                point_value(native_bb, 44, "MA").is_number() &&
                point_value(native_width, 39, "WIDTH").is_number() &&
                point_value(native_width, 44, "MA").is_number(),
            "recovered BB/WIDTH shared legacy-band statistics");
    const auto native_asi = tdx::evaluate_formula_document(sample(45), formula(library, "ASI"));
    require(point_value(native_asi, 0, "ASI").as_number() == 0.0 &&
                point_value(native_asi, 44, "ASI").is_number() &&
                point_value(native_asi, 44, "MA").is_number(),
            "recovered ASI cumulative swing index and smoothing");
    const auto native_sar = tdx::evaluate_formula_document(sample(45), formula(library, "SAR"));
    require(point_value(native_sar, 2, "SAR").as_number() == 0.0 &&
                point_value(native_sar, 3, "SAR").is_number() &&
                point_value(native_sar, 44, "SAR").is_number(),
            "recovered four-parameter system SAR state machine");
    const auto native_vty = tdx::evaluate_formula_document(sample(45), formula(library, "VTY"));
    require(point_value(native_vty, 0, "VTY").as_number() == 0.0 &&
                std::abs(point_value(native_vty, 1, "VTY").as_number() - 9.6) < 1e-5 &&
                std::abs(point_value(native_vty, 44, "VTY").as_number() - 52.6) < 1e-4,
            "recovered VTY true-range EMA and reversal stop state machine");
    auto breadth_kline = sample(45);
    tdx::Json breadth_context = tdx::Json::object();
    breadth_context["series"] = tdx::Json::object();
    breadth_context["series"]["ADVANCE"] = tdx::Json::object();
    breadth_context["series"]["DECLINE"] = tdx::Json::object();
    for (const auto &bar : breadth_kline.at("bars").as_array()) {
        const auto key = bar.at("date").as_string() + "|" + bar.at("time").as_string();
        const int day = std::stoi(bar.at("date").as_string().substr(8));
        breadth_context["series"]["ADVANCE"][key] = 1000.0 + day * day;
        breadth_context["series"]["DECLINE"][key] = 500.0 + day;
    }
    const auto native_msi =
        tdx::evaluate_formula_document(std::move(breadth_kline), formula(library, "MSI"),
                                       {{"N", 2.0}, {"M1", 3.0}, {"M2", 2.0}}, &breadth_context);
    require(point_value(native_msi, 0, "MSI").as_number() == -1000.0 &&
                point_value(native_msi, 1, "MSI").is_number() &&
                point_value(native_msi, 44, "MA").is_number(),
            "recovered MSI breadth deltas, native EMA, accumulation and smoothing");
    tdx::Json mcst_kline = tdx::Json::object();
    mcst_kline["market"] = "sz";
    mcst_kline["code"] = "000001";
    mcst_kline["period"] = "day";
    mcst_kline["bars"] = tdx::Json::array();
    const double mcst_close[]{10.0, 11.0, 12.0};
    const double mcst_amount[]{10000.0, 22000.0, 36000.0};
    const double mcst_volume[]{1000.0, 2000.0, 3000.0};
    tdx::Json mcst_context = tdx::Json::object();
    mcst_context["series"] = tdx::Json::object();
    mcst_context["series"]["CAPITAL"] = tdx::Json::object();
    for (int i = 0; i < 3; ++i) {
        const auto date = "2026-02-0" + std::to_string(i + 1);
        tdx::Json bar = tdx::Json::object();
        bar["date"] = date;
        bar["time"] = "15:00";
        bar["open"] = mcst_close[i];
        bar["high"] = mcst_close[i];
        bar["low"] = mcst_close[i];
        bar["close"] = mcst_close[i];
        bar["amount"] = mcst_amount[i];
        bar["volume"] = mcst_volume[i];
        mcst_kline["bars"].push_back(std::move(bar));
        mcst_context["series"]["CAPITAL"][date + "|15:00"] = 100.0;
    }
    const auto native_mcst = tdx::evaluate_formula_document(
        std::move(mcst_kline), formula(library, "MCST"), {}, &mcst_context);
    require(std::abs(point_value(native_mcst, 0, "MCST").as_number() - 10.0) < 1e-6 &&
                std::abs(point_value(native_mcst, 1, "MCST").as_number() - 10.2) < 1e-5 &&
                std::abs(point_value(native_mcst, 2, "MCST").as_number() - 10.74) < 1e-5,
            "recovered MCST type-103 capital and native cost recursion");
    tdx::Json ssrp_kline = tdx::Json::object();
    ssrp_kline["market"] = "sz";
    ssrp_kline["code"] = "000001";
    ssrp_kline["period"] = "day";
    ssrp_kline["bars"] = tdx::Json::array();
    tdx::Json ssrp_context = tdx::Json::object();
    ssrp_context["series"] = tdx::Json::object();
    ssrp_context["series"]["CAPITAL"] = tdx::Json::object();
    for (int i = 0; i < 2; ++i) {
        const auto date = "2026-03-0" + std::to_string(i + 1);
        const double price = i ? 12.0 : 10.0;
        tdx::Json bar = tdx::Json::object();
        bar["date"] = date;
        bar["time"] = "15:00";
        bar["open"] = price;
        bar["high"] = price;
        bar["low"] = price;
        bar["close"] = price;
        bar["amount"] = price * 100.0;
        bar["volume"] = 100.0;
        ssrp_kline["bars"].push_back(std::move(bar));
        ssrp_context["series"]["CAPITAL"][date + "|15:00"] = 100.0;
    }
    const auto native_ssrp = tdx::evaluate_formula_source_document(
        std::move(ssrp_kline), "S:TDXSSRP(2,3,0);A:TDXSSRP(2,3,1);B:TDXSSRP(2,3,2);", {},
        "SSRPTEST", &ssrp_context);
    require(std::abs(point_value(native_ssrp, 0, "S").as_number() - 10.0) < 1e-5 &&
                std::abs(point_value(native_ssrp, 1, "S").as_number() - (2190.0 / 199.0)) < 1e-4 &&
                point_value(native_ssrp, 1, "A").is_number() &&
                point_value(native_ssrp, 1, "B").is_number(),
            "recovered SSRP 720-bar uniform chip peak and dual native smoothing");
    tdx::Json pav_kline = tdx::Json::object();
    pav_kline["market"] = "sz";
    pav_kline["code"] = "000001";
    pav_kline["period"] = "day";
    pav_kline["bars"] = tdx::Json::array();
    tdx::Json pav_context = tdx::Json::object();
    pav_context["series"] = tdx::Json::object();
    pav_context["series"]["CAPITAL"] = tdx::Json::object();
    const double pav_close[]{11.0, 11.5, 10.5};
    for (int i = 0; i < 3; ++i) {
        const auto date = "2026-04-0" + std::to_string(i + 1);
        tdx::Json bar = tdx::Json::object();
        bar["date"] = date;
        bar["time"] = "15:00";
        bar["open"] = pav_close[i];
        bar["high"] = 12.0;
        bar["low"] = 10.0;
        bar["close"] = pav_close[i];
        bar["amount"] = pav_close[i] * 1000000.0;
        bar["volume"] = 1000000.0;
        pav_kline["bars"].push_back(std::move(bar));
        pav_context["series"]["CAPITAL"][date + "|15:00"] = 100000.0;
    }
    const auto native_pav = tdx::evaluate_formula_source_document(
        std::move(pav_kline),
        "G:TDXPAV(2,3,0);C:TDXPAV(2,3,1);AG:TDXPAV(2,3,2);"
        "AC:TDXPAV(2,3,3);D:TDXPAV(2,3,4);"
        "EC:TDXPAVE(2,3,0);EM:TDXPAVE(2,3,1);ED:TDXPAVE(2,3,2);",
        {}, "PAVTEST", &pav_context);
    require(std::abs(point_value(native_pav, 0, "G").as_number() - 49.5) < 1e-5 &&
                std::abs(point_value(native_pav, 1, "C").as_number() + 25.5) < 1e-5 &&
                std::abs(point_value(native_pav, 2, "AG").as_number() - 43.25) < 1e-5 &&
                std::abs(point_value(native_pav, 2, "AC").as_number() + 53.2777777778) < 1e-4 &&
                std::abs(point_value(native_pav, 2, "D").as_number() + 10.0277777778) < 1e-4 &&
                std::abs(point_value(native_pav, 2, "EC").as_number() + 56.75) < 1e-5 &&
                std::abs(point_value(native_pav, 2, "EM").as_number() + 53.2777777778) < 1e-4 &&
                std::abs(point_value(native_pav, 2, "ED").as_number() + 10.0277777778) < 1e-4,
            "recovered PAV/PAVE type-105 capital, 200-bin chip gravity and smoothing");
    tdx::Json ndb_kline = tdx::Json::object();
    ndb_kline["market"] = "sz";
    ndb_kline["code"] = "000001";
    ndb_kline["name"] = "平安银行";
    ndb_kline["period"] = "day";
    ndb_kline["bars"] = tdx::Json::array();
    const double ndb_high[]{10.0, 11.0, 11.8, 10.6};
    const double ndb_low[]{10.0, 10.0, 11.4, 10.2};
    const double ndb_close[]{10.0, 10.5, 11.6, 10.4};
    for (int i = 0; i < 4; ++i) {
        tdx::Json bar = tdx::Json::object();
        bar["date"] = "2026-05-0" + std::to_string(i + 1);
        bar["time"] = "15:00";
        bar["open"] = ndb_close[i];
        bar["high"] = ndb_high[i];
        bar["low"] = ndb_low[i];
        bar["close"] = ndb_close[i];
        bar["amount"] = 1000.0;
        bar["volume"] = 100.0;
        ndb_kline["bars"].push_back(std::move(bar));
    }
    const auto native_ndb = tdx::evaluate_formula_source_document(
        std::move(ndb_kline), "N:TDXNDB(2,3,0);A:TDXNDB(2,3,1);B:TDXNDB(2,3,2);", {}, "NDBTEST");
    require(std::abs(point_value(native_ndb, 2, "N").as_number() - 0.9) < 1e-5 &&
                std::abs(point_value(native_ndb, 3, "N").as_number() + 0.1) < 1e-5 &&
                std::abs(point_value(native_ndb, 3, "A").as_number() - 0.175) < 1e-5 &&
                std::abs(point_value(native_ndb, 3, "B").as_number() - (1.0 / 6.0)) < 1e-5,
            "recovered NDB limit-gap branches, accumulation and dual smoothing");
    tdx::Json sc_kline = tdx::Json::object();
    sc_kline["market"] = "sz";
    sc_kline["code"] = "000001";
    sc_kline["period"] = "day";
    sc_kline["bars"] = tdx::Json::array();
    for (int i = 0; i < 16; ++i) {
        const double price = i <= 10 ? 20.0 - i : 50.0;
        tdx::Json bar = tdx::Json::object();
        bar["date"] = "2026-06-" + std::string(i + 1 < 10 ? "0" : "") + std::to_string(i + 1);
        bar["time"] = "15:00";
        bar["open"] = price;
        bar["high"] = price;
        bar["low"] = price;
        bar["close"] = price;
        bar["amount"] = price * 100.0;
        bar["volume"] = 100.0;
        sc_kline["bars"].push_back(std::move(bar));
    }
    const auto native_sc =
        tdx::evaluate_formula_source_document(std::move(sc_kline), "S:TDXSC();", {}, "SCTEST");
    require(point_value(native_sc, 10, "S").as_number() == 0.0 &&
                point_value(native_sc, 11, "S").as_number() == 10.0 &&
                point_value(native_sc, 12, "S").as_number() == 9.0 &&
                point_value(native_sc, 13, "S").as_number() == 8.0 &&
                point_value(native_sc, 14, "S").as_number() == 6.0 &&
                point_value(native_sc, 15, "S").as_number() == 3.0,
            "recovered SC price cross and native five-bar pulse weights");
    tdx::Json xlpl_kline = tdx::Json::object();
    xlpl_kline["market"] = "sz";
    xlpl_kline["code"] = "000001";
    xlpl_kline["period"] = "day";
    xlpl_kline["bars"] = tdx::Json::array();
    tdx::Json xlpl_context = tdx::Json::object();
    xlpl_context["series"] = tdx::Json::object();
    xlpl_context["series"]["CAPITAL"] = tdx::Json::object();
    for (int i = 0; i < 40; ++i) {
        const double price = i < 20 ? 20.0 - i * 0.3 : 14.0 + (i - 20) * 0.45;
        const auto date = "2026-07-" + std::string(i + 1 < 10 ? "0" : "") + std::to_string(i + 1);
        tdx::Json bar = tdx::Json::object();
        bar["date"] = date;
        bar["time"] = "15:00";
        bar["open"] = price;
        bar["high"] = price + 0.2;
        bar["low"] = price - 0.2;
        bar["close"] = price;
        bar["amount"] = price * 1000.0;
        bar["volume"] = 1000.0;
        xlpl_kline["bars"].push_back(std::move(bar));
        xlpl_context["series"]["CAPITAL"][date + "|15:00"] = 1000000.0;
    }
    const auto &xlpl_formula = formula(library, "XLPL");
    const auto xlpl_analysis =
        tdx::analyze_formula_source(xlpl_formula.at("source_text").as_string());
    require(xlpl_analysis.at("has_future_function").as_bool() &&
                xlpl_analysis.at("read_only_future_executable").as_bool() &&
                xlpl_analysis.at("has_external_dependency").as_bool(),
            "XLPL is a context-backed read-only future renderer");
    const auto native_xlpl =
        tdx::evaluate_formula_document(std::move(xlpl_kline), xlpl_formula, {}, &xlpl_context);
    require(native_xlpl.at("outputs").as_array().size() == 8 &&
                point_value(native_xlpl, 19, "落").is_number() &&
                point_value(native_xlpl, 21, "吸").is_number() &&
                point_value(native_xlpl, 39, "拉").is_number() &&
                point_value(native_xlpl, 20, "NOTEXT吸").is_number(),
            "recovered XLPL four states and two-bar BACKSET renderings");
    const auto &zxnh_formula = formula(library, "ZXNH");
    const auto zxnh_analysis =
        tdx::analyze_formula_source(zxnh_formula.at("source_text").as_string());
    require(zxnh_analysis.at("has_future_function").as_bool() &&
                zxnh_analysis.at("read_only_future_executable").as_bool() &&
                !zxnh_analysis.at("has_external_dependency").as_bool(),
            "ZXNH is an OHLCV read-only future renderer");
    const auto native_zxnh = tdx::evaluate_formula_document(sample(45), zxnh_formula);
    std::size_t zxnh_anchors = 0;
    for (const auto &point : native_zxnh.at("points").as_array())
        if (point.at("values").at("ZXNH").as_number() == 1.0)
            ++zxnh_anchors;
    require(point_value(native_zxnh, 0, "ZXNH").as_number() == 1.0 &&
                point_value(native_zxnh, 4, "ZXNH").as_number() == 0.0 &&
                point_value(native_zxnh, 44, "ZXNH").as_number() == 1.0 && zxnh_anchors == 2,
            "recovered ZXNH fixed SAR segmentation and fitted-price extrema");
    const auto native_nvi = tdx::evaluate_formula_document(sample(40), formula(library, "NVI"));
    const auto native_pvi = tdx::evaluate_formula_document(sample(40), formula(library, "PVI"));
    require(point_value(native_nvi, 39, "NVI").as_number() == 100.0 &&
                point_value(native_pvi, 39, "PVI").as_number() == 100.0 &&
                point_value(native_nvi, 39, "MA").is_number() &&
                point_value(native_pvi, 39, "MA").is_number(),
            "recovered NVI/PVI native recurrence and smoothing");
    const auto mfi = tdx::evaluate_formula_document(sample(40), formula(library, "MFI"));
    require(mfi.at("outputs").as_array().front().as_string() == "MFI", "real MFI output");
    const auto udl = tdx::evaluate_formula_document(sample(40), formula(library, "UDL"));
    require(point_value(udl, 39, "UDL").is_number(), "real UDL execution");
    const auto acd = tdx::evaluate_formula_document(sample(40), formula(library, "ACD"));
    require(point_value(acd, 39, "ACD").is_number(), "SUM(X,0) must survive initial REF gap");
    require(point_value(acd, 39, "MAACD").is_number(), "real ACD smoothing");
    const auto macd = tdx::evaluate_formula_document(sample(40), formula(library, "MACD"));
    require(point_value(macd, 39, "MACD").is_number(), "drawing suffix must be ignored");
    const auto mtm_buy = tdx::evaluate_formula_document(sample(40), formula(library, "MTM买入"));
    require(point_value(mtm_buy, 39, "IMTM").is_number(), "indicator-style MTM arity");

    for (const auto* code : std::array<const char*, 7>{
             "唐奇安", "MA交易", "MACD交易", "KDJ交易",
             "肯特纳", "日内突破", "HANS123"}) {
        const auto filtered_model =
            tdx::evaluate_formula_document(sample(40), formula(library, code));
        const auto& trade_ir = filtered_model.at("trade_event_ir");
        require(trade_ir.at("autofilter").at("enabled").as_bool() &&
                    trade_ir.at("autofilter").at("marker_count").as_number() == 1.0 &&
                    trade_ir.at("primitive_count").as_number() >= 2.0,
                std::string("built-in trading model enables AUTOFILTER projection: ") + code);
    }

    for (const auto* code : std::array<const char*, 4>{
             "B007", "C128", "C129", "C130"}) {
        const auto& definition = formula(library, code);
        std::vector<std::string> parameter_names;
        const auto& parameters = definition.at("parameters");
        if (parameters.is_array()) {
            for (const auto& parameter : parameters.as_array())
                parameter_names.push_back(parameter.at("name").as_string());
        } else if (parameters.is_object()) {
            parameter_names.push_back(parameters.at("name").as_string());
        }
        const auto limit_analysis = tdx::analyze_formula_source(
            definition.at("source_text").as_string(), parameter_names);
        require(limit_analysis.at("syntax_supported").as_bool() &&
                    limit_analysis.at("numeric_signal_safe").as_bool() &&
                    !limit_analysis.at("has_degraded_numeric_output").as_bool() &&
                    limit_analysis.at("explicit_context_bindable").as_bool() &&
                    string_set(limit_analysis.at(
                        "explicit_context_bindings_required")) ==
                        limit_price_context &&
                    !limit_analysis.at("pure_ohlcv").as_bool(),
                std::string("built-in limit-price formula exposes exact raw host context: ") +
                    code);
    }

    const auto coverage = tdx::analyze_formula_library_document(library);
    require(coverage.at("coverage").at("total").as_number() == 379, "coverage total");
    require(coverage.at("coverage").at("executable").as_number() > 0, "coverage executable count");
    require(coverage.at("analysis_schema_version").as_number() == 7 &&
                coverage.at("coverage").at("analysis_schema_version").as_number() == 7 &&
                coverage.at("coverage").at("numeric_signal_safe").as_number() == 379 &&
                coverage.at("coverage").at("degraded_numeric_output").as_number() == 0 &&
                coverage.at("coverage").at("presentation_semantics_faithful").as_number() == 379 &&
                coverage.at("coverage").at("render_semantics_materialized").as_number() ==
                    coverage.at("coverage").at("graphics").as_number() &&
                coverage.at("coverage").at("unsupported_presentation_directive").as_number() == 0 &&
                coverage.at("coverage").at("semantic_surrogate").as_number() > 0 &&
                coverage.at("coverage").at("presentation_return_surrogate").as_number() ==
                    coverage.at("coverage").at("semantic_surrogate").as_number() &&
                coverage.at("coverage").at("render_ir_available").as_number() ==
                    coverage.at("coverage").at("graphics").as_number(),
            "full library semantic-fidelity coverage");
    const auto &capabilities = coverage.at("coverage").at("capabilities");
    require(
        capabilities.at("schema").as_string() == "tdx-formula-interpreter-capabilities-v1" &&
            capabilities.at("tcalc_registry_evidence")
                    .at("static_registry_entry_count")
                    .as_number() == 390.0 &&
            capabilities.at("tcalc_registry_evidence")
                    .at("static_registry_unique_name_count")
                    .as_number() == 390.0 &&
            capabilities.at("tcalc_registry_evidence")
                    .at("static_registry_boundary_name_count")
                    .as_number() == 71.0 &&
            capabilities.at("tcalc_registry_evidence")
                    .at("static_registry_recognized_name_count")
                    .as_number() == 319.0 &&
            capabilities.at("tcalc_registry_evidence").at("syntax_only_name_count").as_number() ==
                4.0 &&
            capabilities.at("tcalc_registry_evidence")
                    .at("broker_private_signal_name_count")
                    .as_number() == 1.0 &&
            capabilities.at("tcalc_registry_evidence")
                    .at("level2_order_flow_name_count")
                    .as_number() == 13.0 &&
            capabilities.at("tcalc_registry_evidence")
                    .at("level2_context_capable_name_count")
                    .as_number() == 13.0 &&
            capabilities.at("tcalc_registry_evidence")
                    .at("live_trading_state_name_count")
                    .as_number() == 38.0 &&
            capabilities.at("tcalc_registry_evidence")
                    .at("live_trading_context_capable_name_count")
                    .as_number() == 34.0 &&
            capabilities.at("tcalc_registry_evidence")
                    .at("live_trading_context_capable_names")
                    .as_array().size() == 34 &&
            capabilities.at("tcalc_registry_evidence")
                    .at("plugin_callback_name_count")
                    .as_number() == 15.0 &&
            capabilities.at("tcalc_registry_evidence")
                .at("static_registry_fully_classified")
                .as_bool() &&
            capabilities.at("tcalc_registry_evidence")
                    .at("remaining_public_non_l2_candidate_count")
                    .as_number() == 0.0 &&
            capabilities.at("custom_formula_core_function_count").as_number() == 14.0 &&
            capabilities.at("custom_formula_core_symbol_count").as_number() == 6.0 &&
            capabilities.at("custom_formula_sequence_statistics_function_count").as_number() ==
                12.0 &&
            capabilities.at("custom_formula_sequence_statistics_functions").as_array().size() ==
                12 &&
            capabilities.at("custom_formula_rolling_variance_function_count").as_number() == 15.0 &&
            capabilities.at("custom_formula_rolling_variance_functions").as_array().size() == 15 &&
            capabilities.at("custom_formula_benchmark_cumulative_function_count").as_number() ==
                2.0 &&
            capabilities.at("custom_formula_benchmark_cumulative_functions").as_array().size() ==
                2 &&
            capabilities.at("custom_formula_calendar_filter_function_count").as_number() == 8.0 &&
            capabilities.at("custom_formula_calendar_filter_functions").as_array().size() == 8 &&
            capabilities.at("custom_formula_calendar_filter_symbol_count").as_number() == 2.0 &&
            capabilities.at("custom_formula_calendar_filter_symbols").as_array().size() == 2 &&
            capabilities.at("custom_formula_security_string_function_count").as_number() == 18.0 &&
            capabilities.at("custom_formula_security_string_functions").as_array().size() == 18 &&
            capabilities.at("custom_formula_security_string_symbol_count").as_number() == 1.0 &&
            capabilities.at("custom_formula_security_string_symbols").as_array().size() == 1 &&
            capabilities.at("custom_formula_kline_auxiliary_symbol_count").as_number() == 2.0 &&
            capabilities.at("custom_formula_kline_auxiliary_symbols").as_array().size() == 2 &&
            capabilities.at("custom_formula_external_signal_function_count").as_number() == 2.0 &&
            capabilities.at("custom_formula_external_signal_functions").as_array().size() == 2 &&
            capabilities.at("custom_formula_external_series_function_count").as_number() == 3.0 &&
            capabilities.at("custom_formula_external_series_functions").as_array().size() == 3 &&
            capabilities.at("custom_formula_external_series_functions").as_array()[0].as_string() ==
                "EXTDATA_USER" &&
            capabilities.at("custom_formula_external_series_functions").as_array()[1].as_string() ==
                "SIGNALS_SYS" &&
            capabilities.at("custom_formula_external_series_functions").as_array()[2].as_string() ==
                "SIGNALS_USER" &&
            capabilities.at("custom_formula_block_metadata_function_count").as_number() == 6.0 &&
            capabilities.at("custom_formula_block_metadata_functions").as_array().size() == 6 &&
            capabilities.at("custom_formula_block_metadata_symbol_count").as_number() == 11.0 &&
            capabilities.at("custom_formula_transform_function_count").as_number() == 2.0 &&
            capabilities.at("custom_formula_transform_functions").as_array().size() == 2 &&
            capabilities.at("custom_formula_directional_bar_function_count").as_number() == 5.0 &&
            capabilities.at("custom_formula_directional_bar_functions").as_array().size() == 5 &&
            capabilities.at("custom_formula_directional_bar_symbol_count").as_number() == 5.0 &&
            capabilities.at("custom_formula_directional_bar_symbols").as_array().size() == 5 &&
            capabilities.at("custom_formula_adjustment_function_count").as_number() == 1.0 &&
            capabilities.at("custom_formula_adjustment_functions").as_array().size() == 1 &&
            capabilities.at("custom_formula_adjustment_functions").as_array().front().as_string() ==
                "TQFLAG" &&
            capabilities.at("custom_formula_adjustment_symbol_count").as_number() == 1.0 &&
            capabilities.at("custom_formula_adjustment_symbols").as_array().size() == 1 &&
            capabilities.at("custom_formula_adjustment_symbols").as_array().front().as_string() ==
                "TQFLAG" &&
            capabilities.at("custom_formula_host_calendar_function_count").as_number() == 2.0 &&
            capabilities.at("custom_formula_host_calendar_functions").as_array().size() == 2 &&
            capabilities.at("custom_formula_indicator_aggregate_function_count").as_number() ==
                3.0 &&
            capabilities.at("custom_formula_indicator_aggregate_functions").as_array().size() ==
                3 &&
            capabilities.at("custom_formula_capital_turnover_function_count").as_number() == 1.0 &&
            capabilities.at("custom_formula_capital_turnover_functions").as_array().size() == 1 &&
            capabilities.at("custom_formula_capital_turnover_functions")
                    .as_array()
                    .front()
                    .as_string() == "LFS" &&
            capabilities.at("custom_formula_machine_clock_function_count").as_number() == 3.0 &&
            capabilities.at("custom_formula_machine_clock_functions").as_array().size() == 3 &&
            capabilities.at("custom_formula_market_breadth_function_count").as_number() == 2.0 &&
            capabilities.at("custom_formula_market_breadth_functions").as_array().size() == 2 &&
            capabilities.at("custom_formula_market_breadth_symbol_count").as_number() == 2.0 &&
            capabilities.at("custom_formula_dynamic_quote_function_count").as_number() == 4.0 &&
            capabilities.at("custom_formula_dynamic_quote_functions").as_array().size() == 4 &&
            capabilities.at("custom_formula_dynamic_quote_symbol_count").as_number() == 4.0 &&
            capabilities.at("custom_formula_security_relation_function_count").as_number() == 4.0 &&
            capabilities.at("custom_formula_security_relation_functions").as_array().size() == 4 &&
            capabilities.at("custom_formula_security_relation_symbol_count").as_number() == 4.0 &&
            capabilities.at("custom_formula_security_relation_text_symbol_count").as_number() ==
                3.0 &&
            capabilities.at("custom_formula_divfactor_function_count").as_number() == 1.0 &&
            capabilities.at("custom_formula_divfactor_functions").as_array().front().as_string() ==
                "DIVFACTOR" &&
            capabilities.at("supported_function_count").as_number() >= 257.0 &&
            capabilities.at("automatic_symbol_count").as_number() >= 87.0,
        "custom formula capabilities and static TCalc registry evidence");
    const auto explicit_template = tdx::make_formula_explicit_context_template_document(
        library, {"ZJLX", "DDY", "红绿波段"}, {"2026-01-10|15:00"});
    require(
        explicit_template.at("schema").as_string() == "tdx-formula-explicit-context-v1" &&
            explicit_template.at("formula_scalar_bindings").as_object().empty() &&
            explicit_template.at("_template").at("binding_count").as_number() > 10 &&
            explicit_template.at("_template").at("scalar_binding_count").as_number() == 0 &&
            explicit_template.at("_template").at("series_binding_count").as_number() ==
                explicit_template.at("_template").at("binding_count").as_number() &&
            explicit_template.at("series").at("L2_AMO#0#2").at("2026-01-10|15:00").is_null() &&
            explicit_template.at("series").at("TRADENUM").at("2026-01-10|15:00").is_null() &&
            explicit_template.at("series").at("SIGNALS_QS#1#0").at("2026-01-10|15:00").is_null(),
        "explicit context template exposes exact L2 and broker keys without values");
    const auto kline_stamps = tdx::formula_context_stamps_from_kline(sample(40));
    const auto batch_template =
        tdx::make_formula_explicit_context_template_document(library, {"ZJLX"}, kline_stamps);
    require(kline_stamps.size() == 40 &&
                batch_template.at("_template").at("stamp_count").as_number() == 40 &&
                batch_template.at("_template").at("stamp_source").as_string() == "explicit" &&
                batch_template.at("series").at("L2_AMO#0#2").size() == 40,
            "K-line context template must generate one exact key per unique bar");
    const auto audit = tdx::audit_formula_library_document(sample(40), library);
    require(audit.at("audit").at("eligible").as_number() > 0, "runtime audit eligible");
    require(audit.at("audit").at("passed").as_number() > 0, "runtime audit execution");
    require(audit.at("schema_version").as_number() == 2 &&
                audit.at("formulas").as_array().size() == 379 &&
                audit.at("audit").at("reported").as_number() == 379 &&
                audit.at("audit").at("unreported").as_number() == 0,
            "runtime audit reports every library formula");
    require(audit.at("audit").at("dependency_unavailable").as_number() == 0 &&
                audit.at("audit").at("context_unavailable").as_number() > 0 &&
                audit.at("audit").at("future_read_only_disabled").as_number() > 0,
            "runtime audit exposes caller-owned context and disabled future formulas");
    require(audit.at("audit").at("period_inapplicable").as_number() == 1,
            "minute-only settlement formula is not a day-period execution failure");

    auto futures_bars = expansion_sample();
    for (auto &bar : futures_bars["bars"].as_array())
        bar.as_object().erase("hk_short_volume");
    tdx::Json expansion_library = tdx::Json::object();
    expansion_library["formulas"] = tdx::Json::array();
    expansion_library["formulas"].push_back(formula(library, "CCL"));
    expansion_library["formulas"].push_back(formula(library, "SHORTVOL"));
    expansion_library["formulas"].push_back(formula(library, "VOLATILITY"));
    tdx::Json ashare_context_formula = tdx::Json::object();
    ashare_context_formula["code"] = "ASHARECTX";
    ashare_context_formula["name"] = "A-share context";
    ashare_context_formula["kind_key"] = "technical";
    ashare_context_formula["kind_name"] = "technical";
    ashare_context_formula["source_text"] = "X:FINANCE(1);";
    ashare_context_formula["parameters"] = tdx::Json::array();
    expansion_library["formulas"].push_back(std::move(ashare_context_formula));
    const auto expansion_audit =
        tdx::audit_formula_library_document(std::move(futures_bars), std::move(expansion_library));
    require(expansion_audit.at("audit").at("passed").as_number() == 1 &&
                expansion_audit.at("audit").at("market_inapplicable").as_number() == 3 &&
                expansion_audit.at("audit").at("errors").as_number() == 0,
            "expansion audit runs raw market formulas and classifies foreign contexts");
    require(expansion_audit.at("formulas").as_array()[0].at("status").as_string() == "passed" &&
                expansion_audit.at("formulas").as_array()[2].at("status").as_string() ==
                    "market_inapplicable" &&
                expansion_audit.at("formulas")
                        .as_array()[3]
                        .at("unsupported_expansion_dependencies")
                        .as_array()
                        .front()
                        .as_string() == "FINANCE",
            "expansion audit retains precise per-formula applicability evidence");

    auto hk_bars = expansion_sample();
    hk_bars["market"] = "31";
    hk_bars["code"] = "00700";
    tdx::Json hk_missing_library = tdx::Json::object();
    hk_missing_library["formulas"] = tdx::Json::array();
    for (const auto& [code, source] :
         std::array<std::pair<const char*, const char*>, 2>{{
             {"HKFIN3", "X:FINANCE(3);"},
             {"HKFIN7", "X:FINANCE(7);"}}}) {
        tdx::Json item = tdx::Json::object();
        item["code"] = code;
        item["name"] = code;
        item["kind_key"] = "technical";
        item["kind_name"] = "technical";
        item["source_text"] = source;
        item["parameters"] = tdx::Json::array();
        hk_missing_library["formulas"].push_back(std::move(item));
    }
    tdx::Json hk_missing_context = tdx::Json::object();
    hk_missing_context["automatic_market_context"] = true;
    hk_missing_context["finance"] = tdx::Json::object();
    const auto hk_missing_audit = tdx::audit_formula_library_document(
        hk_bars, hk_missing_library, &hk_missing_context);
    require(hk_missing_audit.at("audit").at("passed").as_number() == 0 &&
                hk_missing_audit.at("audit").at("errors").as_number() == 0 &&
                hk_missing_audit.at("audit")
                        .at("context_unavailable")
                        .as_number() == 2,
            "HK audit preclassifies absent automatic FINANCE bindings");
    const auto& hk_missing_rows = hk_missing_audit.at("formulas").as_array();
    require(hk_missing_rows[0].at("status").as_string() ==
                    "context_unavailable" &&
                hk_missing_rows[0]
                        .at("context_bindings_required")
                        .as_array()
                        .front()
                        .as_string() == "FINANCE#3" &&
                hk_missing_rows[0]
                        .at("context_bindings_unavailable")
                        .as_array()
                        .front()
                        .as_string() == "FINANCE#3" &&
                hk_missing_rows[1]
                        .at("context_bindings_unavailable")
                        .as_array()
                        .front()
                        .as_string() == "FINANCE#7",
            "HK audit reports each missing automatic binding exactly");
    tdx::Json hk_supplied_context = tdx::Json::object();
    hk_supplied_context["finance"] = tdx::Json::object();
    hk_supplied_context["finance"]["3"] = 3.0;
    hk_supplied_context["finance"]["7"] = 9122883125.0;
    const auto hk_supplied_audit = tdx::audit_formula_library_document(
        hk_bars, std::move(hk_missing_library), &hk_supplied_context);
    require(hk_supplied_audit.at("audit").at("passed").as_number() == 2 &&
                hk_supplied_audit.at("audit").at("errors").as_number() == 0 &&
                hk_supplied_audit.at("audit")
                        .at("context_unavailable")
                        .as_number() == 0,
            "caller-supplied HK FINANCE bindings remain executable");
    tdx::Json hk_library = tdx::Json::object();
    hk_library["formulas"] = tdx::Json::array();
    for (const auto& [code, source] :
         std::array<std::pair<const char*, const char*>, 4>{{
             {"HKFIN34", "X:FINANCE(34);"},
             {"HKFIN1", "X:FINANCE(1);"},
             {"HKFIN7", "X:FINANCE(7);"},
             {"HKFIN23", "X:FINANCE(23);"}}}) {
        tdx::Json item = tdx::Json::object();
        item["code"] = code;
        item["name"] = code;
        item["kind_key"] = "technical";
        item["kind_name"] = "technical";
        item["source_text"] = source;
        item["parameters"] = tdx::Json::array();
        hk_library["formulas"].push_back(std::move(item));
    }
    tdx::Json hk_context = tdx::Json::object();
    hk_context["automatic_market_context"] = true;
    hk_context["finance"] = tdx::Json::object();
    hk_context["finance"]["34"] = 28.5;
    hk_context["finance"]["1"] = 9122883125.0;
    hk_context["finance"]["7"] = 9122883125.0;
    const auto hk_audit = tdx::audit_formula_library_document(
        std::move(hk_bars), std::move(hk_library), &hk_context);
    require(hk_audit.at("audit").at("passed").as_number() == 3 &&
                hk_audit.at("audit").at("market_inapplicable").as_number() == 1 &&
                hk_audit.at("audit").at("errors").as_number() == 0,
            "HK audit admits only directly recovered TCalc FINANCE selectors");
    const auto& hk_rows = hk_audit.at("formulas").as_array();
    require(hk_rows[0].at("status").as_string() == "passed" &&
                hk_rows[1].at("status").as_string() == "passed" &&
                hk_rows[2].at("status").as_string() == "passed" &&
                hk_rows[3].at("status").as_string() == "market_inapplicable" &&
                hk_rows[3].at("unsupported_expansion_bindings")
                        .as_array().front().as_string() == "FINANCE#23",
            "HK audit publishes the unsupported selector boundary");

    tdx::Json broker_library = tdx::Json::object();
    broker_library["formulas"] = tdx::Json::array();
    tdx::Json broker_formula = tdx::Json::object();
    broker_formula["code"] = "BROKERONLY";
    broker_formula["name"] = "broker only";
    broker_formula["kind_key"] = "technical";
    broker_formula["kind_name"] = "technical";
    broker_formula["source_text"] = "X:SIGNALS_QS(102,0);";
    broker_formula["parameters"] = tdx::Json::array();
    broker_library["formulas"].push_back(std::move(broker_formula));
    const auto broker_audit = tdx::audit_formula_library_document(
        professional_sample, std::move(broker_library), &broker_context);
    require(broker_audit.at("audit").at("passed").as_number() == 1 &&
                broker_audit.at("audit").at("errors").as_number() == 0 &&
                broker_audit.at("formulas").as_array().front().at("explicit_context").as_bool(),
            "library audit executes explicitly supplied broker signal series");

    tdx::Json option_library = tdx::Json::object();
    option_library["formulas"] = tdx::Json::array();
    tdx::Json option_formula = tdx::Json::object();
    option_formula["code"] = "OPTIONONLY";
    option_formula["name"] = "option only";
    option_formula["kind_key"] = "technical";
    option_formula["kind_name"] = "technical";
    option_formula["source_text"] = "IV:IVOLAT(20,1);";
    option_formula["parameters"] = tdx::Json::array();
    option_library["formulas"].push_back(std::move(option_formula));
    tdx::Json empty_context = tdx::Json::object();
    const auto option_audit =
        tdx::audit_formula_library_document(sample(40), std::move(option_library), &empty_context);
    require(option_audit.at("audit").at("market_inapplicable").as_number() == 1 &&
                option_audit.at("audit").at("errors").as_number() == 0,
            "option volatility without option context is market-inapplicable");
    const auto &option_rows = option_audit.at("formulas").as_array();
    require(option_rows.size() == 1 &&
                option_rows.front().at("status").as_string() == "market_inapplicable" &&
                option_rows.front().at("required_market_context").as_array().front().as_string() ==
                    "ivolat",
            "market-inapplicable formula remains visible with its required context");
}

} // namespace formula_engine_test
