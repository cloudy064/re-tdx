#include "tdx/market_stream.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <condition_variable>
#include <ctime>
#include <deque>
#include <iomanip>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>

namespace tdx {
namespace {

using Clock = std::chrono::steady_clock;

std::string now_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%dT%H:%M:%S");
    return output.str();
}

std::string security_key(std::string market, const std::string& code) {
    market = lower_ascii(trim(std::move(market)));
    if (market == "0") market = "sz";
    else if (market == "1") market = "sh";
    else if (market == "2" || market == "44") market = "bj";
    if (market != "sz" && market != "sh" && market != "bj")
        throw Error("market stream market must be sz, sh, or bj");
    if (code.size() != 6 || !std::all_of(code.begin(), code.end(), [](char value) {
            return value >= '0' && value <= '9';
        })) throw Error("market stream code must contain exactly six digits");
    return market + ':' + code;
}

std::string security_id(const std::string& key) {
    const auto colon = key.find(':');
    auto market = key.substr(0, colon);
    std::transform(market.begin(), market.end(), market.begin(), [](unsigned char value) {
        return static_cast<char>(std::toupper(value));
    });
    return market + key.substr(colon + 1);
}

std::string key_from_record(const Json& record) {
    const auto market = static_cast<int>(record.at("market_id").as_number());
    const auto& code = record.at("code").as_string();
    if (market == 0) return "sz:" + code;
    if (market == 1) return "sh:" + code;
    if (market == 2 || market == 44) return "bj:" + code;
    return {};
}

Json boundary_document() {
    Json result = Json::object();
    result["fast_hq_subscribe_used"] = false;
    result["upstream_mode"] = "public 7709 bounded polling";
    result["downstream_mode"] = "local SSE push";
    result["reason"] =
        "FastHQ.Subscribe requires an authenticated tpbus/TaApi CTAJob_InetTQL session";
    return result;
}

Json base_event(const std::string& type, const std::string& key) {
    Json event = Json::object();
    event["schema"] = "tdx-market-l1-stream-event-v1";
    event["schema_version"] = 1;
    event["generated_at"] = now_text();
    event["type"] = type;
    event["security"] = key;
    event["security_id"] = security_id(key);
    event["transport"] = "public-7709-L1-persistent-polling";
    event["delivery"] = "local-sse";
    event["command"] = "0x0547";
    event["fast_hq_boundary"] = boundary_document();
    return event;
}

bool exchange_session_active() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    if (local.tm_wday == 0 || local.tm_wday == 6) return false;
    const int minute = local.tm_hour * 60 + local.tm_min;
    return (minute >= 9 * 60 + 15 && minute <= 11 * 60 + 35) ||
           (minute >= 12 * 60 + 55 && minute <= 15 * 60 + 35);
}

}  // namespace

struct MarketStreamHub::Impl {
    struct Subscriber {
        std::string key;
        std::deque<Json> queue;
        std::uint64_t dropped{};
    };
    struct SecurityState {
        Json record{nullptr};
        Json source{nullptr};
        std::string signature;
        Clock::time_point last_emit{};
        bool force_snapshot{true};
    };

    MarketStreamOptions options;
    MarketStreamFetcher fetcher;
    mutable std::mutex mutex;
    std::condition_variable changed;
    std::map<std::uint64_t, Subscriber> subscribers;
    std::map<std::string, SecurityState, std::less<>> securities;
    std::thread worker;
    bool stopping{};
    std::uint64_t next_subscriber_id{1};
    std::uint64_t next_sequence{1};
    std::uint64_t subscription_version{};
    std::uint64_t poll_count{};
    std::uint64_t successful_polls{};
    std::uint64_t failed_polls{};
    int consecutive_failures{};
    std::string last_error;
    std::string last_success_at;
    Json last_source{nullptr};

