#include "tdx/intelligence.hpp"

#include "intelligence_internal.hpp"

#include <set>

namespace tdx {

using namespace intelligence_detail;
Json build_intelligence_graph(const Json &events, int member_limit) {
    if (!events.is_array())
        throw Error("intelligence graph events must be an array");
    if (member_limit < 1 || member_limit > 20000)
        throw Error("intelligence graph member limit is outside 1..20000");
    Json nodes = Json::array(), edges = Json::array();
    std::set<std::string> security_nodes;
    std::uint64_t relationships = 0, omitted = 0;
    for (const auto &event : events.as_array()) {
        const auto event_id = text_value(event, "event_id");
        Json event_node = Json::object();
        event_node["id"] = "event:" + event_id;
        event_node["kind"] = "event";
        event_node["label"] = text_value(event, "title");
        event_node["source"] = text_value(event, "source");
        event_node["date"] = text_value(event, "date");
        event_node["type"] = text_value(event, "type");
        event_node["member_count"] = event.at("member_count");
        nodes.push_back(std::move(event_node));
        for (const auto &security : event.at("members").as_array()) {
            ++relationships;
            const auto security_id = text_value(security, "security_id");
            if (!security_nodes.count(security_id) &&
                static_cast<int>(security_nodes.size()) >= member_limit) {
                ++omitted;
                continue;
            }
            if (security_nodes.insert(security_id).second) {
                Json security_node = Json::object();
                security_node["id"] = "security:" + security_id;
                security_node["kind"] = "security";
                security_node["label"] = text_value(security, "name").empty()
                                             ? security_id
                                             : text_value(security, "name");
                security_node["security"] = security;
                nodes.push_back(std::move(security_node));
            }
            Json edge = Json::object();
            edge["source"] = "event:" + event_id;
            edge["target"] = "security:" + security_id;
            edge["kind"] = "affects";
            edges.push_back(std::move(edge));
        }
    }
    Json counts = Json::object();
    counts["events"] = static_cast<std::uint64_t>(events.size());
    counts["securities"] = static_cast<std::uint64_t>(security_nodes.size());
    counts["relationships"] = relationships;
    counts["returned_relationships"] = static_cast<std::uint64_t>(edges.size());
    counts["omitted_relationships"] = omitted;
    Json result = Json::object();
    result["nodes"] = std::move(nodes);
    result["edges"] = std::move(edges);
    result["counts"] = std::move(counts);
    result["truncated"] = omitted > 0;
    return result;
}

} // namespace tdx
