#pragma once

#include "tdx/cloud_calc.hpp"

#include "cloud_calc_config_internal.hpp"

#include <filesystem>
#include <map>
#include <set>
#include <string_view>

namespace tdx::cloud_calc_service_detail {

inline constexpr char audit_schema[] = "tdx-tbigdata-cloud-calc-audit-v1";
inline constexpr char evaluation_schema[] = "tdx-tbigdata-cloud-calc-evaluation-v1";
inline constexpr char template_schema[] = "tdx-tbigdata-cloud-calc-template-v1";
inline constexpr char batch_schema[] = "tdx-tbigdata-cloud-calc-batch-v1";

struct BatchRowsDocument {
    Json rows{Json::array()};
    std::size_t available{};
};

const Json *optional(const Json &object, std::string_view key);
Json audit_config(const cloud_calc_detail::Config &config,
                  std::map<std::string, std::uint64_t, std::less<>> &builtin_usage,
                  std::uint64_t &formula_count, std::uint64_t &expression_count,
                  std::uint64_t &builtin_count, std::uint64_t &valid_count,
                  std::uint64_t &executable_count, std::uint64_t &cycle_count);
Json row_from_document(const Json &document, std::size_t row_index);
Json read_row(const std::filesystem::path &path, std::size_t row_index);
BatchRowsDocument batch_rows_from_document(const Json &document, std::size_t maximum);
Json assignment_value(std::string value);
std::filesystem::path resolve_cfg(const std::filesystem::path &root, const std::string &name);
void require_safe_cfg_request_name(const std::string &name);
void validate_cloud_calc_options(const CloudCalcEvaluationOptions &options);
void validate_cloud_calc_row(const Json &row);
void collect_requested_securities(const Json &request, std::string_view key,
                                  std::set<std::string, std::less<>> &destination);
std::vector<std::string> security_vector(const std::set<std::string, std::less<>> &values);
int bounded_index(const std::string &value);

} // namespace tdx::cloud_calc_service_detail
