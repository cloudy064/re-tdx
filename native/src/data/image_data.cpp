#include "tdx/image_data.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string_view>

#include <zlib.h>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr std::size_t record_size = 1032;
constexpr std::size_t wrapper_header_size = 24;
constexpr std::size_t maximum_decoded_size = 512U * 1024U * 1024U;

template <typename T>
void write_scalar(Bytes& output, std::size_t offset, const T& value) {
    if (offset + sizeof(T) > output.size()) throw Error("image-data record write overflow");
    std::memcpy(output.data() + offset, &value, sizeof(T));
}

std::string decoded_text(const std::string& value) {
    if (std::all_of(value.begin(), value.end(), [](unsigned char ch) {
            return ch == '\t' || (ch >= 0x20 && ch <= 0x7E);
        })) return value;
    try {
        return decode_gbk(Bytes(value.begin(), value.end()));
    } catch (...) {
        return value;
    }
}

double parse_double(std::string_view value, std::string_view tag) {
    if (value.empty()) return 0.0;
    const std::string text(value);
    char* end = nullptr;
    errno = 0;
    const double result = std::strtod(text.c_str(), &end);
    if (errno == ERANGE || end != text.c_str() + text.size() || !std::isfinite(result))
        throw Error("image-data field " + std::string(tag) + " is not a finite number");
    return result;
}

std::uint64_t parse_u64(std::string_view value, std::string_view tag) {
    if (value.empty()) return 0;
    if (value.front() == '-') throw Error("image-data field " + std::string(tag) + " is negative");
    const std::string text(value);
    char* end = nullptr;
    errno = 0;
    const auto result = std::strtoull(text.c_str(), &end, 10);
    if (errno == ERANGE || end != text.c_str() + text.size())
        throw Error("image-data field " + std::string(tag) + " is not an unsigned integer");
    return static_cast<std::uint64_t>(result);
}

std::uint32_t parse_u32(std::string_view value, std::string_view tag) {
    const auto parsed = parse_u64(value, tag);
    if (parsed > std::numeric_limits<std::uint32_t>::max())
        throw Error("image-data field " + std::string(tag) + " exceeds uint32");
    return static_cast<std::uint32_t>(parsed);
}

bool looks_like_zlib(const std::uint8_t* data, std::size_t size) {
    if (size < 2 || (data[0] & 0x0F) != 8) return false;
    return (static_cast<unsigned>(data[0]) * 256U + data[1]) % 31U == 0;
}

bool plausible_wrapper(const Bytes& input) {
    if (input.size() < wrapper_header_size + 2) return false;
    const auto compressed = static_cast<std::size_t>(read_u32_le(input.data() + 8));
    const auto decoded = static_cast<std::size_t>(read_u32_le(input.data() + 16));
    return compressed > 0 && decoded > 0 && decoded <= maximum_decoded_size &&
           compressed <= input.size() - wrapper_header_size &&
           looks_like_zlib(input.data() + wrapper_header_size, compressed);
}

Bytes inflate_wrapper(const Bytes& input, std::size_t& compressed,
                      std::size_t& declared_decoded) {
    if (input.size() < wrapper_header_size) throw Error("image-data wrapper is shorter than 24 bytes");
    compressed = read_u32_le(input.data() + 8);
    declared_decoded = read_u32_le(input.data() + 16);
    if (!compressed || compressed > input.size() - wrapper_header_size)
        throw Error("image-data wrapper compressed size is outside the file");
    if (!declared_decoded || declared_decoded > maximum_decoded_size)
        throw Error("image-data wrapper decoded size is outside the safe range");
    Bytes result(declared_decoded);
    uLongf actual = static_cast<uLongf>(result.size());
    const int status = uncompress(result.data(), &actual, input.data() + wrapper_header_size,
                                  static_cast<uLong>(compressed));
    if (status != Z_OK)
        throw Error("cannot inflate image-data wrapper (zlib status " + std::to_string(status) + ")");
    if (actual != declared_decoded)
        throw Error("image-data wrapper decoded length does not match its header");
    return result;
}

