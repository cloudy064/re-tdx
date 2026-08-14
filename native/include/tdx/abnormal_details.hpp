#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct AbnormalDetailsQuery {
    std::string view{"summary"};
    std::string board{"all"};
    std::string type{"all"};
    std::string date;
    std::string market;
    std::string code;
    bool refresh{};
    int cache_ttl_seconds{60};
    int timeout_ms{15000};
};

Json normalize_abnormal_summary_rows(const Json& rows);
Json normalize_abnormal_explanation_rows(const Json& rows);

class AbnormalDetailsService {
public:
    AbnormalDetailsService(std::filesystem::path root, BlockData blocks = {});
    Json query(const AbnormalDetailsQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };

    std::filesystem::path root_;
    BlockData blocks_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_abnormal_details(const std::vector<std::string>& args);

}  // namespace tdx
