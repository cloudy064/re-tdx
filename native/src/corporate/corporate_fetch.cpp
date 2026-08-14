#include "corporate_internal.hpp"

#include "tdx/session_audit.hpp"

#include <algorithm>
#include <cstdint>

namespace tdx {

using namespace corporate_detail;
Json fetch_finance_document(const std::vector<std::string>& securities,
                            const std::vector<Endpoint>& endpoints, int timeout_ms,
                            int batch_size, bool include_raw) {
    const auto codes = parse_securities(securities);
    if (batch_size < 1 || batch_size > 1000) throw Error("batch size must be in 1..1000");
    const auto candidates = effective_endpoints(endpoints);
    std::vector<std::string> failures;
    int connection_attempts = 0, transient_retries = 0, endpoints_attempted = 0;
    for (const auto& endpoint : candidates) {
        ++endpoints_attempted;
        int attempts = 0;
        try {
            auto document = detail::retry_quote_transport([&] {
                QuoteConnection connection(endpoint, timeout_ms);
                Json records = Json::array();
                for (std::size_t begin = 0; begin < codes.size();
                     begin += static_cast<std::size_t>(batch_size)) {
                    const auto end = std::min(
                        codes.size(), begin + static_cast<std::size_t>(batch_size));
                    const std::vector<SecurityCode> batch(
                        codes.begin() + static_cast<std::ptrdiff_t>(begin),
                        codes.begin() + static_cast<std::ptrdiff_t>(end));
                    const auto parsed = parse_finance_batch_payload(
                        connection.call(type_finance, finance_request(batch)).data,
                        include_raw);
                    for (const auto& record : parsed.at("records").as_array())
                        records.push_back(record);
                }
                Json result = Json::object();
                result["schema"] = "tdx-market-finance-native-v1";
                result["generated_at"] = now_text();
                result["command"] = "0x0010";
                result["endpoint"] = endpoint.address();
                result["server_name"] = connection.server_name();
                result["requested"] = static_cast<std::uint64_t>(codes.size());
                result["received"] = static_cast<std::uint64_t>(records.size());
                result["records"] = std::move(records);
                return result;
            }, attempts);
            connection_attempts += attempts;
            transient_retries += std::max(0, attempts - 1);
            document["transport"] = quote_transport_metadata(
                connection_attempts, transient_retries, endpoints_attempted,
                candidates);
            return document;
        } catch (const std::exception& error) {
            connection_attempts += attempts;
            transient_retries += std::max(0, attempts - 1);
            failures.push_back(endpoint.address() + ": " + error.what());
        }
    }
    throw Error("all finance endpoints failed: " + (failures.empty() ? std::string("none")
        : failures.front()));
}

Json fetch_capital_changes_document(const std::vector<std::string>& securities,
                                    const std::vector<Endpoint>& endpoints, int timeout_ms,
                                    bool include_raw) {
    const auto codes = parse_securities(securities);
    const auto candidates = effective_endpoints(endpoints);
    std::vector<std::string> failures;
    int connection_attempts = 0, transient_retries = 0, endpoints_attempted = 0;
    for (const auto& endpoint : candidates) {
        ++endpoints_attempted;
        int attempts = 0;
        try {
            auto document = detail::retry_quote_transport([&] {
                QuoteConnection connection(endpoint, timeout_ms);
                Json blocks = Json::array();
                std::uint64_t event_count = 0;
                for (const auto& code : codes) {
                    auto block = parse_capital_changes_payload(
                        connection.call(type_capital_changes,
                                        capital_request(code)).data,
                        include_raw);
                    event_count += static_cast<std::uint64_t>(
                        block.at("count").as_number());
                    blocks.push_back(std::move(block));
                }
                Json result = Json::object();
                result["schema"] = "tdx-market-capital-native-v1";
                result["generated_at"] = now_text();
                result["command"] = "0x000F";
                result["endpoint"] = endpoint.address();
                result["server_name"] = connection.server_name();
                result["requested"] = static_cast<std::uint64_t>(codes.size());
                result["received"] = static_cast<std::uint64_t>(blocks.size());
                result["event_count"] = event_count;
                result["blocks"] = std::move(blocks);
                return result;
            }, attempts);
            connection_attempts += attempts;
            transient_retries += std::max(0, attempts - 1);
            document["transport"] = quote_transport_metadata(
                connection_attempts, transient_retries, endpoints_attempted,
                candidates);
            return document;
        } catch (const std::exception& error) {
            connection_attempts += attempts;
            transient_retries += std::max(0, attempts - 1);
            failures.push_back(endpoint.address() + ": " + error.what());
        }
    }
    throw Error("all capital-change endpoints failed: " +
                (failures.empty() ? std::string("none") : failures.front()));
}

Json fetch_special_limits_document(const std::vector<Endpoint>& endpoints, int timeout_ms,
                                   int start_index, int max_rows, bool include_raw) {
    if (start_index < 0 || start_index > 0xFFFF)
        throw Error("start index must be in 0..65535");
    if (max_rows < 1 || max_rows > 65536) throw Error("max rows must be in 1..65536");
    const auto candidates = effective_endpoints(endpoints);
    std::vector<std::string> failures;
    int connection_attempts = 0, transient_retries = 0, endpoints_attempted = 0;
    for (const auto& endpoint : candidates) {
        ++endpoints_attempted;
        int attempts = 0;
        try {
            auto document = detail::retry_quote_transport([&] {
                QuoteConnection connection(endpoint, timeout_ms);
                Json records = Json::array();
                int cursor = start_index;
                int page_count = 0;
                bool complete = false;
                while (records.size() < static_cast<std::size_t>(max_rows) &&
                       cursor <= 0xFFFF) {
                    const auto page = parse_special_limits_payload(
                        connection.call(type_special_limits,
                                        limits_request(cursor)).data,
                        cursor, include_raw);
                    ++page_count;
                    const auto& page_records = page.at("records").as_array();
                    if (page_records.empty()) {
                        complete = true;
                        break;
                    }
                    const auto room = static_cast<std::size_t>(max_rows) - records.size();
                    const auto used = std::min(room, page_records.size());
                    for (std::size_t index = 0; index < used; ++index)
                        records.push_back(page_records[index]);
                    if (used < page_records.size()) break;
                    cursor += static_cast<int>(page_records.size());
                }
                Json result = Json::object();
                result["schema"] = "tdx-market-special-limits-native-v1";
                result["generated_at"] = now_text();
                result["command"] = "0x0452";
                result["endpoint"] = endpoint.address();
                result["server_name"] = connection.server_name();
                result["start_index"] = start_index;
                result["page_count"] = page_count;
                result["complete"] = complete;
                result["count"] = static_cast<std::uint64_t>(records.size());
                result["records"] = std::move(records);
                return result;
            }, attempts);
            connection_attempts += attempts;
            transient_retries += std::max(0, attempts - 1);
            document["transport"] = quote_transport_metadata(
                connection_attempts, transient_retries, endpoints_attempted,
                candidates);
            return document;
        } catch (const std::exception& error) {
            connection_attempts += attempts;
            transient_retries += std::max(0, attempts - 1);
            failures.push_back(endpoint.address() + ": " + error.what());
        }
    }
    throw Error("all special-limit endpoints failed: " +
                (failures.empty() ? std::string("none") : failures.front()));
}

}  // namespace tdx