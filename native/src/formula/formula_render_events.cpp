#include "formula_render_internal.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace tdx::formula_render_detail {
namespace {

struct PrimitiveEventEntry {
    std::string_view function;
    PrimitiveEventRenderer render;
};

// The primitives with dedicated geometry.  Everything not listed here falls
// through to the shared per-bar loop in render_general_events, which is what
// the original if/else chain did with its trailing else branch.
constexpr std::array<PrimitiveEventEntry, 8> primitive_event_renderers{{
    {"PLOYLINE", render_polyline_events},
    {"DRAWLINE", render_draw_line_events},
    {"DRAWSL", render_slope_line_events},
    {"DRAWBMP", render_bitmap_events},
    {"DRAWGBK", render_pane_background_events},
    {"DRAWRECTREL", render_pane_rectangle_events},
    {"DRAWNUMBER_DIF", render_sequence_events},
    {"DRAWGBK_DIV", render_region_background_events},
}};

constexpr bool unique_primitive_functions() {
    for (std::size_t first = 0; first < primitive_event_renderers.size(); ++first)
        for (std::size_t second = first + 1;
             second < primitive_event_renderers.size(); ++second)
            if (primitive_event_renderers[first].function ==
                primitive_event_renderers[second].function)
                return false;
    return true;
}

static_assert(unique_primitive_functions(),
              "primitive event renderers must dispatch on distinct functions");

}  // namespace

Json render_primitive_events(const PrimitiveRenderContext& context) {
    Json events = Json::array();
    for (const auto& entry : primitive_event_renderers) {
        if (entry.function != context.function) continue;
        entry.render(context, events);
        return events;
    }
    render_general_events(context, events);
    return events;
}

}  // namespace tdx::formula_render_detail
