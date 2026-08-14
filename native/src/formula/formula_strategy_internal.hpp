#pragma once

#include "tdx/formula_strategy.hpp"

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace tdx {

class Args;
struct BlockData;

namespace formula_strategy_detail {

const Json* optional(const Json& object, std::string_view key);
std::filesystem::path from_utf8(const std::string& value);
std::string upper_ascii(std::string value);
std::string required_text(const Json& object, std::string_view key);
bool valid_identifier(const std::string& value);
std::map<std::string, double> parameter_object(const Json* value);
Json parameter_json(const std::map<std::string, double>& parameters);
const Json& find_selection_formula(const Json& library, const std::string& code);
Json formula_analysis(const Json& formula);
std::set<std::string> presentation_outputs(const Json& analysis);
Json strategy_summary(const Json& strategy);
std::string point_key(const Json& point);
bool has_external_dependency(const Json& analysis);
bool has_finance_dependency(const Json& analysis);
std::set<std::string> dependencies(const Json& analysis);

struct SecurityKey {
    std::string market;
    std::string code;
    std::string name;
};

SecurityKey parse_security(std::string value);
std::vector<Json> kline_documents(const Json& document);
std::vector<SecurityKey> securities_from_document(const Json& document);
int integer_option(Args& args, std::string_view name, int fallback,
                   int minimum, int maximum);
double number_option(Args& args, std::string_view name, double fallback,
                     double minimum, double maximum);
bool manifest_needs_library(const Json& manifest);
void attach_strategy_contexts(Json& kline, const Json& strategy,
                              const std::filesystem::path& root, int timeout,
                              const BlockData* block_data,
                              bool point_in_time_finance,
                              bool historical_backtest,
                              const Json* formula_library);

}  // namespace formula_strategy_detail
}  // namespace tdx
