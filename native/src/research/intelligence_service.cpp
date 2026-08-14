#include "tdx/intelligence.hpp"

#include "intelligence_internal.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <set>

namespace tdx {

using namespace intelligence_detail;
IntelligenceService::IntelligenceService(BlockData data)
    : securities_(std::move(data.securities)) {}

IntelligenceService::FetchSet
IntelligenceService::ensure_resources(const std::vector<std::string> &resources, bool refresh,
                                      int ttl_seconds, int timeout_ms) {
    const auto now = std::time(nullptr);
    std::vector<std::string> stale;
    for (const auto &resource : resources) {
        const auto found = resource_cache_.find(resource);
        if (refresh || found == resource_cache_.end() ||
            now - found->second.fetched_at >= ttl_seconds)
            stale.push_back(resource);
    }
    if (!stale.empty()) {
        const auto documents = fetch_jsn_resources_rows(stale, "bi", timeout_ms);
        if (documents.size() != stale.size())
            throw Error("intelligence resource batch is incomplete");
        const auto fetched_at = std::time(nullptr);
        for (std::size_t index = 0; index < stale.size(); ++index)
            resource_cache_[stale[index]] = {documents.as_array()[index], fetched_at};
    }
    FetchSet result;
    result.refreshed = !stale.empty();
    result.age_seconds = 0;
    for (const auto &resource : resources) {
        const auto found = resource_cache_.find(resource);
        if (found == resource_cache_.end())
            throw Error("intelligence resource cache is incomplete");
        result.age_seconds =
            std::max(result.age_seconds, static_cast<int>(std::max<std::time_t>(
                                             0, std::time(nullptr) - found->second.fetched_at)));
        result.sources.push_back(source_summary(found->second.document));
    }
    return result;
}

Json IntelligenceService::query(const IntelligenceQuery &options) {
    if (options.limit < 1 || options.limit > 10000)
        throw Error("intelligence limit is outside 1..10000");
    if (options.member_limit < 1 || options.member_limit > 20000)
        throw Error("intelligence member limit is outside 1..20000");
    const auto view = lower_ascii(trim(options.view));
    const auto category = lower_ascii(trim(options.category));
    const auto &definition = view_definition(view);
    if (options.offset < 0 || options.offset > 1000000)
        throw Error("intelligence offset is outside 0..1000000");

    if (view == "topic" && !digit_identifier(trim(options.topic_id)))
        throw Error("topic view requires a numeric --topic-id");
    if (view == "event" && !digit_identifier(trim(options.event_id)))
        throw Error("event view requires a numeric --event-id");
    if (!trim(options.category_id).empty() && !digit_identifier(trim(options.category_id)))
        throw Error("value-attention category id must be numeric");

    const auto resources = resources_for_view(definition.view, category, trim(options.topic_id),
                                              trim(options.event_id));
    auto fetched =
        ensure_resources(resources, options.refresh, options.cache_ttl_seconds, options.timeout_ms);

    Json records = Json::array(), risks = Json::array(), highlights = Json::array(),
         events = Json::array(), value_attention = Json::array();
    Json selected_attention(nullptr), graph(nullptr), selected_security(nullptr),
        highlights_summary(nullptr), selected_topic(nullptr), event_reconciliation(nullptr),
        selected_value_attention(nullptr), value_attention_reconciliation(nullptr),
        value_attention_summary(nullptr);
    int selected_market = -1;
    if (view == "security" || !trim(options.market).empty() || !trim(options.code).empty()) {
        selected_market = market_id(options.market);
        if (!digits(trim(options.code), 6))
            throw Error("intelligence security code must contain six digits");
        selected_security = security_json(selected_market, trim(options.code), securities_);
    }

    if (view == "attention" || view == "security") {
        const auto normalized = normalize_attention_rows(
            resource_cache_.at(attention_resource).document.at("rows"), securities_);
        if (view == "attention")
            records = filtered(normalized, options.query, options.limit);
        else
            for (const auto &row : normalized.as_array())
                if (security_matches(row, selected_market, trim(options.code))) {
                    selected_attention = row;
                    break;
                }
    }
    if (view == "value-attention" || view == "security") {
        const auto categories = normalize_value_attention_categories(
            resource_cache_.at(value_attention_resource).document.at("rows"), securities_);
        std::set<std::string> unique_members;
        std::uint64_t relationships = 0;
        for (const auto &category_row : categories.as_array()) {
            const auto &members = category_row.at("members");
            relationships += static_cast<std::uint64_t>(members.size());
            for (const auto &member : members.as_array())
                unique_members.insert(text_value(member, "security_id"));
        }
        value_attention_summary = Json::object();
        value_attention_summary["categories"] = static_cast<std::uint64_t>(categories.size());
        value_attention_summary["relationships"] = relationships;
        value_attention_summary["unique_securities"] =
            static_cast<std::uint64_t>(unique_members.size());
        if (view == "security") {
            for (const auto &category_row : categories.as_array()) {
                bool matched = false;
                for (const auto &member : category_row.at("members").as_array())
                    if (static_cast<int>(member.at("market_id").as_number()) == selected_market &&
                        member.at("code").as_string() == trim(options.code)) {
                        matched = true;
                        break;
                    }
                if (!matched)
                    continue;
                auto hit = category_row;
                hit.as_object().erase("members");
                hit["selected_security_related"] = true;
                value_attention.push_back(std::move(hit));
            }
        } else {
            value_attention = categories;
            const auto selected_id = trim(options.category_id);
            if (selected_id.empty()) {
                records = filtered(categories, options.query, options.limit);
            } else {
                for (const auto &category_row : categories.as_array())
                    if (text_value(category_row, "category_id") == selected_id) {
                        selected_value_attention = category_row;
                        break;
                    }
                if (selected_value_attention.is_null())
                    throw Error(
                        "selected value-attention category is absent from the active catalog");
                const auto detail_resource = "jzgz1/" + selected_id + ".jsn";
                const auto detail_fetch =
                    ensure_resources({detail_resource}, options.refresh, options.cache_ttl_seconds,
                                     options.timeout_ms);
                fetched.refreshed = fetched.refreshed || detail_fetch.refreshed;
                fetched.age_seconds = std::max(fetched.age_seconds, detail_fetch.age_seconds);
                for (const auto &source : detail_fetch.sources.as_array())
                    fetched.sources.push_back(source);
                auto details = normalize_value_attention_detail_rows(
                    resource_cache_.at(detail_resource).document.at("rows"), selected_id,
                    securities_);
                std::set<std::string> inline_ids, dynamic_ids;
                for (const auto &member : selected_value_attention.at("members").as_array())
                    inline_ids.insert(text_value(member, "security_id"));
                for (const auto &detail : details.as_array())
                    dynamic_ids.insert(text_value(detail.at("security"), "security_id"));
                Json inline_only = Json::array(), dynamic_only = Json::array();
                for (const auto &id : inline_ids)
                    if (!dynamic_ids.count(id))
                        inline_only.push_back(id);
                for (const auto &id : dynamic_ids)
                    if (!inline_ids.count(id))
                        dynamic_only.push_back(id);
                value_attention_reconciliation = Json::object();
                value_attention_reconciliation["inline_member_count"] =
                    static_cast<std::uint64_t>(inline_ids.size());
                value_attention_reconciliation["dynamic_member_count"] =
                    static_cast<std::uint64_t>(dynamic_ids.size());
                value_attention_reconciliation["counts_match"] =
                    inline_ids.size() == dynamic_ids.size();
                value_attention_reconciliation["exact_match"] = inline_ids == dynamic_ids;
                value_attention_reconciliation["inline_only"] = std::move(inline_only);
                value_attention_reconciliation["dynamic_only"] = std::move(dynamic_only);
                selected_value_attention["dynamic_member_count"] =
                    static_cast<std::uint64_t>(dynamic_ids.size());
                selected_value_attention["member_source"] = "inline-master+dynamic-detail";
                records = filtered(details, options.query, options.limit);
            }
        }
    }
    if (view == "risks" || view == "security") {
        const auto selected_resources =
            view == "security" ? std::vector<std::string>{observation_resource, potential_resource,
                                                          discredited_resource}
                               : resources;
        for (const auto &resource : selected_resources) {
            auto normalized = normalize_risk_rows(resource_cache_.at(resource).document.at("rows"),
                                                  source_key(resource), securities_);
            for (const auto &row : normalized.as_array()) {
                if (view == "security" &&
                    !security_matches(row, selected_market, trim(options.code)))
                    continue;
                risks.push_back(row);
            }
        }
        std::stable_sort(risks.as_array().begin(), risks.as_array().end(),
                         [](const Json &left, const Json &right) {
                             return number_value(left, "safety_score").value_or(1e12) <
                                    number_value(right, "safety_score").value_or(1e12);
                         });
        if (view == "risks")
            records = filtered(risks, options.query, options.limit);
    }
    if (view == "highlights" || view == "security") {
        auto normalized = normalize_highlight_rows(
            resource_cache_.at(highlights_resource).document.at("rows"), securities_);
        if (view == "highlights") {
            highlights_summary = highlight_summary(normalized);
            const auto selected_sort =
                trim(options.sort).empty() ? "highlight-count" : lower_ascii(trim(options.sort));
            sort_highlight_rows(normalized, selected_sort, options.order);
            const auto needle = lower_ascii(trim(options.query));
            const auto selected_type = trim(options.highlight_type);
            std::uint64_t matched = 0;
            for (const auto &row : normalized.as_array()) {
                if (!selected_type.empty() &&
                    text_value(row, "primary_highlight_type") != selected_type)
                    continue;
                if (!needle.empty() && !contains(row, needle))
                    continue;
                if (matched >= static_cast<std::uint64_t>(options.offset) &&
                    records.size() < static_cast<std::size_t>(options.limit))
                    records.push_back(row);
                ++matched;
            }
            highlights = records;
        } else {
            for (const auto &row : normalized.as_array())
                if (security_matches(row, selected_market, trim(options.code)))
                    highlights.push_back(row);
        }
    }
    if (view == "events" || view == "graph" || view == "security") {
        const auto selected_resources =
            view == "security"
                ? std::vector<std::string>{events_resource, ministries_resource, hotspots_resource}
                : resources;
        for (const auto &resource : selected_resources)
            append_rows(events, normalize_intelligence_event_rows(
                                    resource_cache_.at(resource).document.at("rows"),
                                    source_key(resource), securities_));
        sort_events(events);
        Json matched = Json::array();
        const auto needle = lower_ascii(trim(options.query));
        for (const auto &event : events.as_array()) {
            if (view == "security" &&
                !event_has_security(event, selected_market, trim(options.code)))
                continue;
            if (!needle.empty() && !contains(event, needle))
                continue;
            if (static_cast<int>(matched.size()) >= options.limit)
                break;
            matched.push_back(event);
        }
        events = std::move(matched);
        if (view == "events")
            records = events;
        if (view == "graph")
            graph = build_intelligence_graph(events, options.member_limit);
        if (view == "security") {
            // A security workbench needs the incident neighbourhood around one stock, not every
            // other member of every matched hotspot. Keep the original member_count as context,
            // but return a focused graph and event summaries to prevent multi-hundred-KiB replies.
            Json focused = Json::array();
            for (auto &event : events.as_array()) {
                Json graph_event = event;
                Json member = Json::array();
                member.push_back(selected_security);
                graph_event["members"] = std::move(member);
                focused.push_back(std::move(graph_event));
                event.as_object().erase("members");
                event["selected_security_related"] = true;
            }
            graph = build_intelligence_graph(focused, options.member_limit);
        }
    }
    if (view == "topics" || view == "topic") {
        const auto normalized = normalize_intelligence_topic_rows(
            resource_cache_.at(topics_resource).document.at("rows"));
        if (view == "topics") {
            records = filtered(normalized, options.query, options.limit);
        } else {
            for (const auto &topic : normalized.as_array())
                if (text_value(topic, "topic_id") == trim(options.topic_id)) {
                    selected_topic = topic;
                    break;
                }
            if (selected_topic.is_null())
                throw Error("selected topic is absent from the current topic catalog");
            const auto resource = "ztxx/" + trim(options.topic_id) + ".jsn";
            records = filtered(normalize_intelligence_timeline_rows(
                                   resource_cache_.at(resource).document.at("rows"), "topic"),
                               options.query, options.limit);
        }
    }
    if (view == "news") {
        records = filtered(normalize_intelligence_timeline_rows(
                               resource_cache_.at(news_resource).document.at("rows"), "news"),
                           options.query, options.limit);
    }
    if (view == "market-anomalies") {
        records = filtered(normalize_market_anomaly_rows(
                               resource_cache_.at(market_anomalies_resource).document.at("rows")),
                           options.query, options.limit);
    }
    if (view == "event") {
        auto normalized = normalize_intelligence_event_rows(
            resource_cache_.at(events_resource).document.at("rows"), "events", securities_);
        Json selected(nullptr);
        for (const auto &event : normalized.as_array())
            if (text_value(event, "raw_id") == trim(options.event_id)) {
                selected = event;
                break;
            }
        if (selected.is_null())
            throw Error("selected event is absent from the current event catalog");
        const auto detail_resource = "sjqd/" + trim(options.event_id) + ".jsn";
        auto dynamic_members = normalize_intelligence_event_member_rows(
            resource_cache_.at(detail_resource).document.at("rows"), securities_);
        std::set<std::string> inline_ids, dynamic_ids;
        for (const auto &member : selected.at("members").as_array())
            inline_ids.insert(text_value(member, "security_id"));
        for (const auto &member : dynamic_members.as_array())
            dynamic_ids.insert(text_value(member, "security_id"));
        Json inline_only = Json::array(), dynamic_only = Json::array();
        for (const auto &id : inline_ids)
            if (!dynamic_ids.count(id))
                inline_only.push_back(id);
        for (const auto &id : dynamic_ids)
            if (!inline_ids.count(id))
                dynamic_only.push_back(id);
        event_reconciliation = Json::object();
        event_reconciliation["inline_member_count"] = static_cast<std::uint64_t>(inline_ids.size());
        event_reconciliation["dynamic_member_count"] =
            static_cast<std::uint64_t>(dynamic_ids.size());
        const bool exact_match = inline_ids == dynamic_ids;
        event_reconciliation["counts_match"] = exact_match;
        event_reconciliation["exact_match"] = exact_match;
        event_reconciliation["inline_only"] = std::move(inline_only);
        event_reconciliation["dynamic_only"] = std::move(dynamic_only);
        selected["inline_member_count"] = static_cast<std::uint64_t>(inline_ids.size());
        selected["member_count"] = static_cast<std::uint64_t>(dynamic_ids.size());
        selected["members"] = std::move(dynamic_members);
        selected["member_source"] = "dynamic-detail";
        records.push_back(selected);
        events = records;
    }

    Json result = Json::object();
    result["schema"] = "tdx-market-intelligence-native-v1";
    result["generated_at"] = now_text();
    result["view"] = view;
    result["category"] = category;
    result["highlight_type"] =
        trim(options.highlight_type).empty() ? Json(nullptr) : Json(trim(options.highlight_type));
    result["records"] = std::move(records);
    result["attention"] = std::move(selected_attention);
    result["value_attention"] = std::move(value_attention);
    result["selected_value_attention"] = std::move(selected_value_attention);
    result["value_attention_reconciliation"] = std::move(value_attention_reconciliation);
    result["value_attention_summary"] = std::move(value_attention_summary);
    result["risks"] = std::move(risks);
    result["highlights"] = std::move(highlights);
    result["highlight_summary"] = std::move(highlights_summary);
    result["events"] = std::move(events);
    result["graph"] = std::move(graph);
    result["security"] = std::move(selected_security);
    result["selected_topic"] = std::move(selected_topic);
    result["event_reconciliation"] = std::move(event_reconciliation);
    result["sources"] = fetched.sources;
    const auto health = jsn_sources_health(fetched.sources);
    result["availability"] = health.at("stale").as_bool() ? "stale-cache" : "live";
    result["upstream_health"] = health;
    auto cache = cache_json(fetched.refreshed, fetched.age_seconds);
    cache["stale"] = health.at("stale");
    cache["upstream"] = health;
    result["cache"] = std::move(cache);
    Json counts = Json::object();
    counts["records"] = static_cast<std::uint64_t>(result.at("records").size());
    counts["risks"] = static_cast<std::uint64_t>(result.at("risks").size());
    counts["highlights"] = static_cast<std::uint64_t>(result.at("highlights").size());
    counts["events"] = static_cast<std::uint64_t>(result.at("events").size());
    counts["value_attention"] = static_cast<std::uint64_t>(result.at("value_attention").size());
    result["counts"] = std::move(counts);
    return result;
}

} // namespace tdx