void apply_field(ImageDataRecord& record, std::string_view tag, std::string_view value,
                 bool& bid_queue_selected, std::size_t& queue_cursor) {
    if (tag == "0T") record.time_hhmmss = parse_u32(value, tag);
    else if (tag == "04") record.previous_close = parse_double(value, tag);
    else if (tag == "05") record.open = parse_double(value, tag);
    else if (tag == "06") record.high = parse_double(value, tag);
    else if (tag == "07") record.low = parse_double(value, tag);
    else if (tag == "08") record.last = parse_double(value, tag);
    else if (tag == "09") record.open_interest = parse_u32(value, tag);
    else if (tag == "10") record.cumulative_volume = parse_u64(value, tag);
    else if (tag == "1A") record.cumulative_amount = parse_double(value, tag);
    else if (tag == "1C") record.auxiliary_1c = parse_double(value, tag);
    else if (tag == "1D") record.auxiliary_1d = parse_double(value, tag);
    else if (tag == "1E") record.auxiliary_1e = parse_double(value, tag);
    else if (tag == "1F") record.auxiliary_1f = parse_double(value, tag);
    else if (tag == "1G") record.average_bid_price = parse_double(value, tag);
    else if (tag == "1H") record.total_bid_volume = parse_u64(value, tag);
    else if (tag == "1I") record.average_ask_price = parse_double(value, tag);
    else if (tag == "1J") record.total_ask_volume = parse_u64(value, tag);
    else if (tag == "0D") record.text_0d = decoded_text(std::string(value));
    else if (tag.size() == 2 && tag[0] >= '2' && tag[0] <= '5' &&
             tag[1] >= '0' && tag[1] <= '9') {
        const auto level = static_cast<std::size_t>(tag[1] - '0');
        if (tag[0] == '2') record.bid_prices[level] = parse_double(value, tag);
        else if (tag[0] == '3') record.bid_volumes[level] = parse_u64(value, tag);
        else if (tag[0] == '4') record.ask_prices[level] = parse_double(value, tag);
        else record.ask_volumes[level] = static_cast<std::uint64_t>(parse_double(value, tag));
    } else if (tag == "60" || tag == "61") {
        bid_queue_selected = tag == "60";
        queue_cursor = 0;
        if (parse_double(value, tag) <= 0.0) {
            if (bid_queue_selected) {
                record.bid_queue_count = 0;
                record.bid_queue.fill(0);
            } else {
                record.ask_queue_count = 0;
                record.ask_queue.fill(0);
            }
        }
    } else if (tag == "62") {
        if (bid_queue_selected) record.bid_queue_count = parse_u32(value, tag);
        else record.ask_queue_count = parse_u32(value, tag);
    } else if (tag == "63") {
        if (bid_queue_selected) record.bid_queue.fill(0);
        else record.ask_queue.fill(0);
    } else if (tag == "64") {
        if (queue_cursor < 50) {
            const auto quantity = parse_u32(value, tag);
            if (bid_queue_selected) record.bid_queue[queue_cursor] = quantity;
            else record.ask_queue[queue_cursor] = quantity;
            ++queue_cursor;
        }
    } else {
        record.unknown_fields[std::string(tag)].push_back(decoded_text(std::string(value)));
    }
}

bool same_payload(const ImageDataRecord& left, const ImageDataRecord& right) {
    return left.previous_close == right.previous_close && left.open == right.open &&
           left.high == right.high && left.low == right.low && left.last == right.last &&
           left.auxiliary_1c == right.auxiliary_1c && left.auxiliary_1d == right.auxiliary_1d &&
           left.open_interest == right.open_interest &&
           left.cumulative_volume == right.cumulative_volume &&
           left.cumulative_amount == right.cumulative_amount &&
           left.bid_prices == right.bid_prices && left.bid_volumes == right.bid_volumes &&
           left.ask_prices == right.ask_prices && left.ask_volumes == right.ask_volumes &&
           left.auxiliary_1e == right.auxiliary_1e &&
           left.auxiliary_1f == right.auxiliary_1f &&
           left.average_bid_price == right.average_bid_price &&
           left.total_bid_volume == right.total_bid_volume &&
           left.average_ask_price == right.average_ask_price &&
           left.total_ask_volume == right.total_ask_volume &&
           left.bid_queue_count == right.bid_queue_count &&
           left.ask_queue_count == right.ask_queue_count &&
           left.bid_queue == right.bid_queue && left.ask_queue == right.ask_queue &&
           left.text_0d == right.text_0d;
}

