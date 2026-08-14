#include "technical_signals_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/market.hpp"

#include <filesystem>
#include <map>

namespace fs = std::filesystem;

namespace tdx::technical_signals_detail {
std::string signal_record_identity(const Json& record) {
    if (!record.is_object()) throw Error("technical signal record must be an object");
    if (const auto* security = field(record, "security"); security && security->is_object()) {
        const auto identity = text(*security, "security_id");
        if (!identity.empty()) {
            const auto* signal = field(record, "signal");
            const auto signal_code = signal && signal->is_object()
                ? text(*signal, "code") : std::string{};
            const auto selection_time = text(record, "selection_time");
            if (!signal_code.empty())
                return "security:" + identity + "|signal:" + signal_code +
                       "|time:" + selection_time;
            return "security:" + identity;
        }
    }
    if (const auto* block = field(record, "block"); block && block->is_object()) {
        auto identity = text(*block, "block_id");
        if (identity.empty()) identity = text(*block, "code");
        if (!identity.empty()) return "block:" + identity;
    }
    throw Error("technical signal record has no stable entity identity");
}

std::map<std::string, Json> indexed_signal_records(const Json& document) {
    const auto* records = field(document, "records");
    if (!records || !records->is_array())
        throw Error("technical signal document has no records array");
    std::map<std::string, Json> indexed;
    for (const auto& record : records->as_array()) {
        const auto identity = signal_record_identity(record);
        if (!indexed.emplace(identity, record).second)
            throw Error("technical signal snapshot has a duplicate identity: " + identity);
    }
    return indexed;
}

void validate_snapshot_document(const Json& document) {
    if (!document.is_object() || text(document, "schema") != "tdx-technical-signals-native-v1")
        throw Error("snapshot requires a tdx-technical-signals-native-v1 document");
    if (text(document, "view").empty()) throw Error("snapshot document has no view");
    (void)indexed_signal_records(document);
}

std::string selected_board(int selected_market, const std::string& code) {
    if (selected_market == 2) return "bj";
    if (selected_market == 0 && code.rfind("30", 0) == 0) return "gem";
    if (selected_market == 1 && code.rfind("68", 0) == 0) return "star";
    return "main";
}

Json enrich_factor_quotes(const fs::path& root, Json& records,
                          const BlockData& blocks, int timeout_ms) {
    std::vector<std::string> requested;
    std::set<std::string> seen;
    for (const auto& record : records.as_array()) {
        const auto* security = field(record, "security");
        if (!security || !security->is_object()) continue;
        const auto key = text(*security, "market") + ":" + text(*security, "code");
        if (seen.insert(key).second) requested.push_back(key);
    }
    Json result = Json::object();
    result["requested"] = true;
    result["requested_securities"] = static_cast<std::uint64_t>(requested.size());
    if (requested.empty()) {
        result["status"] = "empty";
        result["received_securities"] = 0;
        result["matched_records"] = 0;
        return result;
    }
    const auto quotes = fetch_market_snapshot_document(
        root, requested, timeout_ms, &blocks);
    std::map<std::string, Json> indexed;
    for (const auto& quote : quotes.at("records").as_array())
        indexed.emplace(text(quote, "security_id"), quote);
    std::size_t matched = 0;
    for (auto& record : records.as_array()) {
        const auto* security = field(record, "security");
        if (!security || !security->is_object()) continue;
        const auto found = indexed.find(text(*security, "security_id"));
        if (found == indexed.end()) continue;
        record["current_quote"] = found->second;
        const auto selected = number(record, "selection_price");
        const auto current = number(found->second, "last_price");
        if (selected)
            record["since_selection_pct"] = current && *selected != 0.0
                ? Json((*current / *selected - 1.0) * 100.0) : Json(nullptr);
        ++matched;
    }
    result["status"] = "live";
    result["received_securities"] = quotes.at("received");
    result["matched_records"] = static_cast<std::uint64_t>(matched);
    result["generated_at"] = quotes.at("generated_at");
    result["command"] = quotes.at("command");
    result["endpoint"] = quotes.at("endpoint");
    result["server_name"] = quotes.at("server_name");
    return result;
}

}  // namespace tdx::technical_signals_detail

namespace tdx {
using namespace technical_signals_detail;
Json diff_technical_signal_documents(const Json& previous, const Json& current) {
    validate_snapshot_document(previous);
    validate_snapshot_document(current);
    const auto previous_view = text(previous, "view");
    const auto current_view = text(current, "view");
    if (previous_view != current_view)
        throw Error("snapshot view mismatch: " + previous_view + " != " + current_view);
    const auto* previous_parameters = field(previous, "parameters");
    const auto* current_parameters = field(current, "parameters");
    if (!previous_parameters || !current_parameters ||
        previous_parameters->dump(-1) != current_parameters->dump(-1))
        throw Error("snapshot parameters differ; use a separate snapshot file");

    const auto before = indexed_signal_records(previous);
    const auto after = indexed_signal_records(current);
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
        } else {
            unchanged.push_back(identity);
        }
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
    result["schema"] = "tdx-technical-signals-diff-v1";
    result["generated_at"] = now_text();
    result["view"] = current_view;
    result["parameters"] = *current_parameters;
    result["previous_generated_at"] = text(previous, "generated_at");
    result["current_generated_at"] = text(current, "generated_at");
    result["counts"] = std::move(counts);
    result["added"] = std::move(added);
    result["removed"] = std::move(removed);
    result["changed"] = std::move(changed);
    result["unchanged_identities"] = std::move(unchanged);
    return result;
}

Json update_technical_signal_snapshot(const fs::path& path, const Json& current) {
    validate_snapshot_document(current);
    if (path.empty()) throw Error("snapshot path must not be empty");
    Json result = Json::object();
    result["schema"] = "tdx-technical-signals-snapshot-update-v1";
    result["path"] = path_utf8(path);
    result["updated_at"] = now_text();
    if (fs::exists(path)) {
        const auto previous = Json::parse(read_text_utf8(path));
        result["created"] = false;
        result["diff"] = diff_technical_signal_documents(previous, current);
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
