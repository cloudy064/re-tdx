#include "blowfish_internal.hpp"

#include "tdx/blowfish.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace tdx {
namespace {

class Blowfish {
public:
    Blowfish() : state_(blowfish_detail::blowfish_initial_state()) {}

    explicit Blowfish(std::string_view key)
        : Blowfish() {
        std::size_t key_index = 0;
        for (std::size_t index = 0; index < 18; ++index) {
            std::uint32_t word = 0;
            for (int byte = 0; byte < 4; ++byte) {
                word = (word << 8U) |
                    static_cast<unsigned char>(key[key_index]);
                key_index = (key_index + 1) % key.size();
            }
            state_[index] ^= word;
        }

        std::uint32_t left = 0;
        std::uint32_t right = 0;
        for (std::size_t index = 0; index < state_.size(); index += 2) {
            encrypt(left, right);
            state_[index] = left;
            state_[index + 1] = right;
        }
    }

    void decrypt(std::uint32_t& left, std::uint32_t& right) const {
        for (int index = 17; index >= 2; --index) {
            left ^= state_[static_cast<std::size_t>(index)];
            right ^= round(left);
            std::swap(left, right);
        }
        std::swap(left, right);
        right ^= state_[1];
        left ^= state_[0];
    }

    void encrypt_block(std::uint32_t& left, std::uint32_t& right) const {
        encrypt(left, right);
    }

private:
    std::uint32_t round(std::uint32_t value) const {
        const auto a = static_cast<std::uint8_t>(value >> 24U);
        const auto b = static_cast<std::uint8_t>(value >> 16U);
        const auto c = static_cast<std::uint8_t>(value >> 8U);
        const auto d = static_cast<std::uint8_t>(value);
        return ((state_[18 + a] + state_[274 + b]) ^ state_[530 + c]) +
               state_[786 + d];
    }

    void encrypt(std::uint32_t& left, std::uint32_t& right) const {
        for (std::size_t index = 0; index < 16; ++index) {
            left ^= state_[index];
            right ^= round(left);
            std::swap(left, right);
        }
        std::swap(left, right);
        right ^= state_[16];
        left ^= state_[17];
    }

    std::array<std::uint32_t, 1042> state_;
};

std::uint32_t load_little_endian(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8U) |
           (static_cast<std::uint32_t>(data[2]) << 16U) |
           (static_cast<std::uint32_t>(data[3]) << 24U);
}

std::uint32_t load_big_endian(const std::uint8_t* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24U) |
           (static_cast<std::uint32_t>(data[1]) << 16U) |
           (static_cast<std::uint32_t>(data[2]) << 8U) |
           static_cast<std::uint32_t>(data[3]);
}

void store_little_endian(std::uint8_t* data, std::uint32_t value) {
    data[0] = static_cast<std::uint8_t>(value);
    data[1] = static_cast<std::uint8_t>(value >> 8U);
    data[2] = static_cast<std::uint8_t>(value >> 16U);
    data[3] = static_cast<std::uint8_t>(value >> 24U);
}

void store_big_endian(std::uint8_t* data, std::uint32_t value) {
    data[0] = static_cast<std::uint8_t>(value >> 24U);
    data[1] = static_cast<std::uint8_t>(value >> 16U);
    data[2] = static_cast<std::uint8_t>(value >> 8U);
    data[3] = static_cast<std::uint8_t>(value);
}

void validate_input(const Bytes& data, std::string_view key) {
    if (key.empty() || key.size() > 56)
        throw Error("Blowfish key must contain 1..56 bytes");
    if (data.size() % 8 != 0)
        throw Error("Blowfish ECB input size must be a multiple of 8 bytes");
}

template <typename Transform>
Bytes transform_blocks(Bytes data, std::string_view key,
                       BlowfishWordOrder word_order, Transform transform) {
    validate_input(data, key);
    Blowfish blowfish(key);
    const auto load = word_order == BlowfishWordOrder::little_endian
        ? load_little_endian : load_big_endian;
    const auto store = word_order == BlowfishWordOrder::little_endian
        ? store_little_endian : store_big_endian;
    for (std::size_t offset = 0; offset < data.size(); offset += 8) {
        auto left = load(data.data() + offset);
        auto right = load(data.data() + offset + 4);
        transform(blowfish, left, right);
        store(data.data() + offset, left);
        store(data.data() + offset + 4, right);
    }
    return data;
}

}  // namespace

namespace blowfish_detail {

Bytes blowfish_initial_state_ecb_decrypt(Bytes ciphertext,
                                         BlowfishWordOrder word_order) {
    if (ciphertext.size() % 8 != 0)
        throw Error("raw Blowfish ECB input size must be a multiple of 8 bytes");
    Blowfish blowfish;
    const auto load = word_order == BlowfishWordOrder::little_endian
        ? load_little_endian : load_big_endian;
    const auto store = word_order == BlowfishWordOrder::little_endian
        ? store_little_endian : store_big_endian;
    for (std::size_t offset = 0; offset < ciphertext.size(); offset += 8) {
        auto left = load(ciphertext.data() + offset);
        auto right = load(ciphertext.data() + offset + 4);
        blowfish.decrypt(left, right);
        store(ciphertext.data() + offset, left);
        store(ciphertext.data() + offset + 4, right);
    }
    return ciphertext;
}

}  // namespace blowfish_detail

Bytes blowfish_ecb_encrypt(Bytes plaintext, std::string_view key,
                           BlowfishWordOrder word_order) {
    return transform_blocks(
        std::move(plaintext), key, word_order,
        [](const Blowfish& blowfish, std::uint32_t& left,
           std::uint32_t& right) {
            blowfish.encrypt_block(left, right);
        });
}

Bytes blowfish_ecb_decrypt(Bytes ciphertext, std::string_view key,
                           BlowfishWordOrder word_order) {
    return transform_blocks(
        std::move(ciphertext), key, word_order,
        [](const Blowfish& blowfish, std::uint32_t& left,
           std::uint32_t& right) {
            blowfish.decrypt(left, right);
        });
}

}  // namespace tdx
