#pragma once

#include "tdx/common.hpp"
#include "tdx/json.hpp"

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace tdx {

struct SecurityDirectoryRecord {
    int market_id{};
    std::string code;
    std::string name;
    std::uint16_t multiple{};
    std::uint8_t decimal{};
    double previous_close_price{};
    double volume_ratio_base{};
    std::string category;
    std::string category_reason;
    std::string board;
    std::string raw_tail_hex;
};

struct SecurityDirectoryQuery {
    std::string market{"all"};
    std::string category{"all"};
    std::string query;
    std::filesystem::path root;
    std::vector<std::string> hosts;
    // Backward-compatible single explicit endpoint. Empty means use hosts or
    // connect.cfg/[HQHOST] primary-first selection.
    std::string endpoint;
    bool refresh{};
    int limit{5000};
    int page_size{1600};
    int cache_ttl_seconds{3600};
    int timeout_ms{15000};
    // Optional caller cancellation/session gate. Empty preserves ordinary
    // initial/manual reads; exceptions abort paging/retries without caching.
    std::function<void()> check;
};

std::vector<SecurityDirectoryRecord> parse_security_directory_page(
    const Bytes& payload, int market_id);
std::string classify_security_directory_record(int market_id,
                                               const std::string& code);

class SecurityDirectoryService {
public:
    Json query(const SecurityDirectoryQuery& options);

private:
    struct MarketCache {
        std::vector<SecurityDirectoryRecord> records;
        std::time_t fetched_at{};
        std::uint64_t reported_count{};
        std::string endpoint;
        std::string server_name;
        Json transport;
    };

    MarketCache fetch_market(int market_id, const SecurityDirectoryQuery& options);
    const MarketCache& ensure_market(int market_id,
                                     const SecurityDirectoryQuery& options);

    std::map<int, MarketCache> caches_;
    std::mutex mutex_;
};

int command_market_securities(const std::vector<std::string>& args);

}  // namespace tdx
