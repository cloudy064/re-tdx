#include "corporate_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/hk_actions.hpp"
#include "tdx/minute.hpp"
#include "tdx/security_identity.hpp"
#include "tdx/session_audit.hpp"

#include <chrono>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace fs = std::filesystem;

namespace tdx::corporate_detail {
struct AdjustmentInputs {
    Json daily;
    Json capital;
};

struct AdjustmentCacheEntry {
    std::shared_ptr<const AdjustmentInputs> value;
    std::chrono::steady_clock::time_point expires_at{};
    std::shared_future<std::shared_ptr<const AdjustmentInputs>> pending;
    std::uint64_t last_use{};
    std::uint64_t generation{};
};

struct AdjustmentCacheLoad {
    std::shared_ptr<const AdjustmentInputs> inputs;
    std::string status;
};

class AdjustmentInputCache {
public:
    AdjustmentCacheLoad load(const std::string& key,
                              const std::string& market,
                              const std::string& code,
                              const fs::path& root,
                              const std::vector<std::string>& hosts,
                              int timeout_ms,
                              int ttl_seconds,
                              bool refresh) {
        if (ttl_seconds < 0 || ttl_seconds > 86400)
            throw Error("adjustment cache TTL must be in 0..86400 seconds");
        if (timeout_ms < 1) throw Error("adjustment timeout must be positive");
        if (ttl_seconds == 0) {
            auto inputs = fetch(market, code, root, hosts, timeout_ms);
            std::lock_guard<std::mutex> lock(mutex_);
            ++misses_;
            return {std::move(inputs), "disabled"};
        }

        std::shared_future<std::shared_ptr<const AdjustmentInputs>> pending;
        std::shared_ptr<std::promise<std::shared_ptr<const AdjustmentInputs>>> producer;
        std::uint64_t producer_generation = 0;
        bool capacity_bypass = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto now = std::chrono::steady_clock::now();
            auto found = entries_.find(key);
            if (!refresh && found != entries_.end() && found->second.value &&
                found->second.expires_at > now) {
                found->second.last_use = ++sequence_;
                ++hits_;
                return {found->second.value, "hit"};
            }
            if (!refresh && found != entries_.end() && found->second.pending.valid()) {
                pending = found->second.pending;
                ++coalesced_;
            } else {
                if (found == entries_.end() &&
                    entries_.size() >= maximum_entries_ &&
                    !evict_one_locked({})) {
                    capacity_bypass = true;
                    ++capacity_bypasses_;
                    ++misses_;
                } else {
                    producer = std::make_shared<
                        std::promise<std::shared_ptr<const AdjustmentInputs>>>();
                    pending = producer->get_future().share();
                    auto& entry = entries_[key];
                    entry.pending = pending;
                    entry.last_use = ++sequence_;
                    entry.generation = producer_generation = sequence_;
                    if (refresh) ++refreshes_;
                    else ++misses_;
                }
            }
        }
        if (capacity_bypass)
            return {fetch(market, code, root, hosts, timeout_ms),
                    "capacity-bypass"};
        if (!producer)
            return {pending.get(), "coalesced"};

        try {
            auto inputs = fetch(market, code, root, hosts, timeout_ms);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto found = entries_.find(key);
                if (found != entries_.end() &&
                    found->second.generation == producer_generation) {
                    auto& entry = found->second;
                    entry.value = inputs;
                    entry.expires_at = std::chrono::steady_clock::now() +
                        std::chrono::seconds(ttl_seconds);
                    entry.pending = {};
                    entry.last_use = ++sequence_;
                    evict_locked(key);
                }
            }
            producer->set_value(inputs);
            return {std::move(inputs), refresh ? "refresh" : "miss"};
        } catch (...) {
            const auto failure = std::current_exception();
            producer->set_exception(failure);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                const auto found = entries_.find(key);
                if (found != entries_.end() &&
                    found->second.generation == producer_generation)
                    entries_.erase(found);
                ++failures_;
            }
            std::rethrow_exception(failure);
        }
    }

    Json document() const {
        std::lock_guard<std::mutex> lock(mutex_);
        Json result = Json::object();
        result["schema"] = "tdx-kline-adjustment-input-cache-v1";
        result["entry_count"] = static_cast<std::uint64_t>(entries_.size());
        result["maximum_entries"] = static_cast<std::uint64_t>(maximum_entries_);
        result["hits"] = hits_;
        result["misses"] = misses_;
        result["coalesced"] = coalesced_;
        result["refreshes"] = refreshes_;
        result["failures"] = failures_;
        result["evictions"] = evictions_;
        result["capacity_bypasses"] = capacity_bypasses_;
        return result;
    }

