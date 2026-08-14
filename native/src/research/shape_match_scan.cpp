#include "tdx/shape_match.hpp"

#include "shape_match_internal.hpp"

#include "tdx/blocks.hpp"
#include "tdx/common.hpp"
#include "tdx/minute.hpp"
#include "tdx/security_directory.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <thread>
#include <tuple>

namespace fs = std::filesystem;

namespace tdx::shape_match_detail {
namespace {

struct ScanSecurity {
    std::string market;
    std::string code;
    std::string name;
    std::optional<Json> embedded_kline;
};

struct ScanOutcome {
    ScanSecurity security;
    std::optional<Json> kline;
    std::optional<Json> score;
    std::string source;
    std::string local_error;
    std::string error;
};

const Json* optional_field(const Json& value, std::string_view name) {
    if (!value.is_object()) return nullptr;
    const auto& object = value.as_object();
    const auto found = object.find(name);
    return found == object.end() ? nullptr : &found->second;
}

std::string optional_string(const Json& value, std::string_view name) {
    const auto* field = optional_field(value, name);
    return field && field->is_string() ? field->as_string() : std::string{};
}

ScanSecurity parse_security(std::string value) {
    value = lower_ascii(trim(value));
    ScanSecurity result;
    const auto colon = value.find(':');
    if (colon != std::string::npos) {
        result.market = value.substr(0, colon);
        result.code = value.substr(colon + 1);
    } else if (value.size() == 8 &&
               (value.rfind("sz", 0) == 0 || value.rfind("sh", 0) == 0 ||
                value.rfind("bj", 0) == 0)) {
        result.market = value.substr(0, 2);
        result.code = value.substr(2);
    } else {
        result.code = value;
        if (value.size() == 6)
            result.market = value.rfind("92", 0) == 0 || value[0] == '8'
                ? "bj" : value[0] == '6' || value[0] == '9' ? "sh" : "sz";
    }
    if ((result.market != "sz" && result.market != "sh" &&
         result.market != "bj") || result.code.size() != 6 ||
        !std::all_of(result.code.begin(), result.code.end(),
                     [](char ch) { return ch >= '0' && ch <= '9'; }))
        throw Error("invalid shape-match security: " + value);
    return result;
}

void add_embedded(std::vector<ScanSecurity>& result, const Json& row) {
    if (!row.is_object())
        throw Error("shape-match scan input rows must be JSON objects");
    const Json* kline = nullptr;
    if (const auto* bars = optional_field(row, "bars"); bars && bars->is_array())
        kline = &row;
    else if (const auto* nested = optional_field(row, "kline");
             nested && nested->is_object() && optional_field(*nested, "bars"))
        kline = nested;
    if (!kline)
        throw Error("shape-match scan input row has no bars or kline.bars");
    auto market = optional_string(row, "market");
    auto code = optional_string(row, "code");
    if (market.empty()) market = optional_string(*kline, "market");
    if (code.empty()) code = optional_string(*kline, "code");
    if (market.empty() || code.empty())
        throw Error("shape-match scan input row requires market and code");
    auto security = parse_security(market + ":" + code);
    security.name = optional_string(row, "name");
    if (security.name.empty()) security.name = optional_string(*kline, "name");
    security.embedded_kline = *kline;
    result.push_back(std::move(security));
}

std::vector<ScanSecurity> embedded_universe(const fs::path& path) {
    const auto document = Json::parse(read_text_utf8(path));
    std::vector<ScanSecurity> result;
    if (document.is_array()) {
        for (const auto& row : document.as_array()) add_embedded(result, row);
    } else if (const auto* rows = optional_field(document, "securities");
               rows && rows->is_array()) {
        for (const auto& row : rows->as_array()) add_embedded(result, row);
    } else {
        add_embedded(result, document);
    }
    return result;
}

std::set<std::string> block_families(const std::string& selector) {
    const auto colon = selector.find(':');
    if (colon != std::string::npos) {
        const auto family = selector.substr(0, colon);
        if (family == "industry" || family == "research-industry" ||
            family == "concept" || family == "style" || family == "index")
            return {family};
    }
    return {"industry", "research-industry", "concept", "style", "index"};
}

bool scope_allows(std::int32_t scope, int id, const std::string& code) {
    if (scope == 0)
        return classify_security_directory_record(id, code) == "a_share";
    return scope == 1;
}

std::vector<ScanSecurity> catalog_universe(const fs::path& root,
                                           const ShapeMatchQuery& query,
                                           std::int32_t scope,
                                           Json& metadata) {
    if (scope == 2)
        throw Error("expansion-market shape scan requires an explicit --input or --security universe");
    const auto data = load_blocks(root, query.block.empty()
        ? std::set<std::string>{} : block_families(query.block));
    std::vector<ScanSecurity> result;
    if (query.all_securities) {
        for (const auto& [key, security] : data.securities) {
            if (!scope_allows(scope, security.market_id, security.code)) continue;
            result.push_back({lower_ascii(security.market), security.code,
                              security.name, std::nullopt});
        }
        metadata["mode"] = "all";
        metadata["scope_applied"] = scope_name(scope);
        return result;
    }

    std::vector<const Block*> selected;
    for (const auto& block : data.blocks) {
        if (block.block_id == query.block || block.block_code == query.block ||
            block.name == query.block)
            selected.push_back(&block);
    }
    if (selected.empty()) throw Error("shape-match block was not found: " + query.block);
    if (selected.size() > 1)
        throw Error("shape-match block selector is ambiguous; use its block_id");
    const auto& block = *selected.front();
    for (const auto& member : data.members) {
        if (member.block_id != block.block_id ||
            !scope_allows(scope, member.market_id, member.code)) continue;
        result.push_back({lower_ascii(member.market), member.code,
                          member.security_name, std::nullopt});
    }
    metadata["mode"] = "block";
    metadata["block_id"] = block.block_id;
    metadata["block_code"] = block.block_code;
    metadata["block_name"] = block.name;
    metadata["block_family"] = block.family;
    metadata["scope_applied"] = scope_name(scope);
    return result;
}

std::vector<ScanSecurity> resolve_universe(const fs::path& root,
                                           const ShapeMatchQuery& query,
                                           std::int32_t scope,
                                           Json& metadata) {
    const int modes = static_cast<int>(!query.scan_input_path.empty()) +
        static_cast<int>(!query.securities.empty()) +
        static_cast<int>(!query.block.empty()) +
        static_cast<int>(query.all_securities);
    if (modes != 1)
        throw Error("scan view requires exactly one of --input, --security, --block or --all");
    std::vector<ScanSecurity> result;
    if (!query.scan_input_path.empty()) {
        result = embedded_universe(query.scan_input_path);
        metadata["mode"] = "input";
        metadata["path"] = path_utf8(query.scan_input_path);
    } else if (!query.securities.empty()) {
        for (const auto& value : query.securities)
            result.push_back(parse_security(value));
        metadata["mode"] = "security-list";
    } else {
        result = catalog_universe(root, query, scope, metadata);
    }
    std::set<std::pair<std::string, std::string>> seen;
    result.erase(std::remove_if(result.begin(), result.end(), [&](const auto& item) {
        return !seen.emplace(item.market, item.code).second;
    }), result.end());
    if (result.empty()) throw Error("shape-match scan universe is empty");
    if (result.size() > static_cast<std::size_t>(query.max_candidates))
        throw Error("shape-match scan universe exceeds --max-candidates");
    metadata["candidate_count"] = static_cast<std::uint64_t>(result.size());
    return result;
}

std::string scan_period(const ShapeMatchQuery& query,
                        const ShapeTemplate& shape) {
    const auto period = query.period.empty() ? period_name(shape.period_code)
                                              : query.period;
    if (period == "unknown" || period == "intraday-custom" ||
        period == "seconds" || period == "seconds-custom")
        throw Error("template period is not available through the public K-line loader; pass --period explicitly");
    return period;
}

template <typename Callback>
void parallel_indices(const std::vector<std::size_t>& indices, int workers,
                      Callback&& callback) {
    std::atomic<std::size_t> next{0};
    auto worker = [&] {
        while (true) {
            const auto position = next.fetch_add(1);
            if (position >= indices.size()) return;
            callback(indices[position]);
        }
    };
    std::vector<std::thread> threads;
    const auto count = std::min<std::size_t>(
        static_cast<std::size_t>(workers), indices.size());
    for (std::size_t index = 0; index < count; ++index)
        threads.emplace_back(worker);
    for (auto& thread : threads) thread.join();
}

Json result_security(const ScanSecurity& security) {
    Json value = Json::object();
    value["market"] = security.market;
    value["code"] = security.code;
    value["name"] = security.name;
    return value;
}

}  // namespace

std::vector<CandidateBar> candidate_bars_from_document(const Json& document) {
    const auto& rows = document.at("bars").as_array();
    std::vector<CandidateBar> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        const auto open = row.at("open").as_number();
        const auto close = row.at("close").as_number();
        const auto volume = row.at("volume").as_number();
        if (!std::isfinite(open) || !std::isfinite(close) ||
            !std::isfinite(volume))
            throw Error("candidate K-line contains a non-finite OHLCV value");
        result.push_back({static_cast<float>(open), static_cast<float>(close),
                          static_cast<float>(volume)});
    }
    return result;
}