std::vector<ImageDataRecord> parse_payload(const Bytes& payload, std::size_t& delimiters,
                                           std::size_t& timestamp_coalesced,
                                           std::size_t& duplicates_removed) {
    ImageDataRecord state;
    bool bid_queue_selected = true;
    std::size_t queue_cursor = 0;
    std::string field;
    std::vector<ImageDataRecord> coalesced;
    bool saw_field_start = false;
    for (std::size_t offset = 0; offset < payload.size(); ++offset) {
        const auto byte = payload[offset];
        if (byte == 3) {
            field.clear();
            saw_field_start = true;
            continue;
        }
        if (byte != 2 && byte != 4) {
            if (field.size() >= 100)
                throw Error("image-data field exceeds the recovered 100-byte limit at offset " +
                            std::to_string(offset));
            field.push_back(static_cast<char>(byte));
            continue;
        }
        if (field.size() < 2)
            throw Error("image-data field is missing its two-byte tag at offset " +
                        std::to_string(offset));
        const std::string_view tag(field.data(), 2);
        const std::string_view value(field.data() + 2, field.size() - 2);
        apply_field(state, tag, value, bid_queue_selected, queue_cursor);
        field.clear();
        if (byte == 4) {
            ++delimiters;
            if (!coalesced.empty() &&
                (!coalesced.back().time_hhmmss ||
                 coalesced.back().time_hhmmss == state.time_hhmmss)) {
                coalesced.back() = state;
                ++timestamp_coalesced;
            } else {
                coalesced.push_back(state);
            }
        }
    }
    if (!field.empty()) throw Error("image-data payload ends inside a field");
    if (!saw_field_start || !delimiters)
        throw Error("input does not contain the recovered image-data control stream");

    std::vector<ImageDataRecord> records;
    records.reserve(coalesced.size());
    for (auto& record : coalesced) {
        if (!records.empty() && same_payload(records.back(), record)) {
            ++duplicates_removed;
            continue;
        }
        records.push_back(std::move(record));
    }
    for (std::size_t index = 1; index < records.size(); ++index) {
        records[index].volume_delta = records[index].cumulative_volume -
                                      records[index - 1].cumulative_volume;
    }
    return records;
}

std::string time_text(std::uint32_t raw) {
    const auto hour = raw / 10000;
    const auto minute = raw / 100 % 100;
    const auto second = raw % 100;
    if (hour > 23 || minute > 59 || second > 59) return {};
    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << hour << ':'
           << std::setw(2) << minute << ':' << std::setw(2) << second;
    return output.str();
}

Json levels_json(const std::array<double, 10>& prices,
                 const std::array<std::uint64_t, 10>& volumes) {
    Json rows = Json::array();
    for (std::size_t index = 0; index < prices.size(); ++index) {
        Json row = Json::object();
        row["level"] = static_cast<std::uint64_t>(index + 1);
        row["price"] = prices[index];
        row["volume"] = volumes[index];
        rows.push_back(std::move(row));
    }
    return rows;
}

Json queue_json(const std::array<std::uint32_t, 50>& values, std::uint32_t reported) {
    Json rows = Json::array();
    const auto count = std::min<std::size_t>(values.size(), reported);
    for (std::size_t index = 0; index < count; ++index)
        rows.push_back(static_cast<std::uint64_t>(values[index]));
    return rows;
}

Json record_json(const ImageDataRecord& record, std::size_t index) {
    Json row = Json::object();
    row["index"] = static_cast<std::uint64_t>(index);
    row["time_raw"] = static_cast<std::uint64_t>(record.time_hhmmss);
    const auto rendered_time = time_text(record.time_hhmmss);
    row["time"] = rendered_time.empty() ? Json(nullptr) : Json(rendered_time);
    row["previous_close"] = record.previous_close;
    row["open"] = record.open;
    row["high"] = record.high;
    row["low"] = record.low;
    row["last"] = record.last;
    row["open_interest"] = static_cast<std::uint64_t>(record.open_interest);
    row["volume_delta"] = record.volume_delta;
    row["cumulative_volume"] = record.cumulative_volume;
    row["cumulative_amount"] = record.cumulative_amount;
    row["bid_levels"] = levels_json(record.bid_prices, record.bid_volumes);
    row["ask_levels"] = levels_json(record.ask_prices, record.ask_volumes);
    row["bid_queue_count"] = static_cast<std::uint64_t>(record.bid_queue_count);
    row["ask_queue_count"] = static_cast<std::uint64_t>(record.ask_queue_count);
    row["bid_queue"] = queue_json(record.bid_queue, record.bid_queue_count);
    row["ask_queue"] = queue_json(record.ask_queue, record.ask_queue_count);
    row["queue_truncated"] = record.bid_queue_count > 50 || record.ask_queue_count > 50;
    Json auxiliary = Json::object();
    auxiliary["1C"] = record.auxiliary_1c;
    auxiliary["1D"] = record.auxiliary_1d;
    auxiliary["1E"] = record.auxiliary_1e;
    auxiliary["1F"] = record.auxiliary_1f;
    row["auxiliary_fields"] = std::move(auxiliary);
    Json aggregate = Json::object();
    aggregate["average_bid_price"] = record.average_bid_price;
    aggregate["total_bid_volume"] = record.total_bid_volume;
    aggregate["average_ask_price"] = record.average_ask_price;
    aggregate["total_ask_volume"] = record.total_ask_volume;
    row["aggregate_order_book"] = std::move(aggregate);
    row["text_0d"] = record.text_0d;
    Json unknown = Json::object();
    for (const auto& [tag, values] : record.unknown_fields) {
        Json items = Json::array();
        for (const auto& value : values) items.push_back(value);
        unknown[tag] = std::move(items);
    }
    row["unknown_fields"] = std::move(unknown);
    return row;
}

