#include "tdx/security_directory.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void append_u16(tdx::Bytes& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}

void append_f32(tdx::Bytes& bytes, float value) {
    std::uint32_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    for (int shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>(bits >> shift));
}

tdx::Bytes fixture() {
    tdx::Bytes bytes;
    append_u16(bytes, 1);
    const std::string code = "600521";
    bytes.insert(bytes.end(), code.begin(), code.end());
    append_u16(bytes, 100);
    const tdx::Bytes name{0xBB, 0xAA, 0xBA, 0xA3, 0xD2, 0xA9, 0xD2, 0xB5};
    bytes.insert(bytes.end(), name.begin(), name.end());
    bytes.insert(bytes.end(), 8, 0);
    append_f32(bytes, 1.25F);
    bytes.push_back(2);
    append_f32(bytes, 17.04F);
    bytes.insert(bytes.end(), {0x11, 0x22, 0x33, 0x44});
    return bytes;
}

}  // namespace

int main() {
    try {
        const auto rows = tdx::parse_security_directory_page(fixture(), 1);
        require(rows.size() == 1, "record count");
        require(rows[0].code == "600521", "code");
        require(rows[0].name == "华海药业", "GBK name");
        require(rows[0].multiple == 100 && rows[0].decimal == 2, "precision fields");
        require(std::abs(rows[0].previous_close_price - 17.04) < 0.001, "previous close");
        require(rows[0].category == "a_share", "A-share category");
        require(tdx::classify_security_directory_record(1, "588000") == "etf", "ETF category");
        require(tdx::classify_security_directory_record(0, "123001") == "convertible_bond", "convertible category");
        std::cout << "security directory tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
