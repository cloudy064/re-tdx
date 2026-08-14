#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace tdx::formula_render_detail {

struct RenderColor {
    bool available{};
    std::uint32_t colorref{};
    const char* source{"none"};
};

RenderColor decode_render_color(std::string_view directive);
bool supported_presentation_directive(const std::string& directive);

}  // namespace tdx::formula_render_detail