int parse_limit(Args& args) {
    const auto text = args.take_option("--limit", "20");
    try {
        std::size_t consumed = 0;
        const int result = std::stoi(text, &consumed, 10);
        if (consumed != text.size() || result < 0 || result > 100000)
            throw std::invalid_argument("range");
        return result;
    } catch (...) {
        throw Error("--limit must be 0..100000");
    }
}

void help() {
    std::cout <<
        "Usage: tdx-tool image-data decode --input FILE [options]\n\n"
        "Decode TdxW zst_cache *.img/*.TCK wrappers or an already-inflated control stream.\n"
        "The operation is offline and does not load TDataParse.dll.\n\n"
        "Options:\n"
        "  --input-format auto|wrapped|raw  Default: auto\n"
        "  --limit N                       Records in JSON; 0 means all\n"
        "  --record-output FILE            Write normalized 1032-byte records\n"
        "  --output FILE                    Write JSON instead of stdout\n"
        "  --compact                        Compact JSON\n";
}

}  // namespace

ImageDataDecodeResult decode_image_data(Bytes input, const std::string& selected_format) {
    ImageDataDecodeResult result;
    result.input_bytes = input.size();
    auto format = lower_ascii(trim(selected_format));
    if (format.empty()) format = "auto";
    Bytes payload;
    if (format == "wrapped" || (format == "auto" && plausible_wrapper(input))) {
        payload = inflate_wrapper(input, result.compressed_bytes, result.declared_decoded_bytes);
        result.input_format = "wrapped-zlib";
    } else if (format == "raw" || format == "auto") {
        payload = std::move(input);
        result.input_format = "raw-control-stream";
    } else {
        throw Error("--input-format must be auto, wrapped, or raw");
    }
    result.payload_bytes = payload.size();
    result.records = parse_payload(payload, result.record_delimiters,
                                   result.timestamp_coalesced_records,
                                   result.adjacent_duplicates_removed);
    return result;
}

Bytes encode_image_data_records(const std::vector<ImageDataRecord>& records) {
    Bytes output(records.size() * record_size, 0);
    for (std::size_t index = 0; index < records.size(); ++index) {
        const auto& record = records[index];
        const auto base = index * record_size;
        write_scalar(output, base + 0, record.time_hhmmss);
        write_scalar(output, base + 4, record.previous_close);
        write_scalar(output, base + 12, record.open);
        write_scalar(output, base + 20, record.high);
        write_scalar(output, base + 28, record.low);
        write_scalar(output, base + 36, record.last);
        write_scalar(output, base + 44, record.auxiliary_1c);
        write_scalar(output, base + 52, record.auxiliary_1d);
        write_scalar(output, base + 60, record.open_interest);
        write_scalar(output, base + 64, record.volume_delta);
        write_scalar(output, base + 72, record.cumulative_volume);
        write_scalar(output, base + 80, record.cumulative_amount);
        for (std::size_t level = 0; level < 10; ++level) {
            write_scalar(output, base + 88 + level * 8, record.bid_prices[level]);
            write_scalar(output, base + 168 + level * 8, record.bid_volumes[level]);
            write_scalar(output, base + 248 + level * 8, record.ask_prices[level]);
            write_scalar(output, base + 328 + level * 8, record.ask_volumes[level]);
        }
        write_scalar(output, base + 408, record.auxiliary_1e);
        write_scalar(output, base + 416, record.auxiliary_1f);
        write_scalar(output, base + 424, record.average_bid_price);
        write_scalar(output, base + 432, record.average_ask_price);
        write_scalar(output, base + 440, record.total_bid_volume);
        write_scalar(output, base + 448, record.total_ask_volume);
        write_scalar(output, base + 456, record.bid_queue_count);
        write_scalar(output, base + 460, record.ask_queue_count);
        for (std::size_t queue = 0; queue < 50; ++queue) {
            write_scalar(output, base + 464 + queue * 4, record.bid_queue[queue]);
            write_scalar(output, base + 664 + queue * 4, record.ask_queue[queue]);
        }
        const auto text_size = std::min<std::size_t>(167, record.text_0d.size());
        std::memcpy(output.data() + base + 864, record.text_0d.data(), text_size);
    }
    return output;
}

