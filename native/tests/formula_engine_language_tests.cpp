#include "formula_engine_test_support.hpp"

namespace formula_engine_test {

void run_language_tests() {
    const auto simple = tdx::evaluate_formula_source_document(
        sample(30), "A:=MA(CLOSE,N); OSC:100*(CLOSE-A); SIGNAL:CROSS(CLOSE,A)&&CLOSE>0;",
        {{"N", 5}}, "TEST");
    require(simple.at("engine").as_string() == "tdx-source-interpreter-v1", "engine identity");
    require(point_value(simple, 3, "OSC").is_null(), "MA warm-up");
    require(std::abs(point_value(simple, 29, "OSC").as_number() - 200.0) < 1e-9,
            "series arithmetic");
    require(point_value(simple, 29, "SIGNAL").as_number() == 0.0, "logical and CROSS");

    const auto native_if = tdx::evaluate_formula_source_document(
        sample(4),
        "COND:=IF(CURRBARSCOUNT=4,DRAWNULL,"
        "IF(CURRBARSCOUNT=3,0,IF(CURRBARSCOUNT=2,2,DRAWNULL)));"
        "I:IF(COND,11,22);F:IFF(COND,11,22);N:IFN(COND,11,22);"
        "S:STRCMP(IF(COND,'Y','N'),'Y');"
        "T:STRCMP(IF(COND,'Y','N'),'N');",
        {}, "NATIVE_IF");
    require(point_value(native_if, 0, "I").is_null() &&
                point_value(native_if, 0, "F").is_null() &&
                point_value(native_if, 0, "N").is_null() &&
                point_value(native_if, 0, "S").as_number() == 1.0 &&
                point_value(native_if, 0, "T").as_number() == 0.0 &&
                point_value(native_if, 1, "I").as_number() == 22.0 &&
                point_value(native_if, 1, "F").as_number() == 22.0 &&
                point_value(native_if, 1, "N").as_number() == 11.0 &&
                point_value(native_if, 1, "S").as_number() == 1.0 &&
                point_value(native_if, 1, "T").as_number() == 0.0 &&
                point_value(native_if, 2, "I").as_number() == 11.0 &&
                point_value(native_if, 2, "F").as_number() == 11.0 &&
                point_value(native_if, 2, "N").as_number() == 22.0 &&
                point_value(native_if, 2, "S").as_number() == 1.0 &&
                point_value(native_if, 2, "T").as_number() == 0.0 &&
                point_value(native_if, 3, "I").as_number() == 11.0 &&
                point_value(native_if, 3, "F").as_number() == 11.0 &&
                point_value(native_if, 3, "N").as_number() == 22.0 &&
                point_value(native_if, 3, "S").as_number() == 1.0 &&
                point_value(native_if, 3, "T").as_number() == 0.0,
            "IF/IFF share TCalc leading-missing semantics while STRCMP broadcasts its final-handle comparison");

    const auto native_logical = tdx::evaluate_formula_source_document(
        sample(1),
        "A0:0 AND DRAWNULL;A1:1 AND DRAWNULL;"
        "A2:DRAWNULL AND 0;A3:DRAWNULL AND 1;"
        "O0:DRAWNULL OR 0;O1:DRAWNULL OR 1;"
        "OP0:0.0000099 OR 0;OP1:0.0000101 OR 0;"
        "ON0:-0.0000099 OR 0;ON1:-0.0000101 OR 0;",
        {}, "NATIVE_LOGICAL");
    require(point_value(native_logical, 0, "A0").as_number() == 0.0 &&
                point_value(native_logical, 0, "A1").is_null() &&
                point_value(native_logical, 0, "A2").as_number() == 0.0 &&
                point_value(native_logical, 0, "A3").is_null(),
            "AND gives exact zero priority and otherwise propagates TCalc missing");
    require(point_value(native_logical, 0, "O0").as_number() == 0.0 &&
                point_value(native_logical, 0, "O1").as_number() == 1.0 &&
                point_value(native_logical, 0, "OP0").as_number() == 0.0 &&
                point_value(native_logical, 0, "OP1").as_number() == 1.0 &&
                point_value(native_logical, 0, "ON0").as_number() == 0.0 &&
                point_value(native_logical, 0, "ON1").as_number() == 1.0,
            "OR treats missing as false and uses TCalc's signed 1e-5 float threshold");

    const auto bar_position = tdx::evaluate_formula_source_document(
        sample(3),
        "USED:USEDDATANUM;TOTAL:TOTALBARSCOUNT;COUNT:BARSCOUNT(CLOSE);CODE:SETCODE;",
        {}, "BARPOSITIONCONSTANTS");
    for (std::size_t index = 0; index < 3; ++index) {
        require(point_value(bar_position, index, "USED").as_number() == 3.0 &&
                    point_value(bar_position, index, "TOTAL").as_number() == 3.0,
                "USEDDATANUM broadcasts TCalc's complete evaluator bar count");
        require(point_value(bar_position, index, "COUNT").as_number() ==
                    static_cast<double>(index),
                "BARSCOUNT starts its native bar sequence at zero");
        require(point_value(bar_position, index, "CODE").as_number() == 0.0,
                "SETCODE preserves the Shenzhen market ID");
    }

    for (const auto& [market, expected] :
         std::array<std::pair<const char*, double>, 5>{{
             {"sh", 1.0}, {"bj", 2.0}, {"1", 1.0}, {"2", 2.0}, {"44", 2.0}}}) {
        auto market_sample = sample(1);
        market_sample["market"] = market;
        const auto set_code = tdx::evaluate_formula_source_document(
            std::move(market_sample), "CODE:SETCODE;", {}, "SETCODEALIASES");
        require(point_value(set_code, 0, "CODE").as_number() == expected,
                "SETCODE accepts the public market aliases and numeric IDs");
    }
    const auto expansion_set_code = tdx::evaluate_formula_source_document(
        expansion_sample(), "CODE:SETCODE;", {}, "SETCODEEXPANSION");
    require(point_value(expansion_set_code, 0, "CODE").as_number() == 47.0 &&
                point_value(expansion_set_code, 1, "CODE").as_number() == 47.0,
            "SETCODE broadcasts the selected expansion-market ID");

    const auto zxnh_market_result = [](const char* market, bool expansion) {
        auto document = sample(24);
        document["market"] = market;
        document["code"] = expansion ? "TEST" : "920001";
        document["expansion_market"] = expansion;
        for (std::size_t index = 0; index < document.at("bars").size(); ++index) {
            auto& bar = document["bars"].as_array()[index];
            const double volume = bar.at("volume").as_number();
            // Make the amount-derived price deliberately disagree with OHLC's
            // typical price so this fixture detects the native market branch.
            bar["amount"] = volume * (index % 3 == 0 ? 500.0 : 5.0);
        }
        return tdx::evaluate_formula_source_document(
            std::move(document), "X:TDXZXNH();", {}, "ZXNHMARKETALIAS");
    };
    const auto same_zxnh_series = [](const tdx::Json& left,
                                     const tdx::Json& right) {
        if (left.at("points").size() != right.at("points").size()) return false;
        for (std::size_t index = 0; index < left.at("points").size(); ++index) {
            const auto& lhs = point_value(left, index, "X");
            const auto& rhs = point_value(right, index, "X");
            if (lhs.is_null() != rhs.is_null()) return false;
            if (lhs.is_number() && lhs.as_number() != rhs.as_number()) return false;
        }
        return true;
    };
    for (const auto& [alias, numeric] :
         std::array<std::pair<const char*, const char*>, 5>{{
             {"qz", "28"}, {"qd", "29"}, {"qs", "30"},
             {"cz", "47"}, {"qg", "66"}}}) {
        const auto aliased = zxnh_market_result(alias, true);
        const auto canonical = zxnh_market_result(numeric, true);
        require(same_zxnh_series(aliased, canonical),
                "TDXZXNH expansion aliases use their canonical TCalc market IDs");
    }
    require(same_zxnh_series(zxnh_market_result("bj", false),
                             zxnh_market_result("2", false)),
            "TDXZXNH preserves the Beijing alias/numeric market branch");
    require(!same_zxnh_series(zxnh_market_result("qz", true),
                              zxnh_market_result("unknown", true)),
            "TDXZXNH alias fixture distinguishes typical from amount-derived price");

    const auto unspecified_expansion_sessions =
        tdx::evaluate_formula_source_document(
            expansion_sample(), "FO:FROMOPEN;TOTAL:TOTALFZNUM;", {},
            "UNSPECIFIEDEXPANSIONSESSIONS");
    require(point_value(unspecified_expansion_sessions, 0, "FO").is_null() &&
                point_value(unspecified_expansion_sessions, 0, "TOTAL").is_null(),
            "non-A-share markets do not guess absent trading sessions");

    auto trade_signal_sample = sample(3);
    const auto trade_signals = tdx::evaluate_formula_source_document(
        std::move(trade_signal_sample),
        "BUY(CLOSE<12,IF(CLOSE=11,DRAWNULL,LOW));"
        "BUYSHORT_BUY(CLOSE>=10,LOW);"
        "SELL_SELLSHORT(CLOSE=CLOSE,HIGH);",
        {}, "TRADE_EVENT_IR");
    require(trade_signals.at("outputs").as_array()[0].as_string() == "RESULT" &&
                point_value(trade_signals, 0, "RESULT").as_number() == 1.0,
            "trading-signal numeric series remains backward compatible");
    const auto& trade_ir = trade_signals.at("trade_event_ir");
    require(trade_ir.at("schema").as_string() ==
                    "tdx-formula-trade-event-ir-v1" &&
                !trade_ir.at("execution_side_effects").as_bool() &&
                !trade_ir.at("order_submission").as_bool() &&
                trade_ir.at("scope").as_string() ==
                    "top-level-trading-signal-statements-only" &&
                trade_ir.at("primitive_count").as_number() == 3.0,
            "trade event IR is an additive read-only top-level projection");
    const auto& buy = trade_ir.at("primitives").as_array()[0];
    require(buy.at("condition_series").as_array()[0].as_number() == 1.0 &&
                buy.at("condition_series").as_array()[1].as_number() == 0.0 &&
                buy.at("price_series").as_array()[1].is_null() &&
                buy.at("latest_host_action").is_null(),
            "native missing condition or price clears condition while preserving price null");
    const auto& combo_buy = trade_ir.at("primitives").as_array()[1];
    const auto& combo_sell = trade_ir.at("primitives").as_array()[2];
    require(combo_buy.at("wrapper_action").as_number() == 0x10000 &&
                combo_buy.at("host_action_bits").as_number() == 0x1001 &&
                combo_buy.at("latest_host_action").at("index").as_number() == 2.0 &&
                combo_buy.at("latest_host_action").at("price").as_number() == 11.0 &&
                !combo_buy.at("latest_host_action")
                     .at("execution_side_effects").as_bool() &&
                combo_sell.at("wrapper_action").as_number() == 0x100000 &&
                combo_sell.at("host_action_bits").as_number() == 0x110,
            "combined trading wrappers expose native latest-bar action bits without orders");
    require(combo_sell.at("historical_signal_candidates").as_array()[0]
                    .at("projection").as_string() == "offline-per-bar" &&
                !combo_sell.at("historical_signal_candidates").as_array()[0]
                     .at("native_host_action").as_bool() &&
                combo_sell.at("historical_signal_candidates").size() == 3 &&
                combo_sell.at("historical_signal_candidates").as_array()[2]
                    .at("index").as_number() == 2.0 &&
                combo_sell.at("native_host_action_guard").as_string() ==
                    "latest condition approximately 1 (epsilon 1e-5)",
            "historical trade candidates are explicitly non-native projections");

    const auto trade_action_map = tdx::evaluate_formula_source_document(
        sample(1),
        "BUY(1,CLOSE);SELL(1,CLOSE);SELLSHORT(1,CLOSE);"
        "BUYSHORT(1,CLOSE);BUYSHORT_BUY(1,CLOSE);"
        "SELL_SELLSHORT(1,CLOSE);",
        {}, "TRADE_ACTION_MAP");
    const auto& mapped_primitives =
        trade_action_map.at("trade_event_ir").at("primitives").as_array();
    const std::array<std::tuple<const char*, std::uint32_t, std::uint32_t>, 6>
        expected_trade_actions{{
            {"BUY", 0x1, 0x1},
            {"SELL", 0x10, 0x10},
            {"SELLSHORT", 0x100, 0x100},
            {"BUYSHORT", 0x1000, 0x1000},
            {"BUYSHORT_BUY", 0x10000, 0x1001},
            {"SELL_SELLSHORT", 0x100000, 0x110},
        }};
    require(mapped_primitives.size() == expected_trade_actions.size(),
            "all six native trading-signal wrappers are materialized");
    for (std::size_t index = 0; index < expected_trade_actions.size(); ++index) {
        const auto& [function, wrapper_action, host_action_bits] =
            expected_trade_actions[index];
        const auto& primitive = mapped_primitives[index];
        require(primitive.at("function").as_string() == function &&
                    primitive.at("wrapper_action").as_number() == wrapper_action &&
                    primitive.at("host_action_bits").as_number() == host_action_bits &&
                    primitive.at("latest_host_action").at("host_action_bits")
                        .as_number() == host_action_bits,
                "trading-signal wrapper and native host action mapping");
    }

    const auto non_exact_host_action = tdx::evaluate_formula_source_document(
        sample(1), "BUY(2,CLOSE);", {}, "TRADE_FINAL_GUARD");
    require(point_value(non_exact_host_action, 0, "RESULT").as_number() == 2.0 &&
                non_exact_host_action.at("trade_event_ir")
                    .at("primitives").as_array()[0]
                    .at("latest_host_action").is_null() &&
                non_exact_host_action.at("trade_event_ir")
                    .at("primitives").as_array()[0]
                    .at("historical_signal_candidates").as_array()[0]
                    .at("condition").as_number() == 2.0,
            "condition copy is preserved while latest host action uses TCalc's near-one guard");
    bool trading_signal_arity_rejected = false;
    try {
        (void)tdx::evaluate_formula_source_document(
            sample(1), "BUY(1);", {}, "TRADE_ARITY");
    } catch (const tdx::Error&) {
        trading_signal_arity_rejected = true;
    }
    require(trading_signal_arity_rejected,
            "trading-signal functions require exactly condition and price");

    const auto autofilter_pairs = tdx::evaluate_formula_source_document(
        sample(6),
        "SELL(CURRBARSCOUNT=6,HIGH);"
        "BUY(CURRBARSCOUNT>=5,LOW);"
        "SELL(CURRBARSCOUNT=4,HIGH);"
        "SELLSHORT(CURRBARSCOUNT=3,HIGH);"
        "BUYSHORT(CURRBARSCOUNT=2,LOW);"
        "BUYSHORT(CURRBARSCOUNT=1,LOW);AUTOFILTER;",
        {}, "AUTOFILTER_PAIRS");
    const auto& pair_ir = autofilter_pairs.at("trade_event_ir");
    const auto& pair_filter = pair_ir.at("autofilter");
    const auto& pair_primitives = pair_ir.at("primitives").as_array();
    require(pair_filter.at("enabled").as_bool() &&
                pair_filter.at("marker_count").as_number() == 1.0 &&
                pair_filter.at("position_model").as_string() ==
                    "single-flat-long-short" &&
                pair_filter.at("traversal_order").as_string() ==
                    "bar-outer-source-statement-inner" &&
                !pair_filter.at("complete_native_action_set").as_bool() &&
                pair_filter.at("out_of_scope_functions").size() == 2 &&
                pair_filter.at("accepted_candidate_count").as_number() == 4.0 &&
                pair_filter.at("filtered_out_candidate_count").as_number() == 3.0 &&
                pair_filter.at("decision_count").as_number() == 7.0 &&
                pair_filter.at("position_change_count").as_number() == 4.0 &&
                pair_filter.at("final_position").as_string() == "flat" &&
                !pair_filter.at("execution_side_effects").as_bool(),
            "AUTOFILTER applies a read-only single-position paired-signal projection");
    const auto& pair_trace = pair_filter.at("decision_trace").as_array();
    require(pair_trace.size() == 7 &&
                pair_trace[0].at("function").as_string() == "SELL" &&
                !pair_trace[0].at("accepted").as_bool() &&
                pair_trace[0].at("rejection_reason").as_string() ==
                    "requires-long" &&
                pair_trace[0].at("position_before").as_string() == "flat" &&
                pair_trace[1].at("function").as_string() == "BUY" &&
                pair_trace[1].at("accepted").as_bool() &&
                pair_trace[1].at("position_after").as_string() == "long" &&
                pair_trace[2].at("rejection_reason").as_string() ==
                    "already-long" &&
                pair_trace[6].at("rejection_reason").as_string() ==
                    "requires-short" &&
                pair_filter.at("rejection_reason_counts")
                    .at("requires-long").as_number() == 1.0 &&
                pair_filter.at("rejection_reason_counts")
                    .at("already-long").as_number() == 1.0 &&
                pair_filter.at("rejection_reason_counts")
                    .at("requires-short").as_number() == 1.0 &&
                !pair_trace[6].at("execution_side_effects").as_bool(),
            "AUTOFILTER exposes an ordered read-only accept/reject decision trace");
    require(pair_primitives[0].at("historical_signal_candidates").size() == 1 &&
                pair_primitives[0].at("filtered_historical_signal_candidates").size() == 0 &&
                pair_primitives[1].at("historical_signal_candidates").size() == 2 &&
                pair_primitives[1].at("historical_signal_candidates")
                    .as_array()[0].at("projection").as_string() ==
                    "offline-per-bar" &&
                pair_primitives[1].at("filtered_historical_signal_candidates").size() == 1 &&
                pair_primitives[1].at("filtered_historical_signal_candidates")
                    .as_array()[0].at("projection").as_string() ==
                    "offline-per-bar-autofiltered" &&
                pair_primitives[1].at("filtered_historical_signal_candidates")
                    .as_array()[0].at("autofilter_position_before").as_string() == "flat" &&
                pair_primitives[1].at("filtered_historical_signal_candidates")
                    .as_array()[0].at("autofilter_position_after").as_string() == "long" &&
                pair_primitives[5].at("latest_host_action").is_object() &&
                pair_primitives[5].at("filtered_latest_host_action").is_null(),
            "AUTOFILTER preserves raw candidates while rejecting unpaired and repeated actions");

    const auto autofilter_statement_order =
        tdx::evaluate_formula_source_document(
            sample(1),
            "BUY(1,LOW);SELL(1,HIGH);BUY(1,LOW);AUTOFILTER;",
            {}, "AUTOFILTER_STATEMENT_ORDER");
    const auto& ordered_ir = autofilter_statement_order.at("trade_event_ir");
    const auto& ordered_primitives = ordered_ir.at("primitives").as_array();
    require(ordered_ir.at("autofilter").at("accepted_candidate_count").as_number() == 3.0 &&
                ordered_ir.at("autofilter").at("final_position").as_string() == "long" &&
                !ordered_primitives[0].at("filtered_latest_host_action").is_null() &&
                !ordered_primitives[1].at("filtered_latest_host_action").is_null() &&
                !ordered_primitives[2].at("filtered_latest_host_action").is_null(),
            "AUTOFILTER processes each bar in source statement order");

    const auto autofilter_composites =
        tdx::evaluate_formula_source_document(
            sample(3),
            "BUYSHORT_BUY(CURRBARSCOUNT=3,LOW);"
            "BUYSHORT_BUY(CURRBARSCOUNT=3,LOW);"
            "SELL_SELLSHORT(CURRBARSCOUNT=2,HIGH);"
            "BUYSHORT_BUY(CURRBARSCOUNT=1,LOW);AUTOFILTER;",
            {}, "AUTOFILTER_COMPOSITES");
    const auto& composite_ir = autofilter_composites.at("trade_event_ir");
    const auto& composite_primitives = composite_ir.at("primitives").as_array();
    require(composite_ir.at("autofilter").at("accepted_candidate_count").as_number() == 3.0 &&
                composite_ir.at("autofilter").at("filtered_out_candidate_count").as_number() == 1.0 &&
                composite_ir.at("autofilter").at("final_position").as_string() == "long" &&
                composite_primitives[0].at("filtered_historical_signal_candidates")
                    .as_array()[0].at("autofilter_effective_action").as_string() ==
                    "open-long-half-of-composite" &&
                composite_primitives[0].at("filtered_historical_signal_candidates")
                    .as_array()[0].at("autofilter_atomic_action").as_bool() &&
                composite_primitives[2].at("filtered_historical_signal_candidates")
                    .as_array()[0].at("autofilter_effective_action").as_string() ==
                    "close-long-and-open-short" &&
                composite_primitives[3].at("filtered_latest_host_action")
                    .at("autofilter_effective_action").as_string() ==
                    "close-short-and-open-long",
            "AUTOFILTER applies combined wrappers as atomic position transitions");
    const auto autofilter_flat_short =
        tdx::evaluate_formula_source_document(
            sample(1), "SELL_SELLSHORT(1,HIGH);AUTOFILTER;", {},
            "AUTOFILTER_FLAT_SHORT");
    require(autofilter_flat_short.at("trade_event_ir").at("primitives")
                .as_array()[0].at("filtered_historical_signal_candidates")
                .as_array()[0].at("autofilter_effective_action").as_string() ==
                "open-short-half-of-composite",
            "AUTOFILTER permits the opening half of a short composite while flat");

    const auto autofilter_latest_guard =
        tdx::evaluate_formula_source_document(
            sample(1), "BUY(2,CLOSE);AUTOFILTER;", {},
            "AUTOFILTER_LATEST_GUARD");
    require(autofilter_latest_guard.at("trade_event_ir").at("primitives")
                .as_array()[0].at("filtered_historical_signal_candidates").size() == 1 &&
                autofilter_latest_guard.at("trade_event_ir").at("primitives")
                .as_array()[0].at("filtered_latest_host_action").is_null(),
            "AUTOFILTER historical projection does not weaken the native latest near-one guard");
    const auto non_marker_autofilter =
        tdx::evaluate_formula_source_document(
            sample(1), "BUY(1,CLOSE);X:AUTOFILTER+0;", {},
            "AUTOFILTER_EXACT_MARKER");
    require(!non_marker_autofilter.at("trade_event_ir").at("autofilter")
                .at("enabled").as_bool() &&
                non_marker_autofilter.at("trade_event_ir").at("autofilter")
                    .at("decision_trace").size() == 0 &&
                non_marker_autofilter.at("trade_event_ir").at("primitives")
                .as_array()[0].at("historical_signal_candidates").size() == 1,
            "AUTOFILTER requires an exact top-level marker expression");

    const auto enriched_mindiff = [](std::uint8_t decimal) {
        auto kline = sample(2);
        const auto original_name = kline.at("name").as_string();
        tdx::SecurityCatalog catalog;
        tdx::Security security;
        security.market_id = 0;
        security.code = "000001";
        security.name = "LOCAL-NAME";
        security.price_precision = decimal;
        catalog[{0, security.code}] = std::move(security);
        require(tdx::enrich_security_metadata(
                    catalog, kline, "sz", "000001") &&
                    kline.at("name").as_string() == original_name,
                "TNF precision is attached even when K-line name already exists");
        return tdx::evaluate_formula_source_document(
            std::move(kline), "TICK:MINDIFF;", {}, "TNFMINDIFF");
    };
    for (const auto& [decimal, expected] :
         std::array<std::pair<std::uint8_t, double>, 2>{{
             {3, 0.001}, {4, 0.0001}}}) {
        const auto result = enriched_mindiff(decimal);
        require(std::abs(point_value(result, 1, "TICK").as_number() -
                         expected) < 1e-8,
                "MINDIFF consumes cached TNF decimal precision");
    }

    auto explicit_precision_sample = sample(2);
    explicit_precision_sample["price_precision"] = 4;
    explicit_precision_sample["min_tick"] = 0.2;
    tdx::SecurityCatalog conflicting_catalog;
    tdx::Security conflicting_security;
    conflicting_security.market_id = 0;
    conflicting_security.code = "000001";
    conflicting_security.price_precision = 3;
    conflicting_catalog[{0, conflicting_security.code}] =
        std::move(conflicting_security);
    (void)tdx::enrich_security_metadata(
        conflicting_catalog, explicit_precision_sample, "sz", "000001");
    const auto explicit_catalog_mindiff =
        tdx::evaluate_formula_source_document(
            std::move(explicit_precision_sample), "TICK:MINDIFF;", {},
            "TNFMINDIFFEXPLICIT");
    require(std::abs(point_value(explicit_catalog_mindiff, 0, "TICK").as_number() -
                     0.2) < 1e-7,
            "explicit min_tick remains ahead of explicit precision and TNF metadata");

    auto missing_catalog_sample = sample(1);
    tdx::SecurityCatalog empty_catalog;
    require(!tdx::enrich_security_metadata(
                empty_catalog, missing_catalog_sample, "sz", "000001"),
            "empty in-memory catalog does not perform any external lookup");
    const auto fallback_mindiff = tdx::evaluate_formula_source_document(
        std::move(missing_catalog_sample), "TICK:MINDIFF;", {},
        "MINDIFFFALLBACK");
    require(std::abs(point_value(fallback_mindiff, 0, "TICK").as_number() -
                     0.01) < 1e-7,
            "absent local precision preserves the established 0.01 fallback");

    auto precision_sample = sample(2);
    precision_sample["price_precision"] = 3;
    const auto precision_mindiff = tdx::evaluate_formula_source_document(
        std::move(precision_sample), "TICK:MINDIFF;", {}, "MINDIFFPRECISION");
    require(std::abs(point_value(precision_mindiff, 0, "TICK").as_number() - 0.001) <
                1e-7,
            "MINDIFF derives the explicit decimal precision when no tick is supplied");
    auto tick_sample = sample(2);
    tick_sample["price_precision"] = 3;
    tick_sample["min_tick"] = 0.2;
    const auto explicit_mindiff = tdx::evaluate_formula_source_document(
        std::move(tick_sample), "TICK:MINDIFF;", {}, "MINDIFFEXPLICIT");
    require(std::abs(point_value(explicit_mindiff, 1, "TICK").as_number() - 0.2) <
                1e-7,
            "an explicit minimum tick takes precedence over decimal precision");
    auto tiny_tick_sample = sample(1);
    tiny_tick_sample["min_tick"] = 0.000001;
    const auto floored_mindiff = tdx::evaluate_formula_source_document(
        std::move(tiny_tick_sample), "TICK:MINDIFF;", {}, "MINDIFFFLOOR");
    require(std::abs(point_value(floored_mindiff, 0, "TICK").as_number() - 0.00001) <
                1e-9,
            "MINDIFF preserves TCalc's 1e-5 native floor");

    const auto lowercase_mtm = tdx::evaluate_formula_source_document(
        sample(5), "PARAM:N;VALUE:MTM;", {{"n", 2}}, "MTMLOWERCASE");
    const auto uppercase_mtm = tdx::evaluate_formula_source_document(
        sample(5), "PARAM:N;VALUE:MTM;", {{"N", 2}}, "MTMUPPERCASE");
    for (std::size_t index = 0; index < 5; ++index) {
        require(point_value(lowercase_mtm, index, "PARAM").as_number() == 2.0,
                "lowercase parameter is bound through the canonical symbol name");
        const auto& lowercase_value = point_value(lowercase_mtm, index, "VALUE");
        const auto& uppercase_value = point_value(uppercase_mtm, index, "VALUE");
        require(lowercase_value.is_null() == uppercase_value.is_null() &&
                    (lowercase_value.is_null() ||
                     lowercase_value.as_number() == uppercase_value.as_number()),
                "bare MTM uses case-insensitive N parameter semantics");
    }
    require(point_value(lowercase_mtm, 0, "VALUE").is_null() &&
                point_value(lowercase_mtm, 1, "VALUE").is_null() &&
                point_value(lowercase_mtm, 2, "VALUE").as_number() == 2.0,
            "bare MTM honors the lowercase two-bar selector");

    auto from_open_sample = sample(10);
    from_open_sample["period"] = "1m";
    const std::array<std::string, 10> from_open_times{
        "09:24", "09:25", "09:29", "09:30", "09:31",
        "11:30", "12:00", "13:00", "13:01", "15:00"};
    for (std::size_t index = 0; index < from_open_times.size(); ++index) {
        from_open_sample["bars"].as_array()[index]["date"] =
            "2026-01-" + std::string(index + 1 < 10 ? "0" : "") +
            std::to_string(index + 1);
        from_open_sample["bars"].as_array()[index]["time"] = from_open_times[index];
    }
    const auto from_open = tdx::evaluate_formula_source_document(
        std::move(from_open_sample), "FO:FROMOPEN;", {}, "FROMOPENSESSIONS");
    const std::array<double, 10> expected_from_open{
        0, 1, 1, 1, 2, 120, 120, 121, 122, 240};
    for (std::size_t index = 0; index < expected_from_open.size(); ++index)
        require(point_value(from_open, index, "FO").as_number() ==
                    expected_from_open[index],
                "FROMOPEN follows TCalc's default A-share session-minute helper");

    auto cross_day_sample = sample(12);
    cross_day_sample["period"] = "1m";
    cross_day_sample["trading_sessions"] = tdx::Json::parse(R"json([
        {"start":"21:00","end":"02:30"},
        {"start":"09:00","end":"10:15"},
        {"start":"10:30","end":"11:30"},
        {"start":"13:30","end":"15:00"}
    ])json");
    const std::array<std::string, 12> cross_day_times{
        "20:54", "20:55", "21:00", "23:00", "01:00", "02:30",
        "08:59", "09:00", "10:15", "10:30", "13:30", "15:00"};
    for (std::size_t index = 0; index < cross_day_times.size(); ++index) {
        cross_day_sample["bars"].as_array()[index]["date"] =
            "2026-02-" + std::string(index + 1 < 10 ? "0" : "") +
            std::to_string(index + 1);
        cross_day_sample["bars"].as_array()[index]["time"] = cross_day_times[index];
    }
    const auto cross_day = tdx::evaluate_formula_source_document(
        std::move(cross_day_sample), "FO:FROMOPEN;TOTAL:TOTALFZNUM;", {},
        "EXPLICITSESSIONS");
    const std::array<double, 12> expected_cross_day{
        0, 1, 1, 121, 241, 330, 330, 331, 405, 406, 466, 555};
    for (std::size_t index = 0; index < expected_cross_day.size(); ++index) {
        const double actual_from_open =
            point_value(cross_day, index, "FO").as_number();
        require(actual_from_open == expected_cross_day[index],
                "explicit cross-day FROMOPEN at bar " + std::to_string(index) +
                    " expected " + std::to_string(expected_cross_day[index]) +
                    ", got " + std::to_string(actual_from_open));
        require(point_value(cross_day, index, "TOTAL").as_number() == 555.0,
                "explicit cross-day sessions drive TOTALFZNUM");
    }

