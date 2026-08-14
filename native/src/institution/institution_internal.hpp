#pragma once

#include "tdx/institution.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::institution_detail {

inline constexpr const char* holder_entry = "CWServ.tdxf10_gg_gdyjcgmx";
inline constexpr const char* holder_base_url = "http://page1.tdx.com.cn:7615/TQLEX";
inline constexpr int holder_max_attempts = 3;
inline constexpr std::uintmax_t maximum_persistent_cache_bytes =
    64ULL * 1024ULL * 1024ULL;

Json query_holder_tqlex(const Json& request, int timeout_ms,
                        const TqlexTransport& transport, int& attempts);
std::filesystem::path persistent_cache_path(
    const std::filesystem::path& directory, std::string_view kind,
    const std::string& key);
std::optional<InstitutionService::CachedDocument> read_persistent_cache(
    const std::filesystem::path& directory, std::string_view kind,
    const std::string& key);
void write_persistent_cache(
    const std::filesystem::path& directory, std::string_view kind,
    const std::string& key, const InstitutionService::CachedDocument& cached);
Json import_holder_cache_snapshots(
    const std::vector<std::string>& input_names,
    const std::filesystem::path& cache_directory);

std::filesystem::path native_path(const std::string& value);
std::string now_text();
const Json* value_ptr(const Json& object, std::string_view name);
Json copy_value(const Json& object, std::string_view name);
std::string text_value(const Json& object, std::string_view name);
std::string scalar_text(const Json& value);
std::string percent_decode(std::string_view value);
int market_id(const std::string& value);
std::string market_name(int value);
std::string market_prefix(int value);
bool six_digits(const std::string& value);
void validate_token(const std::string& value, const std::string& name,
                    bool optional = false);
int bounded_integer(const std::string& text, const std::string& name,
                    int minimum, int maximum);
Json security_document(
    const std::string& market, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json tqlex_request(const std::string& mode, const std::string& code,
                   const std::string& holder_id,
                   const std::string& variant_id);
std::string change_label(const std::string& value);
Json normalize_institution_history(const Json& rows);
Json normalize_holders(const Json& rows, const std::string& reference_code);
Json source_summary(const Json& document);

}  // namespace tdx::institution_detail
