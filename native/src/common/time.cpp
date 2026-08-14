#include "tdx/time.hpp"

#include "tdx/common.hpp"

#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace tdx {
namespace {

std::tm local_time(std::time_t value) {
    std::tm result{};
#ifdef _WIN32
    if (localtime_s(&result, &value)) throw Error("cannot read local time");
#else
    if (!localtime_r(&value, &result)) throw Error("cannot read local time");
#endif
    return result;
}

std::time_t wall_clock_as_utc(std::tm value) {
#ifdef _WIN32
    return static_cast<std::time_t>(_mkgmtime64(&value));
#else
    return timegm(&value);
#endif
}

}  // namespace

std::string local_timestamp_text(const std::tm& local_time_value) {
    auto local = local_time_value;
    const auto stamp = std::mktime(&local);
    if (stamp == static_cast<std::time_t>(-1))
        throw Error("cannot convert local time");
    local = local_time(stamp);
    const auto local_as_utc = wall_clock_as_utc(local);
    if (local_as_utc == static_cast<std::time_t>(-1))
        throw Error("cannot determine local UTC offset");
    const auto offset_seconds = static_cast<long long>(local_as_utc) -
        static_cast<long long>(stamp);
    const auto offset_minutes = offset_seconds / 60;
    const auto absolute_minutes = std::llabs(offset_minutes);

    std::ostringstream output;
    output << std::setfill('0')
           << std::setw(4) << local.tm_year + 1900 << '-'
           << std::setw(2) << local.tm_mon + 1 << '-'
           << std::setw(2) << local.tm_mday << 'T'
           << std::setw(2) << local.tm_hour << ':'
           << std::setw(2) << local.tm_min << ':'
           << std::setw(2) << local.tm_sec
           << (offset_minutes < 0 ? '-' : '+')
           << std::setw(2) << absolute_minutes / 60
           << std::setw(2) << absolute_minutes % 60;
    return output.str();
}

std::string local_timestamp_text() {
    const auto stamp = std::time(nullptr);
    return local_timestamp_text(local_time(stamp));
}

}  // namespace tdx