    const auto require_invalid_sessions = [](const char* text) {
        auto document = sample(1);
        document["trading_sessions"] = tdx::Json::parse(text);
        bool rejected = false;
        try {
            (void)tdx::evaluate_formula_source_document(
                std::move(document), "FO:FROMOPEN;", {}, "INVALIDSESSIONS");
        } catch (const tdx::Error&) {
            rejected = true;
        }
        require(rejected, "malformed explicit trading sessions are rejected");
    };
    require_invalid_sessions("[]");
    require_invalid_sessions(R"json([
        {"start":"09:00","end":"10:00"},
        {"start":"10:00","end":"11:00"},
        {"start":"11:00","end":"12:00"},
        {"start":"13:00","end":"14:00"},
        {"start":"14:00","end":"15:00"}
    ])json");
    require_invalid_sessions(R"([{"start":"24:00","end":"10:00"}])");
    require_invalid_sessions(
        R"([{"start":"09:00","end":"11:00"},{"start":"10:00","end":"12:00"}])");

    const auto auxiliary = tdx::evaluate_formula_source_document(
        expansion_sample(), "Z:ZSTJJ;Q:QHJSJ;", {}, "AUXILIARYFIELDS");
    require(point_value(auxiliary, 0, "Z").as_number() == 3508.0 &&
                point_value(auxiliary, 0, "Q").as_number() == 3508.0 &&
                point_value(auxiliary, 1, "Z").as_number() == 3509.0 &&
                point_value(auxiliary, 1, "Q").as_number() == 3509.0,
            "ZSTJJ and QHJSJ copy the shared K-line auxiliary float");
    const auto average = tdx::evaluate_formula_source_document(
        time_average_sample(), "Z:ZSTJJ;Q:QHJSJ;", {}, "TIMEAVERAGE");
    require(point_value(average, 0, "Z").as_number() == 10.0 &&
                point_value(average, 1, "Z").as_number() ==
                    static_cast<double>(static_cast<float>(3400.0 / 300.0)) &&
                point_value(average, 2, "Z").as_number() == 12.25 &&
                point_value(average, 2, "Q").as_number() ==
                    point_value(average, 2, "Z").as_number(),
            "ordinary time-chart auxiliary field is reconstructed as cumulative VWAP");
    const auto auxiliary_analysis = tdx::analyze_formula_source("Z:ZSTJJ;Q:QHJSJ;");
    require(auxiliary_analysis.at("syntax_supported").as_bool() &&
                auxiliary_analysis.at("executable").as_bool() &&
                !auxiliary_analysis.at("has_external_dependency").as_bool(),
            "K-line auxiliary symbols are automatic market data");

    const auto clock_before = std::time(nullptr);
    const auto machine_clock = tdx::evaluate_formula_source_document(
        sample(4),
        "MD:MACHINEDATE;MT:MACHINETIME;MW:MACHINEWEEK;"
        "MDF:MACHINEDATE();MTF:MACHINETIME();MWF:MACHINEWEEK();",
        {}, "MACHINECLOCKTEST");
    const auto clock_after = std::time(nullptr);
    const auto local_snapshot = [](std::time_t value) {
        std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &value);