Json scan_shape_matches(const fs::path& root, const ShapeMatchQuery& query,
                        const ShapeTemplate& shape, std::int32_t scope) {
    if (query.workers < 1 || query.workers > 16)
        throw Error("shape-match workers must be in 1..16");
    if (query.max_candidates < 1 || query.max_candidates > 10000)
        throw Error("shape-match max candidates must be in 1..10000");
    if (query.max_network_requests < 0 || query.max_network_requests > 10000)
        throw Error("shape-match max network requests must be in 0..10000");
    if (query.result_limit < 1 || query.result_limit > 10000)
        throw Error("shape-match result limit must be in 1..10000");

    Json universe = Json::object();
    auto securities = resolve_universe(root, query, scope, universe);
    const auto period = scan_period(query, shape);
    const auto required = std::max(1, shape.point_count);
    const int page_size = std::min(800, required);
    const int pages = (required + page_size - 1) / page_size;
    std::vector<ScanOutcome> outcomes;
    outcomes.reserve(securities.size());
    for (auto& security : securities) {
        ScanOutcome outcome;
        outcome.security = std::move(security);
        outcomes.push_back(std::move(outcome));
    }

    std::vector<std::size_t> pending;
    for (std::size_t index = 0; index < outcomes.size(); ++index) {
        auto& outcome = outcomes[index];
        if (outcome.security.embedded_kline) {
            outcome.kline = *outcome.security.embedded_kline;
            outcome.source = "input";
        } else {
            pending.push_back(index);
        }
    }

    if (query.source == "local" || query.source == "auto") {
        parallel_indices(pending, query.workers, [&](std::size_t index) {
            auto& outcome = outcomes[index];
            try {
                outcome.kline = load_local_kline_document(
                    root, outcome.security.market, outcome.security.code,
                    query.kind, period, pages, page_size, 0, "all");
                outcome.source = "local";
            } catch (const std::exception& error) {
                outcome.local_error = error.what();
            }
        });
    }

    std::vector<std::size_t> network;
    if (query.source == "network") {
        if (pending.size() > static_cast<std::size_t>(query.max_network_requests))
            throw Error("network scan candidate count exceeds --max-network-requests");
        network = pending;
    } else if (query.source == "auto") {
        for (const auto index : pending) {
            if (outcomes[index].kline) continue;
            if (network.size() < static_cast<std::size_t>(query.max_network_requests))
                network.push_back(index);
            else
                outcomes[index].error = query.max_network_requests == 0
                    ? outcomes[index].local_error
                    : "network request cap reached after local miss";
        }
    } else {
        for (const auto index : pending)
            if (!outcomes[index].kline) outcomes[index].error = outcomes[index].local_error;
    }
    parallel_indices(network, query.workers, [&](std::size_t index) {
        auto& outcome = outcomes[index];
        try {
            outcome.kline = fetch_kline_document(
                outcome.security.market, outcome.security.code, query.kind,
                period, pages, page_size, 0, "all", query.timeout_ms, root, {});
            outcome.source = query.source == "auto" ? "network-after-local-miss"
                                                     : "network";
            outcome.error.clear();
        } catch (const std::exception& error) {
            outcome.error = error.what();
        }
    });

    std::size_t loaded = 0;
    std::size_t eligible = 0;
    std::size_t matched = 0;
    for (auto& outcome : outcomes) {
        if (!outcome.kline) continue;
        try {
            outcome.score = score_template(
                shape, candidate_bars_from_document(*outcome.kline));
            ++loaded;
            if (outcome.score->at("eligible").as_bool()) ++eligible;
            if (outcome.score->at("matched").as_bool()) ++matched;
        } catch (const std::exception& error) {
            outcome.error = error.what();
            outcome.kline.reset();
        }
    }

    std::vector<const ScanOutcome*> ranked;
    for (const auto& outcome : outcomes) {
        if (!outcome.score) continue;
        if (!query.include_unmatched && !outcome.score->at("matched").as_bool())
            continue;
        ranked.push_back(&outcome);
    }
    auto numeric_score = [](const ScanOutcome* outcome) {
        const auto& value = outcome->score->at("score");
        return value.is_number() ? value.as_number()
                                 : -std::numeric_limits<double>::infinity();
    };
    std::sort(ranked.begin(), ranked.end(), [&](const auto* left, const auto* right) {
        const auto left_score = numeric_score(left);
        const auto right_score = numeric_score(right);
        if (left_score != right_score) return left_score > right_score;
        return std::tie(left->security.market, left->security.code) <
               std::tie(right->security.market, right->security.code);
    });

    Json results = Json::array();
    const auto returned = std::min<std::size_t>(
        ranked.size(), static_cast<std::size_t>(query.result_limit));
    for (std::size_t index = 0; index < returned; ++index) {
        const auto& outcome = *ranked[index];
        Json row = Json::object();
        row["security"] = result_security(outcome.security);
        row["source"] = outcome.source;
        row["score"] = *outcome.score;
        if (!outcome.local_error.empty()) row["local_error"] = outcome.local_error;
        results.push_back(std::move(row));
    }
    Json errors = Json::array();
    std::size_t error_count = 0;
    for (const auto& outcome : outcomes) {
        if (outcome.error.empty()) continue;
        ++error_count;
        if (errors.size() >= 100) continue;
        Json row = Json::object();
        row["security"] = result_security(outcome.security);
        row["error"] = outcome.error;
        if (!outcome.local_error.empty() && outcome.error != outcome.local_error)
            row["local_error"] = outcome.local_error;
        errors.push_back(std::move(row));
    }

    Json summary = Json::object();
    summary["candidate_count"] = static_cast<std::uint64_t>(outcomes.size());
    summary["loaded_count"] = static_cast<std::uint64_t>(loaded);
    summary["eligible_count"] = static_cast<std::uint64_t>(eligible);
    summary["matched_count"] = static_cast<std::uint64_t>(matched);
    summary["ranked_count"] = static_cast<std::uint64_t>(ranked.size());
    summary["returned_count"] = static_cast<std::uint64_t>(returned);
    summary["error_count"] = static_cast<std::uint64_t>(error_count);
    summary["errors_returned"] = static_cast<std::uint64_t>(errors.size());
    summary["truncated"] = ranked.size() > returned;

    Json result = Json::object();
    result["universe"] = std::move(universe);
    result["source"] = query.source;
    result["period"] = period;
    result["workers"] = query.workers;
    result["max_candidates"] = query.max_candidates;
    result["max_network_requests"] = query.max_network_requests;
    result["network_requests"] = static_cast<std::uint64_t>(network.size());
    result["include_unmatched"] = query.include_unmatched;
    result["summary"] = std::move(summary);
    result["results"] = std::move(results);
    result["errors"] = std::move(errors);
    return result;
}

}  // namespace tdx::shape_match_detail
