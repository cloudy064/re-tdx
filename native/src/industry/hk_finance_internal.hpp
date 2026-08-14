#pragma once

#include "tdx/common.hpp"
#include "tdx/hk_finance.hpp"

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tdx::hk_finance_detail {

struct FinanceRecord {
    std::array<std::string, 17> raw;
    std::array<std::optional<double>, 17> numeric;
};

std::vector<FinanceRecord> parse_decrypted_resource(
    const Bytes& decrypted, const std::filesystem::path& source);
std::shared_ptr<const std::vector<FinanceRecord>> load_records(
    const std::filesystem::path& root);
Json build_document(const std::vector<FinanceRecord>& records,
                    const std::filesystem::path& source,
                    HkFinanceQuery query);

}  // namespace tdx::hk_finance_detail
