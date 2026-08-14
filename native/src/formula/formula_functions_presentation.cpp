#include "formula_function_dispatch_internal.hpp"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#include <string_view>
#include <unordered_map>

namespace tdx::formula_engine_detail {
namespace {

using FunctionHandler = Series (*)(const std::string&,
                                   const std::vector<Series>&,
                                   const Environment&, std::size_t);

Series string_surrogate(const std::string& name,
                        const std::vector<Series>& args,
                        const Environment&, std::size_t size) {
    static const std::unordered_map<std::string_view, std::size_t> arities{
        {"CON2STR", 2}, {"STRCAT", 2},  {"VAR2STR", 2},
        {"VARCAT", 2},  {"STRCAT6", 6}, {"VARCAT6", 6},
        {"STRSPACE", 1}, {"SUBSTR", 3},
    };
    const auto found = arities.find(name);
    require_arity(name, args, found->second, found->second);
    return constant(0.0, size);
}

Series polyline(const std::string& name, const std::vector<Series>& args,
                const Environment&, std::size_t size) {
    require_arity(name, args, 2, 2);
    Series out(size, missing);
    std::size_t previous = size;
    for (std::size_t i = 0; i < size; ++i) {
        if (!truth(args[0][i]) || !std::isfinite(args[1][i])) continue;
        if (previous == size) {
            out[i] = args[1][i];
        } else {
            const auto width = i - previous;
            for (std::size_t j = previous; j <= i; ++j) {
                const double fraction = width
                    ? static_cast<double>(j - previous) / width : 0.0;
                out[j] = args[1][previous] +
                    (args[1][i] - args[1][previous]) * fraction;
            }
        }
        previous = i;
    }
    return out;
}

Series drawline(const std::string& name, const std::vector<Series>& args,
                const Environment&, std::size_t size) {
    require_arity(name, args, 5, 5);
    Series out(size, missing);
    std::size_t start = size;
    for (std::size_t i = 0; i < size; ++i) {
        if (truth(args[0][i]) && std::isfinite(args[1][i])) start = i;
        if (start == size || !truth(args[2][i]) ||
            !std::isfinite(args[3][i]))
            continue;
        const auto width = i - start;
        const auto first = args[1][start];
        const auto last = args[3][i];
        const auto slope = width
            ? (last - first) / static_cast<double>(width) : 0.0;
        for (std::size_t j = start; j <= i; ++j)
            out[j] = first + slope * static_cast<double>(j - start);
        if (truth(args[4][i]))
            for (std::size_t j = i + 1; j < size; ++j)
                out[j] = last + slope * static_cast<double>(j - i);
        start = size;
    }
    return out;
}

Series partline(const std::string& name, const std::vector<Series>& args,
                const Environment&, std::size_t) {
    require_arity(name, args, 1, 64);
    return args.front();
}

Series rgb(const std::string& name, const std::vector<Series>& args,
           const Environment&, std::size_t size) {
    require_arity(name, args, 3, 3);
    const auto channel = [](double value) -> std::uint32_t {
        constexpr float tcalc_missing_sentinel = -4.0398103e34F;
        constexpr double signed_i64_limit = 9223372036854775808.0;
        constexpr std::uint64_t integer_indefinite = 0x8000000000000000ULL;
        constexpr double float_limit =
            static_cast<double>(std::numeric_limits<float>::max());
        float raw = tcalc_missing_sentinel;
        if (std::isfinite(value)) {
            if (value > float_limit)
                raw = std::numeric_limits<float>::infinity();
            else if (value < -float_limit)
                raw = -std::numeric_limits<float>::infinity();
            else
                raw = static_cast<float>(value);
        }
        std::uint64_t converted = integer_indefinite;
        const double widened = static_cast<double>(raw);
        if (std::isfinite(raw) && widened >= -signed_i64_limit &&
            widened < signed_i64_limit) {
            converted = static_cast<std::uint64_t>(
                static_cast<std::int64_t>(widened));
        }
        const auto low = static_cast<std::uint32_t>(converted);
        return low < 255U ? low & 0xffU : 254U;
    };
    Series out(size, 0.0);
    for (std::size_t i = 0; i < size; ++i) {
        const std::uint32_t red = channel(args[0][i]);
        const std::uint32_t green = channel(args[1][i]);
        const std::uint32_t blue = channel(args[2][i]);
        const std::uint32_t packed = red | (green << 8U) | (blue << 16U);
        out[i] = static_cast<double>(static_cast<float>(packed));
    }
    return out;
}

Series drawing_surrogate(const std::string& name,
                         const std::vector<Series>& args,
                         const Environment&, std::size_t size) {
    static const std::unordered_map<std::string_view, std::size_t> arities{
        {"DRAWTEXT", 3},       {"DRAWTEXT_FIX", 5},
        {"DRAWNUMBER", 3},     {"DRAWNUMBER_FIX", 5},
        {"DRAWSL", 5},         {"DRAWBMP", 3},
        {"DRAWGBK", 6},        {"DRAWRECTREL", 5},
    };
    const auto found = arities.find(name);
    require_arity(name, args, found->second, found->second);
    return constant(0.0, size);
}

Series unchecked_drawing_surrogate(const std::string&,
                                   const std::vector<Series>&,
                                   const Environment&, std::size_t size) {
    return constant(0.0, size);
}

const std::unordered_map<std::string_view, FunctionHandler>&
presentation_registry() {
    static const std::unordered_map<std::string_view, FunctionHandler> registry{
        {"CON2STR", string_surrogate},
        {"STRCAT", string_surrogate},
        {"VAR2STR", string_surrogate},
        {"VARCAT", string_surrogate},
        {"STRCAT6", string_surrogate},
        {"VARCAT6", string_surrogate},
        {"STRSPACE", string_surrogate},
        {"SUBSTR", string_surrogate},
        {"PLOYLINE", polyline},
        {"DRAWLINE", drawline},
        {"PARTLINE", partline},
        {"RGB", rgb},
        {"DRAWTEXT", drawing_surrogate},
        {"DRAWTEXT_FIX", drawing_surrogate},
        {"DRAWNUMBER", drawing_surrogate},
        {"DRAWNUMBER_FIX", drawing_surrogate},
        {"DRAWSL", drawing_surrogate},
        {"DRAWBMP", drawing_surrogate},
        {"DRAWGBK", drawing_surrogate},
        {"DRAWRECTREL", drawing_surrogate},
        {"STICKLINE", unchecked_drawing_surrogate},
        {"DRAWICON", unchecked_drawing_surrogate},
        {"DRAWKLINE", unchecked_drawing_surrogate},
        {"DRAWNUMBER_DIF", unchecked_drawing_surrogate},
        {"DRAWBAND", unchecked_drawing_surrogate},
        {"DRAWGBK_DIV", unchecked_drawing_surrogate},
    };
    return registry;
}

}  // namespace

std::optional<Series> evaluate_presentation_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size) {
    const auto found = presentation_registry().find(name);
    if (found == presentation_registry().end()) return std::nullopt;
    return found->second(name, args, env, size);
}

}  // namespace tdx::formula_engine_detail
