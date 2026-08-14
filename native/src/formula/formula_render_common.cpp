#include "formula_render_internal.hpp"
#include "formula_native_constants.hpp"

#include "formula_engine_support_internal.hpp"
#include "formula_render_support_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/formula_render_profile.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace tdx::formula_render_detail {
using namespace formula_engine_detail;
using namespace formula_engine_support;
using namespace formula_language_detail;
using namespace formula_runtime_detail;
bool directive_present(const Statement& statement, std::string_view wanted) {
    return std::find(statement.directives.begin(), statement.directives.end(), wanted) !=
           statement.directives.end();
}

std::string native_series_mode(const Statement& statement) {
    // TCalc stores the selected renderer as a single style id.  Preserve the
    // source directive order here so that, if a hand-written formula contains
    // more than one renderer directive, the final directive wins as it does in
    // the native parser.
    std::string mode = "line";
    for (const auto& directive : statement.directives) {
        if (directive == "VOLSTICK") mode = "volume-stick";
        else if (directive == "COLORSTICK") mode = "color-stick";
        else if (directive == "STICK") mode = "stick";
        else if (directive == "LINESTICK") mode = "line-stick";
        else if (directive == "CIRCLEDOT") mode = "circle-dot";
        else if (directive == "CROSSDOT") mode = "cross-dot";
        else if (directive == "POINTDOT") mode = "point-dot";
        else if (directive == "DOTLINE") mode = "dot-line";
    }
    return mode;
}

Json render_style_document(const Statement& statement) {
    Json directives = Json::array();
    Json style = Json::object();
    style["visible"] = !directive_present(statement, "NODRAW");
    style["draw_above"] = directive_present(statement, "DRAWABOVE");
    style["draw_cframe"] = directive_present(statement, "DRAWCFRAME");
    style["dot_line"] = native_series_mode(statement) == "dot-line";
    style["line_thickness"] = 1;
    style["color_token"] = "";
    style["color_ref_available"] = false;
    style["color_ref"] = Json(nullptr);
    style["color_source"] = "none";
    style["color_encoding"] = "Windows COLORREF: red | green<<8 | blue<<16";
    for (const auto& directive : statement.directives) {
        directives.push_back(directive);
        if (directive.rfind("LINETHICK", 0) == 0 && directive.size() > 9) {
            try {
                style["line_thickness"] = std::clamp(
                    std::stoi(directive.substr(9)), 1, 16);
            } catch (...) { /* Preserve the raw directive even if malformed. */ }
        }
        const auto color = decode_render_color(directive);
        if (color.available) {
            style["color_token"] = directive;
            style["color_ref_available"] = true;
            style["color_ref"] = static_cast<std::uint64_t>(color.colorref);
            style["color_source"] = color.source;
        }
    }
    style["directives"] = std::move(directives);
    style["directive_order_preserved"] = true;
    return style;
}


std::string render_kind(const std::string& function) {
    for (const auto& definition : render_kind_definitions)
        if (definition.function == function)
            return std::string(definition.kind);
    return "line";
}

Json numeric_arguments_at(const std::vector<Series>& arguments, std::size_t index) {
    Json values = Json::array();
    for (const auto& argument : arguments)
        values.push_back(index < argument.size() && std::isfinite(argument[index])
                             ? Json(argument[index]) : Json(nullptr));
    return values;
}

Json string_arguments_at(const std::vector<StringSeries>& arguments,
                         const std::vector<bool>& available,
                         std::size_t index) {
    Json values = Json::object();
    for (std::size_t argument = 0; argument < arguments.size(); ++argument) {
        if (!available[argument] || index >= arguments[argument].size()) continue;
        values[std::to_string(argument)] = arguments[argument][index];
    }
    return values;
}

std::string drawnumber_dif_label(int value) {
    // TCalc's built-in help defines 11..36 as A..Z; 0..9 remain digits.
    // Preserve out-of-range values as decimal text instead of silently
    // wrapping them into an unrelated glyph.
    if (value >= 11 && value <= 36)
        return std::string(1, static_cast<char>('A' + value - 11));
    return std::to_string(value);
}

bool drawnumber_dif_native_true(double value) {
    return std::isfinite(value) && std::abs(value - 1.0) < 0.0001;
}

bool native_biased_int(double value, int& result) {
    if (!std::isfinite(value)) return false;
    constexpr double native_rounding_bias = tdx::formula_engine_detail::tcalc_constants::integer_bias;
    const double biased = value + native_rounding_bias;
    if (biased < static_cast<double>(std::numeric_limits<int>::min()) ||
        biased > static_cast<double>(std::numeric_limits<int>::max()))
        return false;
    result = static_cast<int>(biased);
    return true;
}

std::string drawnumber_dif_style_mode(double value) {
    if (value == 0.0) return "plain";
    if (value == 1.0) return "leader";
    if (value == 2.0) return "leader-box";
    return "offset-without-leader";
}

bool drawnumber_dif_alpha_label(std::string_view label) {
    return label.size() == 1 && label.front() >= 'A' && label.front() <= 'Z';
}

std::string annotation_text_limit(const std::string& text) {
    std::size_t bytes = 0;
    std::size_t characters = 0;
    while (bytes < text.size() && characters < 250) {
        const auto lead = static_cast<unsigned char>(text[bytes]);
        std::size_t width = lead < 0x80 ? 1 : (lead & 0xE0) == 0xC0 ? 2 :
                            (lead & 0xF0) == 0xE0 ? 3 :
                            (lead & 0xF8) == 0xF0 ? 4 : 1;
        if (bytes + width > text.size()) width = 1;
        bytes += width;
        ++characters;
    }
    return text.substr(0, bytes);
}

