#include "hk_resource_crypto_internal.hpp"

#include "tdx/blowfish.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

namespace tdx::hk_resource_crypto_detail {
namespace {

constexpr std::string_view kKey = "SECURE20090531_TDXSS";

}  // namespace

Bytes decrypt_resource(Bytes encrypted) {
    const auto complete_size = encrypted.size() - encrypted.size() % 8;
    Bytes complete(encrypted.begin(),
                   encrypted.begin() + static_cast<std::ptrdiff_t>(complete_size));
    complete = blowfish_ecb_decrypt(
        std::move(complete), kKey, BlowfishWordOrder::little_endian);
    std::copy(complete.begin(), complete.end(), encrypted.begin());
    return encrypted;
}

}  // namespace tdx::hk_resource_crypto_detail
