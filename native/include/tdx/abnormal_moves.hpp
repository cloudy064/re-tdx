#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct AbnormalMovesQuery {
    std::string board{"all"};
    std::string type{"all"};
    std::string date;
    bool refresh{};
    int limit{5000};
    int cache_ttl_seconds{60};
    int timeout_ms{15000};
};

Json normalize_abnormal_move_rows(const Json& rows, const BlockData& blocks = {});

class AbnormalMovesService {
public:
    AbnormalMovesService(std::filesystem::path root, BlockData blocks = {});
    Json query(const AbnormalMovesQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };
    std::filesystem::path root_;
    BlockData blocks_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_abnormal_moves(const std::vector<std::string>& args);

}  // namespace tdx
