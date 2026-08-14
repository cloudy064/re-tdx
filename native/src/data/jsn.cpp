#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/json.hpp"
#include "tdx/session_audit.hpp"
#include "tdx/transport.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr std::uint16_t type_file_info = 709;
constexpr std::uint16_t type_file_chunk = 1721;
constexpr std::size_t chunk_size = 30'000;
constexpr std::size_t info_path_size = 40;
constexpr std::size_t chunk_path_size = 100;
constexpr std::size_t chunk_request_size = 308;
constexpr std::uint32_t maximum_resource_size = 512U * 1024U * 1024U;
constexpr int attempts_per_endpoint = 3;

class MissingResource : public Error {
public:
    using Error::Error;
};

struct FileInfo {
    std::uint32_t size{};
    bool has_md5{};
    std::string md5;
};

struct SharedRowsCacheEntry {
    std::shared_ptr<const std::string> serialized;
    std::time_t fetched_at{};
    std::uint64_t sequence{};
    std::uint64_t bytes{};
};

constexpr std::size_t shared_rows_cache_max_entries = 512;
constexpr std::uint64_t shared_rows_cache_max_bytes = 256ULL * 1024ULL * 1024ULL;
std::mutex shared_rows_cache_mutex;
std::map<std::string, SharedRowsCacheEntry> shared_rows_cache;
std::uint64_t shared_rows_cache_sequence{};
std::uint64_t shared_rows_cache_bytes{};

void evict_shared_rows_cache(std::uint64_t required_bytes,
                             const std::set<std::string>& protected_keys) {
    while (!shared_rows_cache.empty() &&
           (shared_rows_cache.size() >= shared_rows_cache_max_entries ||
            shared_rows_cache_bytes + required_bytes > shared_rows_cache_max_bytes)) {
        auto oldest = shared_rows_cache.end();
        for (auto item = shared_rows_cache.begin(); item != shared_rows_cache.end(); ++item) {
            if (protected_keys.count(item->first)) continue;
            if (oldest == shared_rows_cache.end() ||
                item->second.sequence < oldest->second.sequence) oldest = item;
        }
        if (oldest == shared_rows_cache.end()) break;
        shared_rows_cache_bytes -= oldest->second.bytes;
        shared_rows_cache.erase(oldest);
    }
}