#else
        localtime_r(&value, &local);
#endif
        return local;
    };
    const auto before_local = local_snapshot(clock_before);
    const auto after_local = local_snapshot(clock_after);
    const auto date_code = [](const std::tm &local) {
        return local.tm_year * 10000 + (local.tm_mon + 1) * 100 + local.tm_mday;
    };
    const auto seconds_of_day = [](int value) {
        return (value / 10000) * 3600 + ((value / 100) % 100) * 60 + value % 100;
    };
    const int machine_date = static_cast<int>(point_value(machine_clock, 0, "MD").as_number());
    const int machine_time = static_cast<int>(point_value(machine_clock, 0, "MT").as_number());
    const int machine_week = static_cast<int>(point_value(machine_clock, 0, "MW").as_number());
    const int machine_hour = machine_time / 10000;
    const int machine_minute = (machine_time / 100) % 100;
    const int machine_second = machine_time % 100;
    const auto cyclic_distance = [](int left, int right) {
        const int direct = std::abs(left - right);
        return std::min(direct, 86400 - direct);
    };
    const int machine_seconds = seconds_of_day(machine_time);
    const int before_seconds =
        before_local.tm_hour * 3600 + before_local.tm_min * 60 + before_local.tm_sec;
    const int after_seconds =
        after_local.tm_hour * 3600 + after_local.tm_min * 60 + after_local.tm_sec;
    require((machine_date == date_code(before_local) || machine_date == date_code(after_local)) &&
                machine_hour >= 0 && machine_hour <= 23 && machine_minute >= 0 &&
                machine_minute <= 59 && machine_second >= 0 && machine_second <= 59 &&
                std::min(cyclic_distance(machine_seconds, before_seconds),
                         cyclic_distance(machine_seconds, after_seconds)) <= 2 &&
                (machine_week == before_local.tm_wday || machine_week == after_local.tm_wday),
            "machine clock matches one native local-time snapshot");
    for (std::size_t index = 0; index < 4; ++index)
        require(point_value(machine_clock, index, "MD").as_number() == machine_date &&
                    point_value(machine_clock, index, "MT").as_number() == machine_time &&
                    point_value(machine_clock, index, "MW").as_number() == machine_week &&
                    point_value(machine_clock, index, "MDF").as_number() == machine_date &&
                    point_value(machine_clock, index, "MTF").as_number() == machine_time &&
                    point_value(machine_clock, index, "MWF").as_number() == machine_week,
                "machine clock bare symbols and zero-argument calls broadcast exactly");
    const auto machine_clock_analysis =
        tdx::analyze_formula_source("D:MACHINEDATE;T:MACHINETIME();W:MACHINEWEEK;");
    require(machine_clock_analysis.at("executable").as_bool() &&
                !machine_clock_analysis.at("pure_ohlcv").as_bool() &&
                machine_clock_analysis.at("has_machine_clock_dependency").as_bool() &&
                !machine_clock_analysis.at("has_external_dependency").as_bool(),
            "machine clock is executable without market context but not pure OHLCV");

    tdx::Json random_context = tdx::Json::object();
    random_context["formula_random_seed"] = 1;
    const auto random = tdx::evaluate_formula_source_document(
        sample(4), "R:RAND(10);S:RAND(10);ONE:RAND(1);BAD:RAND(0);", {}, "RANDCORE",
        &random_context);
    const std::array<double, 4> expected_random{2, 8, 5, 1};
    for (std::size_t index = 0; index < expected_random.size(); ++index)
        require(point_value(random, index, "R").as_number() == expected_random[index] &&
                    point_value(random, index, "S").as_number() == expected_random[index] &&
                    point_value(random, index, "ONE").as_number() == 1.0 &&
                    point_value(random, index, "BAD").is_null(),
                "RAND reproduces the MSVC CRT sequence and resets per call");
    require(random.at("random").at("seed").as_number() == 1.0 &&
                random.at("random").at("seed_mode").as_string() == "explicit-context" &&
                random.at("random").at("algorithm").as_string() == "msvc-crt-rand-lcg-v1",
            "RAND reports the replay seed and exact generator identity");
    const auto random_analysis = tdx::analyze_formula_source("R:RAND(10);");
    require(random_analysis.at("executable").as_bool() &&
                !random_analysis.at("pure_ohlcv").as_bool() &&
                random_analysis.at("has_random_dependency").as_bool() &&
                random_analysis.at("random_seed_bindable").as_bool() &&
                !random_analysis.at("has_external_dependency").as_bool() &&
                !random_analysis.at("has_future_function").as_bool(),
            "RAND is local and executable but explicitly nondeterministic");

    const auto raw_adjustment =
        tdx::evaluate_formula_source_document(sample(4), "T:TQFLAG;TF:TQFLAG();", {}, "TQFLAGRAW");
    auto qfq_sample = sample(4);
    qfq_sample["adjustment_mode"] = "qfq";
    qfq_sample["adjustment"] = tdx::Json::object();
    qfq_sample["adjustment"]["mode"] = "qfq";
    qfq_sample["adjustment"]["method"] = "local-corporate-action-factor-v1";
    const auto qfq_adjustment =
        tdx::evaluate_formula_source_document(qfq_sample, "T:TQFLAG;TF:TQFLAG();", {}, "TQFLAGQFQ");
    auto fixed_hfq_sample = sample(4);
    fixed_hfq_sample["adjustment_mode"] = "fixed_hfq";
    const auto fixed_hfq_adjustment = tdx::evaluate_formula_source_document(
        fixed_hfq_sample, "T:TQFLAG;TF:TQFLAG();", {}, "TQFLAGHFQ");
    auto numeric_adjustment_sample = sample(4);
    numeric_adjustment_sample["adjustment_mode"] = 1;
    const auto numeric_adjustment = tdx::evaluate_formula_source_document(
        numeric_adjustment_sample, "T:TQFLAG;", {}, "TQFLAGNUMERIC");
    for (std::size_t index = 0; index < 4; ++index) {
        require(point_value(raw_adjustment, index, "T").as_number() == 0.0 &&
                    point_value(raw_adjustment, index, "TF").as_number() == 0.0,
                "missing adjustment metadata maps TQFLAG to raw mode 0");
        require(point_value(qfq_adjustment, index, "T").as_number() == 1.0 &&
                    point_value(qfq_adjustment, index, "TF").as_number() == 1.0 &&
                    point_value(numeric_adjustment, index, "T").as_number() == 1.0,
                "qfq and numeric adjustment metadata map TQFLAG to mode 1");
        require(point_value(fixed_hfq_adjustment, index, "T").as_number() == 2.0 &&
                    point_value(fixed_hfq_adjustment, index, "TF").as_number() == 2.0,
                "fixed_hfq adjustment metadata maps TQFLAG to mode 2");
    }
    const auto adjustment_analysis = tdx::analyze_formula_source("T:TQFLAG;TF:TQFLAG();");
    require(qfq_adjustment.at("adjustment_mode").as_string() == "qfq" &&
                qfq_adjustment.at("adjustment").at("mode").as_string() == "qfq" &&
                qfq_adjustment.at("adjustment").at("method").as_string() ==
                    "local-corporate-action-factor-v1",
            "formula result preserves K-line adjustment metadata");
    require(adjustment_analysis.at("executable").as_bool() &&
                !adjustment_analysis.at("pure_ohlcv").as_bool() &&
                adjustment_analysis.at("has_adjustment_mode_dependency").as_bool() &&
                !adjustment_analysis.at("has_external_dependency").as_bool() &&
                !adjustment_analysis.at("has_future_function").as_bool(),
            "TQFLAG is an executable adjustment-context dependency");

    const auto selection = tdx::evaluate_formula_source_document(
        sample(30), "MID:=MA(CLOSE,N); CROSS(CLOSE,MID);", {{"N", 5}}, "SELECT");
    require(selection.at("outputs").as_array().front().as_string() == "RESULT",
            "bare selection output");
    auto reference_sample = sample(4);
    auto& reference_bars = reference_sample["bars"].as_array();
    const double reference_values[]{10.0, 20.0, 30.0, 40.0};
    for (std::size_t i = 0; i < reference_bars.size(); ++i)
        reference_bars[reference_bars.size() - 1 - i]["close"] =
            reference_values[i];
    const auto reference_boundary = tdx::evaluate_formula_source_document(
        reference_sample,
        "R:REF(CLOSE,2);Z:REF(CLOSE,0);"
        "D:REF(CLOSE,IF(CURRBARSCOUNT>=3,0,10));"
        "N:REF(REF(CLOSE,1),1);"
        "SRC:=IF(CURRBARSCOUNT=2,DRAWNULL,CLOSE);"
        "OFF:=IF(CURRBARSCOUNT=1,DRAWNULL,2);L:REF(SRC,OFF);"
        "B:REF(CLOSE,BARSCOUNT(CLOSE)+1);",
        {}, "REFBOUNDARY");
    require(point_value(reference_boundary, 0, "R").is_null() &&
                point_value(reference_boundary, 1, "R").is_null() &&
                point_value(reference_boundary, 2, "R").as_number() == 10.0 &&
                point_value(reference_boundary, 3, "R").as_number() == 20.0,
            "REF preserves native missing values before fixed history exists");
    for (std::size_t i = 0; i < reference_bars.size(); ++i)
        require(point_value(reference_boundary, i, "Z").as_number() ==
                    reference_values[i],
                "REF zero offset is the identity series");
    require(point_value(reference_boundary, 0, "D").as_number() == 10.0 &&
                point_value(reference_boundary, 1, "D").as_number() == 20.0 &&
                point_value(reference_boundary, 2, "D").as_number() == 20.0 &&
                point_value(reference_boundary, 3, "D").as_number() == 20.0,
            "REF dynamic left overflow retains the immediately preceding output");
    require(point_value(reference_boundary, 0, "N").is_null() &&
                point_value(reference_boundary, 1, "N").is_null() &&
                point_value(reference_boundary, 2, "N").as_number() == 10.0 &&
                point_value(reference_boundary, 3, "N").as_number() == 20.0,
            "nested REF preserves each layer's native missing history");
    require(point_value(reference_boundary, 0, "L").is_null() &&
                point_value(reference_boundary, 1, "L").is_null() &&
                point_value(reference_boundary, 2, "L").as_number() == 10.0 &&
                point_value(reference_boundary, 3, "L").as_number() == 10.0,
            "started REF can read past a missing current source and carries across a later missing offset");
    for (std::size_t i = 0; i < reference_bars.size(); ++i)
        require(point_value(reference_boundary, i, "B").is_null(),
                "REF varying period keeps missing when every request crosses the left boundary");

    const auto native_division = tdx::evaluate_formula_source_document(
        sample(4),
        "DEN:=IF(CURRBARSCOUNT=4,2,IF(CURRBARSCOUNT=3,0.000005,"
        "IF(CURRBARSCOUNT=2,-0.00002,0)));Q:10/DEN;"
        "MD:=IF(CURRBARSCOUNT=4,0,IF(CURRBARSCOUNT=3,2,"
        "IF(CURRBARSCOUNT=2,DRAWNULL,0)));QM:10/MD;"
        "NUM:=IF(CURRBARSCOUNT=2,DRAWNULL,10);QN:NUM/2;",
        {}, "NATIVEDIVISION");
    require(std::abs(point_value(native_division, 0, "Q").as_number() - 5.0) < 1e-6 &&
                std::abs(point_value(native_division, 1, "Q").as_number() - 5.0) < 1e-6 &&
                std::abs(point_value(native_division, 2, "Q").as_number() + 500000.0) < 1e-3 &&
                std::abs(point_value(native_division, 3, "Q").as_number() + 500000.0) < 1e-3,
            "division carries the previous output across native near-zero denominators");
    require(point_value(native_division, 0, "QM").is_null() &&
                point_value(native_division, 1, "QM").as_number() == 5.0 &&
                point_value(native_division, 2, "QM").is_null() &&
                point_value(native_division, 3, "QM").is_null() &&
                point_value(native_division, 0, "QN").as_number() == 5.0 &&
                point_value(native_division, 1, "QN").as_number() == 5.0 &&
                point_value(native_division, 2, "QN").is_null() &&
                point_value(native_division, 3, "QN").as_number() == 5.0,
            "division validates either missing operand before applying near-zero carry");

    const auto native_comparison = tdx::evaluate_formula_source_document(
        sample(1),
        "LT0:100<100.000005;LE0:100<=100.000005;"
        "GT0:100>99.999995;GE0:100>=99.999995;"
        "LT1:100<100.00003;LE1:100<=99.99997;"
        "GT1:100>99.99997;GE1:100>=100.00003;"
        "M0:DRAWNULL<1;M1:DRAWNULL<=1;M2:1>DRAWNULL;M3:1>=DRAWNULL;",
        {}, "NATIVECOMPARISON");
    require(point_value(native_comparison, 0, "LT0").as_number() == 0.0 &&
                point_value(native_comparison, 0, "LE0").as_number() == 1.0 &&
                point_value(native_comparison, 0, "GT0").as_number() == 0.0 &&
                point_value(native_comparison, 0, "GE0").as_number() == 1.0 &&
                point_value(native_comparison, 0, "LT1").as_number() == 1.0 &&
                point_value(native_comparison, 0, "LE1").as_number() == 0.0 &&
                point_value(native_comparison, 0, "GT1").as_number() == 1.0 &&
                point_value(native_comparison, 0, "GE1").as_number() == 0.0,
            "relational operators use TCalc float narrowing and left-anchored tolerance");
    require(point_value(native_comparison, 0, "M0").is_null() &&
                point_value(native_comparison, 0, "M1").is_null() &&
                point_value(native_comparison, 0, "M2").is_null() &&
                point_value(native_comparison, 0, "M3").is_null(),
            "tolerant relational operators preserve native missing inputs");

    auto range_sample = sample(6);
    auto &range_bars = range_sample["bars"].as_array();
    const double range_values[]{10.0, 11.0, 12.0, 11.0, 10.999995, 13.0};
    for (std::size_t i = 0; i < range_bars.size(); ++i)
        range_bars[range_bars.size() - 1 - i]["close"] = range_values[i];
    const auto ranges = tdx::evaluate_formula_source_document(
        range_sample, "UP:TOPRANGE(CLOSE); DOWN:LOWRANGE(CLOSE);", {}, "RANGETEST");
    require(point_value(ranges, 2, "UP").as_number() == 2.0,
            "TOPRANGE counts strictly lower preceding bars");
    require(point_value(ranges, 3, "DOWN").as_number() == 1.0, "LOWRANGE stops at equal value");
    require(point_value(ranges, 4, "DOWN").as_number() == 0.0,
            "LOWRANGE honors the native 1e-5 tolerance");
    require(point_value(ranges, 5, "UP").as_number() == 5.0,
            "TOPRANGE extends to the beginning of the valid series");

    auto future_sample = sample(6);
    auto &future_bars = future_sample["bars"].as_array();
    const double future_values[]{10.0, 10.0, 10.0, 11.0, 10.0, 10.0};
    for (std::size_t i = 0; i < future_bars.size(); ++i)
        future_bars[future_bars.size() - 1 - i]["close"] = future_values[i];
    const auto backset = tdx::evaluate_formula_source_document(
        future_sample, "B:BACKSET(CLOSE>REF(CLOSE,1),3);", {}, "BACKSETTEST");
    require(point_value(backset, 0, "B").is_null() &&
                point_value(backset, 1, "B").as_number() == 1.0 &&
                point_value(backset, 2, "B").as_number() == 1.0 &&
                point_value(backset, 3, "B").as_number() == 1.0 &&
                point_value(backset, 4, "B").as_number() == 0.0,
            "BACKSET preserves its leading missing bar and rewrites exactly N bars ending at each true condition");
    const auto backset_analysis = tdx::analyze_formula_source("B:BACKSET(CLOSE>REF(CLOSE,1),3);");
    require(backset_analysis.at("has_future_function").as_bool() &&
                backset_analysis.at("read_only_future_executable").as_bool(),
            "BACKSET is explicitly available for read-only future rendering");
    require(!backset_analysis.at("executable").as_bool() &&
                !backset_analysis.at("executable_with_context").as_bool(),
            "BACKSET remains unavailable to normal execution, scans and backtests");
    const auto future_reference = tdx::evaluate_formula_source_document(
        future_sample, "S:REFX(CLOSE,2); V:REFXV(CLOSE,2);", {}, "REFXTEST");
    require(point_value(future_reference, 0, "S").as_number() == future_values[2] &&
                point_value(future_reference, 0, "V").as_number() == future_values[2],
            "REFX and REFXV read the requested future bar");
    require(point_value(future_reference, 5, "S").is_null() &&
                point_value(future_reference, 5, "V").as_number() ==
                    future_values[5],
            "REFX emits missing while REFXV carries its previous raw output "
            "at the right boundary");
    const auto polyline = tdx::evaluate_formula_source_document(
        future_sample, "P:PLOYLINE(CURRBARSCOUNT=6 OR CURRBARSCOUNT=3,CLOSE);", {}, "PLOYLINETEST");
    require(point_value(polyline, 1, "P").is_number() &&
                std::abs(point_value(polyline, 1, "P").as_number() - (10.0 + 1.0 / 3.0)) < 1e-9 &&
                point_value(polyline, 3, "P").as_number() == 11.0,
            "PLOYLINE interpolates between condition vertices");
    const auto &polyline_primitive = polyline.at("render_ir").at("primitives").as_array().front();
    const auto &polyline_events = polyline_primitive.at("events").as_array();
    require(polyline_primitive.at("kind").as_string() == "polyline" &&
                polyline_primitive.at("segment_coordinate_space").as_string() == "bar-price" &&
                polyline_primitive.at("polyline_vertex_rule").as_string() == "condition-true" &&
                polyline_events.size() == 2 &&
                polyline_events[0].at("segment_from_index").is_null() &&
                polyline_events[1].at("segment_from_index").as_number() == 0.0 &&
                polyline_events[1].at("segment_to_index").as_number() == 3.0 &&
                polyline_events[1].at("segment_from_price").as_number() == 10.0 &&
                polyline_events[1].at("segment_to_price").as_number() == 11.0,
            "PLOYLINE render IR preserves vertices and the exact connecting segment");
    const auto drawline = tdx::evaluate_formula_source_document(
        future_sample, "D:DRAWLINE(CURRBARSCOUNT=6,CLOSE,CURRBARSCOUNT=3,CLOSE,1);", {},
        "DRAWLINETEST");
    require(std::abs(point_value(drawline, 2, "D").as_number() - (10.0 + 2.0 / 3.0)) < 1e-9 &&
                std::abs(point_value(drawline, 5, "D").as_number() - (11.0 + 2.0 / 3.0)) < 1e-9,
            "DRAWLINE connects its two anchors and extends their slope when requested");
    const auto &drawline_primitive = drawline.at("render_ir").at("primitives").as_array().front();
    const auto &drawline_event = drawline_primitive.at("events").as_array().front();
    require(drawline_primitive.at("kind").as_string() == "draw-line" &&
                drawline_primitive.at("line_expand_argument").as_number() == 4.0 &&
                drawline_event.at("segment_from_index").as_number() == 0.0 &&
                drawline_event.at("segment_anchor_to_index").as_number() == 3.0 &&
                drawline_event.at("segment_to_index").as_number() == 5.0 &&
                drawline_event.at("segment_expansion").as_string() == "right" &&
                std::abs(drawline_event.at("segment_to_price").as_number() - (11.0 + 2.0 / 3.0)) <
                    1e-9,
            "DRAWLINE render IR preserves anchors, slope and right extension endpoint");
    const auto anonymous_drawline = tdx::evaluate_formula_source_document(
        future_sample, "DRAWLINE(CURRBARSCOUNT=6,CLOSE,CURRBARSCOUNT=3,CLOSE,0);", {},
        "ANONYMOUSDRAWLINE");
    require(anonymous_drawline.at("render_ir").at("primitive_count").as_number() == 1.0 &&
                anonymous_drawline.at("render_ir")
                        .at("primitives")
                        .as_array()
                        .front()
                        .at("event_count")
                        .as_number() == 1.0,
            "an unnamed DRAWLINE statement remains visible in render IR");
    const auto barsnext = tdx::evaluate_formula_source_document(
        future_sample, "N:BARSNEXT(CURRBARSCOUNT=3);", {}, "BARSNEXTTEST");
    require(point_value(barsnext, 0, "N").as_number() == 3.0 &&
                point_value(barsnext, 3, "N").as_number() == 0.0 &&
                point_value(barsnext, 4, "N").is_null(),
            "BARSNEXT counts forward and remains invalid when no future match exists");
    for (std::size_t i = 0; i < future_bars.size(); ++i) {
        auto &bar = future_bars[future_bars.size() - 1 - i];
        bar["open"] = 9.5;
        bar["close"] = 9.5;
        bar["high"] = 10.0;
        bar["low"] = 9.0;
    }
    future_bars[future_bars.size() - 1]["high"] = 11.0;
    future_bars[future_bars.size() - 1]["low"] = 8.0;
    future_bars[future_bars.size() - 3]["high"] = 11.0;
    future_bars[future_bars.size() - 3]["low"] = 8.0;
    const auto included = tdx::evaluate_formula_source_document(
        future_sample, "P:INCLUDED(0,1); F:INCLUDEDV(0,1);", {}, "INCLUDEDTEST");
    require(point_value(included, 1, "P").as_number() == 1.0 &&
                point_value(included, 1, "F").as_number() == 1.0,
            "INCLUDED and INCLUDEDV test containment in the documented direction");
    auto zig_sample = sample(7);
    auto &zig_bars = zig_sample["bars"].as_array();
    const double zig_values[]{10.0, 11.0, 12.0, 11.5, 10.5, 9.0, 10.0};
    for (std::size_t i = 0; i < zig_bars.size(); ++i)
        zig_bars[zig_bars.size() - 1 - i]["close"] = zig_values[i];
    const auto zig =
        tdx::evaluate_formula_source_document(zig_sample, "Z:ZIG(CLOSE,10);", {}, "ZIGTEST");
    require(point_value(zig, 0, "Z").as_number() == 10.0 &&
                point_value(zig, 2, "Z").as_number() == 12.0 &&
                point_value(zig, 5, "Z").as_number() == 9.0 &&
                point_value(zig, 6, "Z").as_number() == 10.0,
            "ZIG follows confirmed extrema and repaints the current leg to the final bar");
    const auto ziga = tdx::evaluate_formula_source_document(
        zig_sample, "Z:ZIGA(CLOSE,2); S:ZIGA(3,2);", {}, "ZIGATEST");
    const double expected_ziga[]{10.0, 11.0, 12.0, 11.0, 10.0, 9.0, 10.0};
    for (std::size_t index = 0; index < std::size(expected_ziga); ++index) {
        require(std::abs(point_value(ziga, index, "Z").as_number() - expected_ziga[index]) < 1e-6 &&
                    std::abs(point_value(ziga, index, "S").as_number() - expected_ziga[index]) <
                        1e-6,
                "ZIGA explicit series and CLOSE selector match the native absolute Zig vector");
    }
    auto ziga_tail_sample = sample(7);
    auto &ziga_tail_bars = ziga_tail_sample["bars"].as_array();
    const double ziga_tail_values[]{10.0, 15.0, 10.0, 12.0, 9.0, 8.0, 7.0};
    for (std::size_t index = 0; index < ziga_tail_bars.size(); ++index)
        ziga_tail_bars[ziga_tail_bars.size() - 1 - index]["close"] = ziga_tail_values[index];
    const auto ziga_tail =
        tdx::evaluate_formula_source_document(ziga_tail_sample, "Z:ZIGA(CLOSE,4);", {}, "ZIGATAIL");
    const double expected_ziga_tail[]{10.0, 15.0, 10.0, 9.25, 8.5, 7.75, 7.0};
    for (std::size_t index = 0; index < std::size(expected_ziga_tail); ++index)
        require(std::abs(point_value(ziga_tail, index, "Z").as_number() -
                         expected_ziga_tail[index]) < 1e-6,
                "ZIGA preserves an unconfirmed local-extremum candidate in the final leg");
    const auto ziga_invalid = tdx::evaluate_formula_source_document(
        zig_sample, "Z0:ZIGA(CLOSE,0); ZN:ZIGA(CLOSE,-1);", {}, "ZIGAINVALID");
    require(point_value(ziga_invalid, 0, "Z0").as_number() == 0.0 &&
                point_value(ziga_invalid, 6, "Z0").as_number() == 0.0 &&
                point_value(ziga_invalid, 0, "ZN").as_number() == 0.0 &&
                point_value(ziga_invalid, 6, "ZN").as_number() == 0.0,
            "ZIGA keeps the DLL zero-vector boundary for non-positive thresholds");
    const auto ziga_analysis = tdx::analyze_formula_source("Z:ZIGA(CLOSE,2);");
    require(ziga_analysis.at("syntax_supported").as_bool() &&
                ziga_analysis.at("has_future_function").as_bool() &&
                ziga_analysis.at("read_only_future_executable").as_bool() &&
                !ziga_analysis.at("has_external_dependency").as_bool(),
            "ZIGA is a supported local read-only future function");
    const auto turns = tdx::evaluate_formula_source_document(
        zig_sample, "P:PEAK(CLOSE,10,1); T:TROUGH(CLOSE,10,1);", {}, "TURINGTEST");
    require(point_value(turns, 2, "P").as_number() == 12.0 &&
                point_value(turns, 5, "P").as_number() == 12.0 &&
                point_value(turns, 5, "T").as_number() == 9.0,
            "PEAK and TROUGH retain the requested latest ZIG turning value");
    const auto turn_bars = tdx::evaluate_formula_source_document(
        zig_sample, "PB:PEAKBARS(CLOSE,10,1); TB:TROUGHBARS(CLOSE,10,1);", {}, "TURNBARSTEST");
    require(point_value(turn_bars, 2, "PB").as_number() == 0.0 &&
                point_value(turn_bars, 5, "PB").as_number() == 3.0 &&
                point_value(turn_bars, 5, "TB").as_number() == 0.0 &&
                point_value(turn_bars, 6, "TB").as_number() == 1.0,
            "PEAKBARS and TROUGHBARS count from the requested ZIG turn");
    const auto turn_bars_analysis =
        tdx::analyze_formula_source("PB:PEAKBARS(CLOSE,10,1); TB:TROUGHBARS(CLOSE,10,1);");
    require(turn_bars_analysis.at("syntax_supported").as_bool() &&
                turn_bars_analysis.at("has_future_function").as_bool() &&
                turn_bars_analysis.at("read_only_future_executable").as_bool(),
            "turn-bar functions are supported but remain read-only future functions");
    const auto calendar = tdx::evaluate_formula_source_document(
        sample(3), "D:DATETOTODAY(DATE)-DATETOTODAY(REF(DATE,1)); S:BARSSINCE(CLOSE);", {},
        "CALENDARTEST");
    require(point_value(calendar, 1, "D").as_number() == 1.0 &&
                point_value(calendar, 2, "S").as_number() == 2.0,
            "DATETOTODAY and BARSSINCE provide NXTS calendar spans");

    const auto custom_core = tdx::evaluate_formula_source_document(
        sample(3),
        "YR:YEAR;MO:MONTH;DY:DAY;WD:WEEKDAY;BS:BARSTATUS;TB:TOTALBARSCOUNT;"
        "FIXED:CONST(CLOSE);IN:RANGE(CLOSE,10,12);FR:FRACPART(CLOSE/4);"
        "S1:SIGN(CLOSE-11);S2:SGN(CLOSE-11);"
        "TRIG:ACOS(0)+ASIN(0)+ATAN(1)+COS(0)+SIN(0)+TAN(0);",
        {}, "CUSTOMCORE");
    require(point_value(custom_core, 0, "YR").as_number() == 2026.0 &&
                point_value(custom_core, 1, "MO").as_number() == 1.0 &&
                point_value(custom_core, 2, "DY").as_number() == 3.0 &&
                point_value(custom_core, 0, "WD").as_number() == 4.0 &&
                point_value(custom_core, 2, "WD").as_number() == 6.0,
            "TCalc YEAR/MONTH/DAY/WEEKDAY read native bar date fields");
    require(point_value(custom_core, 0, "BS").as_number() == 1.0 &&
                point_value(custom_core, 1, "BS").as_number() == 0.0 &&
                point_value(custom_core, 2, "BS").as_number() == 2.0 &&
                point_value(custom_core, 0, "TB").as_number() == 3.0 &&
                point_value(custom_core, 2, "FIXED").as_number() == 12.0,
            "TCalc BARSTATUS/TOTALBARSCOUNT/CONST native series semantics");
    require(point_value(custom_core, 0, "IN").as_number() == 0.0 &&
                point_value(custom_core, 1, "IN").as_number() == 1.0 &&
                point_value(custom_core, 2, "IN").as_number() == 0.0 &&
                std::abs(point_value(custom_core, 0, "FR").as_number() - 0.5) < 1e-6 &&
                point_value(custom_core, 0, "S1").as_number() == -1.0 &&
                point_value(custom_core, 1, "S1").as_number() == 0.0 &&
                point_value(custom_core, 2, "S2").as_number() == 1.0,
            "TCalc RANGE/FRACPART/SIGN/SGN native tolerance semantics");
    require(std::abs(point_value(custom_core, 2, "TRIG").as_number() -
                     (1.0 + 3.14159265358979323846 * 0.75)) < 1e-5,
            "TCalc inverse and ordinary trigonometric functions");
    const auto custom_core_reverse = tdx::evaluate_formula_source_document(
        sample(6),
        "SEQ:=TOTALBARSCOUNT-CURRBARSCOUNT+1;CA:CONSTA(CLOSE,2);"
        "RP:ROUND2(1.235,2);RN:ROUND2(-1.235,2);"
        "RD:ROUND2(1.235,IF(SEQ=1,0,2));"
        "EC:EXISTR(CLOSE=12,0,0);EW:EXISTR(CLOSE=12,1,0);"
        "G:=IF(SEQ=3,DRAWNULL,IF(SEQ=2,1,0));EG:EXISTR(G,3,1);",
        {}, "CUSTOMREV");
    require(point_value(custom_core_reverse, 0, "CA").as_number() == 13.0 &&
                point_value(custom_core_reverse, 5, "CA").as_number() == 13.0,
            "CONSTA selects the final offset and broadcasts its source value");
    require(std::abs(point_value(custom_core_reverse, 5, "RP").as_number() - 1.24) < 1e-6 &&
                std::abs(point_value(custom_core_reverse, 5, "RN").as_number() + 1.24) < 1e-6 &&
                point_value(custom_core_reverse, 5, "RD").as_number() == 1.0,
            "ROUND2 native bias, sign and first-bar precision semantics");
    require(point_value(custom_core_reverse, 3, "EC").as_number() == 1.0 &&
                point_value(custom_core_reverse, 5, "EC").as_number() == 1.0 &&
                point_value(custom_core_reverse, 3, "EW").as_number() == 1.0 &&
                point_value(custom_core_reverse, 5, "EW").as_number() == 0.0 &&
                point_value(custom_core_reverse, 2, "EG").is_null() &&
                point_value(custom_core_reverse, 5, "EG").as_number() == 1.0,
            "EXISTR cumulative/window boundaries and internal missing-sentinel truth");
    auto transform_sample = sample(16);
    auto &transform_bars = transform_sample["bars"].as_array();
    const double newsar_closes[]{10, 11, 12, 13, 12, 11, 10, 9, 10, 11, 12, 11, 10, 9, 10, 11};
    for (std::size_t index = 0; index < transform_bars.size(); ++index) {
        auto &bar = transform_bars[transform_bars.size() - 1 - index];
        bar["open"] = newsar_closes[index] - 0.25;
        bar["high"] = newsar_closes[index] + 1.0;
        bar["low"] = newsar_closes[index] - 1.0;
        bar["close"] = newsar_closes[index];
    }
    const auto newsar = tdx::evaluate_formula_source_document(transform_sample, "NS:NEWSAR(3,2);",
                                                              {}, "NEWSARTEST");
    const double expected_newsar[]{0,          0,          9,          9.01000023,
                                   9.01798058, 9.02394485, 9.02789688, 13,
                                   12.9919996, 12.9860153, 12.9820433, 12.9701147,
                                   12.9462938, 12.906723,  12.8754692, 12.8524656};
    require(point_value(newsar, 0, "NS").is_null() && point_value(newsar, 1, "NS").is_null(),
            "NEWSAR native period warm-up");
    for (std::size_t index = 2; index < transform_bars.size(); ++index)
        require(std::abs(point_value(newsar, index, "NS").as_number() - expected_newsar[index]) <
                    2e-5,
                "NEWSAR native S/1000 acceleration and close reversal vector");

    const auto fftrans = tdx::evaluate_formula_source_document(
        sample(5),
        "SEQ:=TOTALBARSCOUNT-CURRBARSCOUNT+1;F4:FFTRANS(SEQ,4);"
        "F3:FFTRANS(SEQ,3);N:=IF(SEQ=1,2,IF(SEQ=3,3,99));FD:FFTRANS(SEQ,N);",
        {}, "FFTRANSTEST");
    const double expected_f4[]{10, -4, -2, 0, 5};
    const double expected_f3[]{6, -2, 2, 9, -1};
    const double expected_dynamic[]{3, -1, 12, -2, 4};
    for (std::size_t index = 0; index < 5; ++index) {
        require(std::abs(point_value(fftrans, index, "F4").as_number() - expected_f4[index]) < 2e-5,
                "FFTRANS power-of-two Hartley-compatible native vector");
        require(std::abs(point_value(fftrans, index, "F3").as_number() - expected_f3[index]) < 2e-5,
                "FFTRANS non-power-of-two incomplete butterfly vector");
        require(std::abs(point_value(fftrans, index, "FD").as_number() - expected_dynamic[index]) <
                    2e-5,
                "FFTRANS dynamic segment length vector");
    }
    const auto fftrans_analysis = tdx::analyze_formula_source("F:FFTRANS(CLOSE,30);");
    require(fftrans_analysis.at("syntax_supported").as_bool() &&
                fftrans_analysis.at("has_future_function").as_bool() &&
                fftrans_analysis.at("read_only_future_executable").as_bool(),
            "FFTRANS is admitted only through explicit read-only future execution");
    const auto custom_core_analysis = tdx::analyze_formula_source(
        "X:YEAR+MONTH+DAY+WEEKDAY+BARSTATUS+TOTALBARSCOUNT+"
        "CONST(CLOSE)+CONSTA(CLOSE,2)+EXISTR(CLOSE>0,3,1)+"
        "ROUND2(CLOSE,2)+RANGE(CLOSE,0,100)+FRACPART(CLOSE)+SIGN(CLOSE)+"
        "SGN(CLOSE)+ACOS(0)+ASIN(0)+ATAN(0)+COS(0)+SIN(0)+TAN(0);");
    require(custom_core_analysis.at("syntax_supported").as_bool() &&
                custom_core_analysis.at("executable").as_bool() &&
                custom_core_analysis.at("unsupported").as_array().empty(),
            "custom formula core is admitted by semantic analysis");

    auto calendar_filter_sample = sample(10);
    auto &calendar_filter_bars = calendar_filter_sample["bars"].as_array();
    for (std::size_t i = 0; i < calendar_filter_bars.size(); ++i) {
        auto &bar = calendar_filter_bars[calendar_filter_bars.size() - 1 - i];
        bar["close"] = static_cast<double>(i + 1);
    }
    const auto calendar_filter = tdx::evaluate_formula_source_document(
        calendar_filter_sample,
        "SEQ:=TOTALBARSCOUNT-CURRBARSCOUNT+1;"
        "WOY:WEEKOFYEAR;T2:TIME2;"
        "D0:DATETODAY(901219);D1:DATETOTODAY(901220);"
        "DT:DAYTODATE(13);TS:TIMETOSEC(93000);ST:SECTOTIME(34200);"
        "RAW:=IF(SEQ=2 OR SEQ=4 OR SEQ=5,CLOSE,DRAWNULL);"
        "AR:ALIGNRIGHT(RAW);"
        "B:=SEQ=1 OR SEQ=2 OR SEQ=5 OR SEQ=7 OR SEQ=9 OR SEQ=10;"
        "S:=SEQ=3 OR SEQ=4 OR SEQ=5 OR SEQ=6 OR SEQ=9;"
        "F0:TFILTER(B,S,0);F1:TFILTER(B,S,1);F2:TFILTER(B,S,2);"
        "BO:=SEQ=1 OR SEQ=2 OR SEQ=5 OR SEQ=9;"
        "SC:=SEQ=3 OR SEQ=4 OR SEQ=6 OR SEQ=9;"
        "SO:=SEQ=3 OR SEQ=6 OR SEQ=7;"
        "BC:=SEQ=2 OR SEQ=4 OR SEQ=8 OR SEQ=10;"
        "TT0:TTFILTER(BO,SC,SO,BC,0);TT1:TTFILTER(BO,SC,SO,BC,1);"
        "TT2:TTFILTER(BO,SC,SO,BC,2);TT3:TTFILTER(BO,SC,SO,BC,3);"
        "TT4:TTFILTER(BO,SC,SO,BC,4);",
        {}, "CUSTOMCALENDARFILTER");
    require(point_value(calendar_filter, 0, "WOY").as_number() == 1.0 &&
                point_value(calendar_filter, 3, "WOY").as_number() == 2.0 &&
                point_value(calendar_filter, 0, "T2").as_number() == 150000.0,
            "WEEKOFYEAR uses native Sunday boundaries and TIME2 returns HHMMSS");
    require(point_value(calendar_filter, 0, "D0").as_number() == 0.0 &&
                point_value(calendar_filter, 0, "D1").as_number() == 1.0 &&
                point_value(calendar_filter, 0, "DT").as_number() == 910101.0 &&
                point_value(calendar_filter, 0, "TS").as_number() == 34200.0 &&
                point_value(calendar_filter, 0, "ST").as_number() == 93000.0,
            "native date epoch and time/second conversion round trips");
    for (std::size_t i = 0; i < 7; ++i)
        require(point_value(calendar_filter, i, "AR").is_null(),
                "ALIGNRIGHT leaves the compacted prefix invalid");
    require(point_value(calendar_filter, 7, "AR").as_number() == 2.0 &&
                point_value(calendar_filter, 8, "AR").as_number() == 4.0 &&
                point_value(calendar_filter, 9, "AR").as_number() == 5.0,
            "ALIGNRIGHT compacts valid values at the right edge in source order");
    const double tfilter0[]{1, 0, 2, 0, 1, 2, 1, 0, 2, 0};
    const double tfilter1[]{1, 0, 0, 0, 1, 0, 1, 0, 0, 0};
    const double tfilter2[]{0, 0, 1, 0, 1, 1, 0, 0, 1, 0};
    const double ttfilter0[]{1, 0, 2, 4, 1, 2, 0, 4, 1, 0};
    const double ttfilter1[]{1, 0, 0, 0, 1, 0, 0, 0, 1, 0};
    const double ttfilter2[]{0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
    const double ttfilter3[]{0, 0, 1, 0, 0, 1, 0, 0, 0, 0};
    const double ttfilter4[]{0, 0, 0, 1, 0, 0, 0, 1, 0, 0};
    for (std::size_t i = 0; i < 10; ++i) {
        require(point_value(calendar_filter, i, "F0").as_number() == tfilter0[i] &&
                    point_value(calendar_filter, i, "F1").as_number() == tfilter1[i] &&
                    point_value(calendar_filter, i, "F2").as_number() == tfilter2[i],
                "TFILTER preserves native paired-signal state and simultaneous-bar quirk");
        require(point_value(calendar_filter, i, "TT0").as_number() == ttfilter0[i] &&
                    point_value(calendar_filter, i, "TT1").as_number() == ttfilter1[i] &&
                    point_value(calendar_filter, i, "TT2").as_number() == ttfilter2[i] &&
                    point_value(calendar_filter, i, "TT3").as_number() == ttfilter3[i] &&
                    point_value(calendar_filter, i, "TT4").as_number() == ttfilter4[i],
                "TTFILTER preserves native four-way open/close mode state machines");
    }

    auto tfilt_sample = sample(6);
    auto &tfilt_bars = tfilt_sample["bars"].as_array();
    const char *tfilt_dates[]{"2004-01-01", "2004-01-01", "2004-01-01",
                              "2004-01-01", "2004-01-01", "2004-01-02"};
    const char *tfilt_times[]{"10:00", "10:25:05", "11:00", "13:45", "14:00", "10:25"};
    for (std::size_t i = 0; i < tfilt_bars.size(); ++i) {
        auto &bar = tfilt_bars[tfilt_bars.size() - 1 - i];
        bar["date"] = tfilt_dates[i];
        bar["time"] = tfilt_times[i];
        bar["close"] = static_cast<double>((i + 1) * 10);
    }
    const auto tfilt_result =
        tdx::evaluate_formula_source_document(tfilt_sample,
                                              "F:TFILT(CLOSE,1040101,1025,1040101,1345);"
                                              "L:TFILT(CLOSE,0,0,0,2359);T2:TIME2;",
                                              {}, "CUSTOMTFILT");
    require(point_value(tfilt_result, 0, "F").is_null() &&
                point_value(tfilt_result, 1, "F").as_number() == 20.0 &&
                point_value(tfilt_result, 2, "F").as_number() == 30.0 &&
                point_value(tfilt_result, 3, "F").as_number() == 40.0 &&
                point_value(tfilt_result, 4, "F").is_null() &&
                point_value(tfilt_result, 5, "F").is_null(),
            "TFILT includes both requested date/time boundaries");
    require(point_value(tfilt_result, 4, "L").is_null() &&
                point_value(tfilt_result, 5, "L").as_number() == 60.0 &&
                point_value(tfilt_result, 1, "T2").as_number() == 102505.0,
            "TFILT date zero resolves to the latest bar date and TIME2 keeps seconds");

    auto date_to_cur_sample = sample(6);
    auto &date_to_cur_bars = date_to_cur_sample["bars"].as_array();
    const char *date_to_cur_dates[]{"2024-01-02", "2024-01-02", "2024-01-03",
                                    "2024-01-03", "2024-01-03", "2024-01-05"};
    const char *date_to_cur_times[]{"09:30", "10:00", "09:30", "10:00", "14:59", "09:30"};
    for (std::size_t i = 0; i < date_to_cur_bars.size(); ++i) {
        auto &bar = date_to_cur_bars[date_to_cur_bars.size() - 1 - i];
        bar["date"] = date_to_cur_dates[i];
        bar["time"] = date_to_cur_times[i];
    }
    const auto date_to_cur_result =
        tdx::evaluate_formula_source_document(date_to_cur_sample,
                                              "D:DATETOCUR(1240102);F:DATETOCUR(1240102.9);"
                                              "L:DATETOCUR(IF(CURRBARSCOUNT=1,1240102,1240105));"
                                              "N:DATETOCUR(DRAWNULL);A:DATETOCUR(1240103);",
                                              {}, "CUSTOMDATETOCUR");
    const double date_to_cur_after_jan2[]{0, 0, 3, 3, 3, 4};
    const double date_to_cur_missing[]{2, 2, 5, 5, 5, 6};
    const double date_to_cur_after_jan3[]{0, 0, 0, 0, 0, 1};
    for (std::size_t i = 0; i < 6; ++i) {
        require(
            point_value(date_to_cur_result, i, "D").as_number() == date_to_cur_after_jan2[i] &&
                point_value(date_to_cur_result, i, "F").as_number() == date_to_cur_after_jan2[i] &&
                point_value(date_to_cur_result, i, "L").as_number() == date_to_cur_after_jan2[i] &&
                point_value(date_to_cur_result, i, "N").as_number() == date_to_cur_missing[i] &&
                point_value(date_to_cur_result, i, "A").as_number() == date_to_cur_after_jan3[i],
            "DATETOCUR uses final truncated target and counts complete same-day bars");
    }
    const auto date_to_cur_analysis = tdx::analyze_formula_source("D:DATETOCUR(DATE);");
    require(date_to_cur_analysis.at("syntax_supported").as_bool() &&
                !date_to_cur_analysis.at("executable").as_bool() &&
                date_to_cur_analysis.at("has_future_function").as_bool() &&
                date_to_cur_analysis.at("read_only_future_executable").as_bool() &&
                !date_to_cur_analysis.at("has_external_dependency").as_bool() &&
                date_to_cur_analysis.at("future_functions").as_array().size() == 1 &&
                date_to_cur_analysis.at("future_functions").as_array().front().as_string() ==
                    "DATETOCUR",
            "DATETOCUR is isolated as a context-free read-only future function");

    auto directional_sample = sample(5);
    auto &directional_bars = directional_sample["bars"].as_array();
    const double directional_open[]{10, 11, 12, 11, 13};
    const double directional_high[]{11, 13, 13, 14, 15};
    const double directional_low[]{9, 10, 10, 10, 11};
    const double directional_close[]{10, 12, 11, 13, 14};
    const double directional_volume[]{10, 20, 30, 40, 50};
    for (std::size_t i = 0; i < directional_bars.size(); ++i) {
        auto &bar = directional_bars[directional_bars.size() - 1 - i];
        bar["date"] = "2024-01-0" + std::to_string(i + 2);
        bar["open"] = directional_open[i];
        bar["high"] = directional_high[i];
        bar["low"] = directional_low[i];
        bar["close"] = directional_close[i];
        bar["volume"] = directional_volume[i] * 100.0;
    }
    const auto directional_result = tdx::evaluate_formula_source_document(
        directional_sample, "DH:DHIGH;DO:DOPEN;DL:DLOW;DC:DCLOSE;DV:DVOL;DHF:DHIGH();", {},
        "CUSTOMDIRECTIONALBARS");
    const double expected_dhigh[]{13, 13, 13, 15, 15};
    const double expected_dopen[]{10, 10, 12, 11, 11};
    const double expected_dlow[]{9, 9, 10, 10, 10};
    const double expected_dclose[]{12, 12, 11, 14, 14};
    const double expected_dvol[]{30, 30, 30, 90, 90};
    for (std::size_t i = 0; i < directional_bars.size(); ++i) {
        require(point_value(directional_result, i, "DH").as_number() == expected_dhigh[i] &&
                    point_value(directional_result, i, "DHF").as_number() == expected_dhigh[i] &&
                    point_value(directional_result, i, "DO").as_number() == expected_dopen[i] &&
                    point_value(directional_result, i, "DL").as_number() == expected_dlow[i] &&
                    point_value(directional_result, i, "DC").as_number() == expected_dclose[i] &&
                    point_value(directional_result, i, "DV").as_number() == expected_dvol[i],
                "directional OHLCV projects completed close-direction segments");
    }
    const auto directional_analysis =
        tdx::analyze_formula_source("A:DHIGH+DOPEN+DLOW+DCLOSE+DVOL;");
    require(directional_analysis.at("syntax_supported").as_bool() &&
                !directional_analysis.at("executable").as_bool() &&
                directional_analysis.at("has_future_function").as_bool() &&
                directional_analysis.at("read_only_future_executable").as_bool() &&
                !directional_analysis.at("has_external_dependency").as_bool() &&
                directional_analysis.at("future_functions").size() == 5 &&
                directional_analysis.at("market_dependencies").size() == 5,
            "directional OHLCV symbols are isolated as pure read-only future data");
    const auto calendar_filter_analysis = tdx::analyze_formula_source(
        "A:WEEKOFYEAR+TIME2+DATETODAY(DATE)+DAYTODATE(1)+"
        "TIMETOSEC(TIME2)+SECTOTIME(1)+ALIGNRIGHT(CLOSE)+"
        "TFILT(CLOSE,DATE,TIME,DATE,TIME)+TFILTER(CLOSE>OPEN,CLOSE<OPEN,0)+"
        "TTFILTER(CLOSE>OPEN,CLOSE<OPEN,CLOSE<OPEN,CLOSE>OPEN,0);");
    require(calendar_filter_analysis.at("syntax_supported").as_bool() &&
                calendar_filter_analysis.at("executable").as_bool() &&
                calendar_filter_analysis.at("unsupported").as_array().empty(),
            "calendar/alignment/signal-filter custom core is pure and executable");

    auto security_string_sample = sample(8);
    auto &security_string_bars = security_string_sample["bars"].as_array();
    const double updown_values[]{
        10.0, 10.000005, 10.000020, 10.000005, 9.999980, 0.0, -0.000005, -0.000020,
    };
    for (std::size_t i = 0; i < security_string_bars.size(); ++i)
        security_string_bars[security_string_bars.size() - 1 - i]["close"] = updown_values[i];
    const auto security_string = tdx::evaluate_formula_source_document(
        security_string_sample,
        "N0:NAMELIKE('平安');N1:NAMELIKE('银行');"
        "C0:CODELIKE('000');C1:CODELIKE('600');"
        "I0:NAMEINCLUDE('银行');I1:NAMEINCLUDE('证券');"
        "F0:FINDSTR('多头开仓','开仓');F1:FINDSTR('多头开仓','平仓');"
        "FE:FINDSTR('ABC','');S0:STR2CON('2365.02');S1:STR2CON('BAD');"
        "U:UPDOWN(CLOSE);Z0:NOT(0);Z1:NOT(0.00000001);"
        "DRAWTEXT_FIX(ISLASTBAR,0,0,0,STKNAME);",
        {}, "SECURITYSTRING");
    for (std::size_t i = 0; i < security_string_bars.size(); ++i) {
        require(point_value(security_string, i, "N0").as_number() == 1.0 &&
                    point_value(security_string, i, "N1").as_number() == 0.0 &&
                    point_value(security_string, i, "C0").as_number() == 1.0 &&
                    point_value(security_string, i, "C1").as_number() == 0.0 &&
                    point_value(security_string, i, "I0").as_number() == 1.0 &&
                    point_value(security_string, i, "I1").as_number() == 0.0 &&
                    point_value(security_string, i, "F0").as_number() == 1.0 &&
                    point_value(security_string, i, "F1").as_number() == 0.0 &&
                    point_value(security_string, i, "FE").as_number() == 1.0,
                "security name/code prefix and substring functions fill exact constants");
        require(std::abs(point_value(security_string, i, "S0").as_number() - 2365.02) < 0.001 &&
                    point_value(security_string, i, "S1").as_number() == 0.0 &&
                    point_value(security_string, i, "Z0").as_number() == 1.0 &&
                    point_value(security_string, i, "Z1").as_number() == 0.0,
                "STR2CON follows atof and NOT distinguishes exact from approximate zero");
    }
    const double updown_expected[]{0.0, 1.0, -1.0, -1.0, -1.0, 0.0, -1.0};
    require(point_value(security_string, 0, "U").is_null(),
            "UPDOWN leaves the first valid bar missing");
    for (std::size_t i = 1; i < security_string_bars.size(); ++i)
        require(point_value(security_string, i, "U").as_number() == updown_expected[i - 1],
                "UPDOWN preserves the native relative-plus-absolute tolerance");
    const auto &security_text = security_string.at("render_ir")
                                    .at("primitives")
                                    .as_array()
                                    .back()
                                    .at("events")
                                    .as_array()
                                    .front();
    require(security_text.at("annotation_text").as_string() == "平安银行" &&
                security_text.at("string_arguments").at("4").as_string() == "平安银行",
            "STKNAME is bound from the selected security metadata");
    const auto security_string_analysis =
        tdx::analyze_formula_source("A:NAMELIKE('ST')+CODELIKE('600')+NAMEINCLUDE('银行')+"
                                    "FINDSTR('ABC','B')+STR2CON('1.5')+UPDOWN(CLOSE)+NOT(CLOSE);"
                                    "DRAWTEXT_FIX(ISLASTBAR,0,0,0,STKNAME);");
    require(security_string_analysis.at("syntax_supported").as_bool() &&
                security_string_analysis.at("executable").as_bool() &&
                security_string_analysis.at("numeric_signal_safe").as_bool() &&
                security_string_analysis.at("unsupported").as_array().empty(),
            "security/string custom core is pure, exact and directly executable");

    tdx::Json security_status_context = tdx::Json::object();
    security_status_context["symbols"] = tdx::Json::object();
    security_status_context["symbols"]["IST0CODE"] = 1.0;
    security_status_context["symbols"]["ISSTCODE"] = 0.0;
    security_status_context["symbols"]["ISQUITCODE"] = 1.0;
    security_status_context["symbols"]["ISQHQQCODE"] = 0.0;
    const auto security_status = tdx::evaluate_formula_source_document(
        sample(4), "T0:IST0CODE;ST:ISSTCODE;Q:ISQUITCODE;F:ISQHQQCODE;", {}, "SECURITYSTATUS",
        &security_status_context);
    for (std::size_t i = 0; i < 4; ++i)
        require(point_value(security_status, i, "T0").as_number() == 1.0 &&
                    point_value(security_status, i, "ST").as_number() == 0.0 &&
                    point_value(security_status, i, "Q").as_number() == 1.0 &&
                    point_value(security_status, i, "F").as_number() == 0.0,
                "security-status host values broadcast across every bar");
    const auto security_status_analysis =
        tdx::analyze_formula_source("X:IST0CODE+ISSTCODE+ISQUITCODE+ISQHQQCODE;");
    require(security_status_analysis.at("syntax_supported").as_bool() &&
                !security_status_analysis.at("executable").as_bool() &&
                security_status_analysis.at("context_bindable").as_bool() &&
                security_status_analysis.at("executable_with_context").as_bool() &&
                security_status_analysis.at("automatic_context_dependencies").as_array().size() ==
                    4,
            "security-status functions require exact automatic host context");

    tdx::Json multiplier_context = tdx::Json::object();
    multiplier_context["symbols"] = tdx::Json::object();
    multiplier_context["symbols"]["MULTIPLIER"] = 200.0;
    const auto multiplier = tdx::evaluate_formula_source_document(
        sample(4), "M:MULTIPLIER;", {}, "MULTIPLIER", &multiplier_context);
    for (std::size_t i = 0; i < 4; ++i)
        require(point_value(multiplier, i, "M").as_number() == 200.0,
                "MULTIPLIER host value broadcasts across every bar");
    const auto multiplier_analysis = tdx::analyze_formula_source("M:MULTIPLIER;");
    require(multiplier_analysis.at("syntax_supported").as_bool() &&
                !multiplier_analysis.at("executable").as_bool() &&
                multiplier_analysis.at("context_bindable").as_bool() &&
                multiplier_analysis.at("executable_with_context").as_bool() &&
                multiplier_analysis.at("automatic_context_dependencies").as_array().size() == 1 &&
                multiplier_analysis.at("automatic_context_dependencies")
                        .as_array()
                        .front()
                        .as_string() == "MULTIPLIER",
            "MULTIPLIER requires exact automatic contract metadata context");

    tdx::Json host_calendar_context = tdx::Json::object();
    host_calendar_context["symbols"] = tdx::Json::object();
    host_calendar_context["symbols"]["ISJYDATE"] = 1.0;
    host_calendar_context["symbols"]["LOCALDAYNUM"] = 4321.0;
    const auto host_calendar = tdx::evaluate_formula_source_document(
        sample(4), "J:ISJYDATE;L:LOCALDAYNUM;JF:ISJYDATE();LF:LOCALDAYNUM();", {}, "HOSTCALENDAR",
        &host_calendar_context);
    for (std::size_t i = 0; i < 4; ++i)
        require(point_value(host_calendar, i, "J").as_number() == 1.0 &&
                    point_value(host_calendar, i, "L").as_number() == 4321.0 &&
                    point_value(host_calendar, i, "JF").as_number() == 1.0 &&
                    point_value(host_calendar, i, "LF").as_number() == 4321.0,
                "host-calendar callback values broadcast for symbols and zero-argument calls");
    const auto host_calendar_analysis =
        tdx::analyze_formula_source("X:ISJYDATE+LOCALDAYNUM+ISJYDATE()+LOCALDAYNUM();");
    require(host_calendar_analysis.at("syntax_supported").as_bool() &&
                !host_calendar_analysis.at("executable").as_bool() &&
                host_calendar_analysis.at("context_bindable").as_bool() &&
                host_calendar_analysis.at("executable_with_context").as_bool() &&
                host_calendar_analysis.at("automatic_context_dependencies").as_array().size() == 2,
            "host-calendar functions require exact automatic type-122/168 context");

    const auto string_builders = tdx::evaluate_formula_source_document(
        sample(10),
        "L:STRLEN('通达信');"
        "S0:STRCMP(SUBSTR('通达信',3,2),'达');"
        "S1:STRCMP(SUBSTR('ABC',99,1),'C');"
        "S2:STRCMP(STRSPACE('A'),'A ');"
        "S3:STRCMP(STRCAT6('A','B','C','D','E','F'),'ABCDEF');"
        "S4:STRCMP(VARCAT('A','B'),'AB');"
        "S5:STRCMP(VARCAT6('A','B','C','D','E','F'),'ABCDEF');"
        "N:STR2CON(SUBSTR('2365.02',1,7));"
        "DRAWTEXT(1,LOW,VARCAT6(VAR2STR(CLOSE,1),'|',"
        "CON2STR(CLOSE,1),'|',STRSPACE(SUBSTR('通达信',3,2)),'尾'));"
        "DRAWTEXT(1,HIGH,STRCAT6(VAR2STR(CLOSE,1),'|',"
        "CON2STR(CLOSE,1),'|',STRSPACE('定'),'值'));",
        {}, "STRINGBUILDERS");
    for (std::size_t i = 0; i < 10; ++i)
        require(point_value(string_builders, i, "L").as_number() == 6.0 &&
                    point_value(string_builders, i, "S0").as_number() == 1.0 &&
                    point_value(string_builders, i, "S1").as_number() == 1.0 &&
                    point_value(string_builders, i, "S2").as_number() == 1.0 &&
                    point_value(string_builders, i, "S3").as_number() == 1.0 &&
                    point_value(string_builders, i, "S4").as_number() == 1.0 &&
                    point_value(string_builders, i, "S5").as_number() == 1.0 &&
                    std::abs(point_value(string_builders, i, "N").as_number() - 2365.02) < 0.001,
                "GBK byte string length/slice and nested builders are exact");
    const auto &builder_primitives = string_builders.at("render_ir").at("primitives").as_array();
    std::vector<const tdx::Json *> builder_text_primitives;
    for (const auto &primitive : builder_primitives)
        if (primitive.at("function").as_string() == "DRAWTEXT")
            builder_text_primitives.push_back(&primitive);
    require(builder_text_primitives.size() == 2,
            "both string-builder DRAWTEXT primitives are materialized");
    const auto &variable_events = builder_text_primitives[0]->at("events").as_array();
    const auto &constant_events = builder_text_primitives[1]->at("events").as_array();
    require(variable_events.front().at("annotation_text").as_string() == "10.0|19.0|达 尾" &&
                variable_events.back().at("annotation_text").as_string() == "19.0|19.0|达 尾" &&
                constant_events.front().at("annotation_text").as_string() == "19.0|19.0|定 值" &&
                constant_events.back().at("annotation_text").as_string() == "19.0|19.0|定 值",
            "VAR builders are per-bar while STR/CON builders broadcast the last bar");
    const auto string_builder_analysis =
        tdx::analyze_formula_source("X:STRLEN(SUBSTR(STRCAT6('A','B','C','D','E','F'),1,6))+"
                                    "STRCMP(VARCAT(VAR2STR(CLOSE,2),STRSPACE('X')),'');");
    require(string_builder_analysis.at("syntax_supported").as_bool() &&
                string_builder_analysis.at("executable").as_bool() &&
                string_builder_analysis.at("numeric_signal_safe").as_bool() &&
                string_builder_analysis.at("string_semantics_faithful").as_bool() &&
                string_builder_analysis.at("unsupported").as_array().empty(),
            "nested string builders are accepted as exact numeric consumers");

    auto sequence_sample = sample(8);
    auto &sequence_bars = sequence_sample["bars"].as_array();
    const double sequence_values[]{1.0, 3.0, 2.0, 5.0, 4.0, 7.0, 6.0, 8.0};
    for (std::size_t i = 0; i < sequence_bars.size(); ++i)
        sequence_bars[sequence_bars.size() - 1 - i]["close"] = sequence_values[i];
    const auto sequence_core = tdx::evaluate_formula_source_document(
        sequence_sample,
        "SIG:=CLOSE=3 OR CLOSE=7;"
        "BL1:BARSLASTS(SIG,1);BL2:BARSLASTS(SIG,2);"
        "BSN:BARSSINCEN(SIG,3);BSA:BARSSINCE(SIG);"
        "FX:FILTERX(CLOSE=5 OR CLOSE=4,2);"
        "TM:TMA(CLOSE,0.5,0.5);XM:XMA(CLOSE,3);"
        "CV:COVAR(CLOSE,CLOSE*2+1,3);"
        "RL:RELATE(CLOSE,CLOSE*2+1,3);"
        "BE:BETAEX(CLOSE,CLOSE*2+1,3);"
        "FH:FINDHIGH(CLOSE,1,4,2);FHB:FINDHIGHBARS(CLOSE,1,4,2);"
        "FL:FINDLOW(CLOSE,1,4,2);FLB:FINDLOWBARS(CLOSE,1,4,2);",
        {}, "CUSTOMSEQUENCECORE");
    require(point_value(sequence_core, 0, "BL1").is_null() &&
                point_value(sequence_core, 1, "BL1").as_number() == 0.0 &&
                point_value(sequence_core, 7, "BL1").as_number() == 2.0 &&
                point_value(sequence_core, 7, "BL2").as_number() == 6.0,
            "TCalc BARSLASTS returns the requested reverse occurrence distance");
    require(point_value(sequence_core, 0, "BSA").is_null() &&
                point_value(sequence_core, 7, "BSA").as_number() == 6.0 &&
                point_value(sequence_core, 3, "BSN").as_number() == 2.0 &&
                point_value(sequence_core, 4, "BSN").is_null() &&
                point_value(sequence_core, 7, "BSN").as_number() == 2.0,
            "BARSSINCE and BARSSINCEN preserve native invalid and rolling-first semantics");
    require(point_value(sequence_core, 3, "FX").as_number() == 0.0 &&
                point_value(sequence_core, 4, "FX").as_number() == 1.0,
            "FILTERX retains the later signal and clears its preceding window");
    require(std::abs(point_value(sequence_core, 3, "TM").as_number() - 3.5) < 1e-6 &&
                std::abs(point_value(sequence_core, 0, "XM").as_number() - 2.0) < 1e-6 &&
                std::abs(point_value(sequence_core, 7, "XM").as_number() - 7.0) < 1e-6,
            "TMA recurrence and future XMA centered boundary windows");
    require(std::abs(point_value(sequence_core, 2, "CV").as_number() - 2.0) < 1e-5 &&
                std::abs(point_value(sequence_core, 2, "RL").as_number() - 1.0) < 1e-5 &&
                std::abs(point_value(sequence_core, 2, "BE").as_number() - 0.5) < 1e-5,
            "COVAR/RELATE/BETAEX use native sample covariance and amplification");
    require(point_value(sequence_core, 7, "FH").as_number() == 6.0 &&
                point_value(sequence_core, 7, "FHB").as_number() == 1.0 &&
                point_value(sequence_core, 7, "FL").as_number() == 5.0 &&
                point_value(sequence_core, 7, "FLB").as_number() == 4.0,
            "FINDHIGH/FINDLOW rank the offset historical window and return bar distance");
    const auto sequence_core_analysis =
        tdx::analyze_formula_source("A:BARSLASTS(CLOSE>0,2)+BARSSINCEN(CLOSE>0,5)+"
                                    "TMA(CLOSE,0.5,0.5)+COVAR(CLOSE,OPEN,5)+RELATE(CLOSE,OPEN,5)+"
                                    "BETAEX(CLOSE,OPEN,5)+FINDHIGH(CLOSE,1,5,2)+"
                                    "FINDHIGHBARS(CLOSE,1,5,2)+FINDLOW(CLOSE,1,5,2)+"
                                    "FINDLOWBARS(CLOSE,1,5,2);B:FILTERX(CLOSE>0,2)+XMA(CLOSE,3);");
    require(sequence_core_analysis.at("syntax_supported").as_bool() &&
                sequence_core_analysis.at("has_future_function").as_bool() &&
                sequence_core_analysis.at("read_only_future_executable").as_bool() &&
                sequence_core_analysis.at("unsupported").as_array().empty(),
            "sequence/statistics core is supported and future members remain read-only");

    auto rolling_sample = sample(8);
    auto &rolling_bars = rolling_sample["bars"].as_array();
    const double rolling_values[]{1.0, 2.0, 8.0, 16.0, 4.0, 32.0, 64.0, 128.0};
    for (std::size_t i = 0; i < rolling_bars.size(); ++i)
        rolling_bars[rolling_bars.size() - 1 - i]["close"] = rolling_values[i];
    const auto rolling_core = tdx::evaluate_formula_source_document(
        rolling_sample,
        "SEQ:=TOTALBARSCOUNT-CURRBARSCOUNT+1;"
        "RV:REFV(CLOSE,IF(SEQ=3,5,1));MU:MULAR(CLOSE,2);"
        "AM:AMA(CLOSE,0.25);HR:HOD(CLOSE,3);LR:LOD(CLOSE,3);"
        "HV:HHVLLV(CLOSE,0,2,1);LV:HHVLLV(CLOSE,1,2,1);"
        "IV:ISVALID(VAR(CLOSE,3));"
        "IT:IFF(CLOSE>5,10,20);IN:IFN(CLOSE>5,10,20);"
        "N:NOT(CLOSE>5);MX:MAX6(1,6,2,5,3,4);MN:MIN6(1,6,2,5,3,4);"
        "DS:DEVSQ(CLOSE,3);VS:VAR(CLOSE,3);VP:VARP(CLOSE,3);"
        "SS:STD(CLOSE,3);SP:STDP(CLOSE,3);DV:STDDEV(CLOSE,3);",
        {}, "CUSTOMROLLINGVARIANCE");
    require(point_value(rolling_core, 0, "RV").as_number() == 1.0 &&
                point_value(rolling_core, 1, "RV").as_number() == 1.0 &&
                point_value(rolling_core, 2, "RV").as_number() == 1.0 &&
                point_value(rolling_core, 3, "RV").as_number() == 8.0,
            "REFV smooths a variable offset at the left boundary");
    require(point_value(rolling_core, 3, "MU").as_number() == 128.0 &&
                std::abs(point_value(rolling_core, 3, "AM").as_number() - 6.203125) < 1e-6,
            "MULAR rolling product and AMA final-coefficient recurrence");
    require(point_value(rolling_core, 4, "HR").as_number() == 3.0 &&
                point_value(rolling_core, 4, "LR").as_number() == 1.0 &&
                point_value(rolling_core, 4, "HV").as_number() == 16.0 &&
                point_value(rolling_core, 4, "LV").as_number() == 8.0,
            "HOD/LOD ranks and HHVLLV offset interval extrema");
    require(point_value(rolling_core, 0, "IV").as_number() == 0.0 &&
                point_value(rolling_core, 2, "IV").as_number() == 1.0 &&
                point_value(rolling_core, 0, "IT").as_number() == 20.0 &&
                point_value(rolling_core, 2, "IT").as_number() == 10.0 &&
                point_value(rolling_core, 0, "IN").as_number() == 10.0 &&
                point_value(rolling_core, 2, "IN").as_number() == 20.0 &&
                point_value(rolling_core, 0, "N").as_number() == 1.0 &&
                point_value(rolling_core, 2, "N").as_number() == 0.0 &&
                point_value(rolling_core, 2, "MX").as_number() == 6.0 &&
                point_value(rolling_core, 2, "MN").as_number() == 1.0,
            "ISVALID/IFF/IFN/NOT and six-way extrema native branches");
    require(std::abs(point_value(rolling_core, 2, "DS").as_number() - 86.0 / 3.0) < 1e-4 &&
                std::abs(point_value(rolling_core, 2, "VS").as_number() - 43.0 / 3.0) < 1e-4 &&
                std::abs(point_value(rolling_core, 2, "SS").as_number() - std::sqrt(43.0 / 3.0)) <
                    1e-4,
            "DEVSQ/VAR/STD preserve deviation sum and sample denominator");
    require(point_value(rolling_core, 2, "VP").as_number() == 0.0 &&
                point_value(rolling_core, 2, "SP").as_number() == 0.0 &&
                std::abs(point_value(rolling_core, 3, "VP").as_number() - 296.0 / 9.0) < 1e-4 &&
                std::abs(point_value(rolling_core, 3, "SP").as_number() - std::sqrt(296.0) / 3.0) <
                    1e-4,
            "VARP/STDP retain the native first-full-window zero warmup");
    require(point_value(rolling_core, 2, "DV").is_null() &&
                std::abs(point_value(rolling_core, 3, "DV").as_number() - std::log(2.0) / 2.0) <
                    1e-5,
            "STDDEV is lagged log-return volatility rather than price dispersion");
    const auto rolling_core_analysis =
        tdx::analyze_formula_source("A:REFV(CLOSE,2)+MULAR(CLOSE,5)+AMA(CLOSE,0.2)+HOD(CLOSE,20)+"
                                    "LOD(CLOSE,20)+HHVLLV(CLOSE,0,10,2)+ISVALID(CLOSE)+"
                                    "IFF(CLOSE>OPEN,HIGH,LOW)+IFN(CLOSE>OPEN,HIGH,LOW)+"
                                    "MAX6(CLOSE,OPEN,HIGH,LOW,1,2)+MIN6(CLOSE,OPEN,HIGH,LOW,1,2)+"
                                    "DEVSQ(CLOSE,5)+VAR(CLOSE,5)+VARP(CLOSE,5)+STDP(CLOSE,5);");
    require(rolling_core_analysis.at("syntax_supported").as_bool() &&
                rolling_core_analysis.at("executable").as_bool() &&
                rolling_core_analysis.at("unsupported").as_array().empty(),
            "rolling/variance custom core is admitted by semantic analysis");

    auto cumulative_sample = sample(4);
    auto &cumulative_bars = cumulative_sample["bars"].as_array();
    const double cumulative_values[]{10.0, 1.0, 1.0, 1.0};
    for (std::size_t i = 0; i < cumulative_bars.size(); ++i)
        cumulative_bars[cumulative_bars.size() - 1 - i]["close"] = cumulative_values[i];
    const auto cumulative_core = tdx::evaluate_formula_source_document(
        cumulative_sample,
        "SB:SUMBARS(CLOSE,5);SX:SUMBARSX(CLOSE,5);"
        "ONE:=CLOSE*0+1;NB:SUMBARS(ONE,9);NX:SUMBARSX(ONE,9);",
        {}, "CUSTOMCUMULATIVE");
    for (std::size_t i = 0; i < 4; ++i) {
        require(point_value(cumulative_core, i, "SB").as_number() == static_cast<double>(i) &&
                    point_value(cumulative_core, i, "NB").as_number() == static_cast<double>(i),
                "SUMBARS preserves native preceding-distance and left-boundary fallback");
    }
    require(point_value(cumulative_core, 0, "SX").as_number() == -1.0 &&
                point_value(cumulative_core, 1, "SX").as_number() == 1.0 &&
                point_value(cumulative_core, 2, "SX").as_number() == 2.0 &&
                point_value(cumulative_core, 3, "SX").as_number() == 3.0 &&
                point_value(cumulative_core, 0, "NX").is_null() &&
                point_value(cumulative_core, 3, "NX").is_null(),
            "SUMBARSX returns native distance, strict-current -1 and missing history boundary");

    auto beta_sample = sample(6);
    auto &beta_bars = beta_sample["bars"].as_array();
    const double security_closes[]{50.0, 60.0, 48.0, 67.2, 67.2, 80.64};
    const double benchmark_closes[]{100.0, 110.0, 99.0, 118.8, 118.8, 130.68};
    tdx::Json beta_context = tdx::Json::object();
    beta_context["series"] = tdx::Json::object();
    beta_context["series"]["__BETA_BENCHMARK_CLOSE"] = tdx::Json::object();
    for (std::size_t i = 0; i < beta_bars.size(); ++i) {
        beta_bars[beta_bars.size() - 1 - i]["close"] = security_closes[i];
        const auto day = i + 1;
        const auto stamp =
            "2026-01-" + std::string(day < 10 ? "0" : "") + std::to_string(day) + "|15:00";
        beta_context["series"]["__BETA_BENCHMARK_CLOSE"][stamp] = benchmark_closes[i];
    }
    const auto beta_core = tdx::evaluate_formula_source_document(beta_sample, "B:BETA(3);", {},
                                                                 "CUSTOMBETA", &beta_context);
    require(std::abs(point_value(beta_core, 2, "B").as_number() - 0.5) < 1e-5 &&
                std::abs(point_value(beta_core, 3, "B").as_number() - 2.0) < 1e-5 &&
                std::abs(point_value(beta_core, 4, "B").as_number() - 2.0) < 1e-5 &&
                std::abs(point_value(beta_core, 5, "B").as_number() - 2.0) < 1e-5,
            "BETA uses aligned security/benchmark simple returns and native BETAEX ratio");
    const auto beta_analysis = tdx::analyze_formula_source("B:BETA(20);");
    require(beta_analysis.at("syntax_supported").as_bool() &&
                !beta_analysis.at("executable").as_bool() &&
                beta_analysis.at("executable_with_context").as_bool() &&
                beta_analysis.at("requires_automatic_market_context").as_bool() &&
                beta_analysis.at("unsupported").as_array().empty(),
            "BETA is supported with an explicit automatic benchmark dependency boundary");

    tdx::Json chip_context = tdx::Json::object();
    chip_context["series"] = tdx::Json::object();
    chip_context["series"]["CAPITAL"] = tdx::Json::object();
    chip_context["series"]["CAPITAL"]["2026-01-04|15:00"] = 1000.0;
    chip_context["series"]["CAPITAL"]["2026-01-05|15:00"] = 1000.0;
    const auto chip = tdx::evaluate_formula_source_document(
        chip_sample(),
        "W:WINNER(10.03); C50:COST(50); CE:COSTEX(10.03,10.02); "
        "PW:PWINNER(0,10.03); LW:LWINNER(1,10.03);",
        {}, "CHIPTEST", &chip_context);
    require(std::abs(point_value(chip, 0, "W").as_number() - 0.75) < 1e-6,
            "WINNER matches TCalc's triangular chip allocation");
    require(std::abs(point_value(chip, 0, "C50").as_number() - 10.024) < 1e-5,
            "COST matches TCalc's native percentile-to-price grid");
    require(std::abs(point_value(chip, 0, "CE").as_number() - 10.03) < 1e-5,
            "COSTEX returns the weighted cost between its two price boundaries");
    require(std::abs(point_value(chip, 0, "PW").as_number() - 0.75) < 1e-6,
            "PWINNER with zero lag uses the current accumulated distribution");
    require(std::abs(point_value(chip, 0, "LW").as_number() - 0.75) < 1e-6,
            "LWINNER with a one-bar window rebuilds the current distribution");

    auto chip_window = chip_sample();
    auto older_chip_bar = chip_window.at("bars").as_array().front();
    older_chip_bar["date"] = "2026-01-04";
    auto newer_chip_bar = older_chip_bar;
    newer_chip_bar["date"] = "2026-01-05";
    newer_chip_bar["open"] = 20.02;
    newer_chip_bar["high"] = 20.06;
    newer_chip_bar["low"] = 20.00;
    newer_chip_bar["close"] = 20.03;
    newer_chip_bar["amount"] = 200300.0;
    chip_window["bars"] = tdx::Json::array();
    chip_window["bars"].push_back(std::move(newer_chip_bar));
    chip_window["bars"].push_back(std::move(older_chip_bar));
    const auto chip_windows = tdx::evaluate_formula_source_document(
        chip_window, "P:PWINNER(1,CLOSE); L:LWINNER(1,CLOSE); R:PPART(1);", {}, "CHIPWINDOWTEST",
        &chip_context);
    require(std::abs(point_value(chip_windows, 1, "P").as_number() - 1.0) < 1e-6,
            "PWINNER evaluates the distribution from N bars ago at today's price");
    require(std::abs(point_value(chip_windows, 1, "L").as_number() - 0.8) < 1e-6,
            "LWINNER excludes old chips and preserves TCalc's six-bin midpoint rounding");
    require(std::abs(point_value(chip_windows, 1, "R").as_number() - 0.9) < 1e-6,
            "PPART multiplies the native one-period retained turnover ratio");
    const auto chip_analysis =
        tdx::analyze_formula_source("W:WINNER(CLOSE); C:COST(85); CE:COSTEX(CLOSE,REF(CLOSE,1)); "
                                    "PW:PWINNER(5,CLOSE); LW:LWINNER(5,CLOSE); R:PPART(10);");
    require(chip_analysis.at("executable_with_context").as_bool(),
            "all five chip functions become executable with historical capital context");
    bool has_chip_capital = false;
    for (const auto &dependency : chip_analysis.at("market_dependencies").as_array())
        if (dependency.is_string() && dependency.as_string() == "CAPITAL")
            has_chip_capital = true;
    require(has_chip_capital, "all chip functions declare the historical capital dependency");
    auto intraday_chip = chip_sample();
    intraday_chip["period"] = "1m";
    bool rejected_intraday_chip = false;
    try {
        (void)tdx::evaluate_formula_source_document(intraday_chip, "W:LWINNER(5,CLOSE);", {},
                                                    "CHIPINTRADAY", &chip_context);
    } catch (const tdx::Error &) {
        rejected_intraday_chip = true;
    }
    require(rejected_intraday_chip, "the five price-grid chip functions reject non-daily periods");
    const auto intraday_part = tdx::evaluate_formula_source_document(
        intraday_chip, "R:PPART(0);", {}, "PPARTINTRADAY", &chip_context);
    require(point_value(intraday_part, 0, "R").as_number() == 1.0,
            "PPART preserves TCalc's absence of a daily-period guard");

    const auto evaluate_lfs = [](const std::vector<double> &volumes,
                                 const std::vector<double> &native_capitals,
                                 std::string market = "sz", std::string code = "000001") {
        tdx::Json document = tdx::Json::object();
        document["market"] = std::move(market);
        document["code"] = std::move(code);
        document["period"] = "day";
        document["bars"] = tdx::Json::array();
        tdx::Json context = tdx::Json::object();
        context["series"] = tdx::Json::object();
        context["series"]["CAPITAL"] = tdx::Json::object();
        for (std::size_t i = 0; i < volumes.size(); ++i) {
            const std::string date =
                "2026-02-" + std::string(i + 1 < 10 ? "0" : "") + std::to_string(i + 1);
            tdx::Json bar = tdx::Json::object();
            bar["date"] = date;
            bar["time"] = "15:00";
            bar["open"] = 10.0;
            bar["high"] = 10.1;
            bar["low"] = 9.9;
            bar["close"] = 10.0;
            bar["amount"] = volumes[i] * 10.0;
            bar["volume"] = volumes[i];
            document["bars"].push_back(std::move(bar));
            context["series"]["CAPITAL"][date + "|15:00"] = native_capitals[i] / 100.0;
        }
        return tdx::evaluate_formula_source_document(std::move(document), "X:LFS();", {}, "LFSTEST",
                                                     &context);
    };
    const auto require_lfs_vector = [&](const tdx::Json &result,
                                        const std::vector<double> &expected,
                                        const std::string &message) {
        require(result.at("points").as_array().size() == expected.size(), message + " size");
        for (std::size_t i = 0; i < expected.size(); ++i) {
            const auto &value = point_value(result, i, "X");
            if (!std::isfinite(expected[i])) {
                require(value.is_null(), message + " missing at " + std::to_string(i));
            } else {
                require(value.is_number() && std::abs(value.as_number() - expected[i]) < 0.00005,
                        message + " value at " + std::to_string(i));
            }
        }
    };
    constexpr double nan = std::numeric_limits<double>::quiet_NaN();
    const std::vector<double> standard_volume{100, 200, 50, 400, 100, 300, 700, 20};
    const std::vector<double> valid_capital(8, 1000.0);
    require_lfs_vector(
        evaluate_lfs(standard_volume, valid_capital),
        {0, 3.59550452, 13.6147108, 7.20301151, 16.1161156, 14.9956331, 5.48789358, 17.6482849},
        "LFS original-DLL standard vector");
    require_lfs_vector(
        evaluate_lfs(std::vector<double>(8, 100.0), valid_capital),
        {0, 6.05042219, 11.2079668, 15.5987787, 19.3320427, 22.5021114, 25.190485, 27.4674435},
        "LFS retained-turnover recurrence");
    require_lfs_vector(evaluate_lfs(standard_volume, {0, 0, 1000, 1000, 1000, 1000, 1000, 1000}),
                       {nan, nan, 0, 0.863314271, 10.9888277, 11.316988, 4.4044776, 16.7228985},
                       "LFS leading capital gap");
    require_lfs_vector(
        evaluate_lfs(standard_volume, {1000, 0, 0, 1000, 1000, 0, 1000, 1000}),
        {0, 3.59550452, 13.6147108, 7.20301151, 16.1161156, 14.9956331, 5.48789358, 17.6482849},
        "LFS internal capital carry-forward");
    require_lfs_vector(
        evaluate_lfs({0, 100, 0, 200, 50, 0, 100, 300}, valid_capital),
        {nan, 0, 13.3333368, 6.32656908, 15.5171022, 26.7814884, 25.0766449, 14.2726002},
        "LFS zero-volume recovery");
    require_lfs_vector(evaluate_lfs(standard_volume, std::vector<double>(8, 0.0)),
                       std::vector<double>(8, nan), "LFS absent capital");
    require_lfs_vector(evaluate_lfs(standard_volume, valid_capital, "sz", "399001"),
                       std::vector<double>(8, nan), "LFS Shenzhen index gate");
    const auto lfs_analysis = tdx::analyze_formula_source("X:LFS();");
    require(lfs_analysis.at("syntax_supported").as_bool() &&
                lfs_analysis.at("executable_with_context").as_bool() &&
                lfs_analysis.at("requires_automatic_market_context").as_bool() &&
                lfs_analysis.at("automatic_context_dependencies").as_array().size() == 1 &&
                lfs_analysis.at("automatic_context_dependencies").as_array().front().as_string() ==
                    "LFS",
            "LFS declares one automatic historical-capital host dependency");
}

} // namespace formula_engine_test
