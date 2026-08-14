#pragma once

#include "tdx/blowfish.hpp"

#include <array>
#include <cstdint>

namespace tdx::blowfish_detail {

const std::array<std::uint32_t, 1042>& blowfish_initial_state();

// TCalc PriGS/PriDefault payloads use the Blowfish round function with the
// published initial P/S tables directly, without applying a key schedule.
// Keep this non-standard transform private to native protocol decoders.
Bytes blowfish_initial_state_ecb_decrypt(
    Bytes ciphertext,
    BlowfishWordOrder word_order = BlowfishWordOrder::big_endian);

}  // namespace tdx::blowfish_detail