fs::path path_from_utf8(std::string_view value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string normalize_resource(std::string value) {
    value = trim(std::move(value));
    std::replace(value.begin(), value.end(), '\\', '/');
    while (!value.empty() && value.front() == '/') value.erase(value.begin());
    if (value.empty()) throw Error("JSN resource path is empty");
    for (unsigned char ch : value)
        if (ch > 0x7F) throw Error("JSN resource path must be ASCII: " + value);
    const auto parts = split(value, '/');
    for (const auto& part : parts)
        if (part.empty() || part == "." || part == ".." || part.find(':') != std::string::npos)
            throw Error("unsafe JSN resource path: " + value);
    if (value.size() < 4 || lower_ascii(value.substr(value.size() - 4)) != ".jsn")
        throw Error("resource is not a .jsn file: " + value);
    if (parts.size() == 1) value = "list/" + value;
    return value;
}

std::string remote_path_for(const std::string& resource, std::string prefix) {
    prefix = trim(std::move(prefix));
    while (!prefix.empty() && prefix.front() == '/') prefix.erase(prefix.begin());
    while (!prefix.empty() && prefix.back() == '/') prefix.pop_back();
    if (prefix != "bi" && prefix != "bib" && prefix != "bi_diy")
        throw Error("unknown JSN remote prefix: " + prefix);
    return prefix + "/" + resource;
}

Bytes fixed_path(const std::string& path, std::size_t size) {
    if (path.size() >= size)
        throw Error("remote path exceeds protocol limit of " + std::to_string(size - 1) +
                    " bytes: " + path);
    Bytes result(size, 0);
    std::copy(path.begin(), path.end(), result.begin());
    return result;
}

FileInfo query_info(QuoteConnection& connection, const std::string& remote_path) {
    const auto response = connection.call(type_file_info, fixed_path(remote_path, info_path_size));
    if (response.data.size() < 38) throw Error("JSN metadata response is shorter than 38 bytes");
    FileInfo info;
    info.size = read_u32_le(response.data.data());
    info.has_md5 = response.data[4] != 0;
    const auto begin = response.data.begin() + 5;
    const auto end_limit = response.data.begin() + 37;
    const auto end = std::find(begin, end_limit, 0);
    info.md5.assign(begin, end);
    info.md5 = lower_ascii(info.md5);
    if (info.size == 0) throw MissingResource("server reports a zero-length JSN resource");
    if (info.size > maximum_resource_size)
        throw Error("JSN resource exceeds native safety limit: " + std::to_string(info.size));
    if (info.has_md5) {
        if (info.md5.size() != 32 || !std::all_of(info.md5.begin(), info.md5.end(), [](char ch) {
                return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
            })) throw Error("server returned an invalid JSN MD5: " + info.md5);
    }
    return info;
}

Bytes chunk_request(const std::string& remote_path, std::uint32_t offset,
                    std::uint32_t requested) {
    if (requested < 1 || requested > chunk_size) throw Error("invalid JSN chunk size");
    Bytes result(chunk_request_size, 0);
    for (int shift = 0; shift < 32; shift += 8) {
        result[shift / 8] = static_cast<std::uint8_t>(offset >> shift);
        result[4 + shift / 8] = static_cast<std::uint8_t>(requested >> shift);
    }
    const auto path = fixed_path(remote_path, chunk_path_size);
    std::copy(path.begin(), path.end(), result.begin() + 8);
    return result;
}

Bytes parse_chunk(const Bytes& response, std::uint32_t requested) {
    if (response.size() < 4) throw Error("JSN chunk response has no length field");
    const auto size = read_u32_le(response.data());
    if (size > requested) throw Error("JSN server returned more bytes than requested");
    if (response.size() < static_cast<std::size_t>(size) + 4)
        throw Error("JSN chunk response is truncated");
    return Bytes(response.begin() + 4, response.begin() + 4 + size);
}

Bytes download_memory(QuoteConnection& connection, const std::string& remote_path,
                      const FileInfo& info) {
    Bytes result;
    result.reserve(info.size);
    while (result.size() < info.size) {
        const auto offset = static_cast<std::uint32_t>(result.size());
        const auto requested = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(chunk_size), info.size - offset);
        const auto response = connection.call(type_file_chunk,
                                               chunk_request(remote_path, offset, requested));
        auto chunk = parse_chunk(response.data, requested);
        if (chunk.empty()) throw Error("JSN server returned an empty chunk");
        result.insert(result.end(), chunk.begin(), chunk.end());
    }
    if (result.size() != info.size) throw Error("downloaded JSN length mismatch");
    return result;
}

void replace_file(const fs::path& temporary, const fs::path& destination) {
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw Error("cannot replace output file: " + path_utf8(destination) +
                    ", Win32 " + std::to_string(GetLastError()));
#else
    fs::rename(temporary, destination);
#endif
}

void download_file(QuoteConnection& connection, const std::string& remote_path,
                   const fs::path& destination, const FileInfo& info) {
    if (!destination.parent_path().empty()) fs::create_directories(destination.parent_path());
    fs::path temporary = destination;
    temporary += ".part." + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw Error("cannot create temporary JSN file: " + path_utf8(temporary));
        std::uint32_t downloaded = 0;
        int last_percent = -1;
        while (downloaded < info.size) {
            const auto requested = std::min<std::uint32_t>(
                static_cast<std::uint32_t>(chunk_size), info.size - downloaded);
            const auto response = connection.call(type_file_chunk,
                                                   chunk_request(remote_path, downloaded, requested));
            auto chunk = parse_chunk(response.data, requested);
            if (chunk.empty()) throw Error("JSN server returned an empty chunk");
            output.write(reinterpret_cast<const char*>(chunk.data()),
                         static_cast<std::streamsize>(chunk.size()));
            if (!output) throw Error("cannot write temporary JSN file");
            downloaded += static_cast<std::uint32_t>(chunk.size());
            const int percent = static_cast<int>(downloaded * 100ULL / info.size);
            if (percent >= last_percent + 10 || downloaded == info.size) {
                std::cerr << '\r' << remote_path << ": " << downloaded << '/' << info.size
                          << " (" << percent << "%)" << std::flush;
                last_percent = percent;
            }
        }
        output.close();
        std::cerr << '\n';
        if (downloaded != info.size) throw Error("downloaded JSN length mismatch");
        const auto actual_md5 = md5_file(temporary);
        if (info.has_md5 && actual_md5 != info.md5)
            throw Error("downloaded JSN MD5 mismatch: expected " + info.md5 +
                        ", got " + actual_md5);
        replace_file(temporary, destination);
    } catch (...) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        throw;
    }
}

