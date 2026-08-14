#include "factors_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <map>

namespace fs = std::filesystem;

namespace tdx {

using namespace factor_detail;

namespace {
std::string factor_record_identity(const std::string& view, const Json& record) {
    if (view == "catalog" || view == "patterns") {
        const auto id = text(record, "factor_id");
        if (!id.empty()) return "factor:" + id;
    }
    if (view == "overview" || view == "pattern-matrix" ||
        view == "standard-matrix") {
        const auto* factor = field(record, "factor");
        if (factor && factor->is_object()) {
            const auto id = text(*factor, "factor_id");
            if (!id.empty()) return "factor:" + id;
        }
    }
    const auto* security = field(record, "security");
    if (security && security->is_object()) {
        const auto id = text(*security, "security_id");
        if (!id.empty()) {
            if (view == "intraday-radar")
                return "security:" + id + "|factor:" + text(record, "selected_factor") +
                       "|time:" + text(record, "selection_time");
            return "security:" + id;
        }
    }
    throw Error("factor snapshot record has no stable identity for view " + view);
}

Json factor_snapshot_parameters(const Json& document) {
    if (const auto* parameters = field(document, "parameters");
        parameters && parameters->is_object()) return *parameters;
    Json result = Json::object();
    result["view"] = text(document, "view");
    result["factor_id"] = field(document, "factor_id")
        ? *field(document, "factor_id") : Json(nullptr);
    result["query"] = text(document, "query");
    result["pagination"] = field(document, "pagination")
        ? *field(document, "pagination") : Json(nullptr);
    return result;
}

std::map<std::string, Json> indexed_factor_records(const Json& document) {
    const auto view = text(document, "view");
    const auto* records = field(document, "records");
    if (view.empty() || !records || !records->is_array())
        throw Error("factor snapshot requires a view and records array");
    std::map<std::string, Json> result;
    for (const auto& record : records->as_array()) {
        const auto identity = factor_record_identity(view, record);
        if (!result.emplace(identity, record).second)
            throw Error("factor snapshot has duplicate identity: " + identity);
    }
    return result;
}

void validate_factor_snapshot(const Json& document) {
    if (!document.is_object() || text(document, "schema") != "tdx-factors-native-v1")
        throw Error("snapshot requires a tdx-factors-native-v1 document");
    if (text(document, "availability") == "partial")
        throw Error("cannot snapshot a partial factor result");
    const auto* counts = field(document, "counts");
    if (counts && counts->is_object()) {
        const auto* truncated = field(*counts, "truncated");
        if (truncated && truncated->is_bool() && truncated->as_bool())
            throw Error("cannot snapshot a truncated factor result; raise --limit");
    }
    (void)indexed_factor_records(document);
}

}  // namespace
Json diff_factor_documents(const Json& previous, const Json& current) {
    validate_factor_snapshot(previous);
    validate_factor_snapshot(current);
    const auto previous_view = text(previous, "view");
    const auto current_view = text(current, "view");
    if (previous_view != current_view)
        throw Error("factor snapshot view mismatch: " + previous_view + " != " + current_view);
    const auto previous_parameters = factor_snapshot_parameters(previous);
    const auto current_parameters = factor_snapshot_parameters(current);
    if (previous_parameters.dump(-1) != current_parameters.dump(-1))
        throw Error("factor snapshot parameters differ; use a separate snapshot file");
    const auto before = indexed_factor_records(previous);
    const auto after = indexed_factor_records(current);
    Json added = Json::array(), removed = Json::array(), changed = Json::array();
    Json unchanged = Json::array();
    for (const auto& [identity, record] : after) {
        const auto found = before.find(identity);
        if (found == before.end()) {
            Json item = Json::object(); item["identity"] = identity; item["record"] = record;
            added.push_back(std::move(item));
        } else if (found->second.dump(-1) != record.dump(-1)) {
            Json item = Json::object(); item["identity"] = identity;
            item["previous"] = found->second; item["current"] = record;
            changed.push_back(std::move(item));
        } else unchanged.push_back(identity);
    }
    for (const auto& [identity, record] : before) {
        if (after.find(identity) != after.end()) continue;
        Json item = Json::object(); item["identity"] = identity; item["record"] = record;
        removed.push_back(std::move(item));
    }
    Json counts = Json::object();
    counts["previous"] = static_cast<std::uint64_t>(before.size());
    counts["current"] = static_cast<std::uint64_t>(after.size());
    counts["added"] = static_cast<std::uint64_t>(added.size());
    counts["removed"] = static_cast<std::uint64_t>(removed.size());
    counts["changed"] = static_cast<std::uint64_t>(changed.size());
    counts["unchanged"] = static_cast<std::uint64_t>(unchanged.size());
    Json result = Json::object();
    result["schema"] = "tdx-factors-diff-v1";
    result["generated_at"] = now_text();
    result["view"] = current_view;
    result["parameters"] = current_parameters;
    result["previous_generated_at"] = text(previous, "generated_at");
    result["current_generated_at"] = text(current, "generated_at");
    result["counts"] = std::move(counts);
    result["added"] = std::move(added);
    result["removed"] = std::move(removed);
    result["changed"] = std::move(changed);
    result["unchanged_identities"] = std::move(unchanged);
    return result;
}

Json update_factor_snapshot(const fs::path& path, const Json& current) {
    validate_factor_snapshot(current);
    if (path.empty()) throw Error("snapshot path must not be empty");
    Json result = Json::object();
    result["schema"] = "tdx-factors-snapshot-update-v1";
    result["path"] = path_utf8(path);
    result["updated_at"] = now_text();
    if (fs::exists(path)) {
        const auto previous = Json::parse(read_text_utf8(path));
        result["created"] = false;
        result["diff"] = diff_factor_documents(previous, current);
    } else {
        result["created"] = true;
        result["diff"] = Json(nullptr);
    }
    atomic_write_text(path, current.dump(2) + "\n");
    result["stored_records"] = static_cast<std::uint64_t>(
        field(current, "records")->size());
    return result;
}

}  // namespace tdx