#pragma once

#include "tdx/json.hpp"

#include <functional>
#include <string_view>

namespace tdx::detail {

using CloudJsonAttempt = std::function<Json()>;
using CloudErrorClassifier = std::function<bool(std::string_view)>;

bool is_transient_tqlex_error(std::string_view message) noexcept;
bool is_transient_pbrpc_error(std::string_view message) noexcept;
bool is_transient_cloud_error(std::string_view message) noexcept;

Json retry_cloud_json(const CloudJsonAttempt& operation,
                      const CloudErrorClassifier& classifier,
                      int& attempts,
                      int max_attempts = 3,
                      int retry_delay_ms = 250);

}  // namespace tdx::detail