Json result_json(const std::string& resource, const std::string& remote,
                 const FileInfo* info, const QuoteConnection& connection,
                 const fs::path* output, bool missing) {
    Json result = Json::object();
    result["resource"] = resource;
    result["remote_path"] = remote;
    result["endpoint"] = connection.endpoint().address();
    result["server"] = connection.server_name();
    if (missing) {
        result["status"] = "missing";
    } else if (info) {
        result["size"] = static_cast<std::uint64_t>(info->size);
        result["md5"] = info->md5;
        result["has_md5"] = info->has_md5;
        if (output) result["output"] = path_utf8(*output);
    }
    return result;
}

template <typename Operation>
Json retry_jsn_endpoint_selection(const QuoteEndpointSelection& selection,
                                  Operation&& operation) {
    if (selection.endpoints.empty())
        throw Error("JSN endpoint selection is empty");
    const auto maximum_attempts = static_cast<int>(
        selection.endpoints.size()) * attempts_per_endpoint;
    int started_attempts = 0;
    std::vector<std::string> failures;
    return detail::retry_jsn_rows_fetch([&]() {
        const auto endpoint_index = static_cast<std::size_t>(
            started_attempts / attempts_per_endpoint);
        const auto& endpoint = selection.endpoints.at(endpoint_index);
        ++started_attempts;
        try {
            auto result = operation(endpoint);
            const auto endpoints_attempted = static_cast<int>(endpoint_index + 1);
            const auto transport = public_quote_transport_document(
                selection, started_attempts,
                started_attempts - endpoints_attempted, endpoints_attempted,
                attempts_per_endpoint);
            const auto annotate = [&](Json& document) {
                if (!document.is_object())
                    throw Error("JSN endpoint operation returned a non-object document");
                document["attempts"] = started_attempts;
                document["transport"] = transport;
            };
            if (result.is_array()) {
                for (auto& document : result.as_array()) annotate(document);
            } else {
                annotate(result);
            }
            return result;
        } catch (const MissingResource&) {
            throw;
        } catch (const std::exception& error) {
            failures.push_back(endpoint.address() + " attempt " +
                std::to_string((started_attempts - 1) % attempts_per_endpoint + 1) +
                ": " + error.what());
            if (started_attempts == maximum_attempts) {
                std::string detail = "all JSN 7709 endpoints failed";
                for (const auto& failure : failures) detail += "\n  " + failure;
                throw Error(detail);
            }
            throw Error(error.what());
        }
    }, maximum_attempts, 250);
}

Json transfer_jsn_resource_with_selection(
    const std::string& resource, const std::string& prefix,
    const fs::path& destination, bool download, bool skip_missing,
    int timeout_ms, const QuoteEndpointSelection& endpoint_selection) {
    const auto remote = remote_path_for(resource, prefix);
    return retry_jsn_endpoint_selection(endpoint_selection, [&](const Endpoint& endpoint) {
        QuoteConnection connection(endpoint, timeout_ms);
        try {
            const auto info = query_info(connection, remote);
            if (download) download_file(connection, remote, destination, info);
            return result_json(resource, remote, &info, connection,
                               download ? &destination : nullptr, false);
        } catch (const MissingResource&) {
            if (!skip_missing) throw;
            return result_json(resource, remote, nullptr, connection, nullptr, true);
        }
    });
}