int bounded_document_integer(const Json& document, std::string_view key,
                             int fallback, int minimum, int maximum,
                             bool& available) {
    const auto* value = optional(document, key);
    available = value && value->is_number() && std::isfinite(value->as_number());
    if (!available) return fallback;
    return std::clamp(static_cast<int>(value->as_number()), minimum, maximum);
}

NativeAnnotationNumberFormat native_annotation_number_format(const Json& document) {
    NativeAnnotationNumberFormat result;
    bool chart_available = false, mode_available = false, precision_available = false;
    result.chart_precision = bounded_document_integer(
        document, "price_precision", 2, 0, 8, chart_available);
    result.index_info_mode = bounded_document_integer(
        document, "index_info_format_mode", 0, 0, 255, mode_available);
    result.index_info_precision = bounded_document_integer(
        document, "index_info_format_precision", 2, -128, 127,
        precision_available);
    if (chart_available) result.chart_precision_source = "kline.price_precision";
    if (mode_available || precision_available)
        result.index_info_source = "kline.index_info_format_mode/precision";
    return result;
}

int native_fixed_precision(const NativeAnnotationNumberFormat& format) {
    const int chart = std::max(0, format.chart_precision);
    if (format.index_info_mode == 1) return std::min(chart, 4);
    if (format.index_info_mode == 2) return chart >= 3 ? 4 : chart + 1;
    if (format.index_info_mode != 0)
        return format.index_info_precision >= 0 && format.index_info_precision <= 4
            ? format.index_info_precision : 2;
    return -1;
}

std::string fixed_decimal_label(double value, int precision) {
    std::ostringstream text;
    text << std::fixed << std::setprecision(precision) << value;
    return text.str();
}

std::string annotation_number_label(double value,
                                    const NativeAnnotationNumberFormat& format) {
    // sub_9593C0 reads a float series.  Its IndexInfo mode branch adds 1e-6
    // before applying the fixed %-.0f..%-.5f format selected by sub_59C390.
    const double native_value = static_cast<double>(static_cast<float>(value));
    const int fixed_precision = native_fixed_precision(format);
    if (fixed_precision >= 0)
        return fixed_decimal_label(native_value + 0.0000009999999974752427,
                                   fixed_precision);

    // Ordinary securities use sub_591950.  Exact integer-like values are
    // rendered with zero decimals; every other value uses two or three
    // decimals according to min(chart_precision + 1, 4).
    const auto native_remainder = [&](double scale, int divisor) {
        const double correction = native_value <= 0.00009999999747378752
            ? -0.50300002 : 0.50300002;
        const double rounded = native_value * scale + correction;
        if (rounded < static_cast<double>(std::numeric_limits<int>::min()) ||
            rounded > static_cast<double>(std::numeric_limits<int>::max()))
            return 1;
        return static_cast<int>(rounded) % divisor;
    };
    if (native_remainder(1000.0, 1000) == 0 ||
        native_remainder(100.0, 100) == 0)
        return fixed_decimal_label(native_value, 0);
    const int precision_argument = std::min(format.chart_precision + 1, 4);
    return fixed_decimal_label(native_value, precision_argument < 3 ? 2 : 3);
}

Json annotation_lines(const std::string& raw_text) {
    const auto value = annotation_text_limit(raw_text);
    Json lines = Json::array();
    std::size_t start = 0;
    while (lines.size() < 10) {
        const auto separator = value.find('&', start);
        lines.push_back(value.substr(start, separator == std::string::npos
            ? std::string::npos : separator - start));
        if (separator == std::string::npos || lines.size() == 10) break;
        start = separator + 1;
    }
    return lines;
}

std::string native_colorstick_role(double value) {
    // TdxW!sub_9555B0 converts the source point to float, multiplies it by
    // 10000, then performs two strict comparisons around the renderer epsilon.
    const float scaled = static_cast<float>(value) * 10000.0F;
    if (scaled > native_stick_color_epsilon) return "up";
    if (scaled < native_stick_color_epsilon) return "down";
    return "none";
}

bool native_volstick_open_close_up(const std::vector<Bar>& bars,
                                   std::size_t index) {
    const float open = static_cast<float>(bars[index].open);
    const float close = static_cast<float>(bars[index].close);
    if (close - open > native_stick_color_epsilon) return true;
    if (open - close > native_stick_color_epsilon) return false;
    if (index == 0) return true;
    return static_cast<float>(bars[index - 1].close) - close <=
           native_stick_color_epsilon;
}

bool native_volstick_previous_close_up(const std::vector<Bar>& bars,
                                       std::size_t index) {
    const float reference = index == 0 ? static_cast<float>(bars[index].open)
                                       : static_cast<float>(bars[index - 1].close);
    return reference - static_cast<float>(bars[index].close) <=
           native_stick_color_epsilon;
}

std::string stickline_mode(double empty) {
    if (std::abs(empty) < 1e-9) return "solid";
    if (std::abs(empty + 1.0) < 1e-9) return "dashed-hollow";
    if (std::abs(empty - 2.0) < 1e-9) return "center-full";
    if (std::abs(empty - 3.0) < 1e-9) return "center-half";
    // TCalc treats every other non-zero EMPTY value as the ordinary
    // solid-border hollow form.  The system ICHIMOKU formula relies on
    // fractional values (0.1 and 1.5), so this must not be narrowed to 1.
    return "solid-hollow";
}


}  // namespace tdx::formula_render_detail
