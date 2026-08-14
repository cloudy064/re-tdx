#pragma once

#include "tdx/common.hpp"

#include <string_view>

namespace tdx {

enum class BlowfishWordOrder {
    big_endian,
    little_endian,
};

Bytes blowfish_ecb_encrypt(
    Bytes plaintext, std::string_view key,
    BlowfishWordOrder word_order = BlowfishWordOrder::big_endian);

Bytes blowfish_ecb_decrypt(
    Bytes ciphertext, std::string_view key,
    BlowfishWordOrder word_order = BlowfishWordOrder::big_endian);

}  // namespace tdx
