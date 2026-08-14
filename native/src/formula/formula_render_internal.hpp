#pragma once

#include "formula_engine_internal.hpp"
#include "formula_language_internal.hpp"
#include "formula_runtime_internal.hpp"

#include "tdx/json.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::formula_render_detail {

struct NativeAnnotationNumberFormat {
    int chart_precision{2};
    int index_info_mode{};
    int index_info_precision{2};
    std::string chart_precision_source{"native-constructor-default"};
    std::string index_info_source{"native-ordinary-security-default"};
};

struct RenderKindDefinition {
    std::string_view function;
    std::string_view kind;
};

inline constexpr std::array<RenderKindDefinition, 17> render_kind_definitions{{
    {"STICKLINE", "stick"},
    {"DRAWICON", "icon"},
    {"DRAWKLINE", "candlestick"},
    {"DRAWTEXT", "text"},
    {"DRAWTEXT_FIX", "text"},
    {"DRAWNUMBER", "number"},
    {"DRAWNUMBER_FIX", "number"},
    {"DRAWNUMBER_DIF", "number"},
    {"DRAWBAND", "band"},
    {"DRAWBMP", "bitmap"},
    {"DRAWGBK", "pane-background"},
    {"DRAWGBK_DIV", "background"},
    {"DRAWRECTREL", "pane-rectangle"},
    {"DRAWSL", "slope-line"},
    {"PARTLINE", "part-line"},
    {"DRAWLINE", "draw-line"},
    {"PLOYLINE", "polyline"},
}};

inline constexpr float native_stick_color_epsilon =
    0.00009999999747378752F;

struct PrimitiveRenderContext {
    const formula_language_detail::Statement& statement;
    formula_engine_detail::Environment& environment;
    const std::vector<formula_runtime_detail::Bar>& bars;
    const NativeAnnotationNumberFormat& number_format;
    const std::string& function;
    bool part_line;
    const std::vector<formula_engine_detail::Series>& numeric_arguments;
    const std::vector<formula_runtime_detail::StringSeries>& string_arguments;
    const std::vector<bool>& string_available;
};

bool directive_present(
    const formula_language_detail::Statement& statement,
    std::string_view wanted);
std::string native_series_mode(
    const formula_language_detail::Statement& statement);
Json render_style_document(
    const formula_language_detail::Statement& statement);
std::string render_kind(const std::string& function);
Json numeric_arguments_at(
    const std::vector<formula_engine_detail::Series>& arguments,
    std::size_t index);
Json string_arguments_at(
    const std::vector<formula_runtime_detail::StringSeries>& arguments,
    const std::vector<bool>& available,
    std::size_t index);
std::string drawnumber_dif_label(int value);
bool drawnumber_dif_native_true(double value);
bool native_biased_int(double value, int& result);
std::string drawnumber_dif_style_mode(double value);
bool drawnumber_dif_alpha_label(std::string_view label);
std::string annotation_text_limit(const std::string& text);
std::string annotation_number_label(
    double value,
    const NativeAnnotationNumberFormat& format);
Json annotation_lines(const std::string& raw_text);
std::string native_colorstick_role(double value);
bool native_volstick_open_close_up(
    const std::vector<formula_runtime_detail::Bar>& bars,
    std::size_t index);
bool native_volstick_previous_close_up(
    const std::vector<formula_runtime_detail::Bar>& bars,
    std::size_t index);
std::string stickline_mode(double empty);

NativeAnnotationNumberFormat native_annotation_number_format(
    const Json& document);

bool apply_series_render(
    Json& primitive,
    const formula_language_detail::Statement& statement,
    formula_engine_detail::Environment& environment,
    const std::vector<formula_runtime_detail::Bar>& bars);
void apply_primitive_metadata(
    Json& primitive,
    const PrimitiveRenderContext& context);
Json render_primitive_events(const PrimitiveRenderContext& context);

// Per-primitive event renderers.  Each appends to the events array and reads
// everything it needs from the render context, so the dispatch unit only owns
// the function-name table.  Renderers keep their own arity guards: a primitive
// whose argument count does not match contributes no events.
using PrimitiveEventRenderer = void (*)(const PrimitiveRenderContext&, Json&);
void render_polyline_events(const PrimitiveRenderContext&, Json&);
void render_draw_line_events(const PrimitiveRenderContext&, Json&);
void render_slope_line_events(const PrimitiveRenderContext&, Json&);
void render_bitmap_events(const PrimitiveRenderContext&, Json&);
void render_pane_background_events(const PrimitiveRenderContext&, Json&);
void render_pane_rectangle_events(const PrimitiveRenderContext&, Json&);
void render_sequence_events(const PrimitiveRenderContext&, Json&);
void render_region_background_events(const PrimitiveRenderContext&, Json&);

// Fallback renderer for the primitives that share one per-bar loop
// (DRAWKLINE, DRAWBAND, PARTLINE, STICKLINE, DRAWICON and the text/number
// annotations).  The shared loop owns gating and the segment fields; the three
// helpers below enrich an event that the loop has already opened.
void render_general_events(const PrimitiveRenderContext&, Json&);
void apply_stick_event_fields(
    Json& event, const PrimitiveRenderContext& context, std::size_t index);
void apply_icon_event_fields(
    Json& event, const PrimitiveRenderContext& context, std::size_t index);
void apply_annotation_event_fields(
    Json& event, const PrimitiveRenderContext& context, std::size_t index);
void apply_primitive_post_metadata(
    Json& primitive,
    const PrimitiveRenderContext& context);

Json render_primitive_document(
    const formula_language_detail::Statement& statement,
    formula_engine_detail::Environment& env,
    const formula_runtime_detail::StringEnvironment& strings,
    const std::vector<formula_runtime_detail::Bar>& bars,
    const NativeAnnotationNumberFormat& number_format,
    std::size_t statement_index,
    std::size_t render_order);

}  // namespace tdx::formula_render_detail
