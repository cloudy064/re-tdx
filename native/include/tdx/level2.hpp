#pragma once

#include "tdx/common.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace tdx {

inline constexpr std::size_t level2_offline_payload_limit = 384U * 1024U;

struct Level2Tdxw1369Request {
    std::uint16_t market_id{};
    std::string code;
    std::uint16_t request_count{11};
};

struct Level2Tdxw1371Request {
    std::uint16_t market_id{};
    std::string code;
    // Final raw byte written at +37. No buy/sell meaning is inferred.
    std::uint8_t side_mode_raw{};
    float selected_price{};
    std::int32_t cursor{-1};
    std::uint16_t request_count{5000};
};

enum class Level2DirectRequestVariant {
    standard,
    initial,
};

enum class Level2FastHqOperation {
    subscribe,
    unsubscribe,
};

struct Level2FastHqSubscribeJobPlanRequest {
    std::uint16_t market_id{};
    std::string code;
    // LX is intentionally kept as the signed raw value supplied by the host.
    std::int32_t lx_raw{};
    Level2FastHqOperation operation{Level2FastHqOperation::subscribe};
};

enum class Level2SdkFnReqDataType : int {
    transaction = 1801,
    order = 1802,
    multi_level_quote = 1803,
    price_queue = 1804,
    extended_quote = 18071,
};

struct Level2SdkFnReqDataCallPlanRequest {
    std::uint16_t market_id{};
    std::string code;
    Level2SdkFnReqDataType data_type{Level2SdkFnReqDataType::transaction};
    // Only transaction/order callers expose these fields. Keeping them
    // optional distinguishes an explicit zero cursor from an omitted field.
    std::optional<std::uint32_t> cursor_raw;
    std::optional<std::uint16_t> request_count;
};

struct Level2SdkFnReqData18031CallPlanRequest {
    std::uint16_t market_id{};
    std::string code;
    // Raw byte supplied by the host. No buy/sell meaning is inferred.
    std::uint8_t side_mode_raw{};
    // Exact float input accepted by the recovered TdxW wrapper.
    float selected_price{};
};

enum class Level2SdkFnSubscribeDataType : int {
    transaction = 1801,
    order = 1802,
    multi_level_quote = 1803,
};

struct Level2SdkFnSubscribeBatchCallPlanRequest {
    Level2SdkFnSubscribeDataType data_type{
        Level2SdkFnSubscribeDataType::transaction};
    // Already-resolved SDK symbols such as SZ000001. The builder never
    // consults or simulates TdxW's host view/security resolver.
    std::vector<std::string> symbols;
};

enum class Level2SdkCallbackDataType : int {
    transaction = 1801,
    order = 1802,
    multi_level_quote = 1803,
    price_queue = 1804,
    quote_update = 1807,
    order_queue_at_price = 18031,
    extended_quote = 18071,
};

struct Level2SdkCorrelationRecord {
    // These two values are opaque SDK callback-correlation keys. They are not
    // session identifiers, authentication tokens, or network request IDs.
    std::uint32_t callback_key_1_raw{};
    std::uint32_t callback_key_2_raw{};
    std::uint16_t market_raw{};
    std::string code_ascii;
    Level2SdkCallbackDataType data_type{
        Level2SdkCallbackDataType::transaction};
    std::uint32_t registry_mode_raw{};
};

enum class Level2SdkCorrelationTransitionKind {
    callback_match,
    sdk_key_upsert,
};

struct Level2SdkCorrelationCallbackMatch {
    Level2SdkCallbackDataType data_type{
        Level2SdkCallbackDataType::transaction};
    std::uint32_t callback_key_1_raw{};
    std::uint32_t callback_key_2_raw{};
};

struct Level2SdkCorrelationTransitionRequest {
    Level2SdkCorrelationTransitionKind kind{
        Level2SdkCorrelationTransitionKind::callback_match};
    std::vector<Level2SdkCorrelationRecord> previous_registry;
    // Exactly one action must be present and agree with `kind`. callback_match
    // replays sub_68C750's lookup/consume step; sdk_key_upsert replays the
    // proven portion of sub_68C510 after the SDK has supplied both opaque keys.
    std::optional<Level2SdkCorrelationCallbackMatch> callback_match;
    std::optional<Level2SdkCorrelationRecord> sdk_key_upsert;
};

