#include "formula_render_support_internal.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <utility>

namespace tdx::formula_render_detail {

RenderColor decode_render_color(std::string_view directive) {
    // Extracted from TCalc.dll: sub_10090590 indexes this exact COLORREF table
    // after matching the 16 fixed-width COLOR* name records at aColorblack.
    static constexpr std::pair<std::string_view, std::uint32_t> named_colors[] = {
        {"COLORBLACK", 0x00000000}, {"COLORBLUE", 0x00FF0000},
        {"COLORGREEN", 0x0000FF00}, {"COLORCYAN", 0x00FFFF00},
        {"COLORRED", 0x000000FF}, {"COLORMAGENTA", 0x00FF00FF},
        {"COLORBROWN", 0x00008080}, {"COLORLIGRAY", 0x00C0C0C0},
        {"COLORGRAY", 0x00808080}, {"COLORLIBLUE", 0x00C0C000},
        {"COLORLIGREEN", 0x0040C040}, {"COLORLICYAN", 0x00808000},
        {"COLORLIRED", 0x008080FF}, {"COLORLIMAGENTA", 0x008000FF},
        {"COLORYELLOW", 0x0000FFFF}, {"COLORWHITE", 0x00FFFFFF},
    };
    for (const auto& [token, colorref] : named_colors)
        if (directive == token)
            return {true, colorref, "tcalc-named-colorref"};

    const auto parse_hex6 = [](std::string_view digits, std::uint32_t& value) {
        if (digits.size() != 6 ||
            !std::all_of(digits.begin(), digits.end(), [](char ch) {
                return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
            }))
            return false;
        try {
            value = static_cast<std::uint32_t>(
                std::stoul(std::string(digits), nullptr, 16));
            return true;
        } catch (...) {
            return false;
        }
    };

    std::uint32_t value = 0;
    if (directive.size() == 11 && directive.rfind("COLOR", 0) == 0 &&
        parse_hex6(directive.substr(5), value))
        return {true, value, "tcalc-literal-colorref"};
    if (directive.size() == 10 && directive.rfind("RGBX", 0) == 0 &&
        parse_hex6(directive.substr(4), value)) {
        const auto red = (value >> 16) & 0xFF;
        const auto green = (value >> 8) & 0xFF;
        const auto blue = value & 0xFF;
        return {true, red | (green << 8) | (blue << 16), "tcalc-rgbx-rgb"};
    }
    return {};
}

bool supported_presentation_directive(const std::string& directive) {
    static const std::set<std::string> exact{
        "NODRAW", "DRAWABOVE", "DRAWCFRAME", "NOFRAME",
        "VOLSTICK", "COLORSTICK", "STICK", "LINESTICK",
        "CIRCLEDOT", "CROSSDOT", "POINTDOT", "DOTLINE",
    };
    if (exact.count(directive)) return true;
    if (decode_render_color(directive).available) return true;
    constexpr std::string_view thickness = "LINETHICK";
    if (directive.rfind(thickness, 0) != 0 ||
        directive.size() == thickness.size())
        return false;
    return std::all_of(
        directive.begin() + static_cast<std::ptrdiff_t>(thickness.size()),
        directive.end(), [](unsigned char value) {
            return std::isdigit(value) != 0;
        });
}

}  // namespace tdx::formula_render_detail
