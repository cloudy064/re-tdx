#include "tdx/image_data.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <string>

#include <zlib.h>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

void field(tdx::Bytes& output, const std::string& tag, const std::string& value,
           bool record_end = false) {
    require(tag.size() == 2, "test field tag");
    output.push_back(3);
    output.insert(output.end(), tag.begin(), tag.end());
    output.insert(output.end(), value.begin(), value.end());
    output.push_back(record_end ? 4 : 2);
}

void write_u32(tdx::Bytes& output, std::size_t offset, std::uint32_t value) {
    std::memcpy(output.data() + offset, &value, sizeof(value));
}

tdx::Bytes wrapped(const tdx::Bytes& payload) {
    uLongf capacity = compressBound(static_cast<uLong>(payload.size()));
    tdx::Bytes compressed(capacity);
    require(compress2(compressed.data(), &capacity, payload.data(),
                      static_cast<uLong>(payload.size()), Z_BEST_COMPRESSION) == Z_OK,
            "test zlib compression");
    compressed.resize(capacity);
    tdx::Bytes result(24 + compressed.size(), 0);
    write_u32(result, 8, static_cast<std::uint32_t>(compressed.size()));
    write_u32(result, 16, static_cast<std::uint32_t>(payload.size()));
    std::memcpy(result.data() + 24, compressed.data(), compressed.size());
    return result;
}

}  // namespace

int main() {
    try {
        tdx::Bytes payload;
        field(payload, "0T", "93000");
        field(payload, "04", "10.00");
        field(payload, "05", "10.10");
        field(payload, "06", "10.50");
        field(payload, "07", "9.90");
        field(payload, "08", "10.20");
        field(payload, "09", "123");
        field(payload, "10", "1000");
        field(payload, "1A", "10200.5");
        field(payload, "1G", "10.18");
        field(payload, "1H", "7000");
        field(payload, "1I", "10.22");
        field(payload, "1J", "8000");
        field(payload, "20", "10.19");
        field(payload, "30", "500");
        field(payload, "40", "10.21");
        field(payload, "50", "600");
        field(payload, "60", "1");
        field(payload, "62", "2");
        field(payload, "64", "100");
        field(payload, "64", "200");
        field(payload, "61", "1");
        field(payload, "62", "1");
        field(payload, "64", "300");
        field(payload, "ZZ", "future-field");
        field(payload, "0D", "normal", true);

        field(payload, "0T", "93001");
        field(payload, "08", "10.30");
        field(payload, "10", "1300", true);

        field(payload, "0T", "93002", true);

        const auto decoded = tdx::decode_image_data(payload, "raw");
        require(decoded.record_delimiters == 3, "record delimiters");
        require(decoded.adjacent_duplicates_removed == 1, "adjacent duplicate removal");
        require(decoded.records.size() == 2, "decoded record count");
        const auto& first = decoded.records[0];
        const auto& second = decoded.records[1];
        require(first.time_hhmmss == 93000, "first time");
        require(std::abs(first.previous_close - 10.0) < 1e-12, "previous close");
        require(std::abs(first.last - 10.2) < 1e-12, "last price");
        require(first.bid_volumes[0] == 500 && first.ask_volumes[0] == 600,
                "level volumes");
        require(first.bid_queue_count == 2 && first.bid_queue[1] == 200,
                "bid queue");
        require(first.ask_queue_count == 1 && first.ask_queue[0] == 300,
                "ask queue");
        require(std::abs(first.average_bid_price - 10.18) < 1e-12 &&
                    first.total_bid_volume == 7000 &&
                    std::abs(first.average_ask_price - 10.22) < 1e-12 &&
                    first.total_ask_volume == 8000,
                "aggregate order book");
        require(first.unknown_fields.at("ZZ")[0] == "future-field", "unknown field");
        require(second.time_hhmmss == 93001 && second.cumulative_volume == 1300,
                "stateful second record");
        require(second.volume_delta == 300, "volume delta");
        require(std::abs(second.open - 10.1) < 1e-12, "state inheritance");

        const auto binary = tdx::encode_image_data_records(decoded.records);
        require(binary.size() == 2064, "normalized binary size");
        require(tdx::read_u32_le(binary.data()) == 93000, "binary time offset");
        require(tdx::read_u32_le(binary.data() + 456) == 2, "binary bid queue count");
        require(tdx::read_u32_le(binary.data() + 464) == 100, "binary bid queue value");
        require(tdx::read_u32_le(binary.data() + 1032 + 64) == 300,
                "binary volume delta low word");

        const auto wrapped_decoded = tdx::decode_image_data(wrapped(payload), "auto");
        require(wrapped_decoded.input_format == "wrapped-zlib", "wrapper auto detection");
        require(wrapped_decoded.payload_bytes == payload.size(), "wrapper decoded size");
        require(wrapped_decoded.records.size() == 2, "wrapper records");

        const auto document = tdx::image_data_document(wrapped_decoded, 1);
        require(document.at("schema").as_string() == "tdx-image-data-snapshots-v3",
                "document schema");
        require(document.at("truncated").as_bool(), "document truncation");
        require(document.at("records").as_array()[0].at("time").as_string() == "09:30:00",
                "document time");
        require(document.at("records").as_array()[0].at("aggregate_order_book")
                    .at("total_ask_volume").as_number() == 8000,
                "document aggregate order book");
        require(document.at("consumer_audit").at("fn_tgetimagedata_consumer_count")
                    .as_number() == 6,
                "consumer audit count");
        require(document.at("consumer_audit").at("unconsumed_wire_tags")
                    .as_array().size() == 5,
                "unconsumed wire tags");
        require(document.at("consumer_audit").at("unconsumed_wire_tags")
                    .as_array()[4].as_string() == "0D",
                "unconsumed text tag");
        require(document.at("consumer_audit").at("status").as_string() ==
                    "preserved_unconsumed_in_current_build",
                "consumer audit status");
        std::cout << "Image-data snapshot tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
