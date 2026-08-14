#include "level2_sdk_snapshot_digest.hpp"

#include "tdx/common.hpp"

#include <cstdint>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#endif

namespace tdx::level2_detail {

std::string level2_snapshot_sha256(const Bytes& value) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0;
    DWORD result_size = 0;
    if (BCryptOpenAlgorithmProvider(
            &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        throw Error("level2 snapshot SHA-256 provider is unavailable");
    auto close_algorithm = [&] {
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    };
    if (BCryptGetProperty(
            algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
            &result_size, 0) < 0) {
        close_algorithm();
        throw Error("level2 snapshot SHA-256 object size is unavailable");
    }
    Bytes object(object_size);
    if (BCryptCreateHash(
            algorithm, &hash, object.data(), object_size, nullptr, 0, 0) < 0) {
        close_algorithm();
        throw Error("level2 snapshot SHA-256 initialization failed");
    }
    if (!value.empty() &&
        BCryptHashData(
            hash, const_cast<PUCHAR>(value.data()),
            static_cast<ULONG>(value.size()), 0) < 0) {
        BCryptDestroyHash(hash);
        close_algorithm();
        throw Error("level2 snapshot SHA-256 update failed");
    }
    Bytes digest(32);
    if (BCryptFinishHash(
            hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) {
        BCryptDestroyHash(hash);
        close_algorithm();
        throw Error("level2 snapshot SHA-256 finalization failed");
    }
    BCryptDestroyHash(hash);
    close_algorithm();

    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : digest)
        output << std::setw(2) << static_cast<unsigned>(byte);
    return output.str();
#else
    (void)value;
    throw Error("level2 snapshot SHA-256 is unavailable on this platform");
#endif
}

}  // namespace tdx::level2_detail
