#include "tdx/level2.hpp"

#include "tdx/common.hpp"
#include "level2_sdk_snapshot_digest.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace tdx {
namespace {

constexpr std::size_t header_size = 96;
constexpr std::size_t maximum_raw_size = level2_offline_payload_limit;

int signed_i8(std::uint8_t raw) {
    return raw <= 0x7fU ? static_cast<int>(raw)
                        : static_cast<int>(raw) - 0x100;
}

std::int16_t signed_i16_le(const std::uint8_t* data) {
    const auto raw = static_cast<std::uint16_t>(data[0]) |
                     (static_cast<std::uint16_t>(data[1]) << 8U);
    if (raw <= static_cast<std::uint16_t>(
                   std::numeric_limits<std::int16_t>::max()))
        return static_cast<std::int16_t>(raw);
    return static_cast<std::int16_t>(
        static_cast<std::int32_t>(raw) - 0x10000);
}

Json shape_terms_json(const std::array<int, 6>& terms) {
    Json result = Json::array();
    for (std::size_t index = 0; index < terms.size(); ++index) {
        Json item = Json::object();
        item["offset"] = static_cast<std::uint64_t>(index);
        item["raw_signed_i8"] = terms[index];
        result.push_back(std::move(item));
    }
    return result;
}

Json bounded_code_identity(const Bytes& raw) {
    const auto begin = raw.begin() + 10;
    const auto header_end = raw.begin() +
        static_cast<std::ptrdiff_t>(header_size);
    const auto nul = std::find(begin, header_end, std::uint8_t{0});
    const bool terminated = nul != header_end;
    bool printable_ascii = terminated;
    if (terminated) {
        printable_ascii = std::all_of(begin, nul, [](std::uint8_t value) {
            return value >= 0x20U && value <= 0x7eU;
        });
    }

    Json result = Json::object();
    result["offset"] = 10;
    result["bounded_to_header"] = true;
    result["nul_terminated_within_header"] = terminated;
    result["printable_ascii"] = printable_ascii;
    result["value"] = printable_ascii
        ? Json(std::string(begin, nul))
        : Json(nullptr);
    result["strcmp_executed"] = false;
    return result;
}

Json data_region_candidate(const Bytes& raw, bool selected_by_raw_gate) {
    Json result = Json::object();
    result["selection_condition"] = "raw_u8_at_0 == 1";
    result["selected_by_raw_gate"] = selected_by_raw_gate;
    result["offset"] = selected_by_raw_gate
        ? Json(static_cast<std::uint64_t>(header_size))
        : Json(nullptr);
    if (!selected_by_raw_gate) {
        result["byte_size"] = 0;
        result["sha256"] = Json(nullptr);
        result["body_emitted"] = false;
        return result;
    }

    Bytes bytes(raw.begin() + static_cast<std::ptrdiff_t>(header_size),
                raw.end());
    result["byte_size"] = static_cast<std::uint64_t>(bytes.size());
    result["sha256"] = level2_detail::level2_snapshot_sha256(bytes);
    result["body_emitted"] = false;
    return result;
}

void add_offline_boundary(Json& result) {
    result["input_body_retained"] = false;
    result["raw_body_emitted"] = false;
    result["host_identity_accessed"] = false;
    result["host_identity_comparison_executed"] = false;
    result["previous_host_state_accessed"] = false;
    result["host_clock_accessed"] = false;
    result["handler_executed"] = false;
    result["host_state_write_performed"] = false;
    result["sdk_called"] = false;
    result["callback_executed"] = false;
    result["host_message_dispatch_attempted"] = false;
    result["host_messages_sent"] = 0;
    result["wire_bytes_built"] = false;
    result["network_requests"] = 0;
    result["request_sent"] = false;
    result["subscription_sent"] = false;
    result["authorization_attempted"] = false;
    result["credentials_accessed"] = false;
    result["entitlement_bypass"] = false;
    result["offline"] = true;
}

}  // namespace

