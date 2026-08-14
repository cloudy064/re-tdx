#include "server_formula_internal.hpp"

#include "tdx/cloud_calc.hpp"
#include "tdx/common.hpp"
#include "tdx/tpool.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx::server_detail {

Json query_cloud_calc_audit(const FormulaHttpState& state, const RequestTarget& target) {
    const auto cfg = trim(query_value(target, "cfg"));
    return audit_cloud_calc_request(state.root, cfg);
}

Json query_cloud_calc_template(const FormulaHttpState& state, const RequestTarget& target) {
    const auto cfg = trim(query_value(target, "cfg"));
    return generate_cloud_calc_template_request(state.root, cfg);
}

CloudCalcEvaluationOptions cloud_calc_request_options(const Json& body) {
    CloudCalcEvaluationOptions options;
    if (const auto* value = json_member(body, "quotes")) {
        if (!value->is_bool()) throw Error("cloud-calc quotes must be boolean");
        options.quotes = value->as_bool();
    }
    const auto integer_value = [&](std::string_view key, int fallback,
                                   int minimum, int maximum) {
        const auto* value = json_member(body, key);
        if (!value) return fallback;
        if (!value->is_number() || !std::isfinite(value->as_number()) ||
            std::floor(value->as_number()) != value->as_number() ||
            value->as_number() < minimum || value->as_number() > maximum)
            throw Error("cloud-calc " + std::string(key) + " must be an integer in " +
                        std::to_string(minimum) + ".." + std::to_string(maximum));
        return static_cast<int>(value->as_number());
    };
    options.as_of_yyyymmdd = integer_value("as_of", 0, 0, 22001231);
    options.timeout_ms = integer_value("timeout_ms", 10000, 100, 30000);
    if (const auto* value = json_member(body, "snapshot"))
        options.snapshot = *value;
    if (const auto* value = json_member(body, "finance_snapshot"))
        options.finance_snapshot = *value;
    if (const auto* value = json_member(body, "special_limits_snapshot"))
        options.special_limits_snapshot = *value;
    if (const auto* value = json_member(body, "overrides"))
        options.overrides = *value;
    return options;
}

Json query_cloud_calc_execution(const FormulaHttpState& state, const Json& body) {
    if (!body.is_object()) throw Error("cloud-calc request body must be an object");
    const auto cfg = trim(json_body_string(body, "cfg"));
    const auto* row = json_member(body, "row");
    if (!row) throw Error("cloud-calc row is required");
    if (!row->is_object()) throw Error("cloud-calc row must be an object");
    auto options = cloud_calc_request_options(body);
    return evaluate_cloud_calc_request(state.root, cfg, *row, options);
}

Json query_cloud_calc_batch_execution(const FormulaHttpState& state, const Json& body) {
    if (!body.is_object()) throw Error("cloud-calc batch request body must be an object");
    const auto cfg = trim(json_body_string(body, "cfg"));
    const auto* rows = json_member(body, "rows");
    if (!rows) throw Error("cloud-calc batch rows are required");
    if (!rows->is_array()) throw Error("cloud-calc batch rows must be an array");
    auto options = cloud_calc_request_options(body);
    return evaluate_cloud_calc_batch_request(state.root, cfg, *rows, options);
}

Json query_inline_tpool_evaluation(const FormulaHttpState& state, const Json& body) {
    if (!body.is_object()) throw Error("pool evaluation body must be a JSON object");
    const auto* xml_value = json_member(body, "xml");
    if (!xml_value || !xml_value->is_string() || xml_value->as_string().empty())
        throw Error("pool evaluation requires non-empty inline XML");
    const auto& xml = xml_value->as_string();
    if (xml.size() > 512 * 1024)
        throw Error("inline TPool XML exceeds 512 KiB safety limit");

    std::string source_name = "inline.xml";
    if (const auto* source = json_member(body, "source_name")) {
        if (!source->is_string()) throw Error("source_name must be a string");
        source_name = trim(source->as_string());
    }
    const bool safe_source_name = !source_name.empty() && source_name.size() <= 128 &&
        lower_ascii(fs::path(source_name).extension().string()) == ".xml" &&
        source_name.find("..") == std::string::npos &&
        std::all_of(source_name.begin(), source_name.end(), [](unsigned char ch) {
            return std::isalnum(ch) || ch == '.' || ch == '_' || ch == '-';
        });
    if (!safe_source_name)
        throw Error("source_name must be a safe XML filename");

    const auto body_integer = [&](const char* key, int fallback,
                                  int minimum, int maximum) {
        const auto* value = json_member(body, key);
        if (!value) return fallback;
        if (!value->is_number() || !std::isfinite(value->as_number()) ||
            std::floor(value->as_number()) != value->as_number())
            throw Error(std::string(key) + " must be an integer");
        const auto number = value->as_number();
        if (number < minimum || number > maximum)
            throw Error(std::string(key) + " must be in " +
                        std::to_string(minimum) + ".." + std::to_string(maximum));
        return static_cast<int>(number);
    };
    return evaluate_tpool_xml_document(
        xml, source_name,
        body_integer("pages", 1, 1, 20),
        body_integer("page_size", 800, 1, 800),
        body_integer("timeout_ms", 10000, 100, 600000),
        body_integer("limit", 20, 1, 200),
        &state.formulas, state.root);
}


} // namespace tdx::server_detail