    Impl(const std::filesystem::path& root, const BlockData& blocks,
         MarketStreamOptions selected, MarketStreamFetcher selected_fetcher)
        : options(selected), fetcher(std::move(selected_fetcher)) {
        if (options.interval_ms < 1 || options.interval_ms > 600000)
            throw Error("market stream interval_ms must be in 1..600000");
        if (options.heartbeat_ms < options.interval_ms || options.heartbeat_ms > 600000)
            throw Error("market stream heartbeat_ms must be in interval_ms..600000");
        if (options.max_backoff_ms < options.interval_ms || options.max_backoff_ms > 600000)
            throw Error("market stream max_backoff_ms must be in interval_ms..600000");
        if (options.off_session_interval_ms < options.interval_ms ||
            options.off_session_interval_ms > 600000)
            throw Error("market stream off_session_interval_ms must be in interval_ms..600000");
        if (options.subscriber_queue_limit < 1 || options.subscriber_queue_limit > 4096)
            throw Error("market stream subscriber_queue_limit must be in 1..4096");
        if (!fetcher) {
            auto session = std::make_shared<MarketL1Session>(root, &blocks);
            fetcher = [session](const std::vector<std::string>& values) {
                return session->poll_depth(values);
            };
        }
        worker = std::thread([this] { run(); });
    }

    ~Impl() {
        {
            std::lock_guard<std::mutex> guard(mutex);
            stopping = true;
            changed.notify_all();
        }
        if (worker.joinable()) worker.join();
    }

    void enqueue_locked(Subscriber& subscriber, Json event) {
        event["sequence"] = next_sequence++;
        if (subscriber.queue.size() >= options.subscriber_queue_limit) {
            subscriber.queue.pop_front();
            ++subscriber.dropped;
        }
        subscriber.queue.push_back(std::move(event));
    }

    void publish_locked(const std::string& key, Json event) {
        const auto sequence = next_sequence++;
        event["sequence"] = sequence;
        for (auto& [id, subscriber] : subscribers) {
            (void)id;
            if (subscriber.key != key) continue;
            if (subscriber.queue.size() >= options.subscriber_queue_limit) {
                subscriber.queue.pop_front();
                ++subscriber.dropped;
            }
            subscriber.queue.push_back(event);
        }
        changed.notify_all();
    }

    std::vector<std::string> active_keys_locked() const {
        std::set<std::string> unique;
        for (const auto& [id, subscriber] : subscribers) {
            (void)id;
            unique.insert(subscriber.key);
        }
        return {unique.begin(), unique.end()};
    }

    void publish_success_locked(const Json& document,
                                const std::vector<std::string>& active) {
        std::map<std::string, Json, std::less<>> records;
        const auto& values = document.at("records");
        if (!values.is_array()) throw Error("market stream fetcher returned non-array records");
        for (const auto& record : values.as_array()) {
            const auto key = key_from_record(record);
            if (!key.empty()) records[key] = record;
        }
        Json source = Json::object();
        source["endpoint"] = document.at("endpoint");
        source["server_name"] = document.at("server_name");
        const auto found_session = document.as_object().find("session");
        source["session"] = found_session == document.as_object().end()
            ? Json(nullptr) : found_session->second;
        last_source = source;
        const auto now = Clock::now();
        for (const auto& key : active) {
            auto& state = securities[key];
            const auto found = records.find(key);
            if (found == records.end()) {
                if (!state.record.is_null() || state.force_snapshot) {
                    auto event = base_event("missing", key);
                    event["record"] = Json(nullptr);
                    event["source"] = source;
                    publish_locked(key, std::move(event));
                    state.last_emit = now;
                }
                state.record = Json(nullptr);
                state.signature.clear();
                state.force_snapshot = false;
                continue;
            }
            const auto signature = found->second.dump(-1);
            const bool first = state.signature.empty() || state.force_snapshot;
            const bool changed_record = signature != state.signature;
            const bool heartbeat = state.last_emit.time_since_epoch().count() == 0 ||
                now - state.last_emit >= std::chrono::milliseconds(options.heartbeat_ms);
            if (first || changed_record || heartbeat) {
                auto event = base_event(first ? "snapshot" :
                                        changed_record ? "change" : "heartbeat", key);
                event["record"] = found->second;
                event["source"] = source;
                event["replay"] = false;
                publish_locked(key, std::move(event));
                state.last_emit = now;
            }
            state.record = found->second;
            state.source = source;
            state.signature = signature;
            state.force_snapshot = false;
        }
        consecutive_failures = 0;
        last_error.clear();
        last_success_at = now_text();
        ++successful_polls;
    }