Json image_data_document(const ImageDataDecodeResult& decoded, int limit) {
    if (limit < 0 || limit > 100000) throw Error("image-data limit is outside the safe range");
    Json result = Json::object();
    result["schema"] = "tdx-image-data-snapshots-v3";
    result["offline"] = true;
    result["source_dll_loaded"] = false;
    result["input_format"] = decoded.input_format;
    result["input_bytes"] = static_cast<std::uint64_t>(decoded.input_bytes);
    result["payload_bytes"] = static_cast<std::uint64_t>(decoded.payload_bytes);
    result["compressed_bytes"] = static_cast<std::uint64_t>(decoded.compressed_bytes);
    result["declared_decoded_bytes"] = static_cast<std::uint64_t>(decoded.declared_decoded_bytes);
    result["normalized_record_size"] = static_cast<std::uint64_t>(record_size);
    result["record_delimiters"] = static_cast<std::uint64_t>(decoded.record_delimiters);
    result["timestamp_coalesced_records"] =
        static_cast<std::uint64_t>(decoded.timestamp_coalesced_records);
    result["adjacent_duplicates_removed"] =
        static_cast<std::uint64_t>(decoded.adjacent_duplicates_removed);
    result["record_count"] = static_cast<std::uint64_t>(decoded.records.size());
    const auto returned = limit == 0 ? decoded.records.size()
                                     : std::min<std::size_t>(decoded.records.size(), limit);
    result["records_returned"] = static_cast<std::uint64_t>(returned);
    result["truncated"] = returned < decoded.records.size();
    Json evidence = Json::object();
    evidence["field_start"] = 3;
    evidence["field_end"] = 2;
    evidence["record_end"] = 4;
    evidence["wire_tags"] = "0T,04..09,10,1A,1C..1J,20..29,30..39,40..49,50..59,60..64,0D";
    evidence["aggregate_order_book_labels"] =
        "TdxW UI labels at 0xCA1330..0xCA1348 prove 1G/1H=买均/总买 and 1I/1J=卖均/总卖";
    evidence["semantic_boundary"] =
        "04..08, 20..59 and 1G..1J are named from TdxW consumers; all six current fn_TGetImageData consumers were audited and none reads 1C..1F or the returned 0D text span, so those fields remain wire-tagged without invented business names";
    result["recovery_evidence"] = std::move(evidence);
    Json consumer_audit = Json::object();
    consumer_audit["binary"] = "TdxW.exe";
    consumer_audit["fn_tgetimagedata_loader"] = "0x7218F0";
    consumer_audit["fn_tgetimagedata_consumer_count"] = 6;
    Json consumers = Json::array();
    for (const auto* address : {"0x59DA50", "0x5B4DD0", "0xA19E10",
                                "0xA26C90", "0xA637E0", "0xA85600"})
        consumers.push_back(address);
    consumer_audit["consumer_entry_points"] = std::move(consumers);
    Json unconsumed = Json::array();
    for (const auto* tag : {"1C", "1D", "1E", "1F", "0D"}) unconsumed.push_back(tag);
    consumer_audit["unconsumed_wire_tags"] = std::move(unconsumed);
    consumer_audit["status"] = "preserved_unconsumed_in_current_build";
    consumer_audit["scope"] =
        "static audit of every direct TdxW reference to the resolved fn_TGetImageData pointer; the loader reference is excluded from the consumer count";
    consumer_audit["parser_internal_effects"] =
        "0D still participates in parser state inheritance and adjacent-record equality before records are returned";
    result["consumer_audit"] = std::move(consumer_audit);
    Json records = Json::array();
    for (std::size_t index = 0; index < returned; ++index)
        records.push_back(record_json(decoded.records[index], index));
    result["records"] = std::move(records);
    return result;
}

int command_image_data_decode(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        help();
        return 0;
    }
    const auto input = args.take_option("--input");
    if (input.empty()) throw Error("image-data decode requires --input");
    const auto input_format = args.take_option("--input-format", "auto");
    const int limit = parse_limit(args);
    const auto record_output = args.take_option("--record-output");
    const auto output = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto decoded = decode_image_data(read_bytes(fs::u8path(input)), input_format);
    if (!record_output.empty())
        atomic_write_bytes(fs::u8path(record_output), encode_image_data_records(decoded.records));
    const auto report = image_data_document(decoded, limit).dump(compact ? -1 : 2) + "\n";
    if (output.empty()) std::cout << report;
    else atomic_write_text(fs::u8path(output), report);
    return 0;
}

}  // namespace tdx
