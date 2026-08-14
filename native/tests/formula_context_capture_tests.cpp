#include "tdx/formula_engine.hpp"

#include "tdx/common.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

template <typename Callback>
void require_throws(Callback&& callback, const char* message) {
    try {
        callback();
    } catch (const tdx::Error&) {
        return;
    }
    throw tdx::Error(message);
}

tdx::Json binding_row(const std::string& name, const std::string& shape) {
    auto row = tdx::Json::object();
    row["name"] = name;
    row["source_kind"] = shape == "u16-scalar"
        ? "caller-host-raw-scalar" : "authorized-level2";
    row["value_shape"] = shape;
    row["required"] = true;
    return row;
}

tdx::Json context_template(bool with_scalars = true,
                           bool two_series = true) {
    auto result = tdx::Json::object();
    result["schema"] = "tdx-formula-explicit-context-v1";
    result["automatic_market_context"] = false;
    result["formula_scalar_bindings"] = tdx::Json::object();
    if (with_scalars) {
        result["formula_scalar_bindings"]["HOST_EVALUATOR_MARKET_WORD_RAW"] = nullptr;
        result["formula_scalar_bindings"]["HOST_TYPE120_SECURITY_CLASS_RAW"] = nullptr;
    }
    result["series"] = tdx::Json::object();
    result["series"]["SIGNALS_QS#102#0"] = tdx::Json::object();
    result["series"]["SIGNALS_QS#102#0"]["2026-08-13|14:59"] = nullptr;
    result["series"]["SIGNALS_QS#102#0"]["2026-08-13|15:00"] = nullptr;
    if (two_series) {
        result["series"]["L2_AMO#0#2"] = tdx::Json::object();
        result["series"]["L2_AMO#0#2"]["2026-08-13|14:59"] = nullptr;
        result["series"]["L2_AMO#0#2"]["2026-08-13|15:00"] = nullptr;
    }
    auto metadata = tdx::Json::object();
    metadata["schema"] = "tdx-formula-explicit-context-template-v1";
    metadata["binding_count"] = with_scalars ? (two_series ? 4 : 3)
                                               : (two_series ? 2 : 1);
    metadata["scalar_binding_count"] = with_scalars ? 2 : 0;
    metadata["series_binding_count"] = two_series ? 2 : 1;
    metadata["stamp_count"] = 2;
    metadata["bindings"] = tdx::Json::array();
    if (with_scalars) {
        metadata["bindings"].push_back(binding_row(
            "HOST_EVALUATOR_MARKET_WORD_RAW", "u16-scalar"));
        metadata["bindings"].push_back(binding_row(
            "HOST_TYPE120_SECURITY_CLASS_RAW", "u16-scalar"));
    }
    metadata["bindings"].push_back(binding_row(
        "SIGNALS_QS#102#0", "date-time-series"));
    if (two_series)
        metadata["bindings"].push_back(binding_row(
            "L2_AMO#0#2", "date-time-series"));
    result["_template"] = std::move(metadata);
    return result;
}

tdx::Json record(const std::string& stamp, tdx::Json values) {
    auto result = tdx::Json::object();
    result["stamp"] = stamp;
    result["values"] = std::move(values);
    return result;
}

tdx::Json complete_capture() {
    auto result = tdx::Json::object();
    result["schema"] = "tdx-formula-caller-context-capture-v1";
    result["capture_id"] = "owned-fixture-001";
    result["ownership_confirmed"] = true;
    result["formula_scalar_bindings"] = tdx::Json::object();
    result["formula_scalar_bindings"]["host_evaluator_market_word_raw"] = 44;
    result["formula_scalar_bindings"]["HOST_TYPE120_SECURITY_CLASS_RAW"] = 3;
    result["records"] = tdx::Json::array();
    auto first = tdx::Json::object();
    first["signals_qs#102#0"] = 1.25;
    first["L2_AMO#0#2"] = nullptr;
    result["records"].push_back(record("2026-08-13|14:59", std::move(first)));
    auto second = tdx::Json::object();
    second["SIGNALS_QS#102#0"] = 2.5;
    second["l2_amo#0#2"] = 123456.0;
    result["records"].push_back(record("2026-08-13|15:00", std::move(second)));
    auto outside = tdx::Json::object();
    outside["SIGNALS_QS#102#0"] = 987654321.0;
    result["records"].push_back(record("2026-08-12|15:00", std::move(outside)));
    return result;
}

tdx::Json import(tdx::Json capture, bool allow_partial = false,
                 tdx::Json template_document = context_template()) {
    tdx::FormulaExplicitContextCaptureImportRequest request;
    request.context_template = std::move(template_document);
    request.capture = std::move(capture);
    request.allow_partial = allow_partial;
    return tdx::import_formula_explicit_context_capture(request);
}

}  // namespace

