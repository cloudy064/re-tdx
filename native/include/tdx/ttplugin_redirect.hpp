#pragma once

#include "tdx/common.hpp"
#include "tdx/json.hpp"

#include <vector>
#include <string>

namespace tdx {

Json ttplugin_redirect_contract_document();
Bytes encode_ttplugin_redirect_request(const Json& request);
int command_recon_ttplugin_redirect(const std::vector<std::string>& args);

}  // namespace tdx
