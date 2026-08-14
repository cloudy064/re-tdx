#include "formula_render_internal.hpp"

#include "formula_engine_support_internal.hpp"

#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace tdx::formula_render_detail {
using namespace formula_engine_detail;
using namespace formula_engine_support;
using namespace formula_language_detail;
using namespace formula_runtime_detail;

Json render_primitive_document(const Statement& statement,
                               Environment& env,
                               const StringEnvironment& strings,
                               const std::vector<Bar>& bars,
                               const NativeAnnotationNumberFormat& number_format,
                               std::size_t statement_index,
                               std::size_t render_order) {
    const auto& expression = *statement.expression;
    const bool call = expression.kind == NodeKind::call;
    const std::string function = call ? expression.text : "SERIES";
    const bool event_function = call && render_event_functions.count(function);
    const bool part_line = call && function == "PARTLINE";
    if (!statement.output && !event_function) return Json(nullptr);

    Json primitive = Json::object();
    primitive["statement"] = statement.name;
    primitive["statement_index"] = static_cast<std::uint64_t>(statement_index);
    primitive["render_order"] = static_cast<std::uint64_t>(render_order);
    primitive["render_order_semantics"] = "source-statement-order";
    primitive["function"] = function;
    primitive["kind"] = render_kind(function);
    primitive["output_statement"] = statement.output;
    primitive["style"] = render_style_document(statement);
    primitive["bar_reference"] = "points[index]";

    if (!event_function && !part_line) {
        if (!apply_series_render(primitive, statement, env, bars))
            return Json(nullptr);
        return primitive;
    }

    std::vector<Series> numeric_arguments;
    std::vector<StringSeries> string_arguments(expression.children.size());
    std::vector<bool> string_available(expression.children.size(), false);
    numeric_arguments.reserve(expression.children.size());
    for (std::size_t index = 0; index < expression.children.size(); ++index) {
        numeric_arguments.push_back(evaluate_node(
            *expression.children[index], env, strings, bars.size()));
        string_available[index] = evaluate_string_node(
            *expression.children[index], env, strings, bars.size(),
            string_arguments[index]);
    }
    primitive["argument_count"] = static_cast<std::uint64_t>(numeric_arguments.size());
    primitive["condition_argument"] = event_function &&
        function != "DRAWKLINE" && function != "DRAWBAND" &&
        function != "DRAWRECTREL" ? Json(0) : Json(nullptr);

    const PrimitiveRenderContext context{
        statement, env, bars, number_format, function, part_line,
        numeric_arguments, string_arguments, string_available,
    };
    apply_primitive_metadata(primitive, context);
    auto events = render_primitive_events(context);
    apply_primitive_post_metadata(primitive, context);
    primitive["event_count"] = static_cast<std::uint64_t>(events.size());
    primitive["events"] = std::move(events);
    return primitive;
}

}  // namespace tdx::formula_render_detail