int main() {
    try {
        const auto complete = import(complete_capture());
        const auto& metadata = complete.at("_capture_import");
        require(metadata.at("schema").as_string() ==
                    "tdx-formula-context-capture-import-v1" &&
                    metadata.at("complete").as_bool() &&
                    metadata.at("evaluation_ready").as_bool(),
                "complete capture exposes its import and readiness contract");
        require(metadata.at("capture_record_count").as_number() == 3.0 &&
                    metadata.at("matched_record_count").as_number() == 2.0 &&
                    metadata.at("outside_window_record_count").as_number() == 1.0 &&
                    metadata.at("expected_series_point_count").as_number() == 4.0 &&
                    metadata.at("matched_series_point_count").as_number() == 4.0 &&
                    metadata.at("explicit_missing_series_point_count").as_number() == 1.0,
                "complete capture reports exact alignment counts");
        require(complete.at("formula_scalar_bindings")
                    .at("HOST_EVALUATOR_MARKET_WORD_RAW").as_number() == 44.0 &&
                    complete.at("series").at("SIGNALS_QS#102#0")
                    .at("2026-08-13|15:00").as_number() == 2.5 &&
                    complete.at("series").at("L2_AMO#0#2")
                    .at("2026-08-13|14:59").is_null(),
                "capture values materialize into the existing context schema");
        const auto serialized = complete.dump(-1);
        require(serialized.find("987654321") == std::string::npos &&
                    serialized.find("\"records\":") == std::string::npos &&
                    !metadata.at("capture_document_retained").as_bool() &&
                    !metadata.at("sdk_called").as_bool() &&
                    !metadata.at("subscription_sent").as_bool() &&
                    !metadata.at("account_accessed").as_bool() &&
                    !metadata.at("order_sent").as_bool() &&
                    !metadata.at("authorization_bypass").as_bool() &&
                    metadata.at("network_requests").as_number() == 0.0,
                "outside records and capture envelope are not retained and side effects stay false");

        auto missing_only_capture = complete_capture();
        missing_only_capture["formula_scalar_bindings"] = tdx::Json::object();
        missing_only_capture["records"] = tdx::Json::array();
        auto missing_first = tdx::Json::object();
        missing_first["SIGNALS_QS#102#0"] = nullptr;
        missing_only_capture["records"].push_back(record(
            "2026-08-13|14:59", std::move(missing_first)));
        auto missing_second = tdx::Json::object();
        missing_second["SIGNALS_QS#102#0"] = nullptr;
        missing_only_capture["records"].push_back(record(
            "2026-08-13|15:00", std::move(missing_second)));
        const auto missing_only = import(
            std::move(missing_only_capture), false, context_template(false, false));
        const auto analysis = tdx::analyze_formula_source(
            "X:SIGNALS_QS(102,0);");
        require(tdx::formula_explicit_context_ready(analysis, &missing_only),
                "a complete explicitly-missing capture is distinct from an unfilled template");

        auto partial_capture = complete_capture();
        partial_capture["formula_scalar_bindings"].as_object().erase(
            "HOST_TYPE120_SECURITY_CLASS_RAW");
        partial_capture["records"].as_array().erase(
            partial_capture["records"].as_array().begin() + 1);
        require_throws([&] { (void)import(partial_capture); },
                       "strict import rejects absent scalar and series points");
        const auto partial = import(std::move(partial_capture), true);
        require(!partial.at("_capture_import").at("complete").as_bool() &&
                    !partial.at("_capture_import").at("evaluation_ready").as_bool() &&
                    partial.at("_capture_import")
                        .at("missing_scalar_binding_count").as_number() == 1.0 &&
                    partial.at("_capture_import")
                        .at("missing_series_point_count").as_number() == 2.0 &&
                    partial.at("_capture_import")
                        .at("missing_series_points_preview").size() == 2,
                "partial inspection reports exact missing bindings and points");

        auto unknown = complete_capture();
        unknown["records"].as_array()[0]["values"]["SECRET"] = 1.0;
        require_throws([&] { (void)import(unknown); },
                       "unexpected capture bindings are rejected");
        auto duplicate_stamp = complete_capture();
        duplicate_stamp["records"].push_back(
            duplicate_stamp["records"].as_array()[0]);
        require_throws([&] { (void)import(duplicate_stamp); },
                       "duplicate capture stamps are rejected");
        auto invalid_scalar = complete_capture();
        invalid_scalar["formula_scalar_bindings"]
                      ["HOST_TYPE120_SECURITY_CLASS_RAW"] = 65536;
        require_throws([&] { (void)import(invalid_scalar); },
                       "caller host raw scalars preserve the u16 boundary");
        auto unowned = complete_capture();
        unowned["ownership_confirmed"] = false;
        require_throws([&] { (void)import(unowned); },
                       "ownership confirmation is mandatory");
        auto dirty_template = context_template();
        dirty_template["series"]["L2_AMO#0#2"]["2026-08-13|15:00"] = 1.0;
        require_throws([&] { (void)import(complete_capture(), false, dirty_template); },
                       "import requires a pristine null-placeholder template");

        const auto suffix = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const auto directory = fs::temp_directory_path() /
            ("tdx-formula-context-capture-" + suffix);
        fs::create_directories(directory);
        const auto template_path = directory / "template.json";
        const auto capture_path = directory / "capture.json";
        const auto output_path = directory / "context.json";
        tdx::atomic_write_text(template_path, context_template().dump(-1));
        tdx::atomic_write_text(capture_path, complete_capture().dump(-1));
        require(tdx::command_formulas_context_import({
                    "--template", template_path.string(),
                    "--capture", capture_path.string(),
                    "--output", output_path.string(), "--compact"}) == 0,
                "context-import CLI succeeds");
        const auto cli_result = tdx::Json::parse(tdx::read_text_utf8(output_path));
        require(cli_result.at("_capture_import").at("complete").as_bool() &&
                    cli_result.at("_capture_import").at("capture_id").as_string() ==
                        "owned-fixture-001",
                "context-import CLI emits a directly reusable context");
        require_throws([&] {
            (void)tdx::command_formulas_context_import({
                "--template", template_path.string(),
                "--capture", capture_path.string(),
                "--output", capture_path.string()});
        }, "context-import CLI never overwrites an input capture");
        require_throws([&] {
            (void)tdx::command_formulas_context_import({
                "--template", template_path.string(),
                "--capture", capture_path.string(), "--url", "https://example.invalid"});
        }, "context-import CLI rejects unknown or network-shaped options");
        fs::remove_all(directory);

        std::cout << "formula context capture tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
