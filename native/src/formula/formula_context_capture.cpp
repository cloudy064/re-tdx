#include "tdx/formula_engine.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx {
namespace {

constexpr std::size_t maximum_capture_records = 20000;
constexpr std::size_t maximum_capture_bindings = 512;
constexpr std::size_t maximum_diagnostic_preview = 100;

const Json* member(const Json& value, std::string_view name) {
    if (!value.is_object()) return nullptr;
    const auto found = value.as_object().find(name);
    return found == value.as_object().end() ? nullptr : &found->second;
}

void require_exact_fields(const Json& value,
                          std::initializer_list<std::string_view> allowed,
                          std::string_view path) {
    if (!value.is_object()) throw Error(std::string(path) + " must be an object");
    std::set<std::string, std::less<>> names;
    for (const auto name : allowed) names.emplace(name);
    for (const auto& [name, ignored] : value.as_object()) {
        (void)ignored;
        if (!names.count(name))
            throw Error(std::string(path) + " contains unexpected field " + name);
    }
}

std::string upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

std::string required_string(const Json& object, std::string_view name,
                            std::string_view path) {
    const auto* value = member(object, name);
    if (!value || !value->is_string())
        throw Error(std::string(path) + "." + std::string(name) + " must be a string");
    return value->as_string();
}

std::size_t exact_size(const Json& object, std::string_view name,
                       std::string_view path) {
    const auto* value = member(object, name);
    if (!value || !value->is_number() || !std::isfinite(value->as_number()) ||
        value->as_number() < 0.0 || std::floor(value->as_number()) != value->as_number() ||
        value->as_number() > static_cast<double>(std::numeric_limits<std::size_t>::max()))
        throw Error(std::string(path) + "." + std::string(name) +
                    " must be a non-negative integer");
    return static_cast<std::size_t>(value->as_number());
}

void require_numeric_or_null(const Json& value, const std::string& path) {
    if (value.is_null()) return;
    if (!value.is_number() || !std::isfinite(value.as_number()))
        throw Error(path + " must be a finite number or null");
}

bool safe_identifier(const std::string& value) {
    if (value.empty() || value.size() > 128) return false;
    return std::none_of(value.begin(), value.end(), [](unsigned char ch) {
        return ch < 0x20 || ch == 0x7f;
    });
}

bool safe_stamp(const std::string& value) {
    if (value.empty() || value.size() > 64) return false;
    const auto separator = value.find('|');
    if (separator == std::string::npos || separator == 0 ||
        separator + 1 == value.size() || value.find('|', separator + 1) != std::string::npos)
        return false;
    return std::none_of(value.begin(), value.end(), [](unsigned char ch) {
        return ch < 0x20 || ch == 0x7f;
    });
}

void push_preview(Json& values, Json value, std::size_t& omitted) {
    if (values.size() < maximum_diagnostic_preview) values.push_back(std::move(value));
    else ++omitted;
}

struct TemplatePlan {
    std::set<std::string> scalar_bindings;
    std::set<std::string> series_bindings;
    std::set<std::string> stamps;
    std::map<std::string, std::string> value_shapes;
    std::size_t expected_points{};
};

TemplatePlan parse_template(const Json& input) {
    require_exact_fields(input,
        {"schema", "automatic_market_context", "formula_scalar_bindings",
         "series", "_template"}, "template");
    if (required_string(input, "schema", "template") !=
        "tdx-formula-explicit-context-v1")
        throw Error("template.schema must be tdx-formula-explicit-context-v1");
    const auto* automatic = member(input, "automatic_market_context");
    if (!automatic || !automatic->is_bool() || automatic->as_bool())
        throw Error("template.automatic_market_context must be false");
    const auto* scalars = member(input, "formula_scalar_bindings");
    const auto* series = member(input, "series");
    const auto* metadata = member(input, "_template");
    if (!scalars || !scalars->is_object() || !series || !series->is_object() ||
        !metadata || !metadata->is_object())
        throw Error("template requires formula_scalar_bindings, series, and _template objects");
    if (scalars->size() > maximum_capture_bindings ||
        series->size() > maximum_capture_bindings ||
        scalars->size() + series->size() > maximum_capture_bindings)
        throw Error("template exceeds the 512-binding safety limit");
    if (required_string(*metadata, "schema", "template._template") !=
        "tdx-formula-explicit-context-template-v1")
        throw Error("template._template.schema must be tdx-formula-explicit-context-template-v1");

    TemplatePlan plan;
    for (const auto& [raw_name, value] : scalars->as_object()) {
        const auto name = upper_ascii(raw_name);
        if (name != raw_name || name.empty() || !value.is_null())
            throw Error("template scalar placeholders must use canonical names and null values");
        if (!plan.scalar_bindings.insert(name).second)
            throw Error("template contains a duplicate scalar binding " + name);
    }
    bool first_series = true;
    for (const auto& [raw_name, values] : series->as_object()) {
        const auto name = upper_ascii(raw_name);
        if (name != raw_name || name.empty() || !values.is_object())
            throw Error("template series placeholders must use canonical names and objects");
        if (!plan.series_bindings.insert(name).second)
            throw Error("template contains a duplicate series binding " + name);
        std::set<std::string> binding_stamps;
        for (const auto& [stamp, value] : values.as_object()) {
            if (!safe_stamp(stamp) || !value.is_null())
                throw Error("template series placeholders require safe stamps and null values");
            binding_stamps.insert(stamp);
        }
        if (first_series) {
            plan.stamps = binding_stamps;
            first_series = false;
        } else if (binding_stamps != plan.stamps) {
            throw Error("template series bindings must share one exact stamp set");
        }
        plan.expected_points += binding_stamps.size();
    }
    if (!plan.series_bindings.empty() && plan.stamps.empty())
        throw Error("template series bindings require at least one stamp");

    const auto* bindings = member(*metadata, "bindings");
    if (!bindings || !bindings->is_array())
        throw Error("template._template.bindings must be an array");
    std::set<std::string> described;
    for (const auto& row : bindings->as_array()) {
        if (!row.is_object()) throw Error("template binding descriptions must be objects");
        const auto name = upper_ascii(required_string(row, "name", "template binding"));
        const auto shape = required_string(row, "value_shape", "template binding");
        const auto* required = member(row, "required");
        if (!required || !required->is_bool() || !required->as_bool())
            throw Error("template binding descriptions must be required");
        if (!described.insert(name).second)
            throw Error("template binding descriptions contain duplicate " + name);
        if (plan.scalar_bindings.count(name)) {
            if (shape != "u16-scalar")
                throw Error("template scalar binding " + name + " must use u16-scalar");
        } else if (plan.series_bindings.count(name)) {
            if (shape != "date-time-series")
                throw Error("template series binding " + name + " must use date-time-series");
        } else {
            throw Error("template binding description is not present in its placeholders: " + name);
        }
        plan.value_shapes.emplace(name, shape);
    }
    std::set<std::string> expected = plan.scalar_bindings;
    expected.insert(plan.series_bindings.begin(), plan.series_bindings.end());
    const auto metadata_stamp_count = exact_size(
        *metadata, "stamp_count", "template._template");
    if (described != expected ||
        exact_size(*metadata, "binding_count", "template._template") != expected.size() ||
        exact_size(*metadata, "scalar_binding_count", "template._template") != plan.scalar_bindings.size() ||
        exact_size(*metadata, "series_binding_count", "template._template") != plan.series_bindings.size() ||
        (!plan.series_bindings.empty() && metadata_stamp_count != plan.stamps.size()))
        throw Error("template binding metadata does not match its placeholders");
    return plan;
}

}  // namespace

