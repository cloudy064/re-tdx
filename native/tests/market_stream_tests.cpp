#include "tdx/market_stream.hpp"

#include "tdx/common.hpp"

#include <atomic>
#include <iostream>
#include <string>

namespace {

void require(bool value, const char* message) {
    if (!value) throw tdx::Error(message);
}

tdx::Json document(double price, const std::vector<std::string>& securities) {
    tdx::Json records = tdx::Json::array();
    for (const auto& security : securities) {
        const bool sh = security.rfind("sh:", 0) == 0;
        const bool bj = security.rfind("bj:", 0) == 0;
        tdx::Json row = tdx::Json::object();
        row["market_id"] = sh ? 1 : bj ? 2 : 0;
        row["code"] = security.substr(3);
        row["security_id"] = std::string(sh ? "SH" : bj ? "BJ" : "SZ") +
                             security.substr(3);
        row["last_price"] = price;
        row["buy_levels"] = tdx::Json::array();
        row["sell_levels"] = tdx::Json::array();
        records.push_back(std::move(row));
    }
    tdx::Json session = tdx::Json::object();
    session["persistent"] = true;
    session["connection_generation"] = 1;
    tdx::Json result = tdx::Json::object();
    result["endpoint"] = "test:7709";
    result["server_name"] = "test";
    result["session"] = std::move(session);
    result["records"] = std::move(records);
    return result;
}

tdx::Json next_type(tdx::MarketStreamHub& hub, std::uint64_t id,
                    const std::string& type, int attempts = 20) {
    for (int index = 0; index < attempts; ++index) {
        tdx::Json event;
        if (hub.next(id, event, 250) && event.at("type").as_string() == type)
            return event;
    }
    throw tdx::Error("market stream did not emit expected event: " + type);
}

}  // namespace

int main() {
    try {
        tdx::BlockData blocks;
        tdx::MarketStreamOptions options;
        options.interval_ms = 10;
        options.heartbeat_ms = 50;
        options.max_backoff_ms = 100;
        options.off_session_interval_ms = 50;
        options.market_hours_throttle = false;
        options.subscriber_queue_limit = 8;

        std::atomic<int> price_cents{1000};
        std::atomic<int> fetches{0};
        tdx::MarketStreamHub hub({}, blocks, options,
            [&](const std::vector<std::string>& securities) {
                ++fetches;
                return document(price_cents.load() / 100.0, securities);
            });
        const auto first_id = hub.subscribe("sz", "000001");
        const auto first = next_type(hub, first_id, "snapshot");
        require(first.at("record").at("last_price").as_number() == 10.0,
                "initial market stream snapshot");
        require(first.at("fast_hq_boundary").at("fast_hq_subscribe_used").as_bool() == false,
                "market stream permission boundary");

        price_cents = 1015;
        const auto changed = next_type(hub, first_id, "change");
        require(changed.at("record").at("last_price").as_number() == 10.15,
                "market stream changed record");

        const auto second_id = hub.subscribe("sz", "000001");
        const auto replay = next_type(hub, second_id, "snapshot");
        require(replay.at("replay").as_bool(), "late subscriber snapshot replay");
        const auto status = hub.status();
        require(status.at("subscriber_count").as_number() == 2 &&
                    status.at("active_security_count").as_number() == 1,
                "duplicate subscribers share one active security");
        hub.unsubscribe(first_id);
        hub.unsubscribe(second_id);

        std::atomic<int> attempts{0};
        tdx::MarketStreamHub reconnecting({}, blocks, options,
            [&](const std::vector<std::string>& securities) {
                if (++attempts == 1) throw tdx::Error("synthetic transport failure");
                return document(12.0, securities);
            });
        const auto reconnect_id = reconnecting.subscribe("sh", "600000");
        const auto reconnect = next_type(reconnecting, reconnect_id, "reconnect");
        require(reconnect.at("next_retry_ms").as_number() == 10,
                "market stream bounded reconnect delay");
        const auto recovered = next_type(reconnecting, reconnect_id, "snapshot");
        require(recovered.at("record").at("security_id").as_string() == "SH600000",
                "market stream recovery snapshot");
        const auto encoded = tdx::format_market_stream_sse_event(recovered);
        require(encoded.find("event: snapshot\n") != std::string::npos &&
                    encoded.find("data: {\"") != std::string::npos &&
                    encoded.size() >= 2 && encoded.substr(encoded.size() - 2) == "\n\n",
                "market stream SSE framing");
        reconnecting.unsubscribe(reconnect_id);

        std::cout << "market stream tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
