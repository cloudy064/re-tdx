#include "formula_render_internal.hpp"

#include "formula_engine_support_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace tdx::formula_render_detail {
using namespace formula_engine_detail;
using namespace formula_engine_support;
using namespace formula_language_detail;
using namespace formula_runtime_detail;

// DRAWNUMBER_DIF numbers a run of bars starting from a triggering bar.  The run
// stays open across bars ("active"), so the loop below is a small state machine
// rather than an independent per-bar test: a bar that fails the finite-style
// check still closes the run when it reaches the recorded end.
void render_sequence_events(const PrimitiveRenderContext& context, Json& events) {
    const auto& statement = context.statement;
    const auto& bars = context.bars;
    const auto& numeric_arguments = context.numeric_arguments;
    if (numeric_arguments.size() != 4) return;
    bool active = false;
    std::size_t active_source = 0;
    std::size_t active_end = 0;
    int active_start = 0;
    int active_count = 0;
    for (std::size_t index = 0; index < bars.size(); ++index) {
        const bool continuing = active;
        if (!continuing && !drawnumber_dif_native_true(
                numeric_arguments[0][index]))
            continue;

        if (!continuing) {
            if (!std::isfinite(numeric_arguments[2][index]) ||
                !std::isfinite(numeric_arguments[3][index]))
                continue;
            active_count = std::clamp(
                static_cast<int>(numeric_arguments[3][index]), 0, 250);
            if (active_count <= 0) continue;
            active_source = index;
            active_start = static_cast<int>(numeric_arguments[2][index]);
            active_end = std::min(
                bars.empty() ? std::size_t{0} : bars.size() - 1,
                index + static_cast<std::size_t>(active_count - 1));
            active = active_end > index;
        }

        const double sequence_style = numeric_arguments[1][index];
        if (!std::isfinite(sequence_style)) {
            if (index >= active_end) active = false;
            continue;
        }
        const auto offset = static_cast<int>(index - active_source);
        const int sequence_value = active_start + offset;
        const auto label = drawnumber_dif_label(sequence_value);
        const bool alpha_label = drawnumber_dif_alpha_label(label);
        const auto style_mode = drawnumber_dif_style_mode(sequence_style);
        const bool leader = sequence_style == 1.0 || sequence_style == 2.0;
        const bool boxed = sequence_style == 2.0;
        Json event = Json::object();
        event["index"] = static_cast<std::uint64_t>(index);
        event["source_index"] = static_cast<std::uint64_t>(active_source);
        event["sequence_offset"] = offset;
        event["sequence_source_start"] = active_start;
        event["sequence_source_count"] = active_count;
        event["sequence_style"] = sequence_style;
        event["sequence_style_mode"] = style_mode;
        event["sequence_value"] = sequence_value;
        event["sequence_label"] = label;
        event["sequence_label_class"] = alpha_label ? "alpha" : "numeric";
        event["sequence_leader"] = leader;
        event["sequence_boxed"] = boxed;
        event["sequence_offset_pixels"] = sequence_style == 0.0 ? 0 : 10;
        event["sequence_box_width_pixels"] = alpha_label ? 14 : 8;
        event["sequence_box_height_pixels"] = 14;
        event["sequence_text_alignment"] = alpha_label ? "center" : "right";
        event["sequence_drawtext_flags"] = alpha_label ? 0x825 : 0x826;
        event["anchor"] = directive_present(statement, "DRAWABOVE")
            ? "bar-high" : "bar-low";
        event["arguments"] = numeric_arguments_at(numeric_arguments, index);
        events.push_back(std::move(event));
        if (index >= active_end) active = false;
    }
}

}  // namespace tdx::formula_render_detail
