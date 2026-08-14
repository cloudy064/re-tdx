#pragma once

#include "tdx/json.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

Json parse_tpool_xml_document(const std::string& xml,
                              const std::string& source_name = "memory.xml");
Json inspect_tpool_file_document(const std::filesystem::path& path);
Json inspect_tpool_root_document(const std::filesystem::path& root);
Json parse_tpool_history_xml_document(
    const std::string& xml,
    const std::string& source_name = "memory.dat");
Json parse_tpool_history_text_document(
    const std::string& text,
    const std::string& source_name = "memory_status_his.txt");
Json inspect_tpool_history_file_document(const std::filesystem::path& path);
Json inspect_tpool_history_files_document(
    const std::vector<std::filesystem::path>& paths);
Json inspect_tpool_history_root_document(
    const std::filesystem::path& root,
    const std::string& pool = {},
    const std::string& cell = {},
    const std::string& kind = "all",
    int from_date = 0,
    int to_date = 0,
    std::size_t limit = 1000);
Json attach_tpool_formula_library_document(Json inspection,
                                           const Json& formula_library);
// Evaluate one already-calculated indicator series against a recovered TPool
// <func> rule. This pure helper performs no market request and is also used by
// the pool evaluator, keeping historical-window semantics independently
// testable.
Json evaluate_tpool_rule_document(const Json& rule,
                                  const Json& calculation);
Json rank_tpool_candidates_document(const Json& candidates,
                                    int operation,
                                    double threshold);
Json project_tpool_flow_document(const Json& evaluation);
struct TpoolFlowClock {
    std::int64_t epoch_seconds{};
    int local_date{};       // YYYYMMDD
    int local_hhmmss{};     // HHMMSS
    int local_weekday{};    // 1=Sunday .. 7=Saturday, matching the TPool runtime
};
Json advance_tpool_flow_state_document(const Json& evaluation,
                                       const Json& previous_state,
                                       const TpoolFlowClock& clock);
Json diff_tpool_alerts_document(const Json& previous,
                                const Json& current);
int command_pool_inspect(const std::vector<std::string>& args);
int command_pool_history(const std::vector<std::string>& args);
Json evaluate_tpool_file_document(const std::filesystem::path& path,
                                  int pages = 1,
                                  int page_size = 800,
                                  int timeout_ms = 10000,
                                  int security_limit = 20,
                                  const Json* formula_library = nullptr,
                                  const std::filesystem::path& tdx_root = {});
Json evaluate_tpool_xml_document(const std::string& xml,
                                 const std::string& source_name = "inline.xml",
                                 int pages = 1,
                                 int page_size = 800,
                                 int timeout_ms = 10000,
                                 int security_limit = 20,
                                 const Json* formula_library = nullptr,
                                 const std::filesystem::path& tdx_root = {});
int command_pool_evaluate(const std::vector<std::string>& args);
int command_pool_watch(const std::vector<std::string>& args);

}  // namespace tdx
