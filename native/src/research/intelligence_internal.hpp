#pragma once

#include "tdx/intelligence.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string_view>

namespace tdx::intelligence_detail {

enum class IntelligenceView {
    attention,
    value_attention,
    risks,
    highlights,
    events,
    event,
    graph,
    security,
    topics,
    topic,
    news,
    market_anomalies,
};

struct ViewDefinition {
    std::string_view name;
    IntelligenceView view;
};

inline constexpr std::array<ViewDefinition, 12> view_definitions{{
    {"attention", IntelligenceView::attention},
    {"value-attention", IntelligenceView::value_attention},
    {"risks", IntelligenceView::risks},
    {"highlights", IntelligenceView::highlights},
    {"events", IntelligenceView::events},
    {"event", IntelligenceView::event},
    {"graph", IntelligenceView::graph},
    {"security", IntelligenceView::security},
    {"topics", IntelligenceView::topics},
    {"topic", IntelligenceView::topic},
    {"news", IntelligenceView::news},
    {"market-anomalies", IntelligenceView::market_anomalies},
}};

inline constexpr char attention_resource[] = "list/func_scrd101_1.jsn";
inline constexpr char observation_resource[] = "list/func_bxgc101_1.jsn";
inline constexpr char potential_resource[] = "list/func_qzbl101_1.jsn";
inline constexpr char discredited_resource[] = "list/func_sxbzx101_1.jsn";
inline constexpr char highlights_resource[] = "list/func_ldph101_1.jsn";
inline constexpr char value_attention_resource[] = "list/func_jzgz101_1.jsn";
inline constexpr char events_resource[] = "list/func_sjqd101_1.jsn";
inline constexpr char ministries_resource[] = "list/func_bwyq101_1.jsn";
inline constexpr char hotspots_resource[] = "list/func_rdyc101_1.jsn";
inline constexpr char topics_resource[] = "list/func_ztxx101_1.jsn";
inline constexpr char news_resource[] = "list/func_xwlb101_1.jsn";
inline constexpr char market_anomalies_resource[] = "list/func_dpyd101_1.jsn";

struct RichText {
    std::string headline;
    std::string content;
    std::string source_url;
};

const ViewDefinition &view_definition(std::string_view name);
std::vector<std::string> resources_for_view(IntelligenceView view, const std::string &category,
                                            const std::string &topic_id,
                                            const std::string &event_id);

std::filesystem::path native_path(const std::string &value);
std::string now_text();
const Json *ptr(const Json &object, std::string_view key);
std::string text_value(const Json &object, std::string_view key);
std::optional<double> number_value(const Json &object, std::string_view key);
Json number_json(const std::optional<double> &value);
Json sum_json(const std::optional<double> &left, const std::optional<double> &right);
bool digits(const std::string &value, std::size_t size);
bool digit_identifier(const std::string &value);
RichText parse_rich_text(const std::string &raw);
int market_id(std::string value);
std::string market_name(int id);
std::string market_prefix(int id);
Json security_json(int id, const std::string &code,
                   const std::map<std::pair<int, std::string>, Security> &securities);
Json source_summary(const Json &source);
std::vector<std::pair<int, std::string>> member_keys(const std::string &members);
bool contains(const Json &value, const std::string &needle);
bool security_matches(const Json &row, int id, const std::string &code);
bool event_has_security(const Json &event, int id, const std::string &code);
Json filtered(const Json &rows, const std::string &query, int limit);
int bounded(const std::string &text, std::string_view name, int minimum, int maximum);
std::vector<std::string> risk_resources(const std::string &category);
std::vector<std::string> event_resources(const std::string &category);
std::string source_key(const std::string &resource);
void append_rows(Json &destination, const Json &rows);
void sort_events(Json &events);
Json cache_json(bool refreshed, int age);
Json highlight_summary(const Json &rows);

} // namespace tdx::intelligence_detail
