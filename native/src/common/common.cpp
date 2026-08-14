#include "tdx/common.hpp"
#include "tdx/utf8.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#ifdef _MSC_VER
#pragma comment(lib, "bcrypt.lib")
#endif
#endif

namespace fs = std::filesystem;

namespace tdx {

fs::path running_executable_path() {
#ifdef _WIN32
    std::wstring value(32768, L'\0');
    const DWORD size = GetModuleFileNameW(
        nullptr, value.data(), static_cast<DWORD>(value.size()));
    if (!size || size >= value.size()) return {};
    value.resize(size);
    return fs::path(value);
#else
    std::error_code error;
    const auto path = fs::read_symlink("/proc/self/exe", error);
    return error ? fs::path{} : path;
#endif
}

std::wstring utf8_to_wide(std::string_view value) {
#ifdef _WIN32
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) throw Error("invalid UTF-8 text");
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), size);
    return result;
#else
    return std::wstring(value.begin(), value.end());
#endif
}

std::string wide_to_utf8(std::wstring_view value) {
#ifdef _WIN32
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0,
                                         nullptr, nullptr);
    if (size <= 0) throw Error("invalid UTF-16 text");
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), size,
                        nullptr, nullptr);
    return result;
#else
    return std::string(value.begin(), value.end());
#endif
}

std::string utf8_prefix(std::string_view value, std::size_t max_bytes) {
    std::size_t offset = 0;
    while (offset < value.size() && offset < max_bytes) {
        const auto lead = static_cast<unsigned char>(value[offset]);
        std::size_t width = 0;
        std::uint32_t code_point = 0;
        std::uint32_t minimum = 0;
        if (lead < 0x80u) {
            width = 1;
            code_point = lead;
        } else if ((lead & 0xe0u) == 0xc0u) {
            width = 2;
            code_point = lead & 0x1fu;
            minimum = 0x80u;
        } else if ((lead & 0xf0u) == 0xe0u) {
            width = 3;
            code_point = lead & 0x0fu;
            minimum = 0x800u;
        } else if ((lead & 0xf8u) == 0xf0u) {
            width = 4;
            code_point = lead & 0x07u;
            minimum = 0x10000u;
        } else {
            break;
        }
        if (offset + width > value.size() || offset + width > max_bytes) break;
        bool valid = true;
        for (std::size_t index = 1; index < width; ++index) {
            const auto continuation = static_cast<unsigned char>(value[offset + index]);
            if ((continuation & 0xc0u) != 0x80u) {
                valid = false;
                break;
            }
            code_point = (code_point << 6u) | (continuation & 0x3fu);
        }
        if (!valid || code_point < minimum || code_point > 0x10ffffu ||
            (code_point >= 0xd800u && code_point <= 0xdfffu))
            break;
        offset += width;
    }
    return std::string(value.substr(0, offset));
}

Bytes encode_gbk(std::string_view value) {
#ifdef _WIN32
    if (value.empty()) return {};
    const auto wide = utf8_to_wide(value);
    constexpr UINT gbk_code_page = 936;
    const int size = WideCharToMultiByte(
        gbk_code_page, WC_NO_BEST_FIT_CHARS, wide.data(),
        static_cast<int>(wide.size()),
        nullptr, 0, nullptr, nullptr);
    if (size <= 0) throw Error("cannot encode GBK text");
    Bytes result(static_cast<std::size_t>(size));
    WideCharToMultiByte(
        gbk_code_page, WC_NO_BEST_FIT_CHARS, wide.data(),
        static_cast<int>(wide.size()), reinterpret_cast<char*>(result.data()),
        size, nullptr, nullptr);
    return result;
#else
    return Bytes(value.begin(), value.end());
#endif
}

std::string decode_gbk(const Bytes& value) {
#ifdef _WIN32
    if (value.empty()) return {};
    const char* source = reinterpret_cast<const char*>(value.data());
    constexpr UINT gb18030_code_page = 54936;
    const int wide_size = MultiByteToWideChar(gb18030_code_page, 0, source,
                                               static_cast<int>(value.size()),
                                               nullptr, 0);
    if (wide_size <= 0) throw Error("cannot decode GBK/GB18030 text");
    std::wstring wide(static_cast<std::size_t>(wide_size), L'\0');
    MultiByteToWideChar(gb18030_code_page, 0, source, static_cast<int>(value.size()),
                        wide.data(), wide_size);
    return wide_to_utf8(wide);
#else
    return std::string(value.begin(), value.end());
#endif
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string trim(std::string value) {
    auto non_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), non_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), non_space).base(), value.end());
    return value;
}

std::vector<std::string> split(std::string_view value, char delimiter) {
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto end = value.find(delimiter, start);
        result.emplace_back(value.substr(start, end == std::string_view::npos
                                                    ? value.size() - start
                                                    : end - start));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return result;
}

