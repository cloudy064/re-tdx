#include "institution_internal.hpp"

#include "tdx/cloud_resilience.hpp"
#include "tdx/common.hpp"
#include "tdx/tqlex.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx::institution_detail {

Json query_holder_tqlex(const Json& request, int timeout_ms,
                        const TqlexTransport& transport, int& attempts) {
    return detail::retry_cloud_json([&] {
        return query_tqlex(
            holder_entry, request, holder_base_url, timeout_ms, transport);
    }, detail::is_transient_tqlex_error, attempts, holder_max_attempts, 250);
}

fs::path persistent_cache_path(const fs::path& directory,
                               std::string_view kind,
                               const std::string& key) {
    if (directory.empty()) return {};
    const Bytes bytes(key.begin(), key.end());
    return directory / (std::string(kind) + "-" + md5_bytes(bytes) + ".json");
}

std::optional<InstitutionService::CachedDocument> read_persistent_cache(
    const fs::path& directory, std::string_view kind, const std::string& key) {
    const auto path = persistent_cache_path(directory, kind, key);
    if (path.empty()) return std::nullopt;
    try {
        std::error_code error;
        if (!fs::is_regular_file(path, error) || error ||
            fs::file_size(path, error) > maximum_persistent_cache_bytes || error)
            return std::nullopt;
        const auto envelope = Json::parse(read_text_utf8(path));
        if (!envelope.is_object() ||
            envelope.at("schema").as_string() != "tdx-institution-cache-v1" ||
            envelope.at("kind").as_string() != kind ||
            envelope.at("key").as_string() != key)
            return std::nullopt;
        const auto stamp = envelope.at("fetched_at").as_number();
        const auto document = envelope.at("document");
        if (!std::isfinite(stamp) || stamp < 1 || !document.is_object() ||
            !document.at("rows").is_array())
            return std::nullopt;
        return InstitutionService::CachedDocument{
            document, static_cast<std::time_t>(stamp)};
    } catch (...) {
        return std::nullopt;
    }
}

void write_persistent_cache(const fs::path& directory, std::string_view kind,
                            const std::string& key,
                            const InstitutionService::CachedDocument& cached) {
    const auto path = persistent_cache_path(directory, kind, key);
    if (path.empty()) return;
    try {
        Json envelope = Json::object();
        envelope["schema"] = "tdx-institution-cache-v1";
        envelope["kind"] = std::string(kind);
        envelope["key"] = key;
        envelope["fetched_at"] = static_cast<std::int64_t>(cached.fetched_at);
        envelope["document"] = cached.document;
        atomic_write_text(path, envelope.dump(-1) + "\n");
    } catch (...) {
        // Persistence is a resilience layer. A successful upstream response must
        // remain usable even when the optional cache directory is read-only.
    }
}

Json import_holder_cache_snapshots(const std::vector<std::string>& input_names,
                                   const fs::path& cache_directory) {
    if (input_names.empty()) throw Error("at least one holder cache snapshot is required");
    if (cache_directory.empty()) throw Error("--cache-root is required for --cache-import");
    struct Page {
        std::size_t offset{};
        std::size_t total{};
        Json records{Json::array()};
    };
    std::vector<Page> pages;
    std::string holder_id, variant_id, reference_code, stock_code;
    Json detail_records = Json::array();
    for (const auto& input_name : input_names) {
        const auto snapshot = Json::parse(read_text_utf8(fs::u8path(input_name)));
        if (!snapshot.is_object() ||
            snapshot.at("schema").as_string() != "tdx-holder-history-native-v1")
            throw Error("holder cache snapshot has an unsupported schema: " + input_name);
        const auto& holder = snapshot.at("holder");
        const auto current_holder_id = holder.at("holder_id").as_string();
        const auto current_variant_id = holder.at("variant_id").as_string();
        const auto current_reference_code = holder.at("reference_code").as_string();
        const auto& stock = snapshot.at("stock_detail");
        const auto current_stock_code = stock.at("code").as_string();
        if (pages.empty()) {
            holder_id = current_holder_id;
            variant_id = current_variant_id;
            reference_code = current_reference_code;
            stock_code = current_stock_code;
            detail_records = stock.at("records");
        } else if (holder_id != current_holder_id || variant_id != current_variant_id ||
                   reference_code != current_reference_code || stock_code != current_stock_code ||
                   detail_records.dump(-1) != stock.at("records").dump(-1)) {
            throw Error("holder cache snapshots do not describe the same query");
        }
        const auto& pagination = snapshot.at("pagination");
        Page page;
        page.offset = static_cast<std::size_t>(pagination.at("offset").as_number());
        page.total = static_cast<std::size_t>(pagination.at("total").as_number());
        page.records = snapshot.at("records");
        if (!page.records.is_array() ||
            page.records.size() != static_cast<std::size_t>(
                pagination.at("returned").as_number()))
            throw Error("holder cache snapshot pagination does not match its records");
        pages.push_back(std::move(page));
    }
    std::sort(pages.begin(), pages.end(), [](const Page& left, const Page& right) {
        return left.offset < right.offset;
    });
    const auto total = pages.front().total;
    Json history_rows = Json::array();
    std::size_t next_offset = 0;
    for (const auto& page : pages) {
        if (page.total != total || page.offset != next_offset)
            throw Error("holder cache snapshots are incomplete or overlap");
        for (const auto& row : page.records.as_array()) history_rows.push_back(row);
        next_offset += page.records.size();
    }
    if (next_offset != total || total == 0)
        throw Error("holder cache snapshots do not cover the complete history");
    const auto now = std::time(nullptr);
    const auto history_key = holder_id + "|" + variant_id + "|" + reference_code;
    Json history_document = Json::object();
    history_document["rows"] = history_rows;
    write_persistent_cache(cache_directory, "holder-history", history_key,
                           {history_document, now});
    const auto restored_history = read_persistent_cache(
        cache_directory, "holder-history", history_key);
    if (!restored_history || restored_history->document.at("rows").size() != total)
        throw Error("cannot persist the complete holder history cache");
    if (!stock_code.empty()) {
        if (!detail_records.is_array())
            throw Error("holder cache stock detail must be an array");
        const auto detail_key = holder_id + "|" + variant_id + "|" + stock_code;
        Json detail_document = Json::object();
        detail_document["rows"] = detail_records;
        write_persistent_cache(cache_directory, "holder-detail", detail_key,
                               {detail_document, now});
        const auto restored_detail = read_persistent_cache(
            cache_directory, "holder-detail", detail_key);
        if (!restored_detail ||
            restored_detail->document.at("rows").size() != detail_records.size())
            throw Error("cannot persist the holder stock-detail cache");
    }
    Json result = Json::object();
    result["schema"] = "tdx-holder-cache-import-v1";
    result["holder_id"] = holder_id;
    result["variant_id"] = variant_id;
    result["reference_code"] = reference_code;
    result["stock_code"] = stock_code;
    result["history_records"] = static_cast<std::uint64_t>(total);
    result["stock_periods"] = static_cast<std::uint64_t>(detail_records.size());
    result["snapshots"] = static_cast<std::uint64_t>(pages.size());
    result["cache_directory"] = path_utf8(cache_directory);
    return result;
}

}  // namespace tdx::institution_detail