    int publish_failure_locked(const std::vector<std::string>& active,
                               const std::string& message, int base_delay) {
        ++failed_polls;
        ++consecutive_failures;
        last_error = message;
        const auto factor = 1ULL << std::min(consecutive_failures - 1, 20);
        const int delay = static_cast<int>(std::min<std::uint64_t>(
            static_cast<std::uint64_t>(options.max_backoff_ms),
            static_cast<std::uint64_t>(base_delay) * factor));
        for (const auto& key : active) {
            auto& state = securities[key];
            state.force_snapshot = true;
            auto event = base_event("reconnect", key);
            event["record"] = Json(nullptr);
            event["message"] = message;
            event["consecutive_failures"] = consecutive_failures;
            event["next_retry_ms"] = delay;
            publish_locked(key, std::move(event));
        }
        return delay;
    }

    void run() {
        std::unique_lock<std::mutex> lock(mutex);
        while (!stopping) {
            changed.wait(lock, [&] { return stopping || !subscribers.empty(); });
            if (stopping) break;
            const auto version = subscription_version;
            const auto active = active_keys_locked();
            lock.unlock();
            Json document;
            std::string error;
            try { document = fetcher(active); }
            catch (const std::exception& exception) { error = exception.what(); }
            lock.lock();
            if (stopping) break;
            ++poll_count;
            const int base_delay = !options.market_hours_throttle || exchange_session_active()
                ? options.interval_ms : options.off_session_interval_ms;
            int delay = base_delay;
            try {
                if (error.empty()) publish_success_locked(document, active);
                else delay = publish_failure_locked(active, error, base_delay);
            } catch (const std::exception& exception) {
                delay = publish_failure_locked(active, exception.what(), base_delay);
            }
            changed.wait_for(lock, std::chrono::milliseconds(delay), [&] {
                return stopping || subscription_version != version;
            });
        }
    }
};

MarketStreamHub::MarketStreamHub(const std::filesystem::path& root, const BlockData& blocks,
                                 MarketStreamOptions options, MarketStreamFetcher fetcher)
    : impl_(std::make_unique<Impl>(root, blocks, options, std::move(fetcher))) {}

MarketStreamHub::~MarketStreamHub() = default;

std::uint64_t MarketStreamHub::subscribe(const std::string& market, const std::string& code) {
    const auto key = security_key(market, code);
    std::lock_guard<std::mutex> guard(impl_->mutex);
    const auto id = impl_->next_subscriber_id++;
    Impl::Subscriber subscriber;
    subscriber.key = key;
    auto [found, inserted] = impl_->subscribers.emplace(id, std::move(subscriber));
    (void)inserted;
    const auto state = impl_->securities.find(key);
    if (state != impl_->securities.end() && !state->second.record.is_null()) {
        auto event = base_event("snapshot", key);
        event["record"] = state->second.record;
        event["source"] = state->second.source;
        event["replay"] = true;
        impl_->enqueue_locked(found->second, std::move(event));
    }
    ++impl_->subscription_version;
    impl_->changed.notify_all();
    return id;
}

bool MarketStreamHub::next(std::uint64_t subscriber_id, Json& event, int timeout_ms) {
    if (timeout_ms < 1 || timeout_ms > 600000)
        throw Error("market stream next timeout_ms must be in 1..600000");
    std::unique_lock<std::mutex> lock(impl_->mutex);
    const auto ready = impl_->changed.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&] {
        const auto found = impl_->subscribers.find(subscriber_id);
        return impl_->stopping || found == impl_->subscribers.end() ||
               !found->second.queue.empty();
    });
    if (!ready || impl_->stopping) return false;
    const auto found = impl_->subscribers.find(subscriber_id);
    if (found == impl_->subscribers.end() || found->second.queue.empty()) return false;
    event = std::move(found->second.queue.front());
    found->second.queue.pop_front();
    Json delivery = Json::object();
    delivery["dropped_events"] = found->second.dropped;
    delivery["queue_remaining"] = static_cast<std::uint64_t>(found->second.queue.size());
    event["delivery_health"] = std::move(delivery);
    found->second.dropped = 0;
    return true;
}

void MarketStreamHub::unsubscribe(std::uint64_t subscriber_id) {
    std::lock_guard<std::mutex> guard(impl_->mutex);
    const auto found = impl_->subscribers.find(subscriber_id);
    if (found == impl_->subscribers.end()) return;
    const auto key = found->second.key;
    impl_->subscribers.erase(found);
    bool still_used = false;
    for (const auto& [id, subscriber] : impl_->subscribers) {
        (void)id;
        if (subscriber.key == key) { still_used = true; break; }
    }
    if (!still_used) impl_->securities.erase(key);
    ++impl_->subscription_version;
    impl_->changed.notify_all();
}

Json MarketStreamHub::status() const {
    std::lock_guard<std::mutex> guard(impl_->mutex);
    Json active = Json::array();
    for (const auto& key : impl_->active_keys_locked()) active.push_back(key);
    Json result = Json::object();
    result["schema"] = "tdx-market-l1-stream-status-v1";
    result["generated_at"] = now_text();
    result["upstream_mode"] = "public-7709-L1-persistent-polling";
    result["downstream_mode"] = "local-sse";
    result["command"] = "0x0547";
    result["subscriber_count"] = static_cast<std::uint64_t>(impl_->subscribers.size());
    result["active_security_count"] = static_cast<std::uint64_t>(active.size());
    result["active_securities"] = std::move(active);
    result["poll_count"] = impl_->poll_count;
    result["successful_polls"] = impl_->successful_polls;
    result["failed_polls"] = impl_->failed_polls;
    result["consecutive_failures"] = impl_->consecutive_failures;
    result["last_error"] = impl_->last_error.empty() ? Json(nullptr) : Json(impl_->last_error);
    result["last_success_at"] = impl_->last_success_at.empty()
        ? Json(nullptr) : Json(impl_->last_success_at);
    result["last_source"] = impl_->last_source;
    result["interval_ms"] = impl_->options.interval_ms;
    result["heartbeat_ms"] = impl_->options.heartbeat_ms;
    result["max_backoff_ms"] = impl_->options.max_backoff_ms;
    result["off_session_interval_ms"] = impl_->options.off_session_interval_ms;
    result["market_hours_throttle"] = impl_->options.market_hours_throttle;
    result["exchange_session_active"] = exchange_session_active();
    result["effective_interval_ms"] =
        !impl_->options.market_hours_throttle || exchange_session_active()
            ? impl_->options.interval_ms : impl_->options.off_session_interval_ms;
    result["subscriber_queue_limit"] =
        static_cast<std::uint64_t>(impl_->options.subscriber_queue_limit);
    result["fast_hq_boundary"] = boundary_document();
    return result;
}

std::string format_market_stream_sse_event(const Json& event) {
    const auto& type = event.at("type").as_string();
    const auto sequence = static_cast<std::uint64_t>(event.at("sequence").as_number());
    std::ostringstream output;
    output << "id: " << sequence << "\n"
           << "event: " << type << "\n"
           << "data: " << event.dump(-1) << "\n\n";
    return output.str();
}

}  // namespace tdx
