#pragma once

#include "tdx/json.hpp"
#include "tdx/minute.hpp"

#include <string>
#include <string_view>

namespace tdx::formula_context_detail {

std::string normalized_market(std::string market);
int formula_market_id(std::string market);
ExpansionInstrument cached_expansion_security_record(
    int market_id, const std::string& code, int timeout_ms);

void bind_formula_text_symbol(Json& context, Json& symbols,
                              std::string_view name, std::string value,
                              std::string_view source);

}  // namespace tdx::formula_context_detail
