#include "stats_internal.hpp"

#include "tdx/corporate.hpp"
#include "tdx/market.hpp"
#include "tdx/session_audit.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <future>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {

using namespace stats_detail;

StatsDownloadResult download_stats_resource(const std::vector<std::string>& hosts,
                                             std::string path,
                                             std::uint32_t chunk_size,
                                             int timeout_ms,
                                             const fs::path& root) {
    (void)build_stats_file_request_data(path, 0, chunk_size);
    if (timeout_ms < 1 || timeout_ms > 600000) throw Error("timeout must be positive");
    const auto endpoint_selection = select_public_quote_endpoints(root, hosts);
    std::vector<std::string> failures;
    int connection_attempts = 0, transient_retries = 0, endpoints_attempted = 0;
    for (const auto& endpoint : endpoint_selection.endpoints) {
        ++endpoints_attempted;
        int attempts = 0;
        StatsDownloadResult downloaded;
        try {
            detail::retry_quote_transport([&] {
                QuoteConnection connection(endpoint, timeout_ms);
                Bytes archive;
                while (true) {
                    if (archive.size() >= maximum_archive_bytes)
                        throw Error("statistics ZIP exceeds the 32 MiB safety limit");
                    const auto remaining = maximum_archive_bytes - archive.size();
                    const auto requested = static_cast<std::uint32_t>(
                        std::min<std::size_t>(chunk_size, remaining));
                    const auto response = connection.call(type_file_content,
                        build_stats_file_request_data(
                            path, static_cast<std::uint32_t>(archive.size()), requested));
                    auto chunk = parse_stats_file_chunk(response.data, requested);
                    archive.insert(archive.end(), chunk.begin(), chunk.end());
                    if (chunk.size() < requested) break;
                }
                downloaded.resource = parse_stats_archive(
                    archive, "tdx://" + endpoint.address() + "/" + path);
                downloaded.archive_size = archive.size();
                downloaded.endpoint = endpoint;
                downloaded.server_name = connection.server_name();
                return true;
            }, attempts);
        } catch (const std::exception& error) {
            failures.push_back(endpoint.address() + ": " + error.what());
        }
        connection_attempts += attempts;
        transient_retries += std::max(0, attempts - 1);
        if (downloaded.archive_size) {
            downloaded.failures = std::move(failures);
            downloaded.transport = public_quote_transport_document(
                endpoint_selection, connection_attempts, transient_retries,
                endpoints_attempted);
            return downloaded;
        }
    }
    std::string detail = "all statistics endpoints failed";
    for (const auto& failure : failures) detail += "\n  " + failure;
    throw Error(detail);
}


Json fetch_security_valuation_document(const fs::path& root,
                                       const TdxStatsResource& resource,
                                       int market_id, const std::string& code,
                                       int timeout_ms,
                                       const BlockData* block_data) {
    validate_security(market_id, code);
    const auto market = std::array<std::string, 3>{"sz", "sh", "bj"}.at(
        static_cast<std::size_t>(market_id));
    const auto requested = market + ":" + code;
    const auto quote_endpoints = load_public_quote_endpoints(root).endpoints;
    auto snapshot_future = std::async(std::launch::async, [&] {
        return fetch_market_snapshot_document(root, {requested}, timeout_ms, block_data);
    });
    auto finance_future = std::async(std::launch::async, [&] {
        return fetch_finance_document(
            {requested}, quote_endpoints, timeout_ms, 80, false);
    });
    std::optional<Json> snapshot;
    std::optional<Json> finance;
    std::vector<std::string> errors;
    try { snapshot = snapshot_future.get(); }
    catch (const std::exception& error) {
        errors.push_back(std::string("snapshot: ") + error.what());
    }
    try { finance = finance_future.get(); }
    catch (const std::exception& error) {
        errors.push_back(std::string("finance: ") + error.what());
    }
    if (snapshot && !record_for(&*snapshot, market_id, code))
        errors.push_back("snapshot: selected security is absent from the response");
    if (finance && !record_for(&*finance, market_id, code))
        errors.push_back("finance: selected security is absent from the response");
    const auto found = resource.stat.find({market_id, code});
    auto document = derive_security_valuation_document(
        market_id, code, found == resource.stat.end() ? nullptr : &found->second,
        snapshot ? &*snapshot : nullptr, finance ? &*finance : nullptr, errors);
    Json upstream_transport = Json::object();
    upstream_transport["snapshot"] = snapshot && snapshot->as_object().count("transport")
        ? snapshot->at("transport") : Json(nullptr);
    upstream_transport["finance"] = finance && finance->as_object().count("transport")
        ? finance->at("transport") : Json(nullptr);
    document["upstream_transport"] = std::move(upstream_transport);
    document["stats_source"] = resource.source_path;
    if (block_data) {
        const auto security = block_data->securities.find({market_id, code});
        document["name"] = security == block_data->securities.end()
            ? "" : security->second.name;
        document["name_resolved"] = security != block_data->securities.end();
    } else {
        document["name"] = "";
        document["name_resolved"] = false;
    }
    return document;
}

Json fetch_market_stats_document(const fs::path& root,
                                 const std::vector<std::string>& securities,
                                 std::string path, std::uint32_t chunk_size,
                                 int timeout_ms, const BlockData* block_data,
                                 const std::vector<std::string>& hosts) {
    auto result = download_stats_resource(
        hosts, std::move(path), chunk_size, timeout_ms, root);
    const auto loaded_blocks = block_data ? BlockData{} : load_blocks(root, {});
    return stats_resource_document(result.resource, result.endpoint.address(), result.server_name,
                                   result.archive_size, securities,
                                   block_data ? block_data : &loaded_blocks,
                                   &result.transport);
}


}  // namespace tdx