void print_help() {
    std::cout <<
        "Usage: tdx-tool jsn download --resource FILE [--resource FILE ...] ACTION [options]\n\n"
        "Native 7709 commands 709/1721; no Python runtime is used.\n\n"
        "Actions:\n"
        "  --probe                 Query remote size and MD5 only\n"
        "  --download              Download, verify, and atomically replace output\n\n"
        "Options:\n"
        "  --host HOST[:PORT]      Repeatable; default connect.cfg HQHOST primary-first\n"
        "  --root PATH             TDX installation root; otherwise auto-discovered\n"
        "  --prefix bi|bib|bi_diy  Remote namespace (default bi)\n"
        "  --timeout-ms N          Socket timeout (default 10000)\n"
        "  --output-dir PATH       Download root (default output/tdx-jsn-native)\n"
        "  --output PATH           Output path for one resource\n"
        "  --report FILE          Atomically write a reusable probe/download manifest\n"
        "  --skip-missing          Record zero-length resources as missing\n"
        "  --json                  Always print a JSON array\n";
}

}  // namespace

Json detail::retry_jsn_rows_fetch(const JsnFetchAttempt& attempt,
                                  int max_attempts,
                                  int retry_delay_ms) {
    if (!attempt) throw Error("JSN fetch attempt is empty");
    if (max_attempts < 1 || max_attempts > 10)
        throw Error("JSN max attempts must be in 1..10");
    if (retry_delay_ms < 0 || retry_delay_ms > 10000)
        throw Error("JSN retry delay must be in 0..10000 milliseconds");
    for (int current = 1; current <= max_attempts; ++current) {
        try {
            return attempt();
        } catch (const MissingResource&) {
            throw;
        } catch (const Error&) {
            if (current == max_attempts) throw;
            if (retry_delay_ms)
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    retry_delay_ms * current));
        }
    }
    throw Error("JSN fetch retry loop ended unexpectedly");
}

Json detail::resilient_jsn_rows_fetch(const std::vector<std::string>& cache_keys,
                                      const JsnFetchAttempt& attempt,
                                      int max_attempts,
                                      int retry_delay_ms) {
    if (cache_keys.empty()) throw Error("JSN resilient fetch requires cache keys");
    int attempts = 0;
    try {
        auto documents = retry_jsn_rows_fetch([&]() {
            ++attempts;
            return attempt();
        }, max_attempts, retry_delay_ms);
        if (!documents.is_array() || documents.size() != cache_keys.size())
            throw Error("JSN resilient fetch returned an unexpected document count");

        const auto now = std::time(nullptr);
        std::vector<std::string> serialized_documents;
        serialized_documents.reserve(documents.size());
        for (auto& document : documents.as_array()) {
            document["attempts"] = attempts;
            document["stale"] = false;
            document["age_seconds"] = 0;
            document["upstream_error"] = Json(nullptr);
            serialized_documents.push_back(document.dump(-1));
        }

        std::set<std::string> protected_keys(cache_keys.begin(), cache_keys.end());
        std::lock_guard<std::mutex> lock(shared_rows_cache_mutex);
        for (std::size_t index = 0; index < cache_keys.size(); ++index) {
            const auto bytes = static_cast<std::uint64_t>(
                serialized_documents[index].size());
            if (bytes > shared_rows_cache_max_bytes) continue;
            const auto old = shared_rows_cache.find(cache_keys[index]);
            if (old != shared_rows_cache.end()) {
                shared_rows_cache_bytes -= old->second.bytes;
                shared_rows_cache.erase(old);
            }
            evict_shared_rows_cache(bytes, protected_keys);
            if (shared_rows_cache.size() >= shared_rows_cache_max_entries ||
                shared_rows_cache_bytes + bytes > shared_rows_cache_max_bytes) continue;
            shared_rows_cache[cache_keys[index]] = {
                std::make_shared<const std::string>(
                    std::move(serialized_documents[index])),
                now, ++shared_rows_cache_sequence, bytes};
            shared_rows_cache_bytes += bytes;
        }
        return documents;
    } catch (const MissingResource&) {
        throw;
    } catch (const Error& error) {
        const auto now = std::time(nullptr);
        std::vector<std::shared_ptr<const std::string>> snapshots;
        std::vector<std::uint64_t> ages;
        {
            std::lock_guard<std::mutex> lock(shared_rows_cache_mutex);
            for (const auto& key : cache_keys)
                if (shared_rows_cache.find(key) == shared_rows_cache.end()) throw;
            snapshots.reserve(cache_keys.size());
            ages.reserve(cache_keys.size());
            for (const auto& key : cache_keys) {
                auto& entry = shared_rows_cache.at(key);
                entry.sequence = ++shared_rows_cache_sequence;
                snapshots.push_back(entry.serialized);
                ages.push_back(static_cast<std::uint64_t>(
                    std::max<std::time_t>(0, now - entry.fetched_at)));
            }
        }
        Json documents = Json::array();
        for (std::size_t index = 0; index < snapshots.size(); ++index) {
            auto document = Json::parse(*snapshots[index]);
            document["attempts"] = attempts;
            document["stale"] = true;
            document["age_seconds"] = ages[index];
            document["upstream_error"] = error.what();
            documents.push_back(std::move(document));
        }
        return documents;
    }
}

