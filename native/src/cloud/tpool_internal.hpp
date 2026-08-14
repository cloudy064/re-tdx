#pragma once

#include "tdx/tpool.hpp"

#include <filesystem>
#include <cstdint>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::tpool_detail {

using Attributes = std::map<std::string, std::string, std::less<>>;
using TpoolSecurityNames =
    std::map<std::pair<int, std::string>, std::string>;

struct TpoolNativeTextExport {
    std::string kind;
    std::string text_utf8;
    std::size_t row_count{};
    std::size_t resolved_name_count{};
};

struct FlowScheduleDecision {
    enum class Kind { waiting, ready, expired, invalid } kind{Kind::waiting};
    std::string reason;
};

struct FlowRuntimeState {
    std::uint64_t run_count{};
    std::int64_t first_run_tick{-1};
    std::int64_t last_run_tick{-1};
    bool completed{};
};

inline constexpr int kMaximumTpoolRuleHistoryBars = 16000;

struct RuleHistoryPlan {
    int begin_offset{};
    int end_offset{};
    int calculation_bars{};
    int minimum_bars{1};
    int fetch_bars{1};
    bool supported{};
    std::string blocking_reason;
};

struct RuleRankingObservation {
    int offset_from_latest{};
    double value{};
};

struct RuleResult {
    bool evaluated{};
    bool matched{};
    bool ranking_pending{};
    double left{};
    double right{};
    std::string left_name;
    std::string right_name;
    int window_begin_offset{};
    int window_end_offset{};
    int selected_offset{-1};
    int matched_offset{-1};
    std::size_t requested_anchor_count{};
    std::size_t evaluated_anchor_count{};
    std::vector<RuleRankingObservation> ranking_observations;
    std::string error;
};

enum class RuleSourceKind {
    formula,
    latest_finance,
    realtime_quote,
    unknown,
};

struct BuiltinRulePlan {
    RuleSourceKind kind{RuleSourceKind::unknown};
    bool applicable{};
    bool field_recognized{};
    bool operation_supported{};
    bool execution_ready{};
    bool needs_quote{};
    bool needs_finance{};
    bool needs_directory{};
    bool needs_trade_unit{};
    std::string blocking_reason;
};

std::filesystem::path from_utf8(const std::string& value);
std::vector<Attributes> scan_elements(
    const std::string& xml, std::string_view element);
Json attributes_json(const Attributes& attributes);
std::string attribute(const Attributes& attributes, std::string_view key);
std::string market_from_setcode(const std::string& value);
std::vector<std::filesystem::path> pool_directories(
    const std::filesystem::path& root);
std::string decode_xml_file(const std::filesystem::path& path);
std::string decode_tpool_history_text_file(
    const std::filesystem::path& path);
TpoolNativeTextExport render_tpool_history_native_text(
    const Json& history, const TpoolSecurityNames& names);
std::string upper_ascii(std::string value);
bool supported_operation(int operation);
const Json* library_formula(const Json* library, const std::string& code);
Json formula_analysis(const Json& formula);
void upgrade_tpool_formula_compatibility(
    Json& inspection, const Json* library);
std::string operator_name(int operation);
bool ranking_operation(int operation);
RuleSourceKind tpool_rule_source_kind(int set);
std::string tpool_rule_source_name(RuleSourceKind kind);
std::string tpool_operator_name(int set, int operation);
bool tpool_supported_operation(int set, int operation);
bool tpool_ranking_operation(int set, int operation);
int tpool_canonical_ranking_operation(int set, int operation);
BuiltinRulePlan annotate_tpool_builtin_rule(Json& rule);
RuleResult evaluate_tpool_builtin_rule(
    const Json& rule,
    const Json* quote,
    const Json* finance,
    double volume_ratio_base,
    double trade_unit,
    int elapsed_trading_minutes);
int current_a_share_elapsed_trading_minutes();
RuleHistoryPlan tpool_rule_history_plan(int operation,
                                        int begin_offset,
                                        int end_offset,
                                        int calculation_bars);
RuleHistoryPlan annotate_tpool_rule_history(Json& rule);
RuleResult evaluate_tpool_rule(const Json& rule, const Json& calculation);
Json rank_tpool_observation_groups_document(
    const Json& observations,
    int operation,
    double threshold,
    int window_begin_offset,
    int window_end_offset);
void resolve_cross_security_rankings(
    Json& security_rows,
    const Json::Array& functions,
    std::size_t selected_security_count,
    std::size_t available_security_count);
int integer_text(const std::string& value, int fallback);
double float_text(const std::string& value, double fallback);

std::string period_name(int period);
const Json* optional(const Json& object, std::string_view key);
bool json_bool(
    const Json& object, std::string_view key, bool fallback = false);
std::string local_time_text();
TpoolFlowClock local_flow_clock();
int hhmmss_seconds(int value);
std::int64_t json_integer_number(
    const Json& object, std::string_view key, std::int64_t fallback);
double json_number(
    const Json& object, std::string_view key, double fallback);
Json string_set_json(const std::set<std::string, std::less<>>& values);
std::string json_text(const Json& object, std::string_view key);
int json_integer_text(
    const Json& object, std::string_view key, int fallback);
double json_float_text(
    const Json& object, std::string_view key, double fallback);
Json tpool_action_policy_document(const Json& raw,
                                  const std::string& cell_id);
Json tpool_host_callback_contracts_document();
Json tpool_action_plans_document(
    const Json& inspection,
    std::string_view cell_id,
    const std::set<std::string, std::less<>>& securities,
    std::string_view trigger);
std::string flow_source_signature(const Json& inspection);
FlowScheduleDecision first_flow_schedule(
    const Json& flow, const TpoolFlowClock& clock, std::int64_t pool_tick);
FlowRuntimeState read_flow_runtime(const Json& row);

}  // namespace tdx::tpool_detail
