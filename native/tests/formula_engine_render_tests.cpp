#include "formula_engine_test_support.hpp"

namespace formula_engine_test {

void run_render_tests() {
    const auto analysis =
        tdx::analyze_formula_source("X:FINANCE(7)+REFX(CLOSE,1); Y:MA(CLOSE,N),COLORRED;", {"N"});
    require(!analysis.at("executable").as_bool(), "external/future formula must not execute");
    require(analysis.at("has_external_dependency").as_bool(), "external dependency detection");
    require(analysis.at("has_future_function").as_bool(), "future function detection");
    require(analysis.at("has_graphics").as_bool(), "graphics detection");

    const auto semantic_display_analysis =
        tdx::analyze_formula_source("DRAWTEXT_FIX(ISLASTBAR,0,0,0,STRCAT(CON2STR(N,0),' bars'));"
                                    "SIGNAL:CROSS(CLOSE,MA(CLOSE,N));",
                                    {"N"});
    require(semantic_display_analysis.at("numeric_signal_safe").as_bool() &&
                semantic_display_analysis.at("has_semantic_surrogate").as_bool() &&
                semantic_display_analysis.at("has_presentation_return_surrogate").as_bool() &&
                semantic_display_analysis.at("presentation_semantics_faithful").as_bool() &&
                semantic_display_analysis.at("render_semantics_materialized").as_bool() &&
                semantic_display_analysis.at("semantic_surrogate_scope").as_string() ==
                    "presentation-return-only" &&
                semantic_display_analysis.at("unsupported_presentation_directives")
                    .as_array()
                    .empty() &&
                semantic_display_analysis.at("presentation_only_outputs").size() == 1,
            "materialized display-only returns must not be reported as render gaps");
    const auto partline_analysis =
        tdx::analyze_formula_source("TREND:PARTLINE(MA(CLOSE,20),RGB(255,0,0),0);");
    require(partline_analysis.at("numeric_signal_safe").as_bool() &&
                partline_analysis.at("presentation_semantics_faithful").as_bool() &&
                partline_analysis.at("render_semantics_materialized").as_bool() &&
                partline_analysis.at("semantic_fidelity").as_string() ==
                    "numeric-safe-render-ir-materialized",
            "PARTLINE ordinate and render semantics are separately materialized");
    const auto unsupported_directive_analysis =
        tdx::analyze_formula_source("X:CLOSE,UNRECOVEREDSTYLE;");
    require(unsupported_directive_analysis.at("has_graphics").as_bool() &&
                !unsupported_directive_analysis.at("presentation_semantics_faithful").as_bool() &&
                !unsupported_directive_analysis.at("render_semantics_materialized").as_bool() &&
                unsupported_directive_analysis.at("unsupported_presentation_directives")
                        .as_array()
                        .size() == 1 &&
                unsupported_directive_analysis.at("unsupported_presentation_directives")
                        .as_array()[0]
                        .as_string() == "UNRECOVEREDSTYLE",
            "unknown drawing directives remain an explicit presentation gap");
    const auto string_compare_analysis =
        tdx::analyze_formula_source("MATCH:STRCMP(CODE,'000001');MISS:STRCMP(CODE,'600000');");
    require(string_compare_analysis.at("executable").as_bool() &&
                string_compare_analysis.at("numeric_signal_safe").as_bool(),
            "CODE/STRCMP exact string comparisons are directly executable");
    const auto string_compare = tdx::evaluate_formula_source_document(
        sample(10), "MATCH:STRCMP(CODE,'000001');MISS:STRCMP(CODE,'600000');", {}, "STRCMPTEST");
    require(point_value(string_compare, 9, "MATCH").as_number() == 1.0 &&
                point_value(string_compare, 9, "MISS").as_number() == 0.0,
            "STRCMP compares the exact security code rather than numeric surrogates");
    const auto background_analysis =
        tdx::analyze_formula_source("DRAWGBK_DIV(PERIOD=5,RGB(1,2,3),RGB(4,5,6),17,0);X:CLOSE;");
    require(background_analysis.at("executable").as_bool() &&
                background_analysis.at("numeric_signal_safe").as_bool() &&
                background_analysis.at("presentation_only_outputs").size() == 1,
            "DRAWGBK_DIV is an explicit presentation-only operation");
    const auto background_render = tdx::evaluate_formula_source_document(
        sample(10), "DRAWGBK_DIV(1,RGB(1,2,3),RGB(4,5,6),17,0);X:CLOSE;", {}, "BACKGROUNDRENDER");
    const auto &background_primitive =
        background_render.at("render_ir").at("primitives").as_array().front();
    const auto &background_event = background_primitive.at("events").as_array().front();
    require(background_primitive.at("kind").as_string() == "background" &&
                background_primitive.at("background_condition_scope").as_string() ==
                    "per-bar-contiguous-regions" &&
                background_primitive.at("background_native_render_type").as_number() == 21.0 &&
                background_primitive.at("background_native_renderer").as_string() ==
                    "tdxw-sub_95A6C0-sub_95A330" &&
                background_primitive.at("background_condition_true_rule").as_string() ==
                    "abs(value-1)<0.0001" &&
                background_primitive.at("background_alpha_mode_min").as_number() == 10.0 &&
                background_primitive.at("background_alpha_mode_max").as_number() == 20.0 &&
                background_primitive.at("background_alpha_rule").as_string() ==
                    "255*(mode-10)/10" &&
                background_primitive.at("event_count").as_number() == 10.0 &&
                background_event.at("background_color1_ref").as_number() == 197121.0 &&
                background_event.at("background_color2_ref").as_number() == 394500.0 &&
                background_event.at("background_fill_mode").as_number() == 17.0 &&
                background_event.at("background_fill_mode_name").as_string() == "alpha-solid" &&
                background_event.at("background_fill_compositing").as_string() ==
                    "gdiplus-argb-solid-color1" &&
                background_event.at("background_fill_alpha_byte").as_number() == 178.0 &&
                background_event.at("background_fill_alpha_denominator").as_number() == 255.0 &&
                background_event.at("background_region_leader").as_bool() &&
                background_event.at("background_region_start_index").as_number() == 0.0 &&
                background_event.at("background_region_end_index").as_number() == 9.0 &&
                background_event.at("background_region_end_index_exclusive").as_number() == 10.0 &&
                background_event.at("background_price_aggregation").as_string() == "whole-pane" &&
                background_event.at("background_range_name").as_string() == "pane",
            "DRAWGBK_DIV publishes the native type-21 region and mode-17 alpha semantics");
    const auto non_boolean_background = tdx::evaluate_formula_source_document(
        sample(10), "DRAWGBK_DIV(2,RGB(1,2,3),RGB(4,5,6),0,0);", {}, "BACKGROUNDSTRICTBOOL");
    require(non_boolean_background.at("render_ir")
                    .at("primitives")
                    .as_array()
                    .front()
                    .at("event_count")
                    .as_number() == 0.0,
            "DRAWGBK_DIV native renderer accepts values near one, not arbitrary truthy values");
    const auto high_low_background = tdx::evaluate_formula_source_document(
        sample(10), "DRAWGBK_DIV(1,RGB(1,2,3),RGB(4,5,6),0,1);", {}, "BACKGROUNDRANGE1");
    const auto &high_low_event = high_low_background.at("render_ir")
                                     .at("primitives")
                                     .as_array()
                                     .front()
                                     .at("events")
                                     .as_array()
                                     .front();
    require(high_low_event.at("background_price_aggregation").as_string() ==
                    "region-high-low-extrema" &&
                high_low_event.at("background_price_top").as_number() == 20.0 &&
                high_low_event.at("background_price_bottom").as_number() == 9.0,
            "DRAWGBK_DIV range 1 spans the high/low extrema of the whole region");
    const auto open_close_background = tdx::evaluate_formula_source_document(
        sample(10), "DRAWGBK_DIV(1,RGB(1,2,3),RGB(4,5,6),0,2);", {}, "BACKGROUNDRANGE2");
    const auto &open_close_event = open_close_background.at("render_ir")
                                       .at("primitives")
                                       .as_array()
                                       .front()
                                       .at("events")
                                       .as_array()
                                       .front();
    require(open_close_event.at("background_price_aggregation").as_string() ==
                    "region-first-open-last-close" &&
                std::abs(open_close_event.at("background_price_top").as_number() - 19.0) < 1e-9 &&
                std::abs(open_close_event.at("background_price_bottom").as_number() - 9.8) < 1e-9,
            "DRAWGBK_DIV range 2 spans first OPEN to last CLOSE of the region");

    const auto remaining_draw_analysis =
        tdx::analyze_formula_source("DRAWSL(CLOSE=19,CLOSE,0.5,3,2);"
                                    "DRAWBMP(CLOSE=19,LOW,'标记');"
                                    "DRAWGBK(1,RGB(1,2,3),RGB(4,5,6),1,'背景',0);"
                                    "DRAWRECTREL(0,0,500,500,RGB(7,8,9)),NOFRAME;");
    require(remaining_draw_analysis.at("executable").as_bool() &&
                remaining_draw_analysis.at("numeric_signal_safe").as_bool() &&
                remaining_draw_analysis.at("presentation_semantics_faithful").as_bool() &&
                remaining_draw_analysis.at("render_semantics_materialized").as_bool() &&
                remaining_draw_analysis.at("presentation_functions").size() == 5 &&
                remaining_draw_analysis.at("presentation_only_outputs").size() == 4,
            "remaining native drawing calls are presentation-only materialized operations");

    const auto slope_render = tdx::evaluate_formula_source_document(
        sample(10), "DRAWSL(CLOSE=15,CLOSE,0.5,3,2),COLORRED,LINETHICK2;", {}, "SLOPERENDER");
    const auto &slope_primitive = slope_render.at("render_ir").at("primitives").as_array().front();
    const auto &slope_events = slope_primitive.at("events").as_array();
    require(slope_primitive.at("kind").as_string() == "slope-line" &&
                slope_primitive.at("slope_native_render_type").as_number() == 20.0 &&
                slope_primitive.at("slope_native_renderer").as_string() == "tdxw-sub_957A30" &&
                slope_events.size() == 2 &&
                slope_events[0].at("slope_direction").as_string() == "right" &&
                slope_events[0].at("segment_from_index").as_number() == 5.0 &&
                slope_events[0].at("segment_to_index").as_number() == 8.0 &&
                slope_events[0].at("segment_from_price").as_number() == 15.0 &&
                slope_events[0].at("segment_to_price").as_number() == 16.5 &&
                slope_events[1].at("slope_direction").as_string() == "left" &&
                slope_events[1].at("segment_to_index").as_number() == 2.0 &&
                slope_events[1].at("segment_to_price").as_number() == 13.5,
            "DRAWSL uses final-bar DIRECT and per-bar price slope/length endpoints");
    const auto vertical_slope = tdx::evaluate_formula_source_document(
        sample(10), "DRAWSL(CLOSE=15,CLOSE,10000,12,0);", {}, "VERTICALSLOPE");
    const auto &vertical_event = vertical_slope.at("render_ir")
                                     .at("primitives")
                                     .as_array()
                                     .front()
                                     .at("events")
                                     .as_array()
                                     .front();
    require(vertical_event.at("index").as_number() == 5.0 &&
                vertical_event.at("slope_vertical").as_bool() &&
                vertical_event.at("slope_vertical_pixel_delta").as_number() == -12.0,
            "DRAWSL slope 10000 switches LEN to upward pixel height for DIRECT 0");

    const auto bitmap_render = tdx::evaluate_formula_source_document(
        sample(10), "DRAWBMP(CLOSE=19,LOW,'标记');", {}, "BITMAPRENDER");
    const auto &bitmap_primitive =
        bitmap_render.at("render_ir").at("primitives").as_array().front();
    const auto &bitmap_event = bitmap_primitive.at("events").as_array().front();
    require(bitmap_primitive.at("kind").as_string() == "bitmap" &&
                bitmap_primitive.at("bitmap_native_render_type").as_number() == 9.0 &&
                bitmap_event.at("index").as_number() == 9.0 &&
                bitmap_event.at("bitmap_price").as_number() == 18.0 &&
                bitmap_event.at("bitmap_name").as_string() == "标记" &&
                bitmap_event.at("bitmap_format").as_string() == "bmp",
            "DRAWBMP materializes exact bar/price/name and BMP-only resource semantics");

    const auto pane_background_render =
        tdx::evaluate_formula_source_document(sample(10),
                                              "DRAWGBK(1,RGB(1,2,3),RGB(4,5,6),1,'unused',0);"
                                              "DRAWGBK(1,0,0,0,'背景图',1);",
                                              {}, "PANEBACKGROUND");
    const auto &pane_primitives =
        pane_background_render.at("render_ir").at("primitives").as_array();
    const auto &gradient_event = pane_primitives[0].at("events").as_array().front();
    const auto &image_event = pane_primitives[1].at("events").as_array().front();
    require(pane_primitives[0].at("kind").as_string() == "pane-background" &&
                pane_primitives[0].at("pane_background_native_render_type").as_number() == 10.0 &&
                gradient_event.at("pane_background_mode").as_string() == "gradient" &&
                gradient_event.at("pane_background_color1_ref").as_number() == 197121.0 &&
                gradient_event.at("pane_background_color2_ref").as_number() == 394500.0 &&
                gradient_event.at("pane_background_horizontal").as_bool() &&
                image_event.at("pane_background_mode").as_string() == "image" &&
                image_event.at("pane_background_name").as_string() == "背景图" &&
                image_event.at("pane_background_stretch").as_bool() &&
                image_event.at("pane_background_format").as_string() == "auto",
            "DRAWGBK selects color gradient or BMP-then-PNG pane image with stretch mode");

    const auto rectangle_render = tdx::evaluate_formula_source_document(
        sample(10), "DRAWRECTREL(10,20,510,520,RGB(7,8,9)),NOFRAME;", {}, "RECTANGLERENDER");
    const auto &rectangle_primitive =
        rectangle_render.at("render_ir").at("primitives").as_array().front();
    const auto &rectangle_event = rectangle_primitive.at("events").as_array().front();
    require(rectangle_primitive.at("kind").as_string() == "pane-rectangle" &&
                rectangle_primitive.at("rectangle_native_render_type").as_number() == 11.0 &&
                rectangle_primitive.at("rectangle_no_frame").as_bool() &&
                rectangle_event.at("rectangle_left").as_number() == 10.0 &&
                rectangle_event.at("rectangle_top").as_number() == 20.0 &&
                rectangle_event.at("rectangle_right").as_number() == 510.0 &&
                rectangle_event.at("rectangle_bottom").as_number() == 520.0 &&
                rectangle_event.at("rectangle_color_ref").as_number() == 591879.0 &&
                rectangle_event.at("rectangle_fill").as_bool() &&
                !rectangle_event.at("rectangle_frame").as_bool(),
            "DRAWRECTREL publishes pane-thousandths coordinates, COLORREF fill, and NOFRAME");
    const auto rectangle_overflow = tdx::evaluate_formula_source_document(
        sample(10), "DRAWRECTREL(999999999999999,0,500,500,1);", {}, "RECTANGLEOVERFLOW");
    require(rectangle_overflow.at("render_ir")
                    .at("primitives")
                    .as_array()
                    .front()
                    .at("event_count")
                    .as_number() == 0.0,
            "DRAWRECTREL safely rejects coordinates outside native int32 range");

    const auto stick_render = tdx::evaluate_formula_source_document(
        sample(10), "STICKLINE(CLOSE>OPEN,LOW,HIGH,2,0),COLORRED,LINETHICK2;", {}, "STICKRENDER");
    const auto &stick_ir = stick_render.at("render_ir");
    const auto &stick_primitive = stick_ir.at("primitives").as_array().front();
    require(stick_ir.at("schema").as_string() == "tdx-formula-render-ir-v1" &&
                stick_primitive.at("kind").as_string() == "stick" &&
                stick_primitive.at("event_count").as_number() == 10,
            "STICKLINE produces one sparse render event for each true bar");
    require(stick_primitive.at("style").at("directives").as_array()[0].as_string() == "COLORRED" &&
                stick_primitive.at("style").at("directives").as_array()[1].as_string() ==
                    "LINETHICK2" &&
                stick_primitive.at("style").at("line_thickness").as_number() == 2 &&
                stick_primitive.at("style").at("color_ref_available").as_bool() &&
                stick_primitive.at("style").at("color_ref").as_number() == 255.0 &&
                stick_primitive.at("style").at("color_source").as_string() ==
                    "tcalc-named-colorref",
            "render IR preserves statement-local directive order and decoded style");
    const auto &first_stick = stick_primitive.at("events").as_array().front();
    require(first_stick.at("index").as_number() == 0 &&
                first_stick.at("arguments").as_array()[1].as_number() == 9.0 &&
                first_stick.at("arguments").as_array()[2].as_number() == 11.0,
            "render events reference points by index and retain evaluated arguments");

    const auto color_render = tdx::evaluate_formula_source_document(
        sample(1),
        "X0:CLOSE,COLORBLACK;X1:CLOSE,COLORBLUE;X2:CLOSE,COLORGREEN;"
        "X3:CLOSE,COLORCYAN;X4:CLOSE,COLORRED;X5:CLOSE,COLORMAGENTA;"
        "X6:CLOSE,COLORBROWN;X7:CLOSE,COLORLIGRAY;X8:CLOSE,COLORGRAY;"
        "X9:CLOSE,COLORLIBLUE;X10:CLOSE,COLORLIGREEN;X11:CLOSE,COLORLICYAN;"
        "X12:CLOSE,COLORLIRED;X13:CLOSE,COLORLIMAGENTA;"
        "X14:CLOSE,COLORYELLOW;X15:CLOSE,COLORWHITE;"
        "X16:CLOSE,COLOR00C0C0;X17:CLOSE,RGBX1AAE52;",
        {}, "COLORTABLE");
    const auto &color_primitives = color_render.at("render_ir").at("primitives").as_array();
    const std::vector<double> expected_colorrefs{
        0,        16711680, 65280,   16776960, 255,     16711935, 32896,    12632256, 8421504,
        12632064, 4243520,  8421376, 8421631,  8388863, 65535,    16777215, 49344,    5418522,
    };
    require(color_primitives.size() == expected_colorrefs.size(),
            "all 16 TCalc named colors plus COLOR/RGBX literals render");
    for (std::size_t index = 0; index < expected_colorrefs.size(); ++index) {
        const auto &style = color_primitives[index].at("style");
        require(style.at("color_ref_available").as_bool() &&
                    style.at("color_ref").as_number() == expected_colorrefs[index] &&
                    style.at("color_encoding").as_string() ==
                        "Windows COLORREF: red | green<<8 | blue<<16",
                "render color table matches the TCalc COLORREF values");
    }
    require(color_primitives[16].at("style").at("color_source").as_string() ==
                    "tcalc-literal-colorref" &&
                color_primitives[17].at("style").at("color_source").as_string() == "tcalc-rgbx-rgb",
            "COLOR keeps packed COLORREF while RGBX swaps human RGB into COLORREF");

    const auto stick_modes_render =
        tdx::evaluate_formula_source_document(sample(3),
                                              "STICKLINE(1,OPEN,CLOSE,3,0),COLORRED;"
                                              "STICKLINE(1,OPEN,CLOSE,2,1.5),COLORGREEN;"
                                              "STICKLINE(1,123,CLOSE,2,2),COLORBLUE;"
                                              "STICKLINE(1,456,CLOSE,2,3),COLORYELLOW;"
                                              "STICKLINE(1,LOW,HIGH,-1,-1),COLORWHITE;"
                                              "STICKLINE(1,LOW,HIGH,0.1,0),COLORCYAN;",
                                              {}, "STICKMODES");
    const auto &stick_modes = stick_modes_render.at("render_ir").at("primitives").as_array();
    require(stick_modes.size() == 6 &&
                stick_modes[0].at("stick_width_standard").as_number() == 4.0 &&
                stick_modes[0].at("stick_width_unit").as_string() == "bar-spacing-ratio" &&
                stick_modes[0].at("stick_center_modes_price1_ignored").as_bool() &&
                stick_modes[0].at("stick_modes").at("other-nonzero").as_string() == "solid-hollow",
            "STICKLINE primitive documents the native WIDTH and EMPTY contract");
    const auto &solid_stick = stick_modes[0].at("events").as_array().front();
    const auto &fractional_hollow = stick_modes[1].at("events").as_array().front();
    const auto &center_full = stick_modes[2].at("events").as_array().front();
    const auto &center_half = stick_modes[3].at("events").as_array().front();
    const auto &dashed_hairline = stick_modes[4].at("events").as_array().front();
    const auto &thin_solid = stick_modes[5].at("events").as_array().front();
    require(solid_stick.at("stick_mode").as_string() == "solid" &&
                !solid_stick.at("stick_hollow").as_bool() &&
                solid_stick.at("stick_width_ratio").as_number() == 0.75 &&
                fractional_hollow.at("stick_mode").as_string() == "solid-hollow" &&
                fractional_hollow.at("stick_hollow").as_bool(),
            "STICKLINE EMPTY=0 is solid and every other ordinary non-zero is hollow");
    require(center_full.at("stick_mode").as_string() == "center-full" &&
                center_full.at("stick_anchor").as_string() == "pane-middle" &&
                center_full.at("stick_occupancy").as_string() == "full" &&
                !center_full.at("stick_price1_used").as_bool() &&
                center_half.at("stick_mode").as_string() == "center-half" &&
                center_half.at("stick_occupancy").as_string() == "half" &&
                !center_half.at("stick_price1_used").as_bool(),
            "STICKLINE EMPTY=2/3 materializes the PRICE1-free center modes");
    require(dashed_hairline.at("stick_mode").as_string() == "dashed-hollow" &&
                dashed_hairline.at("stick_border_dashed").as_bool() &&
                dashed_hairline.at("stick_hairline").as_bool() &&
                dashed_hairline.at("stick_width_ratio").as_number() == 0.0 &&
                thin_solid.at("stick_width_ratio").as_number() ==
                    static_cast<double>(0.1F) / 4.0,
            "STICKLINE preserves negative hairlines and fractional WIDTH ratios");

    const auto candle_render = tdx::evaluate_formula_source_document(
        sample(3), "DRAWKLINE(HIGH,OPEN,LOW,CLOSE);", {}, "CANDLERENDER");
    const auto &candle_primitive =
        candle_render.at("render_ir").at("primitives").as_array().front();
    require(candle_primitive.at("kind").as_string() == "candlestick" &&
                candle_primitive.at("event_count").as_number() == 3.0 &&
                candle_primitive.at("candle_argument_order").as_array().size() == 4 &&
                candle_primitive.at("candle_argument_order").as_array()[0].as_string() == "high" &&
                candle_primitive.at("candle_argument_order").as_array()[3].as_string() == "close" &&
                candle_primitive.at("candle_color_rule").as_string() == "close>=open?up:down",
            "DRAWKLINE publishes its native HIGH/OPEN/LOW/CLOSE argument contract");

    const auto line_stick_render = tdx::evaluate_formula_source_document(
        sample(3), "X:CLOSE,LINESTICK,COLORGREEN;", {}, "LINESTICKRENDER");
    const auto &line_stick = line_stick_render.at("render_ir").at("primitives").as_array().front();
    require(
        line_stick.at("kind").as_string() == "line" && line_stick.at("line_stick").as_bool() &&
            line_stick.at("line_stick_components").as_array().size() == 2 &&
            line_stick.at("line_stick_components").as_array()[0].as_string() ==
                "zero-baseline-stick" &&
            line_stick.at("line_stick_components").as_array()[1].as_string() == "indicator-line" &&
            line_stick.at("line_stick_baseline").as_number() == 0 &&
            line_stick.at("line_stick_draw_order").as_string() == "sticks-then-line" &&
            line_stick.at("series_native_mode").as_string() == "line-stick" &&
            line_stick.at("series_native_render_type").as_number() == 5 &&
            line_stick.at("series_native_renderer").as_string() == "tdxw-sub_957F70" &&
            line_stick.at("series_native_stem_shape").as_string() == "one-pixel-vertical-line" &&
            line_stick.at("series_native_missing_value_rule").as_string() == "break-contiguous-run",
        "LINESTICK publishes its zero-baseline sticks plus line contract");

    const auto native_series_render = tdx::evaluate_formula_source_document(
        sample(4),
        "PLAIN:CLOSE;"
        "DOTTED:IF(CURRBARSCOUNT=3,CLOSE,DRAWNULL),DOTLINE,LINETHICK2;"
        "STEM:CLOSE,STICK;"
        "BOTH:CLOSE,LINESTICK;"
        "CIRCLE:CLOSE,CIRCLEDOT;"
        "CROSS:CLOSE,CROSSDOT;"
        "POINT:CLOSE,POINTDOT,LINETHICK3;"
        "LASTDOT:CLOSE,COLORSTICK,DOTLINE;"
        "LASTCOLOR:CLOSE,DOTLINE,COLORSTICK;",
        {}, "NATIVE_SERIES_RENDERERS");
    const auto &native_series = native_series_render.at("render_ir").at("primitives").as_array();
    require(native_series.size() == 9 &&
                native_series[0].at("series_native_mode").as_string() == "line" &&
                native_series[0].at("series_native_render_type").as_number() == 0 &&
                native_series[0].at("series_native_renderer").as_string() == "tdxw-sub_957620" &&
                native_series[0].at("series_native_single_point_rule").as_string() ==
                    "x-minus-3-to-x-horizontal" &&
                native_series[1].at("series_native_mode").as_string() == "dot-line" &&
                native_series[1].at("series_native_render_type").as_number() == 9 &&
                native_series[1].at("series_native_pen_style").as_string() == "PS_DOT" &&
                native_series[1].at("series_native_missing_value_rule").as_string() ==
                    "break-contiguous-run" &&
                native_series[1].at("finite_point_count").as_number() == 1,
            "ordinary and DOTLINE series publish native broken-run and singleton rules");
    require(native_series[2].at("series_native_mode").as_string() == "stick" &&
                native_series[2].at("series_native_render_type").as_number() == 4 &&
                native_series[2].at("series_native_renderer").as_string() == "tdxw-sub_957D70" &&
                native_series[2].at("series_native_stem_baseline").as_number() == 0 &&
                native_series[2].at("series_native_stem_shape").as_string() ==
                    "one-pixel-vertical-line" &&
                native_series[3].at("series_native_mode").as_string() == "line-stick" &&
                native_series[3].at("series_native_render_type").as_number() == 5,
            "STICK and LINESTICK publish native type 4/5 stem geometry");
    require(native_series[4].at("series_native_mode").as_string() == "circle-dot" &&
                native_series[4].at("series_native_render_type").as_number() == 6 &&
                native_series[4].at("series_native_renderer").as_string() == "tdxw-sub_95ABA0" &&
                native_series[4].at("series_native_point_geometry_rule").as_string() ==
                    "spacing<6?four-cardinal-pixels:hollow-circle-radius-3" &&
                native_series[5].at("series_native_mode").as_string() == "cross-dot" &&
                native_series[5].at("series_native_render_type").as_number() == 7 &&
                native_series[5].at("series_native_point_geometry_rule").as_string() ==
                    "spacing<5?diagonal-radius-1:spacing<10?diagonal-radius-2:diagonal-radius-3" &&
                native_series[6].at("series_native_mode").as_string() == "point-dot" &&
                native_series[6].at("series_native_render_type").as_number() == 8 &&
                native_series[6].at("series_native_renderer").as_string() == "tdxw-sub_95B0A0" &&
                native_series[6].at("series_native_point_geometry_rule").as_string() ==
                    "width<2?one-pixel:filled-ellipse-diameter-width",
            "CIRCLEDOT/CROSSDOT/POINTDOT publish native type 6/7/8 geometry");
    require(native_series[7].at("series_native_mode").as_string() == "dot-line" &&
                native_series[7].at("series_native_render_type").as_number() == 9 &&
                native_series[7].at("style").at("dot_line").as_bool() &&
                native_series[7].as_object().find("series_stick_mode") ==
                    native_series[7].as_object().end() &&
                native_series[8].at("series_stick_mode").as_string() == "color-stick" &&
                !native_series[8].at("style").at("dot_line").as_bool() &&
                native_series[8].as_object().find("series_native_mode") ==
                    native_series[8].as_object().end(),
            "the final native renderer directive wins across type 1/2/4..9");

    auto series_stick_sample = sample(4);
    const double stick_opens[]{10.0, 12.0, 10.0, 8.0};
    const double stick_closes[]{11.0, 11.0, 10.0, 9.5};
    for (auto &bar : series_stick_sample["bars"].as_array()) {
        const auto day =
            static_cast<std::size_t>(std::stoi(bar.at("date").as_string().substr(8, 2)) - 1);
        bar["open"] = stick_opens[day];
        bar["close"] = stick_closes[day];
        bar["high"] = std::max(stick_opens[day], stick_closes[day]) + 0.5;
        bar["low"] = std::min(stick_opens[day], stick_closes[day]) - 0.5;
    }
    const auto series_stick_render = tdx::evaluate_formula_source_document(
        std::move(series_stick_sample), "V:VOL,VOLSTICK;C:CLOSE-10.5,COLORSTICK;", {},
        "SERIESSTICKRENDER");
    const auto &series_sticks = series_stick_render.at("render_ir").at("primitives").as_array();
    const auto &volume_stick = series_sticks[0];
    const auto &color_stick = series_sticks[1];
    const auto &volume_events = volume_stick.at("events").as_array();
    const auto &color_events = color_stick.at("events").as_array();
    require(volume_stick.at("series_stick_mode").as_string() == "volume-stick" &&
                volume_stick.at("series_stick_native_render_type").as_number() == 1 &&
                volume_stick.at("series_stick_native_renderer").as_string() == "tdxw-sub_957030" &&
                volume_stick.at("series_stick_shape").as_string() == "volume-body" &&
                volume_stick.at("series_stick_up_fill_rule").as_string() ==
                    "RealUPK?solid:hollow" &&
                volume_stick.at("event_count").as_number() == 4 &&
                volume_events[2].at("series_stick_open_close_color_role").as_string() == "down" &&
                volume_events[3].at("series_stick_open_close_color_role").as_string() == "up" &&
                volume_events[3].at("series_stick_previous_close_color_role").as_string() == "down",
            "VOLSTICK materializes both native color modes and body/fill contract");
    require(color_stick.at("series_stick_mode").as_string() == "color-stick" &&
                color_stick.at("series_stick_native_render_type").as_number() == 2 &&
                color_stick.at("series_stick_native_renderer").as_string() == "tdxw-sub_9555B0" &&
                color_stick.at("series_stick_shape").as_string() == "one-pixel-vertical-line" &&
                color_events[0].at("series_stick_color_role").as_string() == "up" &&
                color_events[2].at("series_stick_color_role").as_string() == "down" &&
                series_stick_render.at("render_ir").at("event_count").as_number() == 8,
            "COLORSTICK materializes native one-pixel zero-baseline color events");

    const auto icon_render = tdx::evaluate_formula_source_document(
        sample(3), "DRAWICON(1,LOW,49);DRAWICON(ISLASTBAR,HIGH,51),DRAWABOVE;", {}, "ICONRENDER");
    const auto &icon_primitives = icon_render.at("render_ir").at("primitives").as_array();
    const auto &list_icon = icon_primitives[0].at("events").as_array().front();
    const auto &k_icon = icon_primitives[1].at("events").as_array().front();
    require(icon_primitives.size() == 2 &&
                icon_primitives[0].at("statement_index").as_number() == 0 &&
                icon_primitives[0].at("render_order").as_number() == 0 &&
                icon_primitives[0].at("render_order_semantics").as_string() ==
                    "source-statement-order" &&
                icon_primitives[1].at("statement_index").as_number() == 1 &&
                icon_primitives[1].at("render_order").as_number() == 1 &&
                icon_primitives[0].at("kind").as_string() == "icon" &&
                icon_primitives[0].at("icon_renderer").as_string() == "tcalc-resource-bitmap" &&
                icon_primitives[0].at("icon_sprite_endpoint").as_string() ==
                    "/api/v1/formulas/drawicon-strip.png" &&
                icon_primitives[0].at("icon_resource_id").as_number() == 2060 &&
                list_icon.at("icon_type").as_number() == 49 &&
                list_icon.at("icon_type_available").as_bool() &&
                list_icon.at("icon_sprite_cell_available").as_bool() &&
                list_icon.at("icon_sprite_x").as_number() == 864 &&
                list_icon.at("icon_vertical_align").as_string() == "below-price" &&
                k_icon.at("index").as_number() == 2 && k_icon.at("icon_type").as_number() == 51 &&
                k_icon.at("icon_sprite_x").as_number() == 900 &&
                k_icon.at("icon_vertical_align").as_string() == "above-price",
            "DRAWICON publishes exact TCalc sprite cells and DRAWABOVE alignment");

    const auto mixed_order_render = tdx::evaluate_formula_source_document(
        sample(3),
        "A:=MA(CLOSE,2);STICKLINE(1,LOW,HIGH,2,0),COLORGRAY;"
        "X:CLOSE,COLORRED;DRAWICON(ISLASTBAR,HIGH,1),DRAWABOVE;",
        {}, "MIXEDORDER");
    const auto &mixed_ir = mixed_order_render.at("render_ir");
    const auto &mixed_primitives = mixed_ir.at("primitives").as_array();
    require(mixed_ir.at("primitive_order").as_string() == "source-statement-order" &&
                mixed_ir.at("primitive_order_contiguous").as_bool() &&
                mixed_ir.at("source_statement_count").as_number() == 4 &&
                mixed_primitives.size() == 3 &&
                mixed_primitives[0].at("statement_index").as_number() == 1 &&
                mixed_primitives[0].at("render_order").as_number() == 0 &&
                mixed_primitives[1].at("statement_index").as_number() == 2 &&
                mixed_primitives[1].at("render_order").as_number() == 1 &&
                mixed_primitives[2].at("statement_index").as_number() == 3 &&
                mixed_primitives[2].at("render_order").as_number() == 2,
            "render IR distinguishes source statement indexes from contiguous draw order");

    const auto text_render =
        tdx::evaluate_formula_source_document(sample(10),
                                              "Z1:=STRCAT('A',CON2STR(CLOSE,1));"
                                              "DRAWTEXT_FIX(ISLASTBAR,0,0,0,Z1),COLOR00C0C0;",
                                              {}, "TEXTRENDER");
    const auto &text_primitive = text_render.at("render_ir").at("primitives").as_array().front();
    const auto &text_event = text_primitive.at("events").as_array().front();
    require(text_primitive.at("kind").as_string() == "text" &&
                text_primitive.at("event_count").as_number() == 1 &&
                text_event.at("index").as_number() == 9 &&
                text_event.at("string_arguments").at("4").as_string() == "A19.0" &&
                text_event.at("annotation_text").as_string() == "A19.0" &&
                text_event.at("annotation_horizontal_align").as_string() == "left" &&
                text_event.at("annotation_vertical_align").as_string() == "top",
            "assigned STRCAT/CON2STR series are materialized into text render events");

    const auto annotation_render = tdx::evaluate_formula_source_document(
        sample(3),
        "DRAWTEXT(1,LOW,'A&B'),COLORRED,DRAWCFRAME;"
        "DRAWNUMBER(1,HIGH,CLOSE),COLORGREEN,DRAWABOVE;"
        "DRAWTEXT_FIX(ISLASTBAR,0.5,0.25,1,'RIGHT&NEXT'),COLORBLUE,DRAWCFRAME;"
        "DRAWNUMBER_FIX(ISLASTBAR,0.75,0.5,0,CLOSE),COLORYELLOW,DRAWCFRAME;",
        {}, "ANNOTATIONS");
    const auto &annotation_primitives =
        annotation_render.at("render_ir").at("primitives").as_array();
    require(
        annotation_primitives.size() == 4 &&
            annotation_primitives[0].at("annotation_coordinate_space").as_string() == "bar-price" &&
            annotation_primitives[0].at("annotation_native_render_type").as_number() == 4 &&
            annotation_primitives[0].at("annotation_native_renderer").as_string() ==
                "tdxw-sub_961290" &&
            annotation_primitives[0].at("annotation_font_table_index").as_number() == 1 &&
            annotation_primitives[0].at("annotation_font_user_ini_ordinal").as_number() == 2 &&
            annotation_primitives[0].at("annotation_background_mode").as_string() ==
                "transparent" &&
            annotation_primitives[0].at("annotation_text_api").as_string() == "TextOutA" &&
            annotation_primitives[0].at("annotation_frame_supported").as_bool() &&
            annotation_primitives[0].at("annotation_frame_directive_effect").as_string() ==
                "native-price-text-frame" &&
            annotation_primitives[0].at("annotation_frame_anchor").as_string() == "bar-high-low" &&
            annotation_primitives[0].at("annotation_frame_price_argument_ignored").as_bool() &&
            annotation_primitives[0].at("annotation_frame_drawabove_ignored").as_bool() &&
            annotation_primitives[0].at("annotation_frame_side_rule").as_string() ==
                "available-below<=available-above?above:below" &&
            annotation_primitives[0].at("annotation_frame_leader_length_pixels").as_number() ==
                20 &&
            annotation_primitives[0].at("annotation_frame_leader_style").as_string() == "dotted" &&
            annotation_primitives[0].at("annotation_frame_leader_dot_step_pixels").as_number() ==
                4 &&
            annotation_primitives[0].at("annotation_frame_corner_radius_pixels").as_number() == 4 &&
            annotation_primitives[0].at("annotation_frame_fill_alpha_byte").as_number() == 0x50 &&
            annotation_primitives[0].at("annotation_frame_border_alpha_byte").as_number() == 0xFF &&
            annotation_primitives[0].at("annotation_frame_multiline_rule").as_string() ==
                "per-line-overlap-same-anchor" &&
            annotation_primitives[2].at("annotation_coordinate_space").as_string() ==
                "pane-fraction" &&
            annotation_primitives[2].at("annotation_native_render_type").as_number() == 7 &&
            annotation_primitives[2].at("annotation_alignments").at("1").as_string() == "right" &&
            !annotation_primitives[2].at("annotation_frame_supported").as_bool() &&
            annotation_primitives[2].at("annotation_frame_directive_effect").as_string() ==
                "ignored-by-native-renderer" &&
            !annotation_primitives[3].at("annotation_frame_supported").as_bool() &&
            annotation_primitives[3].at("annotation_frame_directive_effect").as_string() ==
                "not-forwarded-to-native-renderer",
        "annotation primitives publish price and fixed-coordinate contracts");
    const auto &price_text = annotation_primitives[0].at("events").as_array().front();
    require(price_text.at("annotation_text").as_string() == "A&B" &&
                price_text.at("annotation_lines").as_array().size() == 2 &&
                price_text.at("annotation_lines").as_array()[0].as_string() == "A" &&
                price_text.at("annotation_lines").as_array()[1].as_string() == "B" &&
                price_text.at("annotation_frame").as_bool() &&
                annotation_primitives[0].at("style").at("draw_cframe").as_bool() &&
                price_text.at("annotation_vertical_align").as_string() == "price-origin",
            "DRAWTEXT materializes ampersand line breaks and DRAWCFRAME");
    const auto &price_number = annotation_primitives[1].at("events").as_array().front();
    require(price_number.at("annotation_text").as_string() == "10" &&
                price_number.at("annotation_price").as_number() == 11.0 &&
                price_number.at("annotation_vertical_align").as_string() == "above-price" &&
                price_number.at("annotation_x_offset_pixels").as_number() == -3 &&
                price_number.at("annotation_y_offset_pixels").as_number() == 0,
            "DRAWNUMBER materializes numeric text and DRAWABOVE alignment");
    const auto &fixed_text = annotation_primitives[2].at("events").as_array().front();
    const auto &fixed_number = annotation_primitives[3].at("events").as_array().front();
    require(fixed_text.at("annotation_x").as_number() == 0.5 &&
                fixed_text.at("annotation_y").as_number() == 0.25 &&
                fixed_text.at("annotation_horizontal_align").as_string() == "right" &&
                fixed_text.at("annotation_lines").as_array().size() == 2 &&
                !fixed_text.at("annotation_frame").as_bool() &&
                annotation_primitives[2].at("style").at("draw_cframe").as_bool() &&
                fixed_number.at("annotation_text").as_string() == "12" &&
                fixed_number.at("annotation_horizontal_align").as_string() == "left" &&
                !fixed_number.at("annotation_frame").as_bool() &&
                annotation_primitives[3].at("style").at("draw_cframe").as_bool(),
            "fixed annotations preserve coordinates while native renderers ignore DRAWCFRAME");

    const auto unframed_render =
        tdx::evaluate_formula_source_document(chip_sample(),
                                              "DRAWTEXT(1,LOW,'A&&B'),COLORRED;"
                                              "DRAWTEXT(1,HIGH,'UP&NEXT'),COLORGREEN,DRAWABOVE;"
                                              "DRAWNUMBER(1,CLOSE,CLOSE),COLORBLUE;"
                                              "DRAWNUMBER(1,OPEN,OPEN),COLORYELLOW,DRAWABOVE;"
                                              "DRAWTEXT(0.5,CLOSE,'NOT-NATIVE-TRUE');"
                                              "DRAWNUMBER(1,DRAWNULL,CLOSE);",
                                              {}, "UNFRAMED_ANNOTATIONS");
    const auto &unframed = unframed_render.at("render_ir").at("primitives").as_array();
    const auto &below_text = unframed[0];
    const auto &below_text_event = below_text.at("events").as_array().front();
    const auto &above_text_event = unframed[1].at("events").as_array().front();
    const auto &below_number = unframed[2];
    const auto &below_number_event = below_number.at("events").as_array().front();
    const auto &above_number_event = unframed[3].at("events").as_array().front();
    require(below_text.at("annotation_condition_true_rule").as_string() == "abs(value-1)<0.0001" &&
                below_text.at("annotation_missing_price_rule").as_string() == "skip-event" &&
                below_text.at("annotation_edge_behavior").as_string() == "no-clamp-no-flip" &&
                below_text.at("annotation_collision_behavior").as_string() ==
                    "none-source-order-overpaint" &&
                below_text.at("annotation_max_lines").as_number() == 10 &&
                below_text.at("annotation_unframed_horizontal_rule").as_string() == "bar-x" &&
                below_text.at("annotation_unframed_base_y_rule").as_string() == "price-y-minus-8" &&
                below_text_event.at("annotation_lines").as_array().size() == 3 &&
                below_text_event.at("annotation_lines").as_array()[1].as_string().empty() &&
                below_text_event.at("annotation_line_count").as_number() == 3 &&
                below_text_event.at("annotation_x_offset_pixels").as_number() == 0 &&
                below_text_event.at("annotation_y_offset_pixels").as_number() == -8 &&
                above_text_event.at("annotation_drawabove").as_bool() &&
                above_text_event.at("annotation_line_count").as_number() == 2,
            "DRAWTEXT publishes exact unframed origin, multiline, and DRAWABOVE rules");
    require(below_number.at("annotation_unframed_horizontal_rule").as_string() == "bar-x-minus-3" &&
                below_number.at("annotation_number_chart_precision").as_number() == 2 &&
                below_number.at("annotation_number_chart_precision_source").as_string() ==
                    "native-constructor-default" &&
                below_number.at("annotation_number_format_rule").as_string() ==
                    "sub_591950-integer-or-2/3-decimals" &&
                below_number_event.at("annotation_text").as_string() == "10.030" &&
                below_number_event.at("annotation_vertical_align").as_string() == "price-origin" &&
                above_number_event.at("annotation_text").as_string() == "10.020" &&
                above_number_event.at("annotation_vertical_align").as_string() == "above-price" &&
                unframed[4].at("events").as_array().empty() &&
                unframed[5].at("events").as_array().empty(),
            "DRAWNUMBER reproduces ordinary formatting and native condition/price filtering");

    auto fixed_format_sample = chip_sample();
    fixed_format_sample["price_precision"] = 3;
    fixed_format_sample["index_info_format_mode"] = 2;
    const auto fixed_format_render = tdx::evaluate_formula_source_document(
        std::move(fixed_format_sample), "DRAWNUMBER(1,CLOSE,CLOSE);", {}, "FIXED_NUMBER_FORMAT");
    const auto &fixed_format_primitive =
        fixed_format_render.at("render_ir").at("primitives").as_array().front();
    require(fixed_format_primitive.at("annotation_number_chart_precision_source").as_string() ==
                    "kline.price_precision" &&
                fixed_format_primitive.at("annotation_number_format_rule").as_string() ==
                    "sub_59C390-fixed-0..5-decimals-plus-1e-6" &&
                fixed_format_primitive.at("events")
                        .as_array()
                        .front()
                        .at("annotation_text")
                        .as_string() == "10.0300",
            "DRAWNUMBER honors recovered IndexInfo fixed-precision modes");

    const auto font_root = fs::temp_directory_path() / "tdx-formula-font-profile-test";
    fs::remove_all(font_root);
    fs::create_directories(font_root / "T0002");
    tdx::atomic_write_text(font_root / "T0002" / "user.ini",
                           "[Other]\nNewFontStyle=0\nElderStyle=1\n"
                           "FONTNAME2=Arial\nFONTSIZE2=15\nFontWeigth2=500\n"
                           "RealUPK=1\nVolKUseZT=1\nBoldZBLine=1\n");
    const auto font_environment = tdx::formula_render_environment_document(font_root);
    const auto &annotation_font = font_environment.at("annotation").at("font");
    const auto &sequence_style2_font =
        font_environment.at("annotation").at("conditional_fonts").at("13");
    require(
        font_environment.at("schema").as_string() == "tdx-formula-render-environment-v1" &&
            annotation_font.at("native_font_table_index").as_number() == 1 &&
            annotation_font.at("user_ini_font_ordinal").as_number() == 2 &&
            annotation_font.at("face").as_string() == "Arial" &&
            annotation_font.at("configured_height").as_number() == 15 &&
            annotation_font.at("logical_height").as_number() == 17 &&
            annotation_font.at("logical_height_semantics").as_string() == "gdi-cell-height" &&
            annotation_font.at("weight").as_number() == 500 &&
            annotation_font.at("charset_name").as_string() == "DEFAULT_CHARSET" &&
            annotation_font.at("quality_name").as_string() == "ANTIALIASED_QUALITY" &&
            annotation_font.at("native_textout_aux_mode").as_number() == 0 &&
            annotation_font.at("native_textout_y_adjustment_pixels").as_number() == 0 &&
            font_environment.at("annotation").at("native_chart_row_height_pixels").as_number() ==
                22 &&
            sequence_style2_font.at("native_font_table_index").as_number() == 13 &&
            sequence_style2_font.at("selected_profile").as_string() ==
                "recovered-drawnumber-dif-style2-table" &&
            sequence_style2_font.at("native_source").as_string() == "TdxW.exe!sub_690350" &&
            sequence_style2_font.at("face").as_string() == "Arial" &&
            sequence_style2_font.at("logical_height").as_number() == 15 &&
            sequence_style2_font.at("weight").as_number() == 400 &&
            font_environment.at("annotation").at("background_mode").as_string() == "transparent" &&
            font_environment.at("series_sticks").at("real_up_k").as_bool() &&
            font_environment.at("series_sticks").at("vol_k_use_zt").as_bool() &&
            font_environment.at("series_sticks").at("volume_color_rule").as_string() ==
                "previous-close" &&
            font_environment.at("series_sticks").at("volume_up_fill").as_string() == "solid" &&
            font_environment.at("series_sticks").at("native_up_pen_index").as_number() == 2 &&
            font_environment.at("series_lines").at("bold_zb_line").as_bool() &&
            font_environment.at("series_lines").at("config_keys").at("bold_zb_line").as_string() ==
                "BoldZBLine" &&
            font_environment.at("series_lines").at("native_source").as_string() ==
                "TdxW.exe!sub_957620",
        "formula render environment parses index 1 and preserves the hard-coded index 13 profile");
    fs::remove_all(font_root);

    const auto rgb_render = tdx::evaluate_formula_source_document(
        sample(3), "TREND:PARTLINE(CLOSE,RGB(1,2,3),0);", {}, "RGBRENDER");
    const auto &rgb_event = rgb_render.at("render_ir")
                                .at("primitives")
                                .as_array()
                                .front()
                                .at("events")
                                .as_array()
                                .front();
    require(rgb_event.at("arguments").as_array()[1].as_number() == 197121.0 &&
                rgb_event.at("segment_direction").as_string() == "current-to-next" &&
                rgb_event.at("segment_from_index").as_number() == 0 &&
                rgb_event.at("segment_to_index").as_number() == 1,
            "RGB uses COLORREF and PARTLINE DIRECT=0 colors current-to-next");
    const auto rgb_native_conversion = tdx::evaluate_formula_source_document(
        sample(3),
        "B:RGB(255,-1,256);F:RGB(1.9,2.9,3.9);"
        "S:RGB(-4.0398103E34,1,2);M:RGB(DRAWNULL,1,2);O:RGB(1E39,1,2);",
        {}, "RGBNATIVECONVERSION");
    require(point_value(rgb_native_conversion, 2, "B").as_number() == 16711422.0 &&
                point_value(rgb_native_conversion, 2, "F").as_number() == 197121.0 &&
                point_value(rgb_native_conversion, 2, "S").as_number() == 131328.0 &&
                point_value(rgb_native_conversion, 2, "M").as_number() == 131328.0 &&
                point_value(rgb_native_conversion, 2, "O").as_number() == 131328.0,
            "RGB uses raw-f32 truncating i64 conversion and native unsigned channel bounds");
    const auto reverse_rgb_render = tdx::evaluate_formula_source_document(
        sample(3), "TREND:PARTLINE(CLOSE,RGB(3,2,1),1);", {}, "RGBREVERSE");
    const auto &reverse_events = reverse_rgb_render.at("render_ir")
                                     .at("primitives")
                                     .as_array()
                                     .front()
                                     .at("events")
                                     .as_array();
    require(reverse_events[0].at("segment_from_index").is_null() &&
                reverse_events[1].at("segment_direction").as_string() == "previous-to-current" &&
                reverse_events[1].at("segment_from_index").as_number() == 0 &&
                reverse_events[1].at("segment_to_index").as_number() == 1,
            "PARTLINE DIRECT=1 colors the segment from the previous bar");

    const auto band_render =
        tdx::evaluate_formula_source_document(sample(3),
                                              "DRAWBAND(CLOSE,RGB(255,0,0),OPEN,RGB(0,255,0));"
                                              "DRAWBAND(OPEN,RGB(1,2,3),CLOSE,RGB(4,5,6));",
                                              {}, "BANDRENDER");
    const auto &band_primitives = band_render.at("render_ir").at("primitives").as_array();
    const auto &upper_band_event = band_primitives[0].at("events").as_array()[0];
    const auto &lower_band_event = band_primitives[1].at("events").as_array()[0];
    require(band_primitives[0].at("band_fill_rule").as_string() == "arg0>arg2?arg1:arg3" &&
                band_primitives[0].at("band_fill_opacity").as_number() == 1.0 &&
                band_primitives[0].at("band_fill_compositing").as_string() ==
                    "opaque-gdi-stroke-and-fill-path" &&
                upper_band_event.at("band_side").as_string() == "arg0-above" &&
                upper_band_event.at("fill_color_argument").as_number() == 1 &&
                upper_band_event.at("fill_color_ref").as_number() == 254.0 &&
                lower_band_event.at("band_side").as_string() == "arg2-above" &&
                lower_band_event.at("fill_color_argument").as_number() == 3 &&
                lower_band_event.at("fill_color_ref").as_number() == 394500.0,
            "DRAWBAND materializes the native side-dependent fill COLORREF");

    const auto sequence_render = tdx::evaluate_formula_source_document(
        sample(12),
        "DRAWNUMBER_DIF(CURRBARSCOUNT=10,1,1,3),COLORRED;"
        "DRAWNUMBER_DIF(CURRBARSCOUNT=8,2,11,3),COLORGREEN,DRAWABOVE;",
        {}, "SEQUENCERENDER");
    const auto &sequence_primitives = sequence_render.at("render_ir").at("primitives").as_array();
    const auto &digit_primitive = sequence_primitives[0];
    const auto &digit_events = digit_primitive.at("events").as_array();
    require(digit_primitive.at("kind").as_string() == "sequence-number" &&
                digit_primitive.at("sequence_semantics").as_string() ==
                    "increment-by-one-on-consecutive-bars" &&
                digit_primitive.at("annotation_native_render_type").as_number() == 23 &&
                digit_primitive.at("annotation_native_renderer").as_string() == "tdxw-sub_9626E0" &&
                digit_primitive.at("annotation_text_api").as_string() == "DrawTextA" &&
                digit_primitive.at("annotation_conditional_font_table_index").as_number() == 13 &&
                digit_primitive.at("annotation_conditional_font_rule").as_string() ==
                    "first-rendered-bar-arg1==2" &&
                digit_primitive.at("annotation_conditional_font_scope").as_string() ==
                    "renderer-wide-before-event-loop" &&
                digit_primitive.at("annotation_effective_font_table_index").as_number() == 1 &&
                digit_primitive.at("annotation_effective_font_basis").as_string() ==
                    "evaluation-window-first-bar-fallback;frontend-recomputes-visible-first-bar" &&
                digit_primitive.at("sequence_style_series_alignment").as_string() ==
                    "document-points" &&
                digit_primitive.at("sequence_style_series").as_array().size() == 12 &&
                digit_primitive.at("sequence_style_series").as_array()[0].as_number() == 1 &&
                digit_primitive.at("annotation_drawtext_numeric_flags").as_number() == 0x826 &&
                digit_primitive.at("annotation_drawtext_alpha_flags").as_number() == 0x825 &&
                digit_primitive.at("sequence_condition_true_rule").as_string() ==
                    "abs(value-1)<0.0001" &&
                digit_primitive.at("sequence_overlap_rule").as_string() ==
                    "ignore-trigger-while-active" &&
                digit_primitive.at("sequence_leader_length_pixels").as_number() == 10 &&
                digit_primitive.at("sequence_box_width_numeric_pixels").as_number() == 8 &&
                digit_primitive.at("sequence_box_width_alpha_pixels").as_number() == 14 &&
                digit_primitive.at("sequence_box_height_pixels").as_number() == 14 &&
                digit_primitive.at("sequence_box_fill_alpha_byte").as_number() == 80 &&
                digit_primitive.at("sequence_box_border_path").as_string() ==
                    "closed-gdi-polyline" &&
                digit_events.size() == 3 && digit_events[0].at("index").as_number() == 2 &&
                digit_events[0].at("source_index").as_number() == 2 &&
                digit_events[0].at("sequence_label").as_string() == "1" &&
                digit_events[0].at("sequence_style_mode").as_string() == "leader" &&
                digit_events[0].at("sequence_leader").as_bool() &&
                !digit_events[0].at("sequence_boxed").as_bool() &&
                digit_events[0].at("sequence_offset_pixels").as_number() == 10 &&
                digit_events[0].at("sequence_box_width_pixels").as_number() == 8 &&
                digit_events[0].at("sequence_text_alignment").as_string() == "right" &&
                digit_events[0].at("sequence_drawtext_flags").as_number() == 0x826 &&
                digit_events[2].at("index").as_number() == 4 &&
                digit_events[2].at("sequence_label").as_string() == "3" &&
                digit_events[2].at("anchor").as_string() == "bar-low",
            "DRAWNUMBER_DIF expands START/NUM across consecutive bars");
    const auto &letter_events = sequence_primitives[1].at("events").as_array();
    require(letter_events.size() == 3 &&
                sequence_primitives[1].at("annotation_effective_font_table_index").as_number() ==
                    13 &&
                sequence_primitives[1].at("sequence_style_series").as_array().size() == 12 &&
                sequence_primitives[1].at("sequence_style_series").as_array()[0].as_number() == 2 &&
                letter_events[0].at("sequence_style").as_number() == 2 &&
                letter_events[0].at("sequence_style_mode").as_string() == "leader-box" &&
                letter_events[0].at("sequence_label").as_string() == "A" &&
                letter_events[0].at("sequence_label_class").as_string() == "alpha" &&
                letter_events[0].at("sequence_leader").as_bool() &&
                letter_events[0].at("sequence_boxed").as_bool() &&
                letter_events[0].at("sequence_box_width_pixels").as_number() == 14 &&
                letter_events[0].at("sequence_box_height_pixels").as_number() == 14 &&
                letter_events[0].at("sequence_text_alignment").as_string() == "center" &&
                letter_events[0].at("sequence_drawtext_flags").as_number() == 0x825 &&
                letter_events[2].at("sequence_label").as_string() == "C" &&
                letter_events[2].at("anchor").as_string() == "bar-high",
            "DRAWNUMBER_DIF preserves STYLE and maps native 11..36 values to A..Z");

    const auto sequence_state_render = tdx::evaluate_formula_source_document(
        sample(12),
        "DRAWNUMBER_DIF(CURRBARSCOUNT<=6,IF(CURRBARSCOUNT=5,0,2),11,3),"
        "COLORGREEN,DRAWABOVE;DRAWNUMBER_DIF(0.5,2,11,3),COLORRED;",
        {}, "SEQUENCESTATE");
    const auto &state_primitives =
        sequence_state_render.at("render_ir").at("primitives").as_array();
    const auto &state_events = state_primitives[0].at("events").as_array();
    require(state_events.size() == 6 &&
                state_primitives[0].at("sequence_style_series").as_array().size() == 12 &&
                state_primitives[0].at("sequence_style_series").as_array()[7].as_number() == 0 &&
                state_events[0].at("index").as_number() == 6 &&
                state_events[0].at("source_index").as_number() == 6 &&
                state_events[0].at("sequence_offset").as_number() == 0 &&
                state_events[1].at("index").as_number() == 7 &&
                state_events[1].at("source_index").as_number() == 6 &&
                state_events[1].at("sequence_style").as_number() == 0 &&
                state_events[1].at("sequence_style_mode").as_string() == "plain" &&
                !state_events[1].at("sequence_leader").as_bool() &&
                state_events[1].at("sequence_offset_pixels").as_number() == 0 &&
                state_events[3].at("index").as_number() == 9 &&
                state_events[3].at("source_index").as_number() == 9 &&
                state_events[3].at("sequence_offset").as_number() == 0 &&
                state_events[5].at("source_index").as_number() == 9 &&
                state_primitives[1].at("events").as_array().empty(),
            "DRAWNUMBER_DIF uses native exact true, per-bar style, and suppresses overlapping "
            "triggers");
}

} // namespace formula_engine_test