Json jsn_source_metadata(const Json& document) {
    if (!document.is_object()) throw Error("JSN source metadata requires an object");
    Json result = Json::object();
    for (const auto* key : {"resource", "size", "row_count", "endpoint"})
        result[key] = document.at(key);
    for (const auto* key : {"attempts", "stale", "age_seconds", "upstream_error"}) {
        const auto found = document.as_object().find(key);
        if (found != document.as_object().end()) result[key] = found->second;
    }
    return result;
}

Json jsn_sources_health(const Json& sources) {
    if (!sources.is_array()) throw Error("JSN sources health requires an array");
    bool stale = false;
    std::uint64_t live_count = 0, stale_count = 0, max_attempts = 0, oldest_age = 0;
    Json errors = Json::array();
    for (const auto& source : sources.as_array()) {
        if (!source.is_object()) throw Error("JSN source health item must be an object");
        const auto stale_field = source.as_object().find("stale");
        const bool source_stale = stale_field != source.as_object().end() &&
            stale_field->second.is_bool() && stale_field->second.as_bool();
        stale = stale || source_stale;
        if (source_stale) ++stale_count;
        else ++live_count;
        const auto attempts = source.as_object().find("attempts");
        if (attempts != source.as_object().end() && attempts->second.is_number())
            max_attempts = std::max(max_attempts,
                static_cast<std::uint64_t>(attempts->second.as_number()));
        const auto age = source.as_object().find("age_seconds");
        if (age != source.as_object().end() && age->second.is_number())
            oldest_age = std::max(oldest_age,
                static_cast<std::uint64_t>(age->second.as_number()));
        const auto upstream = source.as_object().find("upstream_error");
        if (source_stale && upstream != source.as_object().end() &&
            upstream->second.is_string()) {
            Json item = Json::object();
            const auto resource = source.as_object().find("resource");
            item["resource"] = resource == source.as_object().end()
                ? Json(nullptr) : resource->second;
            item["message"] = upstream->second;
            errors.push_back(std::move(item));
        }
    }
    Json health = Json::object();
    health["stale"] = stale;
    health["live_sources"] = live_count;
    health["stale_sources"] = stale_count;
    health["max_attempts"] = max_attempts;
    health["oldest_age_seconds"] = oldest_age;
    health["upstream_errors"] = std::move(errors);
    return health;
}

Json transfer_jsn_resource(const std::string& resource_value,
                           const std::string& prefix,
                           const fs::path& output_root,
                           bool download,
                           int timeout_ms,
                           const fs::path& root,
                           const std::vector<std::string>& hosts) {
    if (timeout_ms < 1 || timeout_ms > 600000)
        throw Error("timeout_ms must be in 1..600000");
    const auto resource = normalize_resource(resource_value);
    const auto destination = output_root / path_from_utf8(resource);
    const auto endpoint_selection = select_public_quote_endpoints(root, hosts);
    return transfer_jsn_resource_with_selection(
        resource, prefix, destination, download, false, timeout_ms,
        endpoint_selection);
}

