#include "tqlex_internal.hpp"

#include <algorithm>

namespace tdx::detail {

int json_integer(const Json& value, std::string_view name);

Json::Object& tqlex_request_mapping(Json& request) {
    Json* value = &request;
    if (request.is_array()) {
        auto& array = request.as_array();
        if (array.empty()) throw Error("TQLEX request array must not be empty");
        value = &array.front();
    }
    if (!value->is_object()) throw Error("first TQLEX request item must be an object");
    return value->as_object();
}

const Json::Object& tqlex_request_mapping(const Json& request) {
    const Json* value = &request;
    if (request.is_array()) {
        const auto& array = request.as_array();
        if (array.empty()) throw Error("TQLEX request array must not be empty");
        value = &array.front();
    }
    if (!value->is_object()) throw Error("first TQLEX request item must be an object");
    return value->as_object();
}

int tqlex_request_integer(const Json& request, const std::string& name, int fallback) {
    const auto& mapping = tqlex_request_mapping(request);
    const auto folded = lower_ascii(name);
    const auto found = std::find_if(mapping.begin(), mapping.end(), [&](const auto& item) {
        return lower_ascii(item.first) == folded;
    });
    return found == mapping.end() ? fallback : json_integer(found->second, name);
}

const Json::Object& tqlex_response_object(const Json& value) {
    if (!value.is_object()) throw Error("TQLEX response must be a JSON object");
    return value.as_object();
}

const Json* tqlex_object_field(const Json::Object& object, std::string_view name) {
    const auto found = object.find(name);
    return found == object.end() ? nullptr : &found->second;
}

bool json_truthy(const Json& value) {
    if (value.is_null()) return false;
    if (value.is_bool()) return value.as_bool();
    if (value.is_number()) return value.as_number() != 0;
    if (value.is_string()) return !value.as_string().empty();
    return value.size() != 0;
}

int json_integer(const Json& value, std::string_view name) {
    try {
        if (value.is_number()) return static_cast<int>(value.as_number());
        if (value.is_string()) {
            std::size_t used = 0;
            const int result = std::stoi(value.as_string(), &used);
            if (used == value.as_string().size()) return result;
        }
    } catch (...) {}
    throw Error(std::string(name) + " is not an integer");
}

const Json::Array& tqlex_result_sets(const Json& response) {
    const auto& object = tqlex_response_object(response);
    const auto* sets = tqlex_object_field(object, "ResultSets");
    if (!sets) {
        static const Json::Array empty;
        return empty;
    }
    if (!sets->is_array()) throw Error("TQLEX ResultSets must be an array");
    return sets->as_array();
}

const Json::Array& tqlex_result_content(const Json& result_set) {
    if (!result_set.is_object()) throw Error("TQLEX ResultSets items must be objects");
    const auto* content = tqlex_object_field(result_set.as_object(), "Content");
    if (!content) {
        static const Json::Array empty;
        return empty;
    }
    if (!content->is_array()) throw Error("TQLEX ResultSet Content must be an array");
    return content->as_array();
}

std::size_t tqlex_result_set_rows(const Json& response) {
    std::size_t maximum = 0;
    for (const auto& set : tqlex_result_sets(response))
        maximum = std::max(maximum, tqlex_result_content(set).size());
    return maximum;
}

Json merge_tqlex_pages(const std::vector<Json>& pages, const std::vector<bool>& paged_sets) {
    if (pages.empty()) throw Error("cannot merge an empty TQLEX page list");
    Json merged = pages.front();
    auto& merged_sets_value = merged.as_object().at("ResultSets");
    if (!merged_sets_value.is_array()) throw Error("TQLEX ResultSets must be an array");
    auto& merged_sets = merged_sets_value.as_array();
    for (std::size_t page_index = 1; page_index < pages.size(); ++page_index) {
        const auto& source_sets = tqlex_result_sets(pages[page_index]);
        if (source_sets.size() != merged_sets.size())
            throw Error("TQLEX result-set count changed while paging");
        for (std::size_t index = 0; index < merged_sets.size(); ++index) {
            if (index >= paged_sets.size() || !paged_sets[index]) continue;
            auto& target = merged_sets[index];
            const auto& source = source_sets[index];
            if (!target.is_object() || !source.is_object())
                throw Error("TQLEX ResultSets items must be objects");
            const auto* target_columns = tqlex_object_field(target.as_object(), "ColDes");
            const auto* source_columns = tqlex_object_field(source.as_object(), "ColDes");
            const auto target_signature = target_columns ? target_columns->dump(-1) : "[]";
            const auto source_signature = source_columns ? source_columns->dump(-1) : "[]";
            if (target_signature != source_signature)
                throw Error("TQLEX result-set columns changed while paging");
            auto found = target.as_object().find("Content");
            if (found == target.as_object().end())
                found = target.as_object().emplace("Content", Json::array()).first;
            if (!found->second.is_array()) throw Error("TQLEX Content must be an array");
            auto& target_content = found->second.as_array();
            const auto& source_content = tqlex_result_content(source);
            target_content.insert(target_content.end(), source_content.begin(), source_content.end());
            target.as_object()["RowNum"] = static_cast<std::uint64_t>(target_content.size());
        }
    }
    return merged;
}

}  // namespace tdx::detail
