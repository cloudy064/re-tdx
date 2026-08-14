#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace tdx {

int market_price_divisor_for_code(std::string_view code);

Json fetch_market_snapshot_document(const std::filesystem::path& root,
                                    const std::vector<std::string>& securities,
                                    int timeout_ms = 10000,
                                    const BlockData* block_data = nullptr);
// Decode/fetch the public 0x053E quote surface used by TdxW for the native
// rise-speed field. The response also contains five-level L1 order-book data.
Json decode_market_speed_payload(const Bytes& payload,
                                 std::size_t requested_count);
// Decode the XOR-obfuscated public 0x0547 response for an explicit security
// list.  Exposed alongside the speed decoder so protocol fixtures can verify
// special index records without opening a network connection.
Json decode_market_depth_payload(const Bytes& payload,
                                 const std::vector<std::string>& securities);
Json fetch_market_speed_document(const std::filesystem::path& root,
                                 const std::vector<std::string>& securities,
                                 int timeout_ms = 10000,
                                 const BlockData* block_data = nullptr);
Json fetch_market_depth_document(const std::filesystem::path& root,
                                 const std::vector<std::string>& securities,
                                 int timeout_ms = 10000,
                                 const BlockData* block_data = nullptr);
int command_market_snapshot(const std::vector<std::string>& args);
int command_market_speed(const std::vector<std::string>& args);
int command_market_depth(const std::vector<std::string>& args);
int command_market_watch(const std::vector<std::string>& args);

}  // namespace tdx
