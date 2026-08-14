#include "server_formula_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/formula_engine.hpp"

#include <set>
#include <string>

namespace tdx::server_detail {

Json query_formula_context_import(const Json& body) {
    if (!body.is_object())
        throw Error("formula context-import request must be a JSON object");
    static const std::set<std::string, std::less<>> allowed{
        "template", "capture", "allow_partial"};
    for (const auto& [name, ignored] : body.as_object()) {
        (void)ignored;
        if (!allowed.count(name))
            throw Error("formula context-import request contains unexpected field " + name);
    }
    const auto* context_template = json_member(body, "template");
    const auto* capture = json_member(body, "capture");
    if (!context_template || !context_template->is_object())
        throw Error("formula context-import template must be an inline object");
    if (!capture || !capture->is_object())
        throw Error("formula context-import capture must be an inline object");
    bool allow_partial = false;
    if (const auto* value = json_member(body, "allow_partial")) {
        if (!value->is_bool())
            throw Error("formula context-import allow_partial must be boolean");
        allow_partial = value->as_bool();
    }
    FormulaExplicitContextCaptureImportRequest request;
    request.context_template = *context_template;
    request.capture = *capture;
    request.allow_partial = allow_partial;
    auto result = import_formula_explicit_context_capture(request);
    auto& metadata = result["_capture_import"];
    metadata["request_body_retained"] = false;
    metadata["file_accessed"] = false;
    metadata["path_accepted"] = false;
    return result;
}

}  // namespace tdx::server_detail
