#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace tdx {
namespace {

bool six_digit_code(const std::string& code) {
    return code.size() == 6 &&
           std::all_of(code.begin(), code.end(), [](unsigned char ch) {
               return ch >= '0' && ch <= '9';
           });
}

const char* operation_name(Level2FastHqOperation operation) {
    switch (operation) {
    case Level2FastHqOperation::subscribe: return "subscribe";
    case Level2FastHqOperation::unsubscribe: return "unsubscribe";
    }
    throw Error("FastHQ logical job operation is invalid");
}

int operation_raw(Level2FastHqOperation operation) {
    switch (operation) {
    case Level2FastHqOperation::subscribe: return 1;
    case Level2FastHqOperation::unsubscribe: return 0;
    }
    throw Error("FastHQ logical job operation is invalid");
}

}  // namespace

Json build_level2_fasthq_subscribe_job_plan(
    const Level2FastHqSubscribeJobPlanRequest& request) {
    if (!six_digit_code(request.code))
        throw Error("FastHQ CODE must contain exactly six ASCII digits");
    const auto* operation = operation_name(request.operation);
    const int oper_type = operation_raw(request.operation);

    Json fields = Json::object();
    fields["Name"] = "FastHQ.Subscribe";
    fields["CODE"] = request.code;
    fields["SC"] = static_cast<std::uint64_t>(request.market_id);
    fields["LX"] = static_cast<std::int64_t>(request.lx_raw);
    fields["PkgType"] = 0;
    fields["OperType"] = oper_type;
    fields["PushType"] = 3;
    fields["BatchPush"] = 1;

    Json field_origins = Json::object();
    field_origins["Name"] =
        "IXReq request name and CTAJob_InetTQL envelope Name";
    for (const auto* name : {"CODE", "SC", "LX", "PkgType", "OperType",
                             "PushType", "BatchPush"})
        field_origins[name] = "IXReq body item";

    Json job_envelope = Json::object();
    job_envelope["class_observed"] = "CTAJob_InetTQL";
    job_envelope["Name"] = "FastHQ.Subscribe";
    job_envelope["Body"] = Json(nullptr);
    job_envelope["route_key"] = Json(nullptr);
    job_envelope["materialized"] = false;

    Json result = Json::object();
    result["schema"] = "tdx-level2-fasthq-subscribe-job-plan-v1";
    result["format"] = "fasthq-subscribe-plan";
    result["plan_kind"] = "logical-job-fields";
    result["operation"] = operation;
    result["lx_raw"] = static_cast<std::int64_t>(request.lx_raw);
    result["lx_mapping"] = Json(nullptr);
    result["lx_mapping_status"] = "unclassified-raw";
    result["logical_job"] = std::move(fields);
    result["field_origins"] = std::move(field_origins);
    result["request_field_count"] = 7;
    result["projected_field_count"] = 8;
    result["logical_field_count"] = 8;
    result["job_class_observed"] = "CTAJob_InetTQL";
    result["job_envelope"] = std::move(job_envelope);
    result["body_serialized"] = false;
    result["wire_bytes_built"] = false;
    result["network_request_bytes_built"] = false;
    result["network_requests"] = 0;
    result["job_enqueued"] = false;
    result["operation_executed"] = false;
    result["subscription_sent"] = false;
    result["unsubscribe_sent"] = false;
    result["offline"] = true;
    result["entitlement_bypass"] = false;
    result["push_type_scope"] =
        "FastHQ.Subscribe request field; not incoming push type 111/112";
    result["evidence"] =
        "tpbus sub_10076A7A (HQDataMaintain.cpp:1837-1844)";
    return result;
}

}  // namespace tdx