Json import_formula_explicit_context_capture(
    const FormulaExplicitContextCaptureImportRequest& request) {
    const auto plan = parse_template(request.context_template);
    const auto& capture = request.capture;
    require_exact_fields(capture,
        {"schema", "capture_id", "ownership_confirmed",
         "formula_scalar_bindings", "records"}, "capture");
    if (required_string(capture, "schema", "capture") !=
        "tdx-formula-caller-context-capture-v1")
        throw Error("capture.schema must be tdx-formula-caller-context-capture-v1");
    const auto capture_id = required_string(capture, "capture_id", "capture");
    if (!safe_identifier(capture_id))
        throw Error("capture.capture_id must be 1-128 printable bytes");
    const auto* ownership = member(capture, "ownership_confirmed");
    if (!ownership || !ownership->is_bool() || !ownership->as_bool())
        throw Error("capture.ownership_confirmed must be true");
    const auto* scalars = member(capture, "formula_scalar_bindings");
    const auto* records = member(capture, "records");
    if (!scalars || !scalars->is_object() || !records || !records->is_array())
        throw Error("capture requires formula_scalar_bindings object and records array");
    if (scalars->size() > maximum_capture_bindings)
        throw Error("capture exceeds the 512-scalar safety limit");
    if (records->size() > maximum_capture_records)
        throw Error("capture exceeds the 20000-record safety limit");

    Json result = request.context_template;
    auto& output_scalars = result["formula_scalar_bindings"];
    auto& output_series = result["series"];
    std::set<std::string> supplied_scalars;
    std::size_t numeric_scalar_count = 0;
    for (const auto& [raw_name, value] : scalars->as_object()) {
        const auto name = upper_ascii(raw_name);
        if (!supplied_scalars.insert(name).second)
            throw Error("capture contains duplicate scalar binding " + name);
        if (!plan.scalar_bindings.count(name))
            throw Error("capture contains unexpected scalar binding " + name);
        require_numeric_or_null(value, "capture.formula_scalar_bindings." + raw_name);
        if (value.is_number()) {
            const auto number = value.as_number();
            if (std::floor(number) != number || number < 0.0 || number > 65535.0)
                throw Error("capture scalar binding " + name + " must be a u16 integer or null");
            ++numeric_scalar_count;
        }
        output_scalars[name] = value;
    }

    std::set<std::string> seen_stamps;
    std::map<std::string, std::set<std::string>> supplied_points;
    std::size_t matched_record_count = 0;
    std::size_t outside_window_record_count = 0;
    std::size_t outside_preview_omitted = 0;
    std::size_t matched_point_count = 0;
    std::size_t explicit_missing_point_count = 0;
    std::size_t numeric_point_count = 0;
    Json outside_preview = Json::array();
    for (std::size_t index = 0; index < records->size(); ++index) {
        const auto& record = records->as_array()[index];
        require_exact_fields(record, {"stamp", "values"},
                             "capture.records[" + std::to_string(index) + "]");
        const auto stamp = required_string(
            record, "stamp", "capture.records[" + std::to_string(index) + "]");
        if (!safe_stamp(stamp)) throw Error("capture record contains an invalid stamp");
        if (!seen_stamps.insert(stamp).second)
            throw Error("capture contains duplicate stamp " + stamp);
        const auto* values = member(record, "values");
        if (!values || !values->is_object() || values->size() > maximum_capture_bindings)
            throw Error("capture record values must be an object within the binding limit");
        const bool in_window = plan.stamps.count(stamp) != 0;
        if (in_window) ++matched_record_count;
        else {
            ++outside_window_record_count;
            push_preview(outside_preview, stamp, outside_preview_omitted);
        }
        std::set<std::string> record_bindings;
        for (const auto& [raw_name, value] : values->as_object()) {
            const auto name = upper_ascii(raw_name);
            if (!record_bindings.insert(name).second)
                throw Error("capture record contains duplicate binding " + name);
            if (!plan.series_bindings.count(name))
                throw Error("capture contains unexpected series binding " + name);
            require_numeric_or_null(value, "capture record " + stamp + " binding " + name);
            if (!in_window) continue;
            supplied_points[name].insert(stamp);
            output_series[name][stamp] = value;
            ++matched_point_count;
            if (value.is_null()) ++explicit_missing_point_count;
            else ++numeric_point_count;
        }
    }

    Json missing_scalars = Json::array();
    for (const auto& name : plan.scalar_bindings)
        if (!supplied_scalars.count(name)) missing_scalars.push_back(name);
    const auto missing_scalar_count = missing_scalars.size();
    Json missing_points_preview = Json::array();
    std::size_t missing_points_preview_omitted = 0;
    std::size_t missing_point_count = 0;
    for (const auto& name : plan.series_bindings)
        for (const auto& stamp : plan.stamps)
            if (!supplied_points[name].count(stamp)) {
                ++missing_point_count;
                Json row = Json::object();
                row["binding"] = name;
                row["stamp"] = stamp;
                push_preview(missing_points_preview, std::move(row),
                             missing_points_preview_omitted);
            }
    const bool complete = missing_scalar_count == 0 && missing_point_count == 0;
    if (!request.allow_partial && !complete)
        throw Error("capture is incomplete: missing " +
                    std::to_string(missing_scalar_count) + " scalar bindings and " +
                    std::to_string(missing_point_count) + " series points; pass --allow-partial only for inspection");

    bool evaluation_ready = complete && numeric_scalar_count == plan.scalar_bindings.size();
    if (evaluation_ready)
        for (const auto& name : plan.series_bindings) {
            bool numeric = false;
            for (const auto& [stamp, value] : output_series.at(name).as_object()) {
                (void)stamp;
                if (value.is_number()) { numeric = true; break; }
            }
            // A structurally complete capture may legitimately contain a
            // native-missing series.  Its completeness metadata lets the
            // evaluator distinguish that state from an unfilled template.
            if (!numeric && plan.stamps.empty()) { evaluation_ready = false; break; }
        }

    Json metadata = Json::object();
    metadata["schema"] = "tdx-formula-context-capture-import-v1";
    metadata["capture_schema"] = "tdx-formula-caller-context-capture-v1";
    metadata["capture_id"] = capture_id;
    metadata["ownership_confirmed"] = true;
    metadata["allow_partial"] = request.allow_partial;
    metadata["complete"] = complete;
    metadata["evaluation_ready"] = evaluation_ready;
    metadata["required_binding_count"] = static_cast<std::uint64_t>(
        plan.scalar_bindings.size() + plan.series_bindings.size());
    metadata["expected_scalar_binding_count"] =
        static_cast<std::uint64_t>(plan.scalar_bindings.size());
    metadata["supplied_scalar_binding_count"] =
        static_cast<std::uint64_t>(supplied_scalars.size());
    metadata["numeric_scalar_binding_count"] =
        static_cast<std::uint64_t>(numeric_scalar_count);
    metadata["missing_scalar_binding_count"] =
        static_cast<std::uint64_t>(missing_scalar_count);
    metadata["missing_scalar_bindings"] = std::move(missing_scalars);
    metadata["expected_series_binding_count"] =
        static_cast<std::uint64_t>(plan.series_bindings.size());
    metadata["expected_stamp_count"] =
        static_cast<std::uint64_t>(plan.stamps.size());
    metadata["materialized_bar_count"] =
        static_cast<std::uint64_t>(complete ? plan.stamps.size() : 0);
    metadata["expected_series_point_count"] =
        static_cast<std::uint64_t>(plan.expected_points);
    metadata["matched_series_point_count"] =
        static_cast<std::uint64_t>(matched_point_count);
    metadata["numeric_series_point_count"] =
        static_cast<std::uint64_t>(numeric_point_count);
    metadata["explicit_missing_series_point_count"] =
        static_cast<std::uint64_t>(explicit_missing_point_count);
    metadata["missing_series_point_count"] =
        static_cast<std::uint64_t>(missing_point_count);
    metadata["missing_series_points_preview"] = std::move(missing_points_preview);
    metadata["missing_series_points_preview_omitted"] =
        static_cast<std::uint64_t>(missing_points_preview_omitted);
    metadata["capture_record_count"] =
        static_cast<std::uint64_t>(records->size());
    metadata["matched_record_count"] =
        static_cast<std::uint64_t>(matched_record_count);
    metadata["outside_window_record_count"] =
        static_cast<std::uint64_t>(outside_window_record_count);
    metadata["outside_window_stamps_preview"] = std::move(outside_preview);
    metadata["outside_window_stamps_preview_omitted"] =
        static_cast<std::uint64_t>(outside_preview_omitted);
    metadata["capture_document_retained"] = false;
    metadata["sdk_called"] = false;
    metadata["subscription_sent"] = false;
    metadata["account_accessed"] = false;
    metadata["order_sent"] = false;
    metadata["network_requests"] = 0;
    metadata["authorization_bypass"] = false;
    metadata["semantics"] =
        "Caller-owned records are aligned only by exact template DATE|TIME keys; "
        "outside-window records are reported and ignored, and no missing value is fabricated.";
    result["_capture_import"] = std::move(metadata);
    return result;
}

}  // namespace tdx
