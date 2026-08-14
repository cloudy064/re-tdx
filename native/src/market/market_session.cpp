#include "market_internal.hpp"

#include <algorithm>
#include <iterator>
#include <memory>

namespace fs = std::filesystem;

namespace tdx {

struct MarketL1Session::Impl {
    std::vector<Endpoint> endpoints;
    int timeout_ms{10000};
    int batch_size{80};
    std::map<std::pair<int, std::string>, Security> names;
    std::unique_ptr<QuoteConnection> connection;
    std::size_t endpoint_index{};
    std::uint64_t connection_generation{};
    std::uint64_t connections_opened{};
    std::uint64_t upstream_requests{};
    std::uint64_t successful_polls{};
    std::string last_error;
    std::string endpoint_source;
    std::size_t available_endpoint_count{};
    bool primary_configured{};

    void connect_current() {
        connection =
            std::make_unique<QuoteConnection>(endpoints.at(endpoint_index), timeout_ms);
        ++connection_generation;
        ++connections_opened;
    }
};

MarketL1Session::MarketL1Session(const fs::path &root, const BlockData *blocks,
                                 MarketL1SessionOptions options)
    : impl_(std::make_unique<Impl>()) {
    if (options.timeout_ms < 100 || options.timeout_ms > 600000)
        throw Error("market L1 session timeout_ms must be in 100..600000");
    if (options.batch_size < 1 || options.batch_size > 1000)
        throw Error("market L1 session batch_size must be in 1..1000");
    impl_->timeout_ms = options.timeout_ms;
    impl_->batch_size = options.batch_size;
    if (options.endpoints.empty()) {
        auto selected = load_public_quote_endpoints(root);
        options.endpoint_source = std::move(selected.source);
        options.available_endpoint_count = selected.available_endpoint_count;
        options.primary_configured = selected.primary_configured;
        for (const auto &endpoint : selected.endpoints)
            options.endpoints.push_back(endpoint.address());
    } else if (options.endpoint_source.empty()) {
        options.endpoint_source = "explicit-host";
        options.available_endpoint_count = options.endpoints.size();
    }
    for (const auto &value : options.endpoints)
        impl_->endpoints.push_back(parse_endpoint(value));
    impl_->endpoint_source = std::move(options.endpoint_source);
    impl_->available_endpoint_count = options.available_endpoint_count;
    impl_->primary_configured = options.primary_configured;
    impl_->names = blocks ? blocks->securities : load_blocks(root, {}).securities;
}

MarketL1Session::~MarketL1Session() = default;

Json MarketL1Session::poll_depth(const std::vector<std::string> &securities) {
    using namespace market_detail;
    if (securities.empty())
        throw Error("market L1 session requires at least one security");
    std::vector<QuoteCode> codes;
    std::set<std::pair<int, std::string>> seen;
    for (const auto &value : securities) {
        auto code = parse_security(value);
        if (seen.insert(code.key()).second)
            codes.push_back(std::move(code));
    }
    const bool had_connection = static_cast<bool>(impl_->connection);
    std::vector<std::string> failures;
    for (std::size_t endpoint_attempt = 0;
         endpoint_attempt < impl_->endpoints.size(); ++endpoint_attempt) {
        try {
            bool opened_during_poll = false;
            if (!impl_->connection) {
                impl_->connect_current();
                opened_during_poll = true;
            }
            std::vector<Depth> depths;
            for (std::size_t begin = 0; begin < codes.size();
                 begin += static_cast<std::size_t>(impl_->batch_size)) {
                const auto end =
                    std::min(codes.size(),
                             begin + static_cast<std::size_t>(impl_->batch_size));
                const std::vector<QuoteCode> batch(
                    codes.begin() + static_cast<std::ptrdiff_t>(begin),
                    codes.begin() + static_cast<std::ptrdiff_t>(end));
                ++impl_->upstream_requests;
                const auto response = impl_->connection->call(
                    quote_surface(QuoteSurface::Depth).command, depth_request(batch));
                auto parsed = parse_depths(response.data, batch);
                depths.insert(depths.end(), std::make_move_iterator(parsed.begin()),
                              std::make_move_iterator(parsed.end()));
            }
            Json records = Json::array();
            for (const auto &item : depths)
                records.push_back(depth_json(item, impl_->names));
            Json session = Json::object();
            session["persistent"] = true;
            session["connection_reused"] = had_connection && !opened_during_poll;
            session["connection_generation"] = impl_->connection_generation;
            session["connections_opened"] = impl_->connections_opened;
            session["upstream_requests"] = impl_->upstream_requests;
            session["successful_polls"] = ++impl_->successful_polls;
            session["last_error"] =
                impl_->last_error.empty() ? Json(nullptr) : Json(impl_->last_error);
            session["endpoint_source"] = impl_->endpoint_source;
            session["available_endpoint_count"] =
                static_cast<std::uint64_t>(impl_->available_endpoint_count);
            session["primary_configured"] = impl_->primary_configured;
            session["endpoint_pool_size"] =
                static_cast<std::uint64_t>(impl_->endpoints.size());
            const auto &surface = quote_surface(QuoteSurface::Depth);
            Json document = Json::object();
            document["schema"] = std::string(surface.schema);
            document["generated_at"] = now_text();
            document["command"] = std::string(surface.command_text);
            document["endpoint"] = impl_->connection->endpoint().address();
            document["server_name"] = impl_->connection->server_name();
            document["requested"] = static_cast<std::uint64_t>(codes.size());
            document["received"] = static_cast<std::uint64_t>(depths.size());
            document["records"] = std::move(records);
            document["session"] = std::move(session);
            impl_->last_error.clear();
            return document;
        } catch (const std::exception &error) {
            impl_->last_error = error.what();
            failures.push_back(impl_->endpoints.at(impl_->endpoint_index).address() +
                               ": " + error.what());
            impl_->connection.reset();
            impl_->endpoint_index =
                (impl_->endpoint_index + 1) % impl_->endpoints.size();
        }
    }
    std::string message = "market L1 depth poll failed";
    for (const auto &failure : failures)
        message += "\n  " + failure;
    throw Error(message);
}

Json MarketL1Session::status() const {
    Json result = Json::object();
    result["persistent"] = true;
    result["connected"] = static_cast<bool>(impl_->connection);
    result["endpoint"] = impl_->connection
                             ? Json(impl_->connection->endpoint().address())
                             : Json(nullptr);
    result["server_name"] = impl_->connection
                                ? Json(impl_->connection->server_name())
                                : Json(nullptr);
    result["connection_generation"] = impl_->connection_generation;
    result["connections_opened"] = impl_->connections_opened;
    result["upstream_requests"] = impl_->upstream_requests;
    result["successful_polls"] = impl_->successful_polls;
    result["last_error"] =
        impl_->last_error.empty() ? Json(nullptr) : Json(impl_->last_error);
    result["endpoint_source"] = impl_->endpoint_source;
    result["available_endpoint_count"] =
        static_cast<std::uint64_t>(impl_->available_endpoint_count);
    result["primary_configured"] = impl_->primary_configured;
    result["endpoint_pool_size"] =
        static_cast<std::uint64_t>(impl_->endpoints.size());
    return result;
}

void MarketL1Session::reset() { impl_->connection.reset(); }

} // namespace tdx