struct Level2SdkCallbackRoutePlanRequest {
    Level2SdkCallbackDataType data_type{
        Level2SdkCallbackDataType::transaction};
    // All callback types except 18031 resolve through the local 25-byte
    // registry. Mode 9 retains a matching record; other modes consume it.
    std::optional<std::uint32_t> registry_mode_raw;
    // Only 1807 has two host routes. An absent value preserves both
    // conditional alternatives instead of guessing live host state.
    std::optional<bool> host_time_advanced;
};

struct Level2SdkCallbackInvocationRequest {
    Level2SdkCallbackDataType data_type{
        Level2SdkCallbackDataType::transaction};
    // Fifth callback ABI argument. It is a record count only for 1801/1802;
    // fixed-body callback types preserve it without assigning semantics.
    std::int32_t callback_arg5_raw{};
    // Sixth callback ABI argument. sub_68C750 does not consume this value, so
    // it is retained solely as an uninterpreted unsigned raw value.
    std::uint32_t callback_arg6_raw{};
    // All callback types except 18031 require a matching 25-byte registry
    // record. 18031 uses host-global correlation and must leave this absent.
    std::optional<std::uint32_t> registry_mode_raw;
    Bytes body;
};

// Inputs for the recovered 1807/18071 snapshot-to-host-state transition.
// The host record types were not named by the available symbols, so the
// previous state and unresolved host context remain explicit JSON contracts
// rather than speculative C++ structs.  The returned projected_state can be
// supplied unchanged as the next request's previous value. `previous` must
// contain base_quote, ten ask_levels, ten bid_levels, and aggregate as emitted
// by schema tdx-level2-sdk-host-quote-state-v1. `context` requires these raw
// evidence-bound fields: security_class_raw, price_transition_guard_raw,
// small_last_price_fallback_predicate_raw, special_volume_multiplier_raw,
// security_auxiliary_dword_73_raw, and host_word_280_raw.
struct Level2SdkQuoteTransitionRequest {
    Level2SdkCallbackDataType data_type{
        Level2SdkCallbackDataType::quote_update};
    Bytes body;
    Json previous;
    Json context;
};

struct Level2Sdk1804HostProjectionRequest {
    // One exact authorized SDK 1804 callback body. The projection is local and
    // never invokes the callback, SDK, host storage, or a network operation.
    Bytes body;
};

struct Level2Sdk1803DepthRecordProjectionRequest {
    // One exact authorized SDK 1803 callback body. The projection reproduces
    // only the recovered local 13-byte first/second record arrays and never
    // invokes the callback, SDK, host storage, or a network operation.
    Bytes body;
};

struct Level2Sdk18031QueueRecordProjectionRequest {
    // TdxW's already-closed security classifier consumes the raw market word
    // and first six ASCII code bytes before projecting the fixed callback
    // snapshot into local 6-byte records.
    std::uint16_t market_id{};
    std::string code;
    Bytes body;
};

struct Level2Sdk4654DualSnapshotTransitionRequest {
    // Explicit output of tpbus's upstream decoder. No raw-to-decoded
    // conversion is attempted; the qualified sub_1006743C call copies
    // exactly 48 bytes.
    Bytes decoded;
    // Explicit raw response bytes copied into the second local snapshot.
    Bytes raw;
};

struct Level2Sdk4651DualSnapshotPreviousState {
    // sub_100675A0 compares only the current raw snapshot byte size. The
    // remaining fields are complete opaque metadata carried through when the
    // state is retained. They are not used by the replacement decision.
    // This is the raw vector's logical size, never its capacity. Values are
    // restricted to the recovered non-negative signed 32-bit size domain.
    std::uint64_t raw_byte_size{};
    std::uint64_t decoded_byte_size{};
    std::string raw_sha256;
    std::string decoded_sha256;
    bool ready{};
};

