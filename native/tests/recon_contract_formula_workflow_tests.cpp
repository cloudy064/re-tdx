#include "recon_contract_test_support.hpp"

namespace recon_contract_test {

void run_formula_render_workflow_contracts() {
    const auto inline_formula_retained = tdx::evaluate_api_contract_response(
        "formula-inline-post", 200, "application/json",
        R"({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula_source_mode":"inline-post","request_body_retained":true,"count":120,"outputs":["FAST","SIGNAL","HIST"],"points":[],"analysis":{"syntax_supported":true,"executable":true}})");
    require(!inline_formula_retained.at("passed").as_bool(),
            "inline formula contract must reject retained request bodies");

    const auto icon_manifest = tdx::evaluate_api_contract_response(
        "formula-icons", 200, "application/json",
        R"json({"schema":"tdx-formula-icon-sprite-v1","profile":"tdx-2025-11-14","resource_type":2,"resource_type_name":"RT_BITMAP","resource_id":2060,"width":1800,"height":18,"cell_width":18,"cell_height":18,"cell_count":100,"official_type_min":1,"official_type_max":51,"png_bytes":34990,"png_endpoint":"/api/v1/formulas/drawicon-strip.png","bitmap_endpoint":"/api/v1/formulas/drawicon-strip.bmp"})json");
    require(icon_manifest.at("passed").as_bool(),
            "native DRAWICON sprite manifest contract failed");
    std::string png_fixture(101, '\0');
    const unsigned char png_signature[]{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    for (std::size_t index = 0; index < sizeof(png_signature); ++index)
        png_fixture[index] = static_cast<char>(png_signature[index]);
    const auto icon_sprite = tdx::evaluate_api_contract_response("formula-drawicon-sprite", 200,
                                                                 "image/png", png_fixture);
    require(icon_sprite.at("passed").as_bool(), "native DRAWICON PNG contract failed");

    auto formula_render_fixture = tdx::Json::parse(
        R"json({"analysis_schema_version":7,"formula_engine":"tdx-source-interpreter-v1","total":379,"source_available":379,"syntax_supported":379,"numeric_signal_safe":379,"degraded_numeric_output":0,"graphics":90,"render_ir_available":90,"render_semantics_materialized":90,"presentation_semantics_faithful":379,"unsupported_presentation_directive":0,"semantic_surrogate":56,"presentation_return_surrogate":56,"by_kind":{"technical":{"graphics":88,"render_semantics_materialized":88},"color-k":{"graphics":2,"render_semantics_materialized":2}},"capabilities":{"schema":"tdx-formula-interpreter-capabilities-v1","supported_function_count":207,"custom_formula_core_function_count":14,"custom_formula_core_symbol_count":6,"custom_formula_core_functions":["ACOS","ASIN","ATAN","CONST","CONSTA","COS","EXISTR","FRACPART","RANGE","ROUND2","SGN","SIGN","SIN","TAN"],"custom_formula_core_symbols":["BARSTATUS","DAY","MONTH","TOTALBARSCOUNT","WEEKDAY","YEAR"],"custom_formula_sequence_statistics_function_count":12,"custom_formula_sequence_statistics_functions":["BARSLASTS","BARSSINCEN","BETAEX","COVAR","FILTERX","FINDHIGH","FINDHIGHBARS","FINDLOW","FINDLOWBARS","RELATE","TMA","XMA"],"custom_formula_rolling_variance_function_count":15,"custom_formula_rolling_variance_functions":["AMA","DEVSQ","HHVLLV","HOD","IFF","IFN","ISVALID","LOD","MAX6","MIN6","MULAR","REFV","STDP","VAR","VARP"],"custom_formula_benchmark_cumulative_function_count":2,"custom_formula_benchmark_cumulative_functions":["BETA","SUMBARSX"],"custom_formula_calendar_filter_function_count":7,"custom_formula_calendar_filter_functions":["ALIGNRIGHT","DAYTODATE","SECTOTIME","TFILT","TFILTER","TIMETOSEC","TTFILTER"],"custom_formula_calendar_filter_symbol_count":2,"custom_formula_calendar_filter_symbols":["TIME2","WEEKOFYEAR"],"custom_formula_security_string_function_count":18,"custom_formula_security_string_functions":["CODELIKE","FINDSTR","NAMEINCLUDE","NAMELIKE","NOT","STR2CON","STRCAT6","STRLEN","STRSPACE","SUBSTR","UPDOWN","VAR2STR","VARCAT","VARCAT6","IST0CODE","ISSTCODE","ISQUITCODE","ISQHQQCODE"],"custom_formula_security_string_symbol_count":1,"custom_formula_security_string_symbols":["STKNAME"],"custom_formula_block_metadata_function_count":1,"custom_formula_block_metadata_functions":["INBLOCK"],"custom_formula_block_metadata_symbol_count":6,"custom_formula_block_metadata_symbols":["FGBLOCK","FGBLOCKNUM","GNBLOCKNUM","HYZSCODE","ZSBLOCK","ZSBLOCKNUM"],"custom_formula_transform_function_count":2,"custom_formula_transform_functions":["FFTRANS","NEWSAR"],"custom_formula_host_calendar_function_count":2,"custom_formula_host_calendar_functions":["ISJYDATE","LOCALDAYNUM"],"tcalc_registry_evidence":{"profile":"tdx-2025-11-14","static_registry_entry_count":390,"static_registry_unique_name_count":390}}})json");
    auto &formula_capabilities = formula_render_fixture["capabilities"];
    formula_capabilities["supported_function_count"] = 262;
    formula_capabilities["automatic_symbol_count"] = 87;
    auto &formula_registry = formula_capabilities["tcalc_registry_evidence"];
    formula_registry["static_registry_boundary_name_count"] = 71;
    formula_registry["static_registry_recognized_name_count"] = 319;
    formula_registry["syntax_only_name_count"] = 4;
    formula_registry["broker_private_signal_name_count"] = 1;
    formula_registry["level2_order_flow_name_count"] = 13;
    formula_registry["live_trading_state_name_count"] = 38;
    formula_registry["plugin_callback_name_count"] = 15;
    formula_registry["static_registry_fully_classified"] = true;
    formula_registry["remaining_public_non_l2_candidate_count"] = 0;
    formula_capabilities["custom_formula_future_path_function_count"] = 1;
    formula_capabilities["custom_formula_future_path_functions"] = tdx::Json::array();
    formula_capabilities["custom_formula_future_path_functions"].push_back("ZIGA");
    formula_capabilities["custom_formula_random_function_count"] = 1;
    formula_capabilities["custom_formula_random_functions"] = tdx::Json::array();
    formula_capabilities["custom_formula_random_functions"].push_back("RAND");
    formula_capabilities["custom_formula_security_score_function_count"] = 2;
    formula_capabilities["custom_formula_security_score_functions"] = tdx::Json::array();
    formula_capabilities["custom_formula_security_score_functions"].push_back("SAFESCORE");
    formula_capabilities["custom_formula_security_score_functions"].push_back("SHINESCORE");
    formula_capabilities["custom_formula_block_metadata_function_count"] = 6;
    formula_capabilities["custom_formula_block_metadata_functions"] = tdx::Json::array();
    for (const auto *name :
         {"BLOCKSETNUM", "FGBKZSCODE", "GETNAMEOFCODE", "GNBKZSCODE", "HORCALC", "INBLOCK"})
        formula_capabilities["custom_formula_block_metadata_functions"].push_back(name);
    formula_capabilities["custom_formula_block_metadata_symbol_count"] = 11;
    formula_capabilities["custom_formula_block_metadata_symbols"] = tdx::Json::array();
    for (const auto *name :
         {"FGBLOCK", "FGBLOCKNUM", "GNBLOCKNUM", "HYZSCODE", "SIMIBLOCK", "ZDBLOCK", "ZDBLOCKNUM",
          "ZHBLOCK", "ZHBLOCKNUM", "ZSBLOCK", "ZSBLOCKNUM"})
        formula_capabilities["custom_formula_block_metadata_symbols"].push_back(name);
    formula_capabilities["custom_formula_single_point_function_count"] = 5;
    formula_capabilities["custom_formula_single_point_functions"] = tdx::Json::array();
    for (const auto *name : {"BKJYONE", "FINONE", "GPJYONE", "GPONEDAT", "SCJYONE"})
        formula_capabilities["custom_formula_single_point_functions"].push_back(name);
    formula_capabilities["custom_formula_external_signal_function_count"] = 2;
    formula_capabilities["custom_formula_external_signal_functions"] = tdx::Json::array();
    formula_capabilities["custom_formula_external_signal_functions"].push_back("EXTERNSTR");
    formula_capabilities["custom_formula_external_signal_functions"].push_back("EXTERNVALUE");
    formula_capabilities["custom_formula_external_series_function_count"] = 3;
    formula_capabilities["custom_formula_external_series_functions"] = tdx::Json::array();
    formula_capabilities["custom_formula_external_series_functions"].push_back("EXTDATA_USER");
    formula_capabilities["custom_formula_external_series_functions"].push_back("SIGNALS_SYS");
    formula_capabilities["custom_formula_external_series_functions"].push_back("SIGNALS_USER");
    formula_capabilities["custom_formula_calendar_filter_function_count"] = 8;
    formula_capabilities["custom_formula_calendar_filter_functions"].push_back("DATETOCUR");
    formula_capabilities["custom_formula_capital_turnover_function_count"] = 1;
    formula_capabilities["custom_formula_capital_turnover_functions"] = tdx::Json::array();
    formula_capabilities["custom_formula_capital_turnover_functions"].push_back("LFS");
    formula_capabilities["custom_formula_machine_clock_function_count"] = 3;
    formula_capabilities["custom_formula_machine_clock_functions"] = tdx::Json::array();
    for (const auto *name : {"MACHINEDATE", "MACHINETIME", "MACHINEWEEK"})
        formula_capabilities["custom_formula_machine_clock_functions"].push_back(name);
    formula_capabilities["custom_formula_directional_bar_function_count"] = 5;
    formula_capabilities["custom_formula_directional_bar_symbol_count"] = 5;
    formula_capabilities["custom_formula_directional_bar_functions"] = tdx::Json::array();
    formula_capabilities["custom_formula_directional_bar_symbols"] = tdx::Json::array();
    for (const auto *name : {"DCLOSE", "DHIGH", "DLOW", "DOPEN", "DVOL"}) {
        formula_capabilities["custom_formula_directional_bar_functions"].push_back(name);
        formula_capabilities["custom_formula_directional_bar_symbols"].push_back(name);
    }
    formula_capabilities["custom_formula_adjustment_function_count"] = 1;
    formula_capabilities["custom_formula_adjustment_symbol_count"] = 1;
    formula_capabilities["custom_formula_adjustment_functions"] = tdx::Json::array();
    formula_capabilities["custom_formula_adjustment_functions"].push_back("TQFLAG");
    formula_capabilities["custom_formula_adjustment_symbols"] = tdx::Json::array();
    formula_capabilities["custom_formula_adjustment_symbols"].push_back("TQFLAG");
    formula_capabilities["custom_formula_kline_auxiliary_symbol_count"] = 2;
    formula_capabilities["custom_formula_kline_auxiliary_symbols"] = tdx::Json::array();
    formula_capabilities["custom_formula_kline_auxiliary_symbols"].push_back("QHJSJ");
    formula_capabilities["custom_formula_kline_auxiliary_symbols"].push_back("ZSTJJ");
    formula_capabilities["custom_formula_type167_text_symbol_count"] = 3;
    formula_capabilities["custom_formula_type167_text_symbols"] = tdx::Json::array();
    formula_capabilities["custom_formula_type167_text_symbols"].push_back("LEVEL1HYBLOCK");
    formula_capabilities["custom_formula_type167_text_symbols"].push_back("MAINBUSINESS");
    formula_capabilities["custom_formula_type167_text_symbols"].push_back("MOREHYBLOCK");
    formula_capabilities["custom_formula_security_stat_function_count"] = 4;
    formula_capabilities["custom_formula_security_stat_symbol_count"] = 4;
    formula_capabilities["custom_formula_security_stat_functions"] = tdx::Json::array();
    formula_capabilities["custom_formula_security_stat_symbols"] = tdx::Json::array();
    for (const auto *name : {"BETAVALUE", "SHAPE_LONG", "SHAPE_MID", "SHAPE_SHORT"}) {
        formula_capabilities["custom_formula_security_stat_functions"].push_back(name);
        formula_capabilities["custom_formula_security_stat_symbols"].push_back(name);
    }
    formula_capabilities["custom_formula_industry_valuation_function_count"] = 2;
    formula_capabilities["custom_formula_industry_valuation_symbol_count"] = 2;
    formula_capabilities["custom_formula_industry_valuation_functions"] = tdx::Json::array();
    formula_capabilities["custom_formula_industry_valuation_symbols"] = tdx::Json::array();
    for (const auto *name : {"HYSJL", "HYSYL"}) {
        formula_capabilities["custom_formula_industry_valuation_functions"].push_back(name);
        formula_capabilities["custom_formula_industry_valuation_symbols"].push_back(name);
    }
    formula_capabilities["custom_formula_market_breadth_function_count"] = 2;
    formula_capabilities["custom_formula_market_breadth_symbol_count"] = 2;
    formula_capabilities["custom_formula_market_breadth_functions"] = tdx::Json::array();
    formula_capabilities["custom_formula_market_breadth_symbols"] = tdx::Json::array();
    for (const auto *name : {"INDEXADV", "INDEXDEC"}) {
        formula_capabilities["custom_formula_market_breadth_functions"].push_back(name);
        formula_capabilities["custom_formula_market_breadth_symbols"].push_back(name);
    }
    formula_capabilities["custom_formula_dynamic_quote_function_count"] = 4;
    formula_capabilities["custom_formula_dynamic_quote_symbol_count"] = 4;
    formula_capabilities["custom_formula_dynamic_quote_functions"] = tdx::Json::array();
    formula_capabilities["custom_formula_dynamic_quote_symbols"] = tdx::Json::array();
    for (const auto *name : {"DYNA_LB", "DYNA_NOW", "DYNA_ZAF", "DYNA_ZAS"}) {
        formula_capabilities["custom_formula_dynamic_quote_functions"].push_back(name);
        formula_capabilities["custom_formula_dynamic_quote_symbols"].push_back(name);
    }
    formula_capabilities["custom_formula_security_relation_function_count"] = 4;
    formula_capabilities["custom_formula_security_relation_symbol_count"] = 4;
    formula_capabilities["custom_formula_security_relation_text_symbol_count"] = 3;
    formula_capabilities["custom_formula_security_relation_functions"] = tdx::Json::array();
    formula_capabilities["custom_formula_security_relation_symbols"] = tdx::Json::array();
    for (const auto *name : {"DPZSCODE", "DPZSNAME", "UNDERCODE", "UNDERLYC"}) {
        formula_capabilities["custom_formula_security_relation_functions"].push_back(name);
        formula_capabilities["custom_formula_security_relation_symbols"].push_back(name);
    }
    formula_capabilities["custom_formula_security_relation_text_symbols"] = tdx::Json::array();
    for (const auto *name : {"DPZSCODE", "DPZSNAME", "UNDERCODE"})
        formula_capabilities["custom_formula_security_relation_text_symbols"].push_back(name);
    formula_capabilities["custom_formula_divfactor_function_count"] = 1;
    formula_capabilities["custom_formula_divfactor_functions"] = tdx::Json::array();
    formula_capabilities["custom_formula_divfactor_functions"].push_back("DIVFACTOR");
    const auto formula_render_coverage =
        tdx::evaluate_api_contract_response("formula-coverage-render-fidelity-live", 200,
                                            "application/json", formula_render_fixture.dump(-1));
    require(formula_render_coverage.at("passed").as_bool(),
            "formula render-fidelity coverage contract failed");

    auto combined_formula_render_fixture = formula_render_fixture;
    for (const auto* field : {"total", "source_available", "syntax_supported",
                              "presentation_semantics_faithful"})
        combined_formula_render_fixture[field] = 380;
    combined_formula_render_fixture["numeric_signal_safe"] = 380;
    const auto combined_formula_render_coverage =
        tdx::evaluate_api_contract_response(
            "formula-coverage-render-fidelity-live", 200,
            "application/json", combined_formula_render_fixture.dump(-1));
    require(combined_formula_render_coverage.at("passed").as_bool(),
            "coverage contract must accept an analyzed opt-in user formula");

    auto falsely_degraded_formula_render_fixture = formula_render_fixture;
    falsely_degraded_formula_render_fixture["numeric_signal_safe"] = 375;
    falsely_degraded_formula_render_fixture["degraded_numeric_output"] = 4;
    const auto falsely_degraded_formula_render_coverage =
        tdx::evaluate_api_contract_response(
            "formula-coverage-render-fidelity-live", 200,
            "application/json", falsely_degraded_formula_render_fixture.dump(-1));
    require(!falsely_degraded_formula_render_coverage.at("passed").as_bool(),
            "coverage contract rejects the retired limit-price approximation gate");

    const auto stale_formula_render_coverage = tdx::evaluate_api_contract_response(
        "formula-coverage-render-fidelity-live", 200, "application/json",
        R"json({"analysis_schema_version":6,"formula_engine":"tdx-source-interpreter-v1","total":379,"source_available":379,"syntax_supported":379,"numeric_signal_safe":379,"degraded_numeric_output":0,"graphics":90,"render_ir_available":90,"render_semantics_materialized":0,"presentation_semantics_faithful":289,"unsupported_presentation_directive":0,"semantic_surrogate":56,"presentation_return_surrogate":56,"by_kind":{"technical":{"graphics":88,"render_semantics_materialized":0},"color-k":{"graphics":2,"render_semantics_materialized":0}}})json");
    require(!stale_formula_render_coverage.at("passed").as_bool(),
            "coverage contract must reject stale render-degraded classification");

    const auto inline_icons = tdx::evaluate_api_contract_response(
        "formula-drawicon-inline-post", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula_source_mode":"inline-post","formula":"CONTRACT_DRAWICON","market":"sz","code":"000001","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"function":"DRAWICON","kind":"icon","icon_coordinate_space":"bar-price","icon_renderer":"tcalc-resource-bitmap","icon_sprite_endpoint":"/api/v1/formulas/drawicon-strip.png","icon_resource_type":2,"icon_resource_id":2060,"icon_official_type_min":1,"icon_official_type_max":51,"events":[{"icon_price":10.1,"icon_type":49,"icon_type_available":true,"icon_sprite_cell_available":true,"icon_sprite_x":864,"icon_vertical_align":"below-price"}]},{"function":"DRAWICON","kind":"icon","icon_coordinate_space":"bar-price","icon_renderer":"tcalc-resource-bitmap","icon_sprite_endpoint":"/api/v1/formulas/drawicon-strip.png","icon_resource_type":2,"icon_resource_id":2060,"icon_official_type_min":1,"icon_official_type_max":51,"events":[{"icon_price":11.1,"icon_type":51,"icon_type_available":true,"icon_sprite_cell_available":true,"icon_sprite_x":900,"icon_vertical_align":"above-price"}]}]}})json");
    require(inline_icons.at("passed").as_bool(), "inline native DRAWICON event contract failed");

    const auto inline_drawcframe = tdx::evaluate_api_contract_response(
        "formula-drawcframe-inline-post", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"CONTRACT_DRAWCFRAME","market":"sz","code":"000001","render_environment":{"schema":"tdx-formula-render-environment-v1","native_source":"TdxW.exe","pixel_font_equivalent":false,"annotation":{"background_mode":"transparent","background_mode_value":1,"text_encoding":"Win32-ANSI","font":{"native_font_table_index":1,"user_ini_font_ordinal":2,"face":"Arial","logical_height":15,"weight":400}}},"render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[)json"
        R"json({"function":"DRAWTEXT","annotation_frame_directive_present":true,"annotation_frame_supported":true,"annotation_frame_directive_effect":"native-price-text-frame","annotation_frame_native_renderer":"tdxw-sub_961290","annotation_frame_anchor":"bar-high-low","annotation_frame_price_argument_ignored":true,"annotation_frame_drawabove_ignored":true,"annotation_frame_side_rule":"available-below<=available-above?above:below","annotation_frame_horizontal_rule":"bar-x-minus-half-text-width","annotation_frame_leader_length_pixels":20,"annotation_frame_leader_style":"dotted","annotation_frame_leader_dot_step_pixels":4,"annotation_frame_corner_radius_pixels":4,"annotation_frame_width_padding_pixels":5,"annotation_frame_height_padding_pixels":4,"annotation_frame_text_inset_x_pixels":3,"annotation_frame_text_inset_y_pixels":3,"annotation_frame_fill_alpha_byte":80,"annotation_frame_fill_compositing":"gdiplus-argb-solid-fill","annotation_frame_border_alpha_byte":255,"annotation_frame_border_width_pixels":1,"annotation_frame_multiline_rule":"per-line-overlap-same-anchor","events":[{"annotation_frame":true,"annotation_text_available":true}]},)json"
        R"json({"function":"DRAWTEXT_FIX","annotation_frame_directive_present":true,"annotation_frame_supported":false,"annotation_frame_directive_effect":"ignored-by-native-renderer","events":[{"annotation_frame":false}]},)json"
        R"json({"function":"DRAWNUMBER_FIX","annotation_frame_directive_present":true,"annotation_frame_supported":false,"annotation_frame_directive_effect":"not-forwarded-to-native-renderer","events":[{"annotation_frame":false}]})json"
        R"json(]}})json");
    require(inline_drawcframe.at("passed").as_bool(),
            "inline native DRAWCFRAME geometry and applicability contract failed");

    const auto legacy_drawcframe = tdx::evaluate_api_contract_response(
        "formula-drawcframe-inline-post", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"CONTRACT_DRAWCFRAME","market":"sz","code":"000001","render_environment":{"schema":"tdx-formula-render-environment-v1","native_source":"TdxW.exe","pixel_font_equivalent":false,"annotation":{"background_mode":"transparent","background_mode_value":1,"text_encoding":"Win32-ANSI","font":{"native_font_table_index":1,"user_ini_font_ordinal":2,"face":"Arial","logical_height":15,"weight":400}}},"render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"function":"DRAWTEXT","style":{"draw_cframe":true},"events":[{"annotation_frame":true,"annotation_text_available":true}]}]}})json");
    require(!legacy_drawcframe.at("passed").as_bool(),
            "DRAWCFRAME contract must reject the former generic panel semantics");

    auto inline_price_annotations_document = tdx::evaluate_formula_source_document(
        formula_sample(120),
        "DRAWTEXT(ISLASTBAR,LOW,'A&&B'),COLORRED;"
        "DRAWTEXT(ISLASTBAR,HIGH,'UP&NEXT'),COLORGREEN,DRAWABOVE;"
        "DRAWNUMBER(ISLASTBAR,CLOSE,12.34),COLORBLUE;"
        "DRAWNUMBER(ISLASTBAR,OPEN,56.78),COLORYELLOW,DRAWABOVE;"
        "DRAWTEXT(0.5,CLOSE,'FILTERED');"
        "DRAWNUMBER(ISLASTBAR,DRAWNULL,1.23);X:CLOSE;",
        {}, "CONTRACT_PRICE_ANNOTATIONS");
    inline_price_annotations_document["formula_source_mode"] = "inline-post";
    inline_price_annotations_document["render_environment"] =
        tdx::formula_render_environment_document();
    const auto inline_price_annotations = tdx::evaluate_api_contract_response(
        "formula-price-annotations-inline-post", 200, "application/json",
        inline_price_annotations_document.dump(-1));
    require(inline_price_annotations.at("passed").as_bool(),
            "inline unframed DRAWTEXT/DRAWNUMBER native contract failed");

    auto kline_precision_sample = formula_sample(120);
    kline_precision_sample["price_precision"] = 2;
    auto kline_precision_annotations_document =
        tdx::evaluate_formula_source_document(
            std::move(kline_precision_sample),
            "DRAWTEXT(ISLASTBAR,LOW,'A&&B'),COLORRED;"
            "DRAWTEXT(ISLASTBAR,HIGH,'UP&NEXT'),COLORGREEN,DRAWABOVE;"
            "DRAWNUMBER(ISLASTBAR,CLOSE,12.34),COLORBLUE;"
            "DRAWNUMBER(ISLASTBAR,OPEN,56.78),COLORYELLOW,DRAWABOVE;"
            "DRAWTEXT(0.5,CLOSE,'FILTERED');"
            "DRAWNUMBER(ISLASTBAR,DRAWNULL,1.23);X:CLOSE;",
            {}, "CONTRACT_PRICE_ANNOTATIONS");
    kline_precision_annotations_document["formula_source_mode"] = "inline-post";
    kline_precision_annotations_document["render_environment"] =
        tdx::formula_render_environment_document();
    const auto kline_precision_annotations =
        tdx::evaluate_api_contract_response(
            "formula-price-annotations-inline-post", 200,
            "application/json", kline_precision_annotations_document.dump(-1));
    require(kline_precision_annotations.at("passed").as_bool(),
            "price annotation contract must accept K-line precision provenance");

    auto legacy_price_annotations = inline_price_annotations_document;
    legacy_price_annotations["render_ir"]["primitives"].as_array()[0]["annotation_edge_behavior"] =
        "flip-at-pane-edge";
    const auto rejected_legacy_price_annotations =
        tdx::evaluate_api_contract_response("formula-price-annotations-inline-post", 200,
                                            "application/json", legacy_price_annotations.dump(-1));
    require(!rejected_legacy_price_annotations.at("passed").as_bool(),
            "unframed annotation contract must reject guessed edge flipping");

    auto inline_drawnumber_dif_document = tdx::evaluate_formula_source_document(
        formula_sample(120),
        "DRAWNUMBER_DIF(CURRBARSCOUNT<=10,2,11,4),COLORGREEN,DRAWABOVE;"
        "DRAWNUMBER_DIF(CURRBARSCOUNT=20,1,1,3),COLORRED;"
        "DRAWNUMBER_DIF(CURRBARSCOUNT=25,0,7,2),COLORBLUE;X:CLOSE;",
        {}, "CONTRACT_DRAWNUMBER_DIF");
    inline_drawnumber_dif_document["formula_source_mode"] = "inline-post";
    inline_drawnumber_dif_document["render_environment"] =
        tdx::formula_render_environment_document();
    const auto inline_drawnumber_dif = tdx::evaluate_api_contract_response(
        "formula-drawnumber-dif-inline-post", 200, "application/json",
        inline_drawnumber_dif_document.dump(-1));
    require(inline_drawnumber_dif.at("passed").as_bool(),
            "inline native DRAWNUMBER_DIF state and STYLE=2 contract failed");

    auto nested_drawnumber_dif = inline_drawnumber_dif_document;
    nested_drawnumber_dif["render_ir"]["primitives"].as_array()[0]["sequence_overlap_rule"] =
        "nested-trigger-expansion";
    const auto legacy_drawnumber_dif =
        tdx::evaluate_api_contract_response("formula-drawnumber-dif-inline-post", 200,
                                            "application/json", nested_drawnumber_dif.dump(-1));
    require(!legacy_drawnumber_dif.at("passed").as_bool(),
            "DRAWNUMBER_DIF contract must reject overlapping nested expansion");

    auto inline_series_sticks_document =
        tdx::evaluate_formula_source_document(formula_sample(120),
                                              "COLOR:IF(CURRBARSCOUNT=1,1,-1),COLORSTICK;"
                                              "VOLUME:VOL,VOLSTICK;",
                                              {}, "CONTRACT_SERIES_STICKS");
    inline_series_sticks_document["formula_source_mode"] = "inline-post";
    inline_series_sticks_document["render_environment"] =
        tdx::formula_render_environment_document();
    const auto inline_series_sticks = tdx::evaluate_api_contract_response(
        "formula-series-sticks-inline-post", 200, "application/json",
        inline_series_sticks_document.dump(-1));
    require(inline_series_sticks.at("passed").as_bool(),
            "inline native COLORSTICK/VOLSTICK contract failed");

    auto wide_colorstick = inline_series_sticks_document;
    wide_colorstick["render_ir"]["primitives"].as_array()[0]["series_stick_shape"] =
        "wide-filled-histogram";
    const auto rejected_wide_colorstick = tdx::evaluate_api_contract_response(
        "formula-series-sticks-inline-post", 200, "application/json", wide_colorstick.dump(-1));
    require(!rejected_wide_colorstick.at("passed").as_bool(),
            "series-stick contract must reject the former wide histogram guess");

    auto inline_native_series_document = tdx::evaluate_formula_source_document(
        formula_sample(120),
        "BASE:CLOSE;"
        "DOTTED:IF(CURRBARSCOUNT=1 OR CURRBARSCOUNT=3,CLOSE,DRAWNULL),"
        "DOTLINE,LINETHICK2,COLORRED;"
        "STEM:CLOSE,STICK,COLORGREEN;"
        "BOTH:CLOSE,LINESTICK,COLORBLUE;"
        "CIRCLE:CLOSE,CIRCLEDOT,COLORCYAN;"
        "CROSS:CLOSE,CROSSDOT,COLORYELLOW;"
        "POINT:CLOSE,POINTDOT,LINETHICK3,COLORMAGENTA;"
        "LASTDOT:CLOSE,COLORSTICK,DOTLINE;"
        "LASTCOLOR:CLOSE,DOTLINE,COLORSTICK;",
        {}, "CONTRACT_NATIVE_SERIES_STYLES");
    inline_native_series_document["formula_source_mode"] = "inline-post";
    inline_native_series_document["render_environment"] =
        tdx::formula_render_environment_document();
    const auto inline_native_series = tdx::evaluate_api_contract_response(
        "formula-native-series-styles-inline-post", 200, "application/json",
        inline_native_series_document.dump(-1));
    require(inline_native_series.at("passed").as_bool(),
            "inline native type 0/4/5/6/7/8/9 series contract failed");

    auto wide_linestick = inline_native_series_document;
    wide_linestick["render_ir"]["primitives"].as_array()[3]["series_native_stem_shape"] =
        "wide-histogram";
    const auto rejected_wide_linestick =
        tdx::evaluate_api_contract_response("formula-native-series-styles-inline-post", 200,
                                            "application/json", wide_linestick.dump(-1));
    require(!rejected_wide_linestick.at("passed").as_bool(),
            "native series contract must reject the former LINESTICK histogram guess");

    const auto inline_background = tdx::evaluate_api_contract_response(
        "formula-background-inline-post", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula_source_mode":"inline-post","formula":"CONTRACT_BACKGROUND","market":"sz","code":"000001","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"function":"DRAWGBK_DIV","kind":"background","background_condition_scope":"per-bar-contiguous-regions","background_native_render_type":21,"background_native_renderer":"tdxw-sub_95A6C0-sub_95A330","background_condition_true_rule":"abs(value-1)<0.0001","background_region_rule":"maximal-contiguous-native-true-bars","background_alpha_mode_min":10,"background_alpha_mode_max":20,"background_alpha_rule":"255*(mode-10)/10","background_fill_mode_argument":3,"background_range_argument":4,"events":[{"index":0,"background_color1_available":true,"background_color2_available":true,"background_color1_ref":197121,"background_color2_ref":394500,"background_fill_mode":0,"background_fill_mode_name":"vertical-gradient","background_fill_compositing":"opaque-gdi-gradient-or-solid","background_fill_alpha_byte":255,"background_fill_alpha_denominator":255,"background_range":0,"background_range_name":"pane","background_region_leader":true,"background_region_start_index":0,"background_region_end_index":2,"background_price_aggregation":"whole-pane"},{"index":1}]},{"function":"DRAWGBK_DIV","kind":"background","background_condition_scope":"per-bar-contiguous-regions","background_native_render_type":21,"background_native_renderer":"tdxw-sub_95A6C0-sub_95A330","background_condition_true_rule":"abs(value-1)<0.0001","background_region_rule":"maximal-contiguous-native-true-bars","background_alpha_mode_min":10,"background_alpha_mode_max":20,"background_alpha_rule":"255*(mode-10)/10","background_fill_mode_argument":3,"background_range_argument":4,"events":[{"index":0,"background_color1_available":true,"background_color2_available":true,"background_color1_ref":197121,"background_color2_ref":394500,"background_fill_mode":17,"background_fill_mode_name":"alpha-solid","background_fill_compositing":"gdiplus-argb-solid-color1","background_fill_alpha_byte":178,"background_fill_alpha_denominator":255,"background_range":0,"background_range_name":"pane","background_region_leader":true,"background_region_start_index":0,"background_region_end_index":2,"background_price_aggregation":"whole-pane"},{"index":1}]}]}})json");
    require(inline_background.at("passed").as_bool(),
            "inline native background-region contract failed");

    const auto legacy_background = tdx::evaluate_api_contract_response(
        "formula-background-inline-post", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula_source_mode":"inline-post","formula":"CONTRACT_BACKGROUND","market":"sz","code":"000001","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"function":"DRAWGBK_DIV","kind":"background","background_condition_scope":"per-bar-contiguous-regions","background_fill_mode_argument":3,"background_range_argument":4,"events":[{"index":119,"background_color1_available":true,"background_color2_available":true,"background_color1_ref":197121,"background_color2_ref":394500,"background_fill_mode":0,"background_fill_mode_name":"vertical-gradient","background_range":0,"background_range_name":"pane"}]}]}})json");
    require(!legacy_background.at("passed").as_bool(),
            "background contract must reject the former last-bar-only event");

    const auto legacy_translucent_background = tdx::evaluate_api_contract_response(
        "formula-background-inline-post", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula_source_mode":"inline-post","formula":"CONTRACT_BACKGROUND","market":"sz","code":"000001","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"function":"DRAWGBK_DIV","kind":"background","background_condition_scope":"per-bar-contiguous-regions","background_fill_mode_argument":3,"background_range_argument":4,"events":[{"index":0,"background_color1_available":true,"background_color2_available":true,"background_color1_ref":197121,"background_color2_ref":394500,"background_fill_mode":17,"background_fill_mode_name":"vendor-defined","background_range":0,"background_range_name":"pane"},{"index":1}]}]}})json");
    require(!legacy_translucent_background.at("passed").as_bool(),
            "background contract must reject the old guessed mode-17 opacity");

    const auto explicit_formula = tdx::evaluate_api_contract_response(
        "formula-explicit-context-post", 200, "application/json; charset=utf-8",
        R"({"engine":"tdx-source-interpreter-v1","formula_source_mode":"inline-post","request_body_retained":false,"analysis":{"syntax_supported":true,"executable":false,"executable_with_context":false,"explicit_context_bindable":true,"requires_automatic_market_context":false,"explicit_context_bindings_required":["L2_AMO#0#2","LARGEINTRDVOL","LARGEOUTTRDVOL"]},"points":[{"date":"2026-08-05","time":"15:00","values":{"AMO0":123456,"FLOW":180}}]})");
    require(explicit_formula.at("passed").as_bool(),
            "explicit authorized formula context POST contract failed");

    const auto library_context_formula = tdx::evaluate_api_contract_response(
        "formula-library-context-post", 200, "application/json; charset=utf-8",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula_source_mode":"library-post","library_formula_kind":"technical","library_formula_code":"ZJLX","formula":"ZJLX","request_body_retained":false,"points":[{"date":"2026-08-07","time":"15:00","values":{"主买净额":-400}}]})json");
    require(library_context_formula.at("passed").as_bool(),
            "built-in formula explicit context POST contract failed");

    const auto library_context_leaked = tdx::evaluate_api_contract_response(
        "formula-library-context-post", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","formula_source_mode":"library-post","library_formula_kind":"technical","library_formula_code":"ZJLX","formula":"ZJLX","request_body_retained":false,"source_text":"private","points":[{"date":"2026-08-07","time":"15:00","values":{"主买净额":-400}}]})json");
    require(!library_context_leaked.at("passed").as_bool(),
            "library formula context contract must reject retained source");

    const auto context_template = tdx::evaluate_api_contract_response(
        "formula-context-template-live", 200, "application/json; charset=utf-8",
        R"json({"schema":"tdx-formula-explicit-context-v1","automatic_market_context":false,"series":{"L2_AMO#0#2":{"2026-08-08|15:00":null},"L2_AMO#0#3":{"2026-08-08|15:00":null},"L2_AMO#1#2":{"2026-08-08|15:00":null},"L2_AMO#1#3":{"2026-08-08|15:00":null},"L2_AMO#2#2":{"2026-08-08|15:00":null},"L2_AMO#2#3":{"2026-08-08|15:00":null},"L2_AMO#3#2":{"2026-08-08|15:00":null},"L2_AMO#3#3":{"2026-08-08|15:00":null}},"_template":{"schema":"tdx-formula-explicit-context-template-v1","binding_count":8}})json");
    require(context_template.at("passed").as_bool(), "formula context template contract failed");

    auto kline_context_body = tdx::Json::parse(
        R"json({"schema":"tdx-formula-explicit-context-v1","automatic_market_context":false,"series":{},"_template":{"schema":"tdx-formula-explicit-context-template-v1","binding_count":8,"stamp_count":20,"stamp_source":"kline","market":"sz","code":"000001","period":"day","bar_count":20}})json");
    for (int binding = 0; binding < 8; ++binding) {
        auto values = tdx::Json::object();
        for (int bar = 0; bar < 20; ++bar)
            values["2026-07-" + std::to_string(10 + bar) + "|15:00"] = nullptr;
        kline_context_body["series"]["L2_AMO#" + std::to_string(binding)] = std::move(values);
    }
    const auto kline_context =
        tdx::evaluate_api_contract_response("formula-context-template-kline-live", 200,
                                            "application/json", kline_context_body.dump(-1));
    require(kline_context.at("passed").as_bool(), "K-line batch context template contract failed");

    const auto limit_price_context = tdx::evaluate_api_contract_response(
        "formula-limit-price-context-template-live", 200,
        "application/json; charset=utf-8",
        R"json({"schema":"tdx-formula-explicit-context-v1","automatic_market_context":false,"formula_scalar_bindings":{"HOST_EVALUATOR_MARKET_WORD_RAW":null,"HOST_TYPE120_SECURITY_CLASS_RAW":null},"series":{},"_template":{"schema":"tdx-formula-explicit-context-template-v1","binding_count":2,"scalar_binding_count":2,"series_binding_count":0,"stamp_count":0,"stamp_source":"not-required-scalar-only","kline_fetch_skipped":true,"requested_market":"sz","requested_code":"000001","requested_period":"day","bar_count":0,"bindings":[{"name":"HOST_EVALUATOR_MARKET_WORD_RAW","source_kind":"caller-host-raw-scalar","value_shape":"u16-scalar","required":true},{"name":"HOST_TYPE120_SECURITY_CLASS_RAW","source_kind":"caller-host-raw-scalar","value_shape":"u16-scalar","required":true}]}})json");
    require(limit_price_context.at("passed").as_bool(),
            "limit-price raw scalar context template contract failed");
    const auto duplicated_limit_price_context =
        tdx::evaluate_api_contract_response(
            "formula-limit-price-context-template-live", 200,
            "application/json",
            R"json({"schema":"tdx-formula-explicit-context-v1","automatic_market_context":false,"formula_scalar_bindings":{},"series":{"HOST_EVALUATOR_MARKET_WORD_RAW":{"2026-08-13|15:00":null},"HOST_TYPE120_SECURITY_CLASS_RAW":{"2026-08-13|15:00":null}},"_template":{"binding_count":2,"scalar_binding_count":0,"series_binding_count":2,"stamp_count":1,"bindings":[]}})json");
    require(!duplicated_limit_price_context.at("passed").as_bool(),
            "limit-price context contract rejects per-bar raw host duplication");

    const auto cross_market_audit = tdx::evaluate_api_contract_response(
        "formula-cross-market-audit-hk-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","market":"31","code":"00700","market_scope":"tdx-expansion","audit":{"passed":230,"errors":0,"unreported":0},"formulas":[{"code":"SHORTVOL","status":"passed"}]})json");
    require(cross_market_audit.at("passed").as_bool(),
            "cross-market formula audit contract failed");

    const auto futures_formula = tdx::evaluate_api_contract_response(
        "formula-cross-market-futures-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","market":"47","code":"IFL9","formula":"CCL","expansion_market":true,"points":[{"values":{"持仓量":123456}}],"render_ir":{"primitives":[{"function":"STICKLINE","events":[{"stick_mode":"center-half","stick_width":2,"stick_width_ratio":0.5,"stick_price1_used":false,"stick_anchor":"pane-middle","stick_occupancy":"half"}]}]}})json");
    require(futures_formula.at("passed").as_bool(), "cross-market futures formula contract failed");

    const auto contract_multiplier = tdx::evaluate_api_contract_response(
        "formula-contract-multiplier-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","market":"47","code":"IFL9","formula":"CONTRACT_MULTIPLIER","expansion_market":true,"points":[{"values":{"合约乘数":300}}],"context_metadata":{"contract_multiplier":300,"contract_multiplier_raw":300,"contract_multiplier_category":3,"contract_multiplier_mode":"tcalc-opcode1252-type105-offset42-signed-int16-broadcast","contract_multiplier_source":"tdx-7727-0x23f5-offset56-u32-low-word"}})json");
    require(contract_multiplier.at("passed").as_bool(),
            "cross-market contract MULTIPLIER formula contract failed");

    const auto wrong_contract_multiplier = tdx::evaluate_api_contract_response(
        "formula-contract-multiplier-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","market":"47","code":"IFL9","formula":"CONTRACT_MULTIPLIER","expansion_market":true,"points":[{"values":{"合约乘数":200}}],"context_metadata":{"contract_multiplier":200,"contract_multiplier_raw":200,"contract_multiplier_category":3,"contract_multiplier_mode":"tcalc-opcode1252-type105-offset42-signed-int16-broadcast","contract_multiplier_source":"tdx-7727-0x23f5-offset56-u32-low-word"}})json");
    require(!wrong_contract_multiplier.at("passed").as_bool(),
            "contract MULTIPLIER contract must reject a wrong IF multiplier");

    const auto auxiliary_fields = tdx::evaluate_api_contract_response(
        "formula-kline-auxiliary-fields-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","market":"47","code":"IFL9","formula":"CONTRACT_KLINE_AUXILIARY","expansion_market":true,"auxiliary_field":"settlement_price","points":[{"date":"2026-08-06","values":{"分时均价":4579.2,"结算价":4579.2}},{"date":"2026-08-07","values":{"分时均价":4611.2,"结算价":4611.2}}]})json");
    require(auxiliary_fields.at("passed").as_bool(),
            "shared ZSTJJ/QHJSJ K-line auxiliary contract failed");

    const auto split_auxiliary_fields = tdx::evaluate_api_contract_response(
        "formula-kline-auxiliary-fields-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","market":"47","code":"IFL9","formula":"CONTRACT_KLINE_AUXILIARY","expansion_market":true,"auxiliary_field":"settlement_price","points":[{"date":"2026-08-06","values":{"分时均价":4579.2,"结算价":4579.2}},{"date":"2026-08-07","values":{"分时均价":4611.2,"结算价":4600}}]})json");
    require(!split_auxiliary_fields.at("passed").as_bool(),
            "auxiliary contract must reject diverging native aliases");

    const auto option_formula = tdx::evaluate_api_contract_response(
        "formula-cross-market-option-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","market":"7","code":"HO8W03UX","formula":"VOLATILITY","expansion_market":true,"points":[{"values":{"隐含波动率":18.5}}]})json");
    require(option_formula.at("passed").as_bool(), "cross-market option formula contract failed");

    const auto option_formula_missing_iv = tdx::evaluate_api_contract_response(
        "formula-cross-market-option-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","market":"7","formula":"VOLATILITY","expansion_market":true,"points":[{"values":{"隐含波动率":null}}]})json");
    require(!option_formula_missing_iv.at("passed").as_bool(),
            "option formula contract must require a numeric latest IV");

    const auto sqjz_sequence = tdx::evaluate_api_contract_response(
        "formula-sqjz-sequence-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"SQJZ","market":"sz","code":"000001","future_execution_mode":"explicit-read-only-lookahead","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"function":"DRAWNUMBER_DIF","kind":"sequence-number","sequence_semantics":"increment-by-one-on-consecutive-bars","events":[{"index":12,"source_index":10,"sequence_label":"3","anchor":"bar-low"}]}]}})json");
    require(sqjz_sequence.at("passed").as_bool(), "SQJZ expanded sequence render contract failed");

    const auto wavekx_partline = tdx::evaluate_api_contract_response(
        "formula-wavekx-partline-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"WAVEKX","market":"sz","code":"000001","future_execution_mode":"explicit-read-only-lookahead","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"function":"PARTLINE","segment_directions":{"0":"current-to-next","1":"previous-to-current"},"events":[{"index":11,"segment_direction":"previous-to-current","segment_from_index":10,"segment_to_index":11}]}]}})json");
    require(wavekx_partline.at("passed").as_bool(), "WAVEKX PARTLINE direction contract failed");

    const auto rgband_fill = tdx::evaluate_api_contract_response(
        "formula-rgband-fill-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"RGBAND","market":"sz","code":"000001","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"function":"DRAWBAND","band_fill_rule":"arg0>arg2?arg1:arg3","band_fill_opacity":1,"band_fill_compositing":"opaque-gdi-stroke-and-fill-path","events":[{"index":0,"band_side":"arg0-above","fill_color_argument":1,"fill_color_available":true,"fill_color_ref":255}]}]}})json");
    require(rgband_fill.at("passed").as_bool(), "RGBAND dynamic fill contract failed");

    const auto rgband_translucent_legacy = tdx::evaluate_api_contract_response(
        "formula-rgband-fill-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"RGBAND","market":"sz","code":"000001","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"function":"DRAWBAND","band_fill_rule":"arg0>arg2?arg1:arg3","band_fill_opacity":0.24,"events":[{"index":0,"band_side":"arg0-above","fill_color_argument":1,"fill_color_available":true,"fill_color_ref":255}]}]}})json");
    require(!rgband_translucent_legacy.at("passed").as_bool(),
            "RGBAND contract must reject the old translucent fill semantics");

    const auto tjcjl_stickline = tdx::evaluate_api_contract_response(
        "formula-tjcjl-stickline-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"TJCJL","market":"sz","code":"000001","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"function":"STICKLINE","stick_width_standard":4,"stick_center_modes_price1_ignored":true,"stick_modes":{"0":"solid","-1":"dashed-hollow","1":"solid-hollow","2":"center-full","3":"center-half","other-nonzero":"solid-hollow"},"events":[{"index":0,"stick_mode":"solid","stick_width":1,"stick_width_ratio":0.25,"stick_price1_used":true,"stick_anchor":"price-pair"}]},{"function":"DRAWTEXT_FIX","annotation_coordinate_space":"pane-fraction","annotation_alignments":{"0":"left","1":"right"},"events":[{"annotation_text_available":true,"annotation_text":"说明","annotation_lines":["说明"],"annotation_x":0,"annotation_y":0,"annotation_horizontal_align":"left"}]}]}})json");
    require(!tjcjl_stickline.at("passed").as_bool(),
            "TJCJL contract must reject fixed text without native font semantics");
    const auto tjcjl_native_text = tdx::evaluate_api_contract_response(
        "formula-tjcjl-stickline-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"TJCJL","market":"sz","code":"000001","render_environment":{"schema":"tdx-formula-render-environment-v1","native_source":"TdxW.exe","pixel_font_equivalent":false,"annotation":{"background_mode":"transparent","background_mode_value":1,"text_encoding":"Win32-ANSI","font":{"native_font_table_index":1,"user_ini_font_ordinal":2,"face":"Arial","logical_height":15,"weight":400}}},"render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[)json"
        R"json({"function":"STICKLINE","stick_width_standard":4,"stick_center_modes_price1_ignored":true,"stick_modes":{"0":"solid","-1":"dashed-hollow","1":"solid-hollow","2":"center-full","3":"center-half","other-nonzero":"solid-hollow"},"events":[{"index":0,"stick_mode":"solid","stick_width":1,"stick_width_ratio":0.25,"stick_price1_used":true,"stick_anchor":"price-pair"}]},)json"
        R"json({"function":"DRAWTEXT_FIX","annotation_coordinate_space":"pane-fraction","annotation_native_render_type":7,"annotation_native_renderer":"tdxw-sub_958F90","annotation_font_table_index":1,"annotation_background_mode":"transparent","annotation_text_api":"TextOutA","annotation_alignments":{"0":"left","1":"right"},"events":[{"annotation_text_available":true,"annotation_text":"说明","annotation_lines":["说明"],"annotation_x":0,"annotation_y":0,"annotation_horizontal_align":"left"}]})json"
        R"json(]}})json");
    require(tjcjl_native_text.at("passed").as_bool(),
            "TJCJL native STICKLINE and fixed-text font contract failed");

    const auto ichimoku_stickline = tdx::evaluate_api_contract_response(
        "formula-ichimoku-stickline-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"ICHIMOKU","market":"sz","code":"000001","future_execution_mode":"explicit-read-only-lookahead","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"function":"STICKLINE","stick_width_standard":4,"stick_center_modes_price1_ignored":true,"stick_modes":{"0":"solid","-1":"dashed-hollow","1":"solid-hollow","2":"center-full","3":"center-half","other-nonzero":"solid-hollow"},"events":[{"index":26,"stick_mode":"solid-hollow","stick_width":2,"stick_width_ratio":0.5,"stick_price1_used":true,"stick_anchor":"price-pair"}]},{"function":"PLOYLINE","kind":"polyline","segment_coordinate_space":"bar-price","polyline_vertex_rule":"condition-true","polyline_connection_rule":"previous-vertex-to-current","events":[{"index":27,"segment_from_index":26,"segment_from_price":10.5,"segment_to_index":27,"segment_to_price":10.6}]}]}})json");
    require(ichimoku_stickline.at("passed").as_bool(),
            "ICHIMOKU fractional EMPTY and PLOYLINE contract failed");

    const auto cyx_drawline = tdx::evaluate_api_contract_response(
        "formula-cyx-drawline-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"CYX","market":"sz","code":"000001","future_execution_mode":"explicit-read-only-lookahead","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"function":"DRAWLINE","kind":"draw-line","segment_coordinate_space":"bar-price","line_start_condition_argument":0,"line_start_price_argument":1,"line_end_condition_argument":2,"line_end_price_argument":3,"line_expand_argument":4,"events":[{"segment_from_index":172,"segment_from_price":11.6,"segment_anchor_to_index":199,"segment_anchor_to_price":11.39,"segment_to_index":239,"segment_to_price":11.078,"segment_slope_per_bar":-0.0077,"segment_expansion":"right"}]}]}})json");
    require(cyx_drawline.at("passed").as_bool(),
            "CYX DRAWLINE anchor and expansion contract failed");

    const auto fkx_candles = tdx::evaluate_api_contract_response(
        "formula-fkx-candles-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"FKX","market":"sz","code":"000001","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"function":"DRAWKLINE","kind":"candlestick","candle_argument_order":["high","open","low","close"],"candle_color_rule":"close>=open?up:down","events":[{"arguments":[-9,-9.8,-11,-10]}]}]}})json");
    require(fkx_candles.at("passed").as_bool(), "FKX DRAWKLINE candle argument contract failed");

    const auto slzt_line_stick = tdx::evaluate_api_contract_response(
        "formula-slzt-linestick-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"SLZT","market":"sz","code":"000001","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"statement":"青龙","function":"SAR","kind":"line","finite_point_count":240,"series_native_mode":"line-stick","series_native_render_type":5,"series_native_renderer":"tdxw-sub_957F70","series_native_stem_shape":"one-pixel-vertical-line","series_native_missing_value_rule":"break-contiguous-run","line_stick":true,"line_stick_components":["zero-baseline-stick","indicator-line"],"line_stick_baseline":0,"line_stick_draw_order":"sticks-then-line"}]}})json");
    require(slzt_line_stick.at("passed").as_bool(),
            "SLZT LINESTICK dual-component contract failed");

    const auto mixed_render_order = tdx::evaluate_api_contract_response(
        "formula-mixed-render-order-inline-post", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"CONTRACT_MIXED_ORDER","market":"sz","code":"000001","render_ir":{"schema":"tdx-formula-render-ir-v1","primitive_order":"source-statement-order","primitive_order_contiguous":true,"source_statement_count":4,"primitives":[{"statement_index":1,"render_order":0,"render_order_semantics":"source-statement-order","function":"STICKLINE"},{"statement_index":2,"render_order":1,"render_order_semantics":"source-statement-order","function":"SERIES"},{"statement_index":3,"render_order":2,"render_order_semantics":"source-statement-order","function":"DRAWICON"}]}})json");
    require(mixed_render_order.at("passed").as_bool(),
            "mixed formula source/render order contract failed");

    const auto mixed_render_order_legacy = tdx::evaluate_api_contract_response(
        "formula-mixed-render-order-inline-post", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"CONTRACT_MIXED_ORDER","market":"sz","code":"000001","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"function":"STICKLINE"},{"function":"SERIES"},{"function":"DRAWICON"}]}})json");
    require(!mixed_render_order_legacy.at("passed").as_bool(),
            "mixed formula order contract must reject unordered legacy IR");

    const auto colorref_styles = tdx::evaluate_api_contract_response(
        "formula-colorref-inline-post", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula_source_mode":"inline-post","formula":"CONTRACT_COLORREF","market":"sz","code":"000001","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"style":{"color_ref_available":true,"color_ref":255,"color_source":"tcalc-named-colorref","color_encoding":"Windows COLORREF: red | green<<8 | blue<<16"}},{"style":{"color_ref_available":true,"color_ref":65280,"color_source":"tcalc-named-colorref","color_encoding":"Windows COLORREF: red | green<<8 | blue<<16"}},{"style":{"color_ref_available":true,"color_ref":49344,"color_source":"tcalc-literal-colorref","color_encoding":"Windows COLORREF: red | green<<8 | blue<<16"}},{"style":{"color_ref_available":true,"color_ref":5418522,"color_source":"tcalc-rgbx-rgb","color_encoding":"Windows COLORREF: red | green<<8 | blue<<16"}}]}})json");
    require(colorref_styles.at("passed").as_bool(),
            "TCalc named/literal COLORREF style contract failed");

    const auto colorref_styles_legacy = tdx::evaluate_api_contract_response(
        "formula-colorref-inline-post", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula_source_mode":"inline-post","formula":"CONTRACT_COLORREF","market":"sz","code":"000001","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"style":{"color_token":"COLORRED"}},{"style":{"color_token":"COLORGREEN"}},{"style":{"color_token":"COLOR00C0C0"}},{"style":{"color_token":"RGBX1AAE52"}}]}})json");
    require(!colorref_styles_legacy.at("passed").as_bool(),
            "COLORREF contract must reject token-only legacy render styles");

    const auto cpbs_text = tdx::evaluate_api_contract_response(
        "formula-cpbs-text-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"CPBS","market":"sz","code":"000001","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"function":"DRAWTEXT","annotation_coordinate_space":"bar-price","annotation_price_argument":1,"annotation_text_argument":2,"annotation_line_break":"&","annotation_max_characters":250,"events":[{"annotation_text_available":true,"annotation_text":"B","annotation_lines":["B"],"annotation_price":10.5,"annotation_horizontal_align":"left","annotation_vertical_align":"above-price"}]}]}})json");
    require(!cpbs_text.at("passed").as_bool(),
            "CPBS contract must reject text without native font semantics");
    const auto cpbs_native_text_without_environment = tdx::evaluate_api_contract_response(
        "formula-cpbs-text-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"CPBS","market":"sz","code":"000001","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[)json"
        R"json({"function":"DRAWTEXT","annotation_coordinate_space":"bar-price","annotation_price_argument":1,"annotation_text_argument":2,"annotation_line_break":"&","annotation_max_characters":250,"annotation_native_render_type":4,"annotation_native_renderer":"tdxw-sub_961290","annotation_font_selector":"tdxw-sub_68F020","annotation_font_table_index":1,"annotation_font_user_ini_ordinal":2,"annotation_background_mode":"transparent","annotation_background_mode_value":1,"annotation_measurement_api":"GetTextExtentPoint32A","annotation_text_api":"TextOutA","events":[{"annotation_text_available":true,"annotation_text":"B","annotation_lines":["B"],"annotation_price":10.5,"annotation_horizontal_align":"left","annotation_vertical_align":"above-price"}]})json"
        R"json(]}})json");
    require(!cpbs_native_text_without_environment.at("passed").as_bool(),
            "CPBS contract must reject a missing top-level font environment");
    const auto cpbs_native_text = tdx::evaluate_api_contract_response(
        "formula-cpbs-text-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"CPBS","market":"sz","code":"000001","render_environment":{"schema":"tdx-formula-render-environment-v1","native_source":"TdxW.exe","pixel_font_equivalent":false,"annotation":{"background_mode":"transparent","background_mode_value":1,"text_encoding":"Win32-ANSI","font":{"native_font_table_index":1,"user_ini_font_ordinal":2,"face":"Arial","logical_height":15,"weight":400}}},"render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[)json"
        R"json({"function":"DRAWTEXT","annotation_coordinate_space":"bar-price","annotation_price_argument":1,"annotation_text_argument":2,"annotation_line_break":"&","annotation_max_characters":250,"annotation_native_render_type":4,"annotation_native_renderer":"tdxw-sub_961290","annotation_font_selector":"tdxw-sub_68F020","annotation_font_table_index":1,"annotation_font_user_ini_ordinal":2,"annotation_background_mode":"transparent","annotation_background_mode_value":1,"annotation_measurement_api":"GetTextExtentPoint32A","annotation_text_api":"TextOutA","events":[{"annotation_text_available":true,"annotation_text":"B","annotation_lines":["B"],"annotation_price":10.5,"annotation_horizontal_align":"left","annotation_vertical_align":"above-price"}]})json"
        R"json(]}})json");
    require(cpbs_native_text.at("passed").as_bool(), "CPBS native price-text font contract failed");

    const auto fscage_number = tdx::evaluate_api_contract_response(
        "formula-fscage-number-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"FSCAGE","market":"sz","code":"000001","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"function":"DRAWNUMBER","annotation_coordinate_space":"bar-price","annotation_price_argument":1,"annotation_text_argument":2,"annotation_line_break":"&","annotation_max_characters":250,"events":[{"annotation_text_available":true,"annotation_text":"11.41","annotation_lines":["11.41"],"annotation_price":11.41,"annotation_horizontal_align":"left","annotation_vertical_align":"above-price"}]}]}})json");
    require(!fscage_number.at("passed").as_bool(),
            "FSCAGE contract must reject number text without native font semantics");
    const auto fscage_native_number = tdx::evaluate_api_contract_response(
        "formula-fscage-number-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"FSCAGE","market":"sz","code":"000001","render_environment":{"schema":"tdx-formula-render-environment-v1","native_source":"TdxW.exe","pixel_font_equivalent":false,"annotation":{"background_mode":"transparent","background_mode_value":1,"text_encoding":"Win32-ANSI","font":{"native_font_table_index":1,"user_ini_font_ordinal":2,"face":"Arial","logical_height":15,"weight":400}}},"render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[)json"
        R"json({"function":"DRAWNUMBER","annotation_coordinate_space":"bar-price","annotation_price_argument":1,"annotation_text_argument":2,"annotation_line_break":"&","annotation_max_characters":250,"annotation_native_render_type":6,"annotation_native_renderer":"tdxw-sub_9593C0","annotation_font_selector":"tdxw-sub_68F020","annotation_font_table_index":1,"annotation_font_user_ini_ordinal":2,"annotation_background_mode":"transparent","annotation_background_mode_value":1,"annotation_measurement_api":"GetTextExtentPoint32A","annotation_text_api":"TextOutA","events":[{"annotation_text_available":true,"annotation_text":"11.41","annotation_lines":["11.41"],"annotation_price":11.41,"annotation_horizontal_align":"left","annotation_vertical_align":"above-price"}]})json"
        R"json(]}})json");
    require(fscage_native_number.at("passed").as_bool(),
            "FSCAGE native price-number font contract failed");

    const auto tjcjl_legacy_candle = tdx::evaluate_api_contract_response(
        "formula-tjcjl-stickline-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"TJCJL","market":"sz","code":"000001","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"function":"STICKLINE","events":[{"index":0,"arguments":[1,0,100,1,0]}]}]}})json");
    require(!tjcjl_legacy_candle.at("passed").as_bool(),
            "STICKLINE contract must reject the former candlestick surrogate");

    const auto sqjz_legacy_marker = tdx::evaluate_api_contract_response(
        "formula-sqjz-sequence-live", 200, "application/json",
        R"json({"engine":"tdx-source-interpreter-v1","execution_mode":"native-cpp","formula":"SQJZ","market":"sz","code":"000001","future_execution_mode":"explicit-read-only-lookahead","render_ir":{"schema":"tdx-formula-render-ir-v1","primitives":[{"function":"DRAWNUMBER_DIF","kind":"number","events":[{"index":12,"arguments":[1,1,1,8]}]}]}})json");
    require(!sqjz_legacy_marker.at("passed").as_bool(),
            "SQJZ contract must reject the former one-marker surrogate");

    const auto cloud_calc_audit = tdx::evaluate_api_contract_response(
        "formula-cloud-calc-audit", 200, "application/json",
        R"json({"schema":"tdx-tbigdata-cloud-calc-audit-v1","execution_mode":"native-cpp-offline","dll_loaded":false,"summary":{"cfg_files":660,"calc_columns":1370,"current_config_executable":1370,"current_config_unimplemented":0,"registered_builtin_implemented":36,"parse_errors":0,"dependency_cycles":0,"unresolved_host_columns":0}})json");
    require(cloud_calc_audit.at("passed").as_bool(), "TBigData cloud-calc audit contract failed");

    const auto cloud_calc_template = tdx::evaluate_api_contract_response(
        "formula-cloud-calc-template", 200, "application/json",
        R"json({"schema":"tdx-tbigdata-cloud-calc-template-v1","execution_mode":"native-cpp-offline","dll_loaded":false,"cfg_name":"func_kzz_kzzsy101.cfg","counts":{"input_fields":19,"host_fields":3,"derived_fields":1,"calculated_fields":15,"units":1},"row_template":{"$SC":null,"$SC1":null,"$ZQDM":null,"$ZQDM1":null,"DQRQ":null,"FXPL1":null,"HSCFBL":null,"LLLXBZ":null,"MZ":null,"QSCFBL":null,"QXRQ":null,"SGFXRQ":null,"SYFXCS":null,"SYFXLLXL":null,"SYNX":null,"SYNXSYL":null,"XGFXRQ":null,"XXCFBL":null,"ZGJ":null}})json");
    require(cloud_calc_template.at("passed").as_bool(),
            "TBigData minimal input template contract failed");

    const auto cloud_calc_live = tdx::evaluate_api_contract_response(
        "formula-cloud-calc-live-post", 200, "application/json",
        R"json({"schema":"tdx-tbigdata-cloud-calc-evaluation-v1","execution_mode":"native-cpp-public-l1-finance","dll_loaded":false,"row_source":"inline-request","counts":{"calculated":15,"evaluated":15,"unavailable":0,"errors":0},"host_context":{"snapshot_source":"public-l1-0x054c","finance_source":"public-finance-0x0010","bindings":[{},{},{},{},{},{},{},{}],"unresolved":[]}})json");
    require(cloud_calc_live.at("passed").as_bool(),
            "TBigData live cloud-calc POST contract failed");

    const auto cloud_calc_leaked = tdx::evaluate_api_contract_response(
        "formula-cloud-calc-live-post", 200, "application/json",
        R"json({"schema":"tdx-tbigdata-cloud-calc-evaluation-v1","execution_mode":"native-cpp-public-l1-finance","dll_loaded":false,"row_source":"inline-request","row":{"secret":"retained"},"counts":{"calculated":15,"evaluated":15,"unavailable":0,"errors":0},"host_context":{"snapshot_source":"public-l1-0x054c","finance_source":"public-finance-0x0010","bindings":[{},{},{},{},{},{},{},{}],"unresolved":[]}})json");
    require(!cloud_calc_leaked.at("passed").as_bool(),
            "cloud-calc contract must reject retained source rows");

    const auto cloud_calc_batch = tdx::evaluate_api_contract_response(
        "formula-cloud-calc-batch-post", 200, "application/json",
        R"json({"schema":"tdx-tbigdata-cloud-calc-batch-v1","execution_mode":"native-cpp-public-l1-finance","dll_loaded":false,"row_source":"inline-request-batch","request_body_retained":false,"counts":{"rows":2,"succeeded":2,"failed":0,"calculated":30,"evaluated":30,"unavailable":0,"errors":0},"fetch_plan":{"quote_mode":"snapshot-0x054c","unique_quote_securities":2,"quote_document_fetches":1},"rows":[{"row_index":0,"status":"ok","result":{"host_context":{"unresolved":[]}}},{"row_index":1,"status":"ok","result":{"host_context":{"unresolved":[]}}}]})json");
    require(cloud_calc_batch.at("passed").as_bool(),
            "TBigData batch cloud-calc POST contract failed");

    const auto cloud_calc_batch_leaked = tdx::evaluate_api_contract_response(
        "formula-cloud-calc-batch-post", 200, "application/json",
        R"json({"schema":"tdx-tbigdata-cloud-calc-batch-v1","execution_mode":"native-cpp-public-l1-finance","dll_loaded":false,"row_source":"inline-request-batch","request_body_retained":false,"counts":{"rows":2,"succeeded":2,"failed":0,"calculated":30,"evaluated":30,"unavailable":0,"errors":0},"fetch_plan":{"quote_mode":"snapshot-0x054c","unique_quote_securities":2,"quote_document_fetches":1},"rows":[{"row_index":0,"status":"ok","result":{"row":{"secret":"retained"},"host_context":{"unresolved":[]}}},{"row_index":1,"status":"ok","result":{"host_context":{"unresolved":[]}}}]})json");
    require(!cloud_calc_batch_leaked.at("passed").as_bool(),
            "cloud-calc batch contract must reject retained source rows");

    const auto tpool_inline = tdx::evaluate_api_contract_response(
        "tpool-inline-evaluate-post", 200, "application/json",
        R"json({"schema":"tdx-tpool-native-evaluation-v1","source":"contract-inline.xml","inline_source":true,"read_only":true,"tdx_state_mutated":false,"request_body_retained":false,"available_security_count":1,"evaluated_security_count":1,"evaluated_rule_count":1,"matched_rule_count":1,"flow_graph_evaluated":true,"inspection":{"source":"contract-inline.xml","inline_source":true,"action_policy_count":1,"configured_action_count":3},"flow_projection":{"planned_action_count":3,"host_actions_executed":false,"transitions":[{"planned_actions":[{"kind":"record-entry-log","path_template":"tpool/<pool>/<cell>/<YYYYMMDD>.log","executed":false},{"kind":"sound","effective_file":"sound\\default.wav","executed":false},{"kind":"save-block","security_record_size_bytes":7,"security_buffer_bytes":7,"callback_argument_count":7,"callback_operation_id":88,"clear_before_save":true,"executed":false}]}]}})json");
    require(tpool_inline.at("passed").as_bool(), "inline TPool evaluation POST contract failed");

    const auto tpool_inline_leaked = tdx::evaluate_api_contract_response(
        "tpool-inline-evaluate-post", 200, "application/json",
        R"json({"schema":"tdx-tpool-native-evaluation-v1","source":"contract-inline.xml","inline_source":true,"read_only":true,"tdx_state_mutated":false,"request_body_retained":false,"available_security_count":1,"evaluated_security_count":1,"evaluated_rule_count":1,"matched_rule_count":1,"flow_graph_evaluated":true,"xml":"secret","inspection":{"source":"contract-inline.xml","inline_source":true,"action_policy_count":1,"configured_action_count":3},"flow_projection":{"planned_action_count":3,"host_actions_executed":false,"transitions":[{"planned_actions":[{"kind":"record-entry-log","path_template":"tpool/<pool>/<cell>/<YYYYMMDD>.log","executed":false},{"kind":"sound","effective_file":"sound\\default.wav","executed":false},{"kind":"save-block","security_record_size_bytes":7,"security_buffer_bytes":7,"callback_argument_count":7,"callback_operation_id":88,"clear_before_save":true,"executed":false}]}]}})json");
    require(!tpool_inline_leaked.at("passed").as_bool(),
            "inline TPool contract must reject retained XML");

    const auto inline_scan = tdx::evaluate_api_contract_response(
        "formula-inline-scan-post", 200, "application/json; charset=utf-8",
        R"({"engine":"tdx-source-interpreter-v1","formula_source_mode":"inline-post","request_body_retained":false,"kind":"selection","requested_count":2,"evaluated":2,"error_count":0,"fetch_error_count":0,"fetch_workers":2,"match_count":1,"adjustment_mode":"qfq","adjustment_summary":{"mode":"qfq","security_scope":"per-security","source_command":"0x000F","method":"local-corporate-action-factor-v1","input_cache_counts":{"miss":2},"shared_cache":{"schema":"tdx-kline-adjustment-input-cache-v1","entry_count":2,"maximum_entries":512}},"matches":[{"security_id":"sz000001","adjustment_mode":"qfq"}],"analysis":{"syntax_supported":true,"executable":true}})");
    require(inline_scan.at("passed").as_bool(), "inline formula scan POST contract failed");

    const auto inline_scan_with_error = tdx::evaluate_api_contract_response(
        "formula-inline-scan-post", 200, "application/json",
        R"({"engine":"tdx-source-interpreter-v1","formula_source_mode":"inline-post","request_body_retained":false,"kind":"selection","requested_count":2,"evaluated":1,"error_count":0,"fetch_error_count":1,"matches":[],"analysis":{"syntax_supported":true,"executable":true}})");
    require(!inline_scan_with_error.at("passed").as_bool(),
            "inline scan contract must reject incomplete market evaluation");

    const auto inline_backtest = tdx::evaluate_api_contract_response(
        "formula-inline-backtest-post", 200, "application/json; charset=utf-8",
        R"({"engine":"tdx-source-backtest-v1","formula_source_mode":"inline-post","request_body_retained":false,"kind":"expert","count":240,"trade_count":12,"signal_timing":"signal at close, execute at next open","adjustment_mode":"qfq","adjustment":{"mode":"qfq","source_command":"0x000F","method":"local-corporate-action-factor-v1"},"analysis":{"syntax_supported":true,"numeric_signal_safe":true,"has_future_function":false}})");
    require(inline_backtest.at("passed").as_bool(), "inline formula backtest POST contract failed");

    const auto inline_backtest_future = tdx::evaluate_api_contract_response(
        "formula-inline-backtest-post", 200, "application/json",
        R"({"engine":"tdx-source-backtest-v1","formula_source_mode":"inline-post","request_body_retained":false,"kind":"expert","count":240,"trade_count":12,"signal_timing":"signal at close, execute at next open","analysis":{"syntax_supported":true,"numeric_signal_safe":true,"has_future_function":true}})");
    require(!inline_backtest_future.at("passed").as_bool(),
            "inline backtest contract must reject future functions");

    const auto strategy_scan = tdx::evaluate_api_contract_response(
        "formula-strategy-scan-post", 200, "application/json; charset=utf-8",
        R"({"engine":"tdx-formula-strategy-v1","strategy_source_mode":"manifest-post","request_body_retained":false,"strategy":{"operator":"all","minimum_matches":2,"rules":[{"id":"trend","source_mode":"inline"},{"id":"momentum","source_mode":"inline"}]},"requested_count":2,"evaluated":2,"error_count":0,"fetch_error_count":0,"fetch_workers":2,"adjustment_mode":"qfq","adjustment_summary":{"mode":"qfq","security_scope":"per-security","source_command":"0x000F","method":"local-corporate-action-factor-v1","input_cache_counts":{"hit":2},"shared_cache":{"schema":"tdx-kline-adjustment-input-cache-v1","entry_count":2,"maximum_entries":512}},"matches":[{"security_id":"sz000001","adjustment_mode":"qfq"}]})");
    require(strategy_scan.at("passed").as_bool(), "formula strategy scan contract failed");

    const auto strategy_scan_leaked = tdx::evaluate_api_contract_response(
        "formula-strategy-scan-post", 200, "application/json",
        R"({"engine":"tdx-formula-strategy-v1","strategy_source_mode":"manifest-post","request_body_retained":false,"strategy":{"operator":"all","minimum_matches":2,"rules":[{"id":"trend","source":"RESULT:CLOSE>10;"},{"id":"momentum"}]},"requested_count":2,"evaluated":2,"error_count":0,"fetch_error_count":0,"matches":[]})");
    require(!strategy_scan_leaked.at("passed").as_bool(),
            "formula strategy scan contract must reject source retention");

    const auto strategy_backtest = tdx::evaluate_api_contract_response(
        "formula-strategy-backtest-post", 200, "application/json; charset=utf-8",
        R"({"engine":"tdx-formula-strategy-portfolio-v1","execution_mode":"native-cpp","signal_timing":"shared bar close signal; rebalance at next shared bar open","security_count":2,"aligned_bar_count":240,"initial_capital":100000,"final_equity":101200,"fetch_error_count":0,"fetch_workers":2,"request_body_retained":false,"adjustment_mode":"qfq","adjustment_summary":{"mode":"qfq","security_scope":"per-security","source_command":"0x000F","method":"local-corporate-action-factor-v1","input_cache_counts":{"coalesced":1,"miss":1},"shared_cache":{"schema":"tdx-kline-adjustment-input-cache-v1","entry_count":2,"maximum_entries":512}},"attribution":[{"security_id":"sz000001","net_contribution":2000},{"security_id":"sh600000","net_contribution":-800}]})");
    require(strategy_backtest.at("passed").as_bool(), "formula strategy portfolio contract failed");

    const auto strategy_backtest_bad_attribution = tdx::evaluate_api_contract_response(
        "formula-strategy-backtest-post", 200, "application/json",
        R"({"engine":"tdx-formula-strategy-portfolio-v1","execution_mode":"native-cpp","signal_timing":"shared bar close signal; rebalance at next shared bar open","security_count":2,"aligned_bar_count":240,"initial_capital":100000,"final_equity":101200,"fetch_error_count":0,"request_body_retained":false,"attribution":[{"security_id":"sz000001","net_contribution":2000},{"security_id":"sh600000","net_contribution":-700}]})");
    require(!strategy_backtest_bad_attribution.at("passed").as_bool(),
            "portfolio contract must reject unreconciled attribution");
}

} // namespace recon_contract_test
