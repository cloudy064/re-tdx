#pragma once

#include "tdx/formula_context.hpp"
#include "tdx/formula_engine.hpp"
#include "tdx/formula_render_profile.hpp"

#include "tdx/blocks.hpp"
#include "tdx/common.hpp"
#include "tdx/external_series.hpp"
#include "tdx/external_signals.hpp"
#include "tdx/formulas.hpp"
#include "tdx/local_signals.hpp"
#include "tdx/options.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <limits>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace formula_engine_test {

namespace fs = std::filesystem;

void require(bool condition, const std::string &message);
tdx::Json sample(int count);
tdx::Json chip_sample();
tdx::Json expansion_sample();
tdx::Json time_average_sample();
const tdx::Json &point_value(const tdx::Json &result, std::size_t index, const std::string &name);
const tdx::Json &formula(const tdx::Json &library, const std::string &code);

void run_context_builder_tests();
void run_language_tests();
void run_native_operator_tests();
void run_native_binary_operator_tests();
void run_native_scalar_tests();
void run_workflow_tests();
void run_render_tests();
void run_context_and_library_tests();

} // namespace formula_engine_test
