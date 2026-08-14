#pragma once

#include <ctime>
#include <string>

namespace tdx {

std::string local_timestamp_text();
std::string local_timestamp_text(const std::tm& local_time);

}  // namespace tdx
