#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tdx {

struct MarketL1SessionOptions {
    std::vector<std::string> endpoints;
    std::string endpoint_source;
    std::size_t available_endpoint_count{};
    bool primary_configured{};
    int timeout_ms{10000};
    int batch_size{80};
};

// Reuses one public 7709 session across repeated 0x0547 requests.  The depth
// response already contains the L1 quote, so one request supplies both the
// workbench header and the five-level order book.
class MarketL1Session {
public:
    MarketL1Session(const std::filesystem::path& root, const BlockData* blocks = nullptr,
                    MarketL1SessionOptions options = {});
    ~MarketL1Session();
    MarketL1Session(const MarketL1Session&) = delete;
    MarketL1Session& operator=(const MarketL1Session&) = delete;

    Json poll_depth(const std::vector<std::string>& securities);
    Json status() const;
    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct MarketStreamOptions {
    int interval_ms{1000};
    int heartbeat_ms{15000};
    int max_backoff_ms{30000};
    int off_session_interval_ms{15000};
    bool market_hours_throttle{true};
    std::size_t subscriber_queue_limit{32};
};

using MarketStreamFetcher =
    std::function<Json(const std::vector<std::string>& securities)>;

// A single worker polls the union of active securities and fans out changed
// records.  Subscribers have bounded queues, so a slow browser cannot grow the
// server without limit or delay other subscribers.
class MarketStreamHub {
public:
    MarketStreamHub(const std::filesystem::path& root, const BlockData& blocks,
                    MarketStreamOptions options = {}, MarketStreamFetcher fetcher = {});
    ~MarketStreamHub();
    MarketStreamHub(const MarketStreamHub&) = delete;
    MarketStreamHub& operator=(const MarketStreamHub&) = delete;

    std::uint64_t subscribe(const std::string& market, const std::string& code);
    bool next(std::uint64_t subscriber_id, Json& event, int timeout_ms = 30000);
    void unsubscribe(std::uint64_t subscriber_id);
    Json status() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

std::string format_market_stream_sse_event(const Json& event);

}  // namespace tdx
