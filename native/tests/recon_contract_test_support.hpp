#pragma once

#include "tdx/common.hpp"
#include "tdx/formula_engine.hpp"
#include "tdx/formula_render_profile.hpp"
#include "tdx/recon.hpp"

#include <array>
#include <cmath>
#include <ctime>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace recon_contract_test {

void require(bool value, const char *message);
tdx::Json formula_sample(int count);

void run_formula_evaluation_contracts();
void run_formula_render_workflow_contracts();
void run_market_core_contracts();
void run_research_signal_contracts();
void run_realtime_corporate_contracts();
void run_curated_issuance_contracts();
void run_calendar_resilience_web_contracts();

} // namespace recon_contract_test