Json fetch_jsn_resource_rows(const std::string& resource_value,
                             const std::string& prefix,
                             int timeout_ms,
                             const fs::path& root,
                             const std::vector<std::string>& hosts) {
    auto documents = fetch_jsn_resources_rows(
        {resource_value}, prefix, timeout_ms, root, hosts);
    return documents.as_array().front();
}

Json fetch_jsn_resources_rows(const std::vector<std::string>& resource_values,
                              const std::string& prefix,
                              int timeout_ms,
                              const fs::path& root,
                              const std::vector<std::string>& hosts) {
    if (timeout_ms < 1 || timeout_ms > 600000)
        throw Error("timeout_ms must be in 1..600000");
    if (resource_values.empty()) throw Error("at least one JSN resource is required");
    std::vector<std::pair<std::string, std::string>> resources;
    for (const auto& resource_value : resource_values) {
        const auto resource = normalize_resource(resource_value);
        const auto remote = remote_path_for(resource, prefix);
        resources.emplace_back(resource, remote);
    }
    std::vector<std::string> cache_keys;
    for (const auto& item : resources) cache_keys.push_back(prefix + "/" + item.first);
    const auto endpoint_selection = select_public_quote_endpoints(root, hosts);
    const auto maximum_attempts = static_cast<int>(
        endpoint_selection.endpoints.size()) * attempts_per_endpoint;
    int started_attempts = 0;
    std::vector<std::string> failures;
    return detail::resilient_jsn_rows_fetch(cache_keys, [&]() {
        const auto endpoint_index = static_cast<std::size_t>(
            started_attempts / attempts_per_endpoint);
        const auto& endpoint = endpoint_selection.endpoints.at(endpoint_index);
        ++started_attempts;
        try {
            QuoteConnection connection(endpoint, timeout_ms);
            Json results = Json::array();
            for (const auto& [resource, remote] : resources) {
                const auto info = query_info(connection, remote);
                const auto payload = download_memory(connection, remote, info);
                const auto document = Json::parse(decode_gbk(payload));
                if (!document.is_array())
                    throw Error("JSN root is not an array: " + resource);

                Json rows = Json::array();
                std::size_t group_index = 0;
                for (const auto& group : document.as_array()) {
                    if (!group.is_object())
                        throw Error("JSN result group is not an object");
                    const auto& headers = group.at("colheader");
                    const auto& data = group.at("data");
                    if (!headers.is_array() || !data.is_array())
                        throw Error("JSN result group lacks colheader/data arrays");
                    for (const auto& row : data.as_array()) {
                        if (!row.is_array() || row.size() != headers.size())
                            throw Error("JSN row width differs from colheader: " + resource);
                        Json object = Json::object();
                        object["_group"] = static_cast<std::uint64_t>(group_index);
                        for (std::size_t column = 0; column < headers.size(); ++column)
                            object[jsn_scalar_text(headers.as_array()[column])] =
                                row.as_array()[column];
                        rows.push_back(std::move(object));
                    }
                    ++group_index;
                }
                Json result = result_json(
                    resource, remote, &info, connection, nullptr, false);
                result["row_count"] = static_cast<std::uint64_t>(rows.size());
                result["rows"] = std::move(rows);
                results.push_back(std::move(result));
            }
            const auto endpoints_attempted = static_cast<int>(endpoint_index + 1);
            const auto transport = public_quote_transport_document(
                endpoint_selection, started_attempts,
                started_attempts - endpoints_attempted, endpoints_attempted,
                attempts_per_endpoint);
            for (auto& result : results.as_array()) result["transport"] = transport;
            return results;
        } catch (const MissingResource&) {
            throw;
        } catch (const std::exception& error) {
            failures.push_back(endpoint.address() + " attempt " +
                std::to_string((started_attempts - 1) % attempts_per_endpoint + 1) +
                ": " + error.what());
            if (started_attempts == maximum_attempts) {
                std::string detail = "all JSN 7709 endpoints failed";
                for (const auto& failure : failures) detail += "\n  " + failure;
                throw Error(detail);
            }
            throw Error(error.what());
        }
    }, maximum_attempts, 250);
}