struct Level2Sdk4651DualSnapshotTransitionRequest {
    // Both snapshots are explicit non-empty opaque byte strings from the
    // dispatcher-qualified call. The recovered code does not prove a
    // raw-to-decoded transform in this handler.
    Bytes decoded;
    Bytes raw;
    std::optional<Level2Sdk4651DualSnapshotPreviousState> previous;
};

struct Level2Sdk4655CompanionRawTransitionRequest {
    // Explicit 46-byte companion snapshot. The recovered caller clones it
    // from the target object's existing this+456 vector before invoking
    // sub_10079A02; it is not derived from the new raw response.
    Bytes companion_snapshot;
    // Explicit dispatcher-qualified raw body. Its shape is validated using
    // the recovered signed i16 count and signed i8 attach flag expression.
    Bytes raw;
};

struct Level2Tpbus4650DispatchPreflightRequest {
    // Explicit raw Body bytes from a legally obtained tpbus 4650 response.
    // Only the recovered sub_10068065 shape and sub_1007CB4E caller gates are
    // inspected. The broad mutable host state needed by sub_1007A75E is not
    // accepted or guessed by this preflight.
    Bytes raw;
};

struct Level2Tpbus4650PricePrimitivesRequest {
    // Explicit dispatcher-shaped raw Body bytes from a legally obtained
    // tpbus 4650 response. Only the proven raw+96 120-byte quote snapshot is
    // read; the broad mutable host object is not accepted or projected.
    Bytes raw;
    // Explicit raw value corresponding to the recovered target object +72
    // scalar consumed by sub_10066A34. Its business enum remains unresolved.
    std::uint32_t target_market_or_mode_raw{0};
};

struct Level2Tpbus115BatchDecodeRequest {
    // Explicit PushBody bytes already obtained by the caller. sub_1007BFBE
    // consumes only the first outer u16-length-prefixed segment; no EventBus,
    // host-state, SDK, or network path is part of this offline decoder.
    Bytes payload;
    int summary_limit{20};
};

enum class Level2SdkHostSnapshotDataType : int {
    multi_level_quote = 1803,
    order_queue_at_price = 18031,
};

struct Level2SdkHostSnapshotReplacementRequest {
    Level2SdkHostSnapshotDataType data_type{
        Level2SdkHostSnapshotDataType::multi_level_quote};
    // One exact authorized fixed-body callback snapshot. No previous-state
    // input is needed because the recovered branches overwrite the complete
    // logical state byte-for-byte without inspecting its prior contents.
    Bytes body;
};

struct Level2Sdk1801HostProjectionRequest {
    // TdxW's security classifier consumes the raw market word and the first
    // six ASCII code bytes before projecting the counted callback records.
    std::uint16_t market_id{};
    std::string code;
    // A non-empty exact array of 52-byte SDK 1801 callback records. The
    // offline projection is capped at 384 KiB and replaces one logical host
    // callback state; it never invokes the SDK or host storage.
    Bytes body;
};

struct Level2Sdk1802HostProjectionRequest {
    std::uint16_t market_id{};
    std::string code;
    // A non-empty exact array of 40-byte SDK 1802 callback records, subject to
    // the same 384 KiB pure-offline boundary as the 1801 projection.
    Bytes body;
};

struct Level2SdkCompatibilityPreflightRequest {
    // Explicit TongDaXin installation root. The preflight never discovers a
    // root implicitly and never reads usercomm.ini or any credential source.
    std::filesystem::path root;
    bool auto_use_no_sdk_l2_agent_raw{};
    bool sdk_l2_agent_raw{true};
    int host_edition_raw{};
};

Bytes build_level2_direct_request(const std::string& kind, int market_id,
                                  const std::string& code,
                                  std::uint32_t cursor = 0,
                                  std::uint16_t count = 1500,
                                  Level2DirectRequestVariant variant =
                                      Level2DirectRequestVariant::standard);
Bytes build_level2_sdk_redirect_request(
    int function_id, int market_id, const std::string& code,
    std::uint16_t want_number = 80, int depth = 10,
    bool attach_info = false, bool repurchase_time = false);
