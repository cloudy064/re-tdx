#include "flow_followup_internal.hpp"

#include "tdx/common.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace tdx::flow_followup_detail {

Json result_set_metadata(const Json& upstream, std::size_t decoded_rows) {
    Json result = Json::object();
    const auto* sets = field(upstream.at("response"), "ResultSets");
    if (!sets || !sets->is_array() || sets->size() == 0) {
        result["declared_row_count"] = Json(nullptr);
        result["decoded_row_count"] = static_cast<std::uint64_t>(decoded_rows);
        result["declared_column_count"] = Json(nullptr);
        result["decoded_column_count"] = 0;
        result["consistent"] = false;
        return result;
    }
    const auto& table = sets->as_array().front();
    const auto declared_rows = number(table, "RowNum");
    const auto declared_columns = number(table, "ColNum");
    const auto* descriptions = field(table, "ColDes");
    const auto decoded_columns = descriptions && descriptions->is_array()
        ? descriptions->size() : 0;
    result["declared_row_count"] = number_json(declared_rows);
    result["decoded_row_count"] = static_cast<std::uint64_t>(decoded_rows);
    result["declared_column_count"] = number_json(declared_columns);
    result["decoded_column_count"] = static_cast<std::uint64_t>(decoded_columns);
    result["consistent"] = declared_rows && declared_columns &&
        static_cast<std::size_t>(*declared_rows) == decoded_rows &&
        static_cast<std::size_t>(*declared_columns) == decoded_columns;
    result["row_count_trusted"] = "decoded-content";
    return result;
}

Json tqlex_source_document(const Json& upstream, const std::string& request_id,
                           const std::string& module, std::size_t decoded_rows) {
    Json result = Json::object();
    result["transport"] = "TQLEX reqformat=2";
    result["request_id"] = request_id;
    result["entry"] = upstream.at("entry");
    result["source_file"] = upstream.at("source_file");
    result["module"] = module;
    result["result_set_metadata"] = result_set_metadata(upstream, decoded_rows);
    return result;
}


bool transient_error(const std::string& message) {
    for (const auto* token : {"PBRPC HTTP status 429", "PBRPC HTTP status 502",
                              "PBRPC HTTP status 503", "PBRPC HTTP status 504",
                              "PBRPC business ErrorCode 4", "RpcID -1"})
        if (message.find(token) != std::string::npos) return true;
    for (const auto* token : {"TQLEX HTTP status 429", "TQLEX HTTP status 502",
                              "TQLEX HTTP status 503", "TQLEX HTTP status 504",
                              "TQLEX server returned ErrorCode 4"})
        if (message.find(token) != std::string::npos) return true;
    return false;
}


Json source_document(const Json& upstream, const Json& raw_rows,
                     const std::string& request_id) {
    const auto& response = upstream.at("response");
    const auto& result_sets = response.at("ResultSets");
    if (!result_sets.is_array() || result_sets.size() == 0)
        throw Error("flow follow-up response has no result set");
    const auto& table = result_sets.as_array().front();
    const auto declared_rows = number(table, "RowNum");
    const auto declared_columns = number(table, "ColNum");
    const auto* descriptions = field(table, "ColDes");
    const std::size_t decoded_columns = descriptions && descriptions->is_array()
        ? descriptions->size() : 0;
    const bool metadata_consistent = declared_rows && declared_columns &&
        static_cast<std::size_t>(*declared_rows) == raw_rows.size() &&
        static_cast<std::size_t>(*declared_columns) == decoded_columns;
    Json metadata = Json::object();
    metadata["declared_row_count"] = number_json(declared_rows);
    metadata["decoded_row_count"] = static_cast<std::uint64_t>(raw_rows.size());
    metadata["declared_column_count"] = number_json(declared_columns);
    metadata["decoded_column_count"] = static_cast<std::uint64_t>(decoded_columns);
    metadata["consistent"] = metadata_consistent;
    metadata["row_count_trusted"] = "decoded-content";

    Json result = Json::object();
    result["transport"] = "PBRPC reqformat=22";
    result["request_id"] = request_id;
    result["source_file"] = upstream.at("source_file");
    result["module"] = upstream.at("module");
    result["rpc_id"] = upstream.at("rpc_id");
    result["rounds"] = upstream.at("rounds");
    result["raw_size"] = upstream.at("raw_size");
    result["result_set_metadata"] = std::move(metadata);
    return result;
}

}  // namespace tdx::flow_followup_detail
