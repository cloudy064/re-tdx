#pragma once

#include "tdx/common.hpp"
#include "tdx/json.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct ImageDataRecord {
    std::uint32_t time_hhmmss{};
    double previous_close{};
    double open{};
    double high{};
    double low{};
    double last{};
    double auxiliary_1c{};
    double auxiliary_1d{};
    std::uint32_t open_interest{};
    std::uint64_t volume_delta{};
    std::uint64_t cumulative_volume{};
    double cumulative_amount{};
    std::array<double, 10> bid_prices{};
    std::array<std::uint64_t, 10> bid_volumes{};
    std::array<double, 10> ask_prices{};
    std::array<std::uint64_t, 10> ask_volumes{};
    double auxiliary_1e{};
    double auxiliary_1f{};
    double average_bid_price{};
    std::uint64_t total_bid_volume{};
    double average_ask_price{};
    std::uint64_t total_ask_volume{};
    std::uint32_t bid_queue_count{};
    std::uint32_t ask_queue_count{};
    std::array<std::uint32_t, 50> bid_queue{};
    std::array<std::uint32_t, 50> ask_queue{};
    std::string text_0d;
    std::map<std::string, std::vector<std::string>, std::less<>> unknown_fields;
};

struct ImageDataDecodeResult {
    std::string input_format;
    std::size_t input_bytes{};
    std::size_t payload_bytes{};
    std::size_t compressed_bytes{};
    std::size_t declared_decoded_bytes{};
    std::size_t record_delimiters{};
    std::size_t timestamp_coalesced_records{};
    std::size_t adjacent_duplicates_removed{};
    std::vector<ImageDataRecord> records;
};

ImageDataDecodeResult decode_image_data(Bytes input, const std::string& input_format = "auto");
Bytes encode_image_data_records(const std::vector<ImageDataRecord>& records);
Json image_data_document(const ImageDataDecodeResult& decoded, int limit = 20);
int command_image_data_decode(const std::vector<std::string>& args);

}  // namespace tdx