private:
    static std::shared_ptr<const AdjustmentInputs> fetch(
        const std::string& market,
        const std::string& code,
        const fs::path& root,
        const std::vector<std::string>& hosts,
        int timeout_ms) {
        const auto selection = select_public_quote_endpoints(root, hosts);
        auto daily_future = std::async(std::launch::async, [&] {
            return fetch_kline_document(market, code, "stock", "day", 20, 800,
                                        0, "all", timeout_ms, root, hosts);
        });
        auto capital_future = std::async(std::launch::async, [&] {
            return fetch_capital_changes_document(
                {market + ":" + code}, selection.endpoints, timeout_ms, false);
        });
        auto inputs = std::make_shared<AdjustmentInputs>();
        inputs->daily = daily_future.get();
        inputs->capital = capital_future.get();
        return inputs;
    }

    bool evict_one_locked(const std::string& protected_key) {
        auto victim = entries_.end();
        for (auto iterator = entries_.begin(); iterator != entries_.end(); ++iterator) {
            if (iterator->first == protected_key || iterator->second.pending.valid())
                continue;
            if (victim == entries_.end() ||
                iterator->second.last_use < victim->second.last_use)
                victim = iterator;
        }
        if (victim == entries_.end()) return false;
        entries_.erase(victim);
        ++evictions_;
        return true;
    }

    void evict_locked(const std::string& protected_key) {
        while (entries_.size() > maximum_entries_ &&
               evict_one_locked(protected_key)) {}
    }

    static constexpr std::size_t maximum_entries_ = 512;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, AdjustmentCacheEntry> entries_;
    std::uint64_t sequence_{};
    std::uint64_t hits_{};
    std::uint64_t misses_{};
    std::uint64_t coalesced_{};
    std::uint64_t refreshes_{};
    std::uint64_t failures_{};
    std::uint64_t evictions_{};
    std::uint64_t capacity_bypasses_{};
};

AdjustmentInputCache& adjustment_input_cache() {
    static AdjustmentInputCache cache;
    return cache;
}

std::string adjustment_cache_key(const fs::path& root,
                                 const std::vector<std::string>& hosts,
                                 const std::string& market,
                                 const std::string& code) {
    std::string result = lower_ascii(trim(market)) + ":" + trim(code) +
        "|root=" + path_utf8(root);
    for (const auto& host : hosts)
        result += "|host=" + lower_ascii(trim(host));
    return result;
}

}  // namespace tdx::corporate_detail

namespace tdx {

using namespace corporate_detail;
Json adjust_security_kline_document(
    Json document,
    const std::string& requested_market,
    const std::string& code,
    const std::string& kind,
    const std::string& requested_mode,
    const std::string& anchor_date,
    const fs::path& root,
    const std::vector<std::string>& hosts,
    int timeout_ms,
    int cache_ttl_seconds,
    bool refresh_cache) {
    const auto mode = normalize_kline_adjustment_mode(requested_mode);
    auto market = lower_ascii(trim(requested_market));
    if (market == "0") market = "sz";
    else if (market == "1") market = "sh";
    else if (market == "2") market = "bj";

    const auto* existing_value = object_value(document, "adjustment_mode");
    std::string existing_mode;
    if (existing_value) {
        if (existing_value->is_string())
            existing_mode = normalize_kline_adjustment_mode(
                existing_value->as_string());
        else if (existing_value->is_number()) {
            const auto numeric = existing_value->as_number();
            if (numeric == 0) existing_mode = "none";
            else if (numeric == 1) existing_mode = "qfq";
            else if (numeric == 2) existing_mode = "hfq";
            else throw Error("K-line adjustment_mode must be 0, 1, or 2");
        } else {
            throw Error("K-line adjustment_mode must be a string or integer");
        }
    }
    if (mode == "none") {
        if (!existing_value) document["adjustment_mode"] = "none";
        return document;
    }
    if (!existing_mode.empty() && existing_mode != "none") {
        if (existing_mode != mode)
            throw Error("K-line document is already adjusted as " + existing_mode +
                        "; refusing to apply " + mode + " again");
        auto* adjustment = document.is_object() &&
                document.as_object().count("adjustment") &&
                document.as_object().at("adjustment").is_object()
                ? &document.as_object().at("adjustment") : nullptr;
        if (mode == "fixed_qfq" || mode == "fixed_hfq") {
            const auto requested_anchor = normalized_date(anchor_date, true);
            const auto* existing_anchor = adjustment
                ? object_value(*adjustment, "anchor_date") : nullptr;
            if (!existing_anchor || !existing_anchor->is_string() ||
                normalized_date(existing_anchor->as_string(), true) !=
                    requested_anchor)
                throw Error("fixed adjustment anchor does not match the "
                            "already adjusted K-line document");
        }
        if (adjustment) {
            Json cache = Json::object();
            cache["status"] = "provided-document";
            cache["shared"] = false;
            (*adjustment)["input_cache"] = std::move(cache);
        }
        return document;
    }
    const auto normalized_kind = lower_ascii(trim(kind));
    if (normalized_kind == "index" ||
        (normalized_kind == "auto" && market == "sh" &&
         is_tdx_block_index_code(code)))
        throw Error("corporate-action adjustment is only available for securities");
    if (is_hk_action_market(market))
        return apply_hk_kline_adjustment(
            std::move(document), root, code, mode, anchor_date);
    if (market != "sz" && market != "sh" && market != "bj")
        throw Error("corporate-action adjustment is unavailable for this expansion market");

    const auto loaded = adjustment_input_cache().load(
        adjustment_cache_key(root, hosts, market, code), market, code, root,
        hosts, timeout_ms, cache_ttl_seconds, refresh_cache);
    auto adjusted = apply_kline_adjustment(
        std::move(document), loaded.inputs->daily, loaded.inputs->capital,
        mode, anchor_date);
    Json cache = Json::object();
    cache["status"] = loaded.status;
    cache["shared"] = true;
    cache["ttl_seconds"] = cache_ttl_seconds;
    adjusted["adjustment"]["input_cache"] = std::move(cache);
    return adjusted;
}

Json kline_adjustment_cache_document() {
    return adjustment_input_cache().document();
}

}  // namespace tdx