Bytes read_bytes(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw Error("cannot open file: " + path_utf8(path));
    stream.seekg(0, std::ios::end);
    const auto length = stream.tellg();
    if (length < 0) throw Error("cannot determine file size: " + path_utf8(path));
    stream.seekg(0, std::ios::beg);
    Bytes result(static_cast<std::size_t>(length));
    if (!result.empty()) {
        stream.read(reinterpret_cast<char*>(result.data()), length);
        if (!stream) throw Error("short read: " + path_utf8(path));
    }
    return result;
}

std::string read_text_utf8(const fs::path& path) {
    const auto data = read_bytes(path);
    return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

void atomic_write_bytes(const fs::path& path, const Bytes& value) {
    if (!path.parent_path().empty()) fs::create_directories(path.parent_path());
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path temporary = path;
    temporary += ".tmp." + std::to_string(stamp);
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) throw Error("cannot create file: " + path_utf8(temporary));
        if (!value.empty()) {
            stream.write(reinterpret_cast<const char*>(value.data()),
                         static_cast<std::streamsize>(value.size()));
        }
        stream.flush();
        if (!stream) throw Error("cannot write file: " + path_utf8(temporary));
    }
    std::error_code error;
    fs::rename(temporary, path, error);
    if (error) {
        fs::remove(path, error);
        error.clear();
        fs::rename(temporary, path, error);
    }
    if (error) {
        fs::remove(temporary);
        throw Error("cannot replace file: " + path_utf8(path) + ": " + error.message());
    }
}

void atomic_write_text(const fs::path& path, std::string_view value) {
    atomic_write_bytes(path, Bytes(value.begin(), value.end()));
}

std::string sha256_file(const fs::path& path) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0;
    DWORD result_size = 0;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
        throw Error("BCryptOpenAlgorithmProvider(SHA256) failed");
    }
    auto close_algorithm = [&] { if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); };
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                          reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
                          &result_size, 0) < 0) {
        close_algorithm();
        throw Error("BCryptGetProperty failed");
    }
    Bytes object(object_size);
    if (BCryptCreateHash(algorithm, &hash, object.data(), object_size,
                         nullptr, 0, 0) < 0) {
        close_algorithm();
        throw Error("BCryptCreateHash failed");
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        BCryptDestroyHash(hash);
        close_algorithm();
        throw Error("cannot open file for SHA256: " + path_utf8(path));
    }
    Bytes buffer(1 << 20);
    while (stream) {
        stream.read(reinterpret_cast<char*>(buffer.data()),
                    static_cast<std::streamsize>(buffer.size()));
        const auto count = stream.gcount();
        if (count > 0 && BCryptHashData(hash, buffer.data(),
                                        static_cast<ULONG>(count), 0) < 0) {
            BCryptDestroyHash(hash);
            close_algorithm();
            throw Error("BCryptHashData failed");
        }
    }
    Bytes digest(32);
    if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) {
        BCryptDestroyHash(hash);
        close_algorithm();
        throw Error("BCryptFinishHash failed");
    }
    BCryptDestroyHash(hash);
    close_algorithm();
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (auto byte : digest) output << std::setw(2) << static_cast<unsigned>(byte);
    return output.str();
#else
    (void)path;
    throw Error("SHA256 is not implemented on this platform");
#endif
}

namespace {
std::string bcrypt_hash_file(const fs::path& path, LPCWSTR algorithm_name,
                             std::size_t digest_size) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0;
    DWORD result_size = 0;
    if (BCryptOpenAlgorithmProvider(&algorithm, algorithm_name, nullptr, 0) < 0)
        throw Error("BCryptOpenAlgorithmProvider failed");
    auto close_algorithm = [&] { if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); };
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                          reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
                          &result_size, 0) < 0) {
        close_algorithm();
        throw Error("BCryptGetProperty failed");
    }
    Bytes object(object_size);
    if (BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0) < 0) {
        close_algorithm();
        throw Error("BCryptCreateHash failed");
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        BCryptDestroyHash(hash); close_algorithm();
        throw Error("cannot open file for hash: " + path_utf8(path));
    }
    Bytes buffer(1 << 20);
    while (stream) {
        stream.read(reinterpret_cast<char*>(buffer.data()),
                    static_cast<std::streamsize>(buffer.size()));
        const auto count = stream.gcount();
        if (count > 0 && BCryptHashData(hash, buffer.data(), static_cast<ULONG>(count), 0) < 0) {
            BCryptDestroyHash(hash); close_algorithm();
            throw Error("BCryptHashData failed");
        }
    }
    Bytes digest(digest_size);
    if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) {
        BCryptDestroyHash(hash); close_algorithm();
        throw Error("BCryptFinishHash failed");
    }
    BCryptDestroyHash(hash); close_algorithm();
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (auto byte : digest) output << std::setw(2) << static_cast<unsigned>(byte);
    return output.str();
#else
    (void)path; (void)algorithm_name; (void)digest_size;
    throw Error("native hashing is not implemented on this platform");
#endif
}

