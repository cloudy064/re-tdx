#include "fund_analytics_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <string>

namespace tdx::fund_analytics_detail {

const ViewDefinition* view_definition(std::string_view view) {
    const auto found = std::find_if(
        view_definitions.begin(), view_definitions.end(),
        [view](const ViewDefinition& value) { return value.view == view; });
    return found == view_definitions.end() ? nullptr : &*found;
}

const StyleDefinition* style_definition(std::string_view code) {
    const auto found = std::find_if(
        style_definitions.begin(), style_definitions.end(),
        [code](const StyleDefinition& value) { return value.code == code; });
    return found == style_definitions.end() ? nullptr : &*found;
}

Json views_document() {
    Json result = Json::array();
    for (const auto& definition : view_definitions)
        result.push_back(definition.view);
    return result;
}

Json benchmark_document(int benchmark) {
    if (benchmark < 0 ||
        benchmark >= static_cast<int>(benchmark_definitions.size()))
        throw Error("benchmark selector is outside the catalog");
    const auto& definition = benchmark_definitions[
        static_cast<std::size_t>(benchmark)];
    Json result = Json::object();
    result["selector"] = benchmark;
    result["market"] = "sh";
    result["market_id"] = 1;
    result["code"] = definition.code;
    result["index_id"] = std::string("SH") + definition.code;
    result["name"] = definition.name;
    return result;
}

}  // namespace tdx::fund_analytics_detail