int command_jsn_download(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        print_help();
        return 0;
    }
    const auto requested_resources = args.take_options("--resource");
    const bool probe = args.take_flag("--probe");
    const bool download = args.take_flag("--download");
    const bool skip_missing = args.take_flag("--skip-missing");
    const bool force_json = args.take_flag("--json");
    const auto hosts = args.take_options("--host");
    const auto root_text = args.take_option("--root", "");
    const auto prefix = args.take_option("--prefix", "bi");
    const auto timeout_text = args.take_option("--timeout-ms", "10000");
    const auto output_dir_text = args.take_option("--output-dir", "output/tdx-jsn-native");
    const auto explicit_output_text = args.take_option("--output", "");
    const auto report_text = args.take_option("--report", "");
    args.require_empty();

    if (requested_resources.empty()) throw Error("at least one --resource is required");
    if (probe == download) throw Error("choose exactly one of --probe and --download");
    int timeout_ms = 0;
    try {
        std::size_t used = 0;
        timeout_ms = std::stoi(timeout_text, &used);
        if (used != timeout_text.size() || timeout_ms < 1) throw std::invalid_argument("timeout");
    } catch (...) {
        throw Error("--timeout-ms must be a positive integer");
    }
    if (!explicit_output_text.empty() && requested_resources.size() != 1)
        throw Error("--output requires exactly one resource");

    std::vector<std::string> resources;
    std::set<std::string> seen;
    for (const auto& value : requested_resources) {
        auto normalized = normalize_resource(value);
        if (seen.insert(normalized).second) resources.push_back(std::move(normalized));
    }

    const auto root = root_text.empty()
        ? fs::path{} : find_tdx_root(path_from_utf8(root_text));
    const auto endpoint_selection = select_public_quote_endpoints(root, hosts);
    const auto output_dir = path_from_utf8(output_dir_text);
    const auto explicit_output = explicit_output_text.empty()
        ? fs::path{} : path_from_utf8(explicit_output_text);
    Json results = Json::array();
    for (const auto& resource : resources) {
        const auto destination = explicit_output.empty()
            ? output_dir / path_from_utf8(resource) : explicit_output;
        results.push_back(transfer_jsn_resource_with_selection(
            resource, prefix, destination, download, skip_missing,
            timeout_ms, endpoint_selection));
    }

    if (!report_text.empty()) {
        std::uint64_t available = 0, missing = 0, bytes = 0;
        for (const auto& item : results.as_array()) {
            const auto status = item.as_object().find("status");
            if (status != item.as_object().end() && status->second.is_string() &&
                status->second.as_string() == "missing") {
                ++missing;
                continue;
            }
            ++available;
            const auto size = item.as_object().find("size");
            if (size != item.as_object().end() && size->second.is_number())
                bytes += static_cast<std::uint64_t>(size->second.as_number());
        }
        Json manifest = Json::object();
        manifest["schema"] = "tdx-jsn-remote-manifest-native-v1";
        manifest["action"] = probe ? "probe" : "download";
        manifest["prefix"] = prefix;
        Json summary = Json::object();
        summary["resource_count"] = static_cast<std::uint64_t>(results.size());
        summary["available_count"] = available;
        summary["missing_count"] = missing;
        summary["total_bytes"] = bytes;
        manifest["summary"] = std::move(summary);
        manifest["resources"] = results;
        atomic_write_text(path_from_utf8(report_text), manifest.dump(2) + "\n");
    }

    if (force_json || results.size() != 1 || skip_missing) {
        std::cout << results.dump(2) << '\n';
    } else {
        const auto& item = results.as_array().front();
        std::cout << item.at("resource").as_string() << ": "
                  << static_cast<std::uint64_t>(item.at("size").as_number())
                  << " bytes, md5=" << item.at("md5").as_string()
                  << ", server=" << item.at("endpoint").as_string();
        if (download) std::cout << ", output=" << item.at("output").as_string();
        std::cout << '\n';
    }
    return 0;
}

}  // namespace tdx
