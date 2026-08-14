#include "intelligence_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace intelligence_detail {

fs::path native_path(const std::string &value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string now_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output;
    output << local_timestamp_text(local);
    return output.str();
}

const Json *ptr(const Json &object, std::string_view key) {
    if (!object.is_object())
        return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string text_value(const Json &object, std::string_view key) {
    const auto *value = ptr(object, key);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

std::optional<double> number_value(const Json &object, std::string_view key) {
    const auto *value = ptr(object, key);
    if (!value || value->is_null())
        return std::nullopt;
    if (value->is_number())
        return value->as_number();
    const auto text = trim(jsn_scalar_text(*value));
    if (text.empty())
        return std::nullopt;
    try {
        std::size_t used = 0;
        const auto result = std::stod(text, &used);
        return used == text.size() && std::isfinite(result) ? std::optional<double>(result)
                                                            : std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

Json number_json(const std::optional<double> &value) {
    return value ? Json(*value) : Json(nullptr);
}

Json sum_json(const std::optional<double> &left, const std::optional<double> &right) {
    return left || right ? Json(left.value_or(0.0) + right.value_or(0.0)) : Json(nullptr);
}

bool digits(const std::string &value, std::size_t size) {
    return value.size() == size &&
           std::all_of(value.begin(), value.end(), [](char ch) { return ch >= '0' && ch <= '9'; });
}

bool digit_identifier(const std::string &value) {
    return !value.empty() && value.size() <= 32 &&
           std::all_of(value.begin(), value.end(), [](char ch) { return ch >= '0' && ch <= '9'; });
}

void replace_text(std::string &value, const std::string &from, const std::string &to) {
    if (from.empty())
        return;
    std::size_t offset = 0;
    while ((offset = value.find(from, offset)) != std::string::npos) {
        value.replace(offset, from.size(), to);
        offset += to.size();
    }
}

RichText parse_rich_text(const std::string &raw) {
    RichText result;
    const auto marker = raw.find("TXT:");
    result.headline = trim(marker == std::string::npos ? raw : raw.substr(0, marker));
    const auto body = marker == std::string::npos ? std::string{} : raw.substr(marker + 4);
    for (const auto quote : {'\'', '"'}) {
        const auto prefix = std::string("href=") + quote;
        const auto start = body.find(prefix);
        if (start == std::string::npos)
            continue;
        const auto value_start = start + prefix.size();
        const auto end = body.find(quote, value_start);
        if (end != std::string::npos)
            result.source_url = body.substr(value_start, end - value_start);
        break;
    }
    std::string plain;
    plain.reserve(body.size());
    bool in_tag = false;
    for (std::size_t index = 0; index < body.size(); ++index) {
        const char ch = body[index];
        if (ch == '<') {
            in_tag = true;
            const auto end = body.find('>', index + 1);
            if (end != std::string::npos) {
                auto tag = lower_ascii(body.substr(index + 1, end - index - 1));
                if (tag.rfind("p", 0) == 0 || tag.rfind("/p", 0) == 0 || tag.rfind("br", 0) == 0 ||
                    tag.rfind("li", 0) == 0 || tag.rfind("/li", 0) == 0)
                    plain.push_back('\n');
            }
            continue;
        }
        if (ch == '>') {
            in_tag = false;
            continue;
        }
        if (!in_tag)
            plain.push_back(ch);
    }
    replace_text(plain, "&nbsp;", " ");
    replace_text(plain, "&amp;", "&");
    replace_text(plain, "&lt;", "<");
    replace_text(plain, "&gt;", ">");
    replace_text(plain, "&quot;", "\"");
    replace_text(plain, "&#39;", "'");
    std::string compact;
    compact.reserve(plain.size());
    bool previous_space = false, previous_newline = false;
    for (const char ch : plain) {
        if (ch == '\r')
            continue;
        if (ch == '\n') {
            while (!compact.empty() && compact.back() == ' ')
                compact.pop_back();
            if (!compact.empty() && !previous_newline)
                compact.push_back('\n');
            previous_newline = true;
            previous_space = false;
        } else if (ch == ' ' || ch == '\t') {
            if (!previous_space && !previous_newline)
                compact.push_back(' ');
            previous_space = true;
        } else {
            compact.push_back(ch);
            previous_space = false;
            previous_newline = false;
        }
    }
    result.content = trim(compact);
    return result;
}

int market_id(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "sz" || value == "szse")
        return 0;
    if (value == "sh" || value == "sse")
        return 1;
    if (value == "bj" || value == "bse")
        return 2;
    try {
        std::size_t used = 0;
        const int id = std::stoi(value, &used);
        if (used != value.size() || (id != 0 && id != 1 && id != 2 && id != 44))
            throw std::invalid_argument("market");
        return id == 44 ? 2 : id;
    } catch (...) {
        throw Error("market must be sz/sh/bj or 0/1/2");
    }
}

std::string market_name(int id) { return id == 0 ? "sz" : id == 1 ? "sh" : "bj"; }

std::string market_prefix(int id) { return id == 0 ? "SZ" : id == 1 ? "SH" : "BJ"; }

Json security_json(int id, const std::string &code,
                   const std::map<std::pair<int, std::string>, Security> &securities) {
    const auto found = securities.find({id, code});
    Json result = Json::object();
    result["market"] = market_name(id);
    result["market_id"] = id;
    result["code"] = code;
    result["security_id"] = market_prefix(id) + code;
    result["name"] = found == securities.end() ? "" : found->second.name;
    result["name_resolved"] = found != securities.end();
    return result;
}

Json source_summary(const Json &source) { return jsn_source_metadata(source); }

std::vector<std::pair<int, std::string>> member_keys(const std::string &members) {
    std::vector<std::pair<int, std::string>> result;
    std::set<std::pair<int, std::string>> seen;
    for (const auto &token : split(members, ',')) {
        const auto parts = split(trim(token), '|');
        if (parts.size() != 2 || !digits(trim(parts[1]), 6))
            continue;
        try {
            const auto key = std::make_pair(market_id(trim(parts[0])), trim(parts[1]));
            if (seen.insert(key).second)
                result.push_back(key);
        } catch (...) {
        }
    }
    return result;
}

bool contains(const Json &value, const std::string &needle) {
    if (value.is_string())
        return lower_ascii(value.as_string()).find(needle) != std::string::npos;
    if (value.is_number() || value.is_bool())
        return lower_ascii(jsn_scalar_text(value)).find(needle) != std::string::npos;
    if (value.is_array())
        for (const auto &child : value.as_array())
            if (contains(child, needle))
                return true;
    if (value.is_object())
        for (const auto &[key, child] : value.as_object())
            if (lower_ascii(key).find(needle) != std::string::npos || contains(child, needle))
                return true;
    return false;
}

bool security_matches(const Json &row, int id, const std::string &code) {
    const auto *security = ptr(row, "security");
    return security && security->is_object() &&
           static_cast<int>(security->at("market_id").as_number()) == id &&
           security->at("code").as_string() == code;
}

bool event_has_security(const Json &event, int id, const std::string &code) {
    const auto *members = ptr(event, "members");
    if (!members || !members->is_array())
        return false;
    return std::any_of(members->as_array().begin(), members->as_array().end(),
                       [&](const Json &security) {
                           return static_cast<int>(security.at("market_id").as_number()) == id &&
                                  security.at("code").as_string() == code;
                       });
}

Json filtered(const Json &rows, const std::string &query, int limit) {
    Json result = Json::array();
    const auto needle = lower_ascii(trim(query));
    for (const auto &row : rows.as_array()) {
        if (!needle.empty() && !contains(row, needle))
            continue;
        if (static_cast<int>(result.size()) >= limit)
            break;
        result.push_back(row);
    }
    return result;
}

int bounded(const std::string &text, std::string_view name, int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int result = std::stoi(text, &used);
        if (used != text.size() || result < minimum || result > maximum)
            throw std::invalid_argument("range");
        return result;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

std::vector<std::string> risk_resources(const std::string &category) {
    if (category == "all")
        return {observation_resource, potential_resource, discredited_resource};
    if (category == "observation")
        return {observation_resource};
    if (category == "potential")
        return {potential_resource};
    if (category == "discredited")
        return {discredited_resource};
    throw Error("risk category must be all, observation, potential, or discredited");
}

std::vector<std::string> event_resources(const std::string &category) {
    if (category == "all")
        return {events_resource, ministries_resource, hotspots_resource};
    if (category == "events")
        return {events_resource};
    if (category == "ministries")
        return {ministries_resource};
    if (category == "hotspots")
        return {hotspots_resource};
    throw Error("event category must be all, events, ministries, or hotspots");
}

std::string source_key(const std::string &resource) {
    if (resource == events_resource)
        return "events";
    if (resource == ministries_resource)
        return "ministries";
    if (resource == hotspots_resource)
        return "hotspots";
    if (resource == observation_resource)
        return "observation";
    if (resource == potential_resource)
        return "potential";
    if (resource == discredited_resource)
        return "discredited";
    if (resource == highlights_resource)
        return "highlights";
    return resource;
}

void append_rows(Json &destination, const Json &rows) {
    for (const auto &row : rows.as_array())
        destination.push_back(row);
}

void sort_events(Json &events) {
    std::stable_sort(events.as_array().begin(), events.as_array().end(),
                     [](const Json &left, const Json &right) {
                         const auto left_key =
                             text_value(left, "date") + ":" +
                             std::to_string(number_value(left, "importance").value_or(0));
                         const auto right_key =
                             text_value(right, "date") + ":" +
                             std::to_string(number_value(right, "importance").value_or(0));
                         return left_key > right_key;
                     });
}

Json cache_json(bool refreshed, int age) {
    Json value = Json::object();
    value["refreshed"] = refreshed;
    value["age_seconds"] = age;
    return value;
}

const ViewDefinition &view_definition(std::string_view name) {
    const auto found =
        std::find_if(view_definitions.begin(), view_definitions.end(),
                     [name](const ViewDefinition &definition) { return definition.name == name; });
    if (found == view_definitions.end()) {
        throw Error("intelligence view must be attention, value-attention, risks, highlights, "
                    "events, event, graph, security, topics, topic, news, or market-anomalies");
    }
    return *found;
}

std::vector<std::string> resources_for_view(IntelligenceView view, const std::string &category,
                                            const std::string &topic_id,
                                            const std::string &event_id) {
    switch (view) {
    case IntelligenceView::attention:
        return {attention_resource};
    case IntelligenceView::value_attention:
        return {value_attention_resource};
    case IntelligenceView::risks:
        return risk_resources(category);
    case IntelligenceView::highlights:
        return {highlights_resource};
    case IntelligenceView::events:
    case IntelligenceView::graph:
        return event_resources(category);
    case IntelligenceView::event:
        return {events_resource, "sjqd/" + event_id + ".jsn"};
    case IntelligenceView::topics:
        return {topics_resource};
    case IntelligenceView::topic:
        return {topics_resource, "ztxx/" + topic_id + ".jsn"};
    case IntelligenceView::news:
        return {news_resource};
    case IntelligenceView::market_anomalies:
        return {market_anomalies_resource};
    case IntelligenceView::security:
        return {attention_resource, value_attention_resource, observation_resource,
                potential_resource, discredited_resource,     highlights_resource,
                events_resource,    ministries_resource,      hotspots_resource};
    }
    throw Error("unsupported intelligence view");
}

} // namespace intelligence_detail

} // namespace tdx
