#include "market_internal.hpp"

#include "tdx/session_audit.hpp"

#include <algorithm>

namespace tdx::market_detail {
namespace {

template <typename Row, typename Request, typename Parse>
BatchDownload<Row> download_batches(const CommonOptions &selected,
                                    QuoteSurface surface_kind, Request make_request,
                                    Parse parse) {
    std::vector<Row> rows;
    std::size_t batch_index = 0;
    const std::size_t batch_count =
        (selected.codes.size() + selected.batch_size - 1) / selected.batch_size;
    std::vector<std::string> failures;
    std::string endpoint_used;
    std::string server;
    int connection_attempts = 0;
    int transient_retries = 0;
    int endpoints_attempted = 0;
    const auto command = quote_surface(surface_kind).command;
    for (const auto &endpoint : selected.endpoints) {
        if (batch_index >= batch_count)
            break;
        ++endpoints_attempted;
        int attempts = 0;
        try {
            detail::retry_quote_transport(
                [&] {
                    QuoteConnection connection(endpoint, selected.timeout_ms);
                    while (batch_index < batch_count) {
                        const auto begin =
                            batch_index * static_cast<std::size_t>(selected.batch_size);
                        const auto end =
                            std::min(selected.codes.size(),
                                     begin + static_cast<std::size_t>(selected.batch_size));
                        const std::vector<QuoteCode> batch(
                            selected.codes.begin() + static_cast<std::ptrdiff_t>(begin),
                            selected.codes.begin() + static_cast<std::ptrdiff_t>(end));
                        const auto response = connection.call(command, make_request(batch));
                        auto parsed = parse(response.data, batch);
                        rows.insert(rows.end(), parsed.begin(), parsed.end());
                        ++batch_index;
                        endpoint_used = endpoint.address();
                        server = connection.server_name();
                    }
                    return true;
                },
                attempts);
        } catch (const std::exception &error) {
            failures.push_back(endpoint.address() + ": " + error.what());
        }
        connection_attempts += attempts;
        transient_retries += std::max(0, attempts - 1);
    }
    if (batch_index < batch_count) {
        std::string detail = "online quote batches incomplete";
        for (const auto &failure : failures)
            detail += "\n  " + failure;
        throw Error(detail);
    }
    return {std::move(rows), endpoint_used, server, connection_attempts,
            transient_retries, endpoints_attempted};
}

} // namespace

BatchDownload<Snapshot> download_snapshot_batches(const CommonOptions &selected) {
    return download_batches<Snapshot>(
        selected, QuoteSurface::Snapshot, snapshot_request,
        [](const Bytes &payload, const std::vector<QuoteCode> &batch) {
            return parse_snapshots(payload, batch.size());
        });
}

BatchDownload<Speed> download_speed_batches(const CommonOptions &selected) {
    return download_batches<Speed>(
        selected, QuoteSurface::Speed, snapshot_request,
        [](const Bytes &payload, const std::vector<QuoteCode> &batch) {
            return parse_speeds(payload, batch.size());
        });
}

} // namespace tdx::market_detail