Json level2_tpbus_4650_dispatch_preflight_document(
    const Level2Tpbus4650DispatchPreflightRequest& request) {
    if (request.raw.size() < header_size)
        throw Error(
            "tpbus 4650 dispatch preflight raw input must contain at least "
            "96 bytes");
    if (request.raw.size() > maximum_raw_size)
        throw Error(
            "tpbus 4650 dispatch preflight raw input exceeds the 384 KiB "
            "offline safety limit");

    std::array<int, 6> terms{};
    for (std::size_t index = 0; index < terms.size(); ++index)
        terms[index] = signed_i8(request.raw[index]);
    const auto variable_units =
        static_cast<std::int64_t>(terms[5]) +
        6LL * (static_cast<std::int64_t>(terms[0]) +
               2LL * (static_cast<std::int64_t>(terms[2]) +
                      static_cast<std::int64_t>(terms[3]) +
                      static_cast<std::int64_t>(terms[4]))) +
        4LL * static_cast<std::int64_t>(terms[1]);
    const auto expected_size =
        static_cast<std::int64_t>(header_size) + 20LL * variable_units;
    if (expected_size != static_cast<std::int64_t>(request.raw.size()))
        throw Error(
            "tpbus 4650 raw input size must equal 96 + 20*(term5 + "
            "6*(term0 + 2*(term2 + term3 + term4)) + 4*term1), with "
            "all terms read as signed i8");

    const bool trigger_gate = request.raw[5] != 0 || request.raw[86] != 0;
    const bool data_branch = request.raw[0] == 1;

    Json validator = Json::object();
    validator["qualified"] = true;
    validator["minimum_byte_size"] =
        static_cast<std::uint64_t>(header_size);
    validator["maximum_byte_size"] =
        static_cast<std::uint64_t>(maximum_raw_size);
    validator["actual_byte_size"] =
        static_cast<std::uint64_t>(request.raw.size());
    validator["expected_byte_size_signed"] = expected_size;
    validator["variable_units_signed"] = variable_units;
    validator["shape_expression"] =
        "96 + 20*(term5 + 6*(term0 + 2*(term2 + term3 + term4)) + 4*term1)";
    validator["terms"] = shape_terms_json(terms);

    Json identity = Json::object();
    identity["market_raw_signed_i16"] =
        signed_i16_le(request.raw.data() + 8);
    identity["code_raw"] = bounded_code_identity(request.raw);
    identity["expected_host_code_available"] = false;
    identity["expected_host_market_available"] = false;
    identity["host_code_match"] = Json(nullptr);
    identity["host_market_match"] = Json(nullptr);
    identity["fully_evaluated"] = false;

    Json gates = Json::object();
    gates["body_pointer_present"] = true;
    gates["body_size_nonzero"] = true;
    gates["raw_u8_at_5"] = static_cast<std::uint64_t>(request.raw[5]);
    gates["raw_u8_at_86"] = static_cast<std::uint64_t>(request.raw[86]);
    gates["trigger_expression"] = "raw_u8_at_5 != 0 || raw_u8_at_86 != 0";
    gates["trigger_passed"] = trigger_gate;
    gates["raw_u8_at_0"] = static_cast<std::uint64_t>(request.raw[0]);
    gates["raw_plus_96_branch_selected"] = data_branch;
    gates["host_identity_gates_resolved"] = false;
    gates["target_object_lookup_resolved"] = false;
    gates["previous_vector_nonempty_resolved"] = false;
    gates["handler_call_possible"] = Json(nullptr);

    Json result = Json::object();
    result["schema"] = "tdx-level2-tpbus-4650-dispatch-preflight-v1";
    result["format"] = "tpbus-4650-dispatch-preflight";
    result["function_id"] = 4650;
    result["normalization_kind"] =
        "offline-raw-shape-and-caller-gate-preflight";
    result["raw_validator"] = std::move(validator);
    result["raw_identity"] = std::move(identity);
    result["caller_gates"] = std::move(gates);
    result["raw_plus_96_candidate"] =
        data_region_candidate(request.raw, data_branch);
    result["dispatcher_fully_qualified"] = false;
    result["state_projection_closed"] = false;
    result["state_projection_performed"] = false;
    result["unresolved_dependencies"] = Json::array();
    for (const auto* dependency : {
             "host-code-and-market-identity", "target-object-lookup",
             "prior-target-vector", "host-clock-and-server-time-policy",
             "mutable-host-quote-and-subscription-state"})
        result["unresolved_dependencies"].push_back(dependency);
    result["evidence"] =
        "tpbus sub_1007CB4E and sub_10068065; "
        "output/ida-tpbus-sdk-4650-targeted-20260813.json; "
        "output/ida-tpbus-sdk-4650-previous-state-targeted-20260813.json";
    add_offline_boundary(result);
    return result;
}

}  // namespace tdx