// Build an offline, typed SDK 1807 subscription plan from packed 7-byte
// security identities.  The result never contains or sends network bytes.
Json build_level2_sdk_1807_request_plan(const Bytes& packed_securities);
// Reproduce only the recovered FastHQ.Subscribe logical job fields. No TQL
// body, transport bytes, job queue operation, or subscription is produced.
Json build_level2_fasthq_subscribe_job_plan(
    const Level2FastHqSubscribeJobPlanRequest& request);
// Describe the recovered fnReqData call slots and callback registry template.
// Host pointers and callback keys remain unresolved; no SDK call or wire bytes
// are produced.
Json build_level2_sdk_fnreqdata_call_plan(
    const Level2SdkFnReqDataCallPlanRequest& request);
// Describe the separate 18031 fnReqData call and its host-global callback
// correlation. No 25-byte registry record, SDK call, or wire bytes are built.
Json build_level2_sdk_fnreqdata_18031_call_plan(
    const Level2SdkFnReqData18031CallPlanRequest& request);
// Describe one recovered fnSubscribeData batch call from pre-resolved
// symbols. No SDK invocation, callback record, or subscription is produced.
Json build_level2_sdk_fnsubscribe_batch_call_plan(
    const Level2SdkFnSubscribeBatchCallPlanRequest& request);
// Describe TdxW's post-callback host-window routing. This is offline
// metadata: no SDK callback, SendMessage/PostMessage call, or network action
// is performed.
Json build_level2_sdk_callback_route_plan(
    const Level2SdkCallbackRoutePlanRequest& request);
// Validate and normalize the recovered sub_68C750 callback ABI envelope while
// reusing the existing SDK body decoders and callback route plan. No callback,
// host message, SDK call, or network operation is executed.
Json decode_level2_sdk_callback_invocation(
    const Level2SdkCallbackInvocationRequest& request, int limit = 20);
// Project one exact 380-byte 1807/18071 callback body onto a previous host
// quote state. This is a pure offline transition: no SDK, callback, host
// message, request construction, or network operation is performed.
Json project_level2_sdk_quote_transition(
    const Level2SdkQuoteTransitionRequest& request);
// Project one exact 432-byte SDK 1804 body into the 105 raw f32 slots prepared
// by TdxW before sub_525600. Counts remain bit-exact in slots 3/4 while only
// the number of copied quantity slots is clamped to 50.
Json project_level2_sdk_1804_host_projection(
    const Level2Sdk1804HostProjectionRequest& request);
// Project one exact SDK 1803 callback body into the two recovered arrays of
// packed 13-byte local depth records. First/second naming is retained raw;
// no buy/sell direction is inferred.
Json project_level2_sdk_1803_depth_record_projection(
    const Level2Sdk1803DepthRecordProjectionRequest& request);
// Project one exact SDK 18031 callback body into the recovered packed 6-byte
// local queue records. This is a pure offline domain transform and neither
// reads previous host state nor invokes any SDK, message, or network path.
Json project_level2_sdk_18031_queue_record_projection(
    const Level2Sdk18031QueueRecordProjectionRequest& request);
// Reproduce tpbus sub_1006743C's unconditional replacement of an exact
// 48-byte decoded snapshot and a bounded raw snapshot. Bodies are not emitted.
Json project_level2_sdk_4654_dual_snapshot_transition(
    const Level2Sdk4654DualSnapshotTransitionRequest& request);
// Reproduce tpbus sub_100675A0's conditional dual-snapshot replacement. The
// previous raw byte size is the only field used by the transition decision;
// optional prior hashes/readiness are carried through on retention only.
Json project_level2_sdk_4651_dual_snapshot_transition(
    const Level2Sdk4651DualSnapshotTransitionRequest& request);
// Project only sub_10079A02's conditional replacement branch from an explicit
// caller-cloned companion snapshot and raw response. The unavailable host-state
// gate, date formatting,
// notification, SDK, callback, message, and network paths are not executed.
Json project_level2_sdk_4655_companion_raw_transition(
    const Level2Sdk4655CompanionRawTransitionRequest& request);
