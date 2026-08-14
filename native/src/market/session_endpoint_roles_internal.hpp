#pragma once

#include "tdx/json.hpp"

#include <string_view>

namespace tdx::session_audit_detail {

// Static, evidence-graded semantics recovered from the TdxW endpoint loaders.
// These helpers never inspect credentials and never probe an endpoint.
Json endpoint_group_role_document(std::string_view section);
int endpoint_group_loader_default_port(std::string_view section);
bool endpoint_group_uses_section_primary(std::string_view section);

}  // namespace tdx::session_audit_detail
