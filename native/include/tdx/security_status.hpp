#pragma once

#include "tdx/finance_eligibility.hpp"

#include <filesystem>
#include <memory>
#include <set>
#include <string>

namespace tdx {

// TdxW populates ISQUITCODE from T0002/hq_cache/infoharbor_spec.cfg.
// The fourth field is the activation date; the fifth field is retained by the
// file format but is not consulted by the original flag loader.
struct SecurityQuitResource {
    std::string source_path;
    int effective_date{};
    std::size_t parsed_record_count{};
    std::set<SecurityKey> active;
};

SecurityQuitResource parse_security_quit_text(
    const std::string& utf8_text,
    int effective_date,
    std::string source_path = {});

SecurityQuitResource load_security_quit_resource(
    const std::filesystem::path& tdx_root,
    int effective_date = 0);

std::shared_ptr<const SecurityQuitResource> cached_security_quit_resource(
    const std::filesystem::path& tdx_root,
    int effective_date = 0);

int tcalc_security_class(int market, const std::string& code);
bool tcalc_security_status_applicable(int market, const std::string& code);
bool tcalc_is_st_security(int market, const std::string& code,
                          const std::string& name);
bool tcalc_is_t0_primary_security(
    const FinanceEligibilityResource& resource,
    int market,
    const std::string& code);
bool tcalc_is_t0_security(const FinanceEligibilityResource& resource,
                          int market,
                          const std::string& code);
bool tcalc_is_quit_security(const SecurityQuitResource& resource,
                            int market,
                            const std::string& code);
bool tcalc_is_futures_or_option_category(int category);

}  // namespace tdx