// Validate the recovered tpbus 4650 raw-size expression and expose only the
// caller's raw identity/branch gates. This is not a host-state projection and
// never runs the merge handler, host clock, SDK, messages, or network paths.
Json level2_tpbus_4650_dispatch_preflight_document(
    const Level2Tpbus4650DispatchPreflightRequest& request);
Json project_level2_tpbus_4650_price_primitives(
    const Level2Tpbus4650PricePrimitivesRequest& request);
// Decode the PushType 115 batch envelope recovered at tpbus sub_1007BFBE.
// Each non-empty inner body is fingerprinted and may reuse the existing
// offline 111/112 summary; bodies are never emitted or retained.
Json decode_level2_tpbus_115_batch(
    const Level2Tpbus115BatchDecodeRequest& request);
// Model the complete fixed-size state replacement performed by the SDK 1803
// and 18031 callback branches. The body is neither transformed nor emitted;
// only its exact size, digest, and replacement metadata are returned.
Json project_level2_sdk_host_snapshot_replacement(
    const Level2SdkHostSnapshotReplacementRequest& request);
// Reproduce the SDK 1801/1802 branches of TdxW sub_68C750 through the exact
// 20-byte records prepared for the host vectors. Epochs and volumes are read
// as signed i64 values. The 1801 first/second/qualifier fields remain raw;
// 1802 uses the recovered order_id/side-or-cancel-qualifier/action host roles.
Json project_level2_sdk_1801_host_projection(
    const Level2Sdk1801HostProjectionRequest& request);
Json project_level2_sdk_1802_host_projection(
    const Level2Sdk1802HostProjectionRequest& request);
// Replay the proven local 25-byte callback-correlation registry transition.
// The registry is capped at 10,000 records so the unresolved native cleanup
// path above that boundary is never guessed. No SDK, callback, host lookup,
// message dispatch, credential access, or network operation is performed.
Json project_level2_sdk_correlation_transition(
    const Level2SdkCorrelationTransitionRequest& request);
// Decode a snapshot of TdxW's local 25-byte SDK callback-correlation
// registry. The records are in-process bookkeeping, not wire/session data.
Json decode_level2_sdk_correlation_registry(const Bytes& payload,
                                            int limit = 20);
// Normalize the JSON document returned by tpbus's recovered SDK adapters.
// This consumes an already obtained, authorized SDK response and never sends
// a request or constructs wire bytes. requested_depth is used only by 4680.
// The dedicated 4653 entry point exposes the strict Data-array contract and
// response-derived header/record semantics without constructing its native
// body or filling request/cache-dependent fields.
Json normalize_level2_sdk_json_4653(const Json& document, int limit = 20);
Json normalize_level2_sdk_json(int function_id, const Json& document,
                               int limit = 20, int requested_depth = 10);
// Reproduce the fixed TdxW internal IPC bodies offline. These functions do
// not frame, send, or describe SDK/network requests.
Bytes build_level2_tdxw_1369_request(const Level2Tdxw1369Request& request);
Bytes build_level2_tdxw_1371_request(const Level2Tdxw1371Request& request);
Json decode_level2_document(const std::string& format, Bytes payload,
                            int limit = 20, int xor_key = -1,
                            int max_fields = 100, int sample_bytes = 64);
Json materialize_tcalc_level2_formula_context(Json context,
                                              const Json& kline_document);
Json level2_session_preflight_document(const std::string& process_name = "TdxW.exe",
                                       std::uint32_t process_id = 0);
// Inspect the selected SDK DLL's on-disk PE export table without loading it.
// No configuration, token, proxy setting, SDK entry point, or network resource
// is accessed.
Json level2_sdk_compatibility_preflight_document(
    const Level2SdkCompatibilityPreflightRequest& request);
int command_level2_build(const std::vector<std::string>& args);
int command_level2_decode(const std::vector<std::string>& args);
int command_level2_project(const std::vector<std::string>& args);
int command_level2_session(const std::vector<std::string>& args);
int command_level2_preflight(const std::vector<std::string>& args);

}  // namespace tdx