std::string bcrypt_hash_bytes(const Bytes& value, LPCWSTR algorithm_name,
                              std::size_t digest_size) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0, result_size = 0;
    if (BCryptOpenAlgorithmProvider(&algorithm, algorithm_name, nullptr, 0) < 0)
        throw Error("BCryptOpenAlgorithmProvider failed");
    auto close_algorithm = [&] { if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); };
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                          reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
                          &result_size, 0) < 0) {
        close_algorithm(); throw Error("BCryptGetProperty failed");
    }
    Bytes object(object_size);
    if (BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0) < 0) {
        close_algorithm(); throw Error("BCryptCreateHash failed");
    }
    if (!value.empty() && BCryptHashData(hash, const_cast<PUCHAR>(value.data()),
                                         static_cast<ULONG>(value.size()), 0) < 0) {
        BCryptDestroyHash(hash); close_algorithm(); throw Error("BCryptHashData failed");
    }
    Bytes digest(digest_size);
    if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) {
        BCryptDestroyHash(hash); close_algorithm(); throw Error("BCryptFinishHash failed");
    }
    BCryptDestroyHash(hash); close_algorithm();
    std::ostringstream output; output << std::hex << std::setfill('0');
    for (auto byte : digest) output << std::setw(2) << static_cast<unsigned>(byte);
    return output.str();
#else
    (void)value; (void)algorithm_name; (void)digest_size;
    throw Error("native hashing is not implemented on this platform");
#endif
}
}  // namespace

std::string md5_file(const fs::path& path) {
#ifdef _WIN32
    return bcrypt_hash_file(path, BCRYPT_MD5_ALGORITHM, 16);
#else
    (void)path;
    throw Error("MD5 is not implemented on this platform");
#endif
}

std::string md5_bytes(const Bytes& value) {
#ifdef _WIN32
    return bcrypt_hash_bytes(value, BCRYPT_MD5_ALGORITHM, 16);
#else
    (void)value;
    throw Error("MD5 is not implemented on this platform");
#endif
}

fs::path find_tdx_root(const fs::path& explicit_root) {
    std::vector<fs::path> candidates;
    if (!explicit_root.empty()) candidates.push_back(explicit_root);
#ifdef _WIN32
    if (const wchar_t* environment = _wgetenv(L"TDX_ROOT")) candidates.emplace_back(environment);
    candidates.emplace_back(L"C:\\new_tdx");
#endif
    for (const auto& candidate : candidates) {
        std::error_code error;
        const auto resolved = fs::weakly_canonical(candidate, error);
        const auto& root = error ? candidate : resolved;
        if (fs::is_directory(root / "T0002") &&
            (fs::exists(root / "TdxW.exe") || fs::exists(root / "tdxw.exe"))) {
            return root;
        }
    }
    throw Error("cannot locate TDX root; pass --root or set TDX_ROOT");
}

std::string path_utf8(const fs::path& path) {
#ifdef _WIN32
    return wide_to_utf8(path.native());
#else
    return path.string();
#endif
}

Args::Args(std::vector<std::string> values) : values_(std::move(values)) {}

bool Args::take_flag(std::string_view name) {
    const auto iterator = std::find(values_.begin(), values_.end(), name);
    if (iterator == values_.end()) return false;
    values_.erase(iterator);
    return true;
}

std::string Args::take_option(std::string_view name, std::string fallback) {
    for (auto iterator = values_.begin(); iterator != values_.end(); ++iterator) {
        const std::string prefix = std::string(name) + "=";
        if (iterator->compare(0, prefix.size(), prefix) == 0) {
            std::string result = iterator->substr(prefix.size());
            values_.erase(iterator);
            return result;
        }
        if (*iterator == name) {
            auto value = std::next(iterator);
            if (value == values_.end()) throw Error(std::string(name) + " requires a value");
            std::string result = *value;
            values_.erase(value);
            values_.erase(iterator);
            return result;
        }
    }
    return fallback;
}

std::vector<std::string> Args::take_options(std::string_view name) {
    std::vector<std::string> result;
    while (has(name)) result.push_back(take_option(name));
    return result;
}

bool Args::has(std::string_view name) const {
    const std::string prefix = std::string(name) + "=";
    return std::any_of(values_.begin(), values_.end(), [&](const std::string& value) {
        return value == name || value.compare(0, prefix.size(), prefix) == 0;
    });
}

const std::vector<std::string>& Args::remaining() const noexcept { return values_; }

void Args::require_empty() const {
    if (!values_.empty()) throw Error("unknown argument: " + values_.front());
}

std::uint16_t read_u16_le(const std::uint8_t* data) {
    return static_cast<std::uint16_t>(data[0] | (data[1] << 8));
}
std::uint32_t read_u32_le(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}
std::int32_t read_i32_le(const std::uint8_t* data) {
    return static_cast<std::int32_t>(read_u32_le(data));
}
float read_f32_le(const std::uint8_t* data) {
    const auto bits = read_u32_le(data);
    float result = 0;
    static_assert(sizeof(result) == sizeof(bits));
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

}  // namespace tdx
