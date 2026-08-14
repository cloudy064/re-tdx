#include "block_trades_internal.hpp"

#include "tdx/jsn.hpp"

#include <algorithm>
#include <ctime>
#include <set>
#include <utility>

namespace tdx {
namespace bt_detail = block_trade_detail;

BlockTradeService::BlockTradeService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

BlockTradeService::FetchResult BlockTradeService::fetch_core(
    const BlockTradeQuery& options) {
    const auto now = std::time(nullptr);
    const int age = core_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(
              0, now - core_cache_.fetched_at)) : 0;
    if (!options.refresh && core_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {core_cache_.document, false, age};

    const auto documents = fetch_jsn_resources_rows(
        bt_detail::core_resource_names(), "bi", options.timeout_ms);
    const auto& month_source = bt_detail::document_for_resource(
        documents,
        bt_detail::core_resource(bt_detail::CoreDataset::month).resource);
    const auto& trade_source = bt_detail::document_for_resource(
        documents,
        bt_detail::core_resource(bt_detail::CoreDataset::trades).resource);
    const auto& intention_source = bt_detail::document_for_resource(
        documents,
        bt_detail::core_resource(bt_detail::CoreDataset::intentions).resource);
    Json document = Json::object();
    document["monthly"] = normalize_block_trade_month_rows(
        month_source.at("rows"));
    document["trades"] = normalize_block_trade_rows(
        trade_source.at("rows"), securities_);
    document["intentions"] = normalize_block_trade_intention_rows(
        intention_source.at("rows"), securities_);

    double trade_amount = 0.0, intention_amount = 0.0;
    std::set<std::string> trade_securities, intention_securities;
    for (const auto& row : document.at("trades").as_array()) {
        trade_amount += bt_detail::number_value(
            row, "amount_yuan").value_or(0.0);
        trade_securities.insert(bt_detail::text_value(
            row.at("security"), "security_id"));
    }
    for (const auto& row : document.at("intentions").as_array()) {
        intention_amount += bt_detail::number_value(
            row, "amount_yuan").value_or(0.0);
        intention_securities.insert(bt_detail::text_value(
            row.at("security"), "security_id"));
    }
    Json summary = Json::object();
    summary["monthly_points"] = static_cast<std::uint64_t>(
        document.at("monthly").size());
    summary["recent_trades"] = static_cast<std::uint64_t>(
        document.at("trades").size());
    summary["recent_securities"] = static_cast<std::uint64_t>(
        trade_securities.size());
    summary["recent_amount_yuan"] = trade_amount;
    summary["intention_rows"] = static_cast<std::uint64_t>(
        document.at("intentions").size());
    summary["intention_securities"] = static_cast<std::uint64_t>(
        intention_securities.size());
    summary["intention_amount_yuan"] = intention_amount;
    document["summary"] = std::move(summary);
    Json sources = Json::array();
    sources.push_back(bt_detail::source_summary(month_source));
    sources.push_back(bt_detail::source_summary(trade_source));
    sources.push_back(bt_detail::source_summary(intention_source));
    document["sources"] = std::move(sources);
    core_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

BlockTradeService::FetchResult BlockTradeService::fetch_resource(
    const std::string& resource, const BlockTradeQuery& options) {
    const auto now = std::time(nullptr);
    for (auto item = resource_cache_.begin();
         item != resource_cache_.end();) {
        if (now - item->second.fetched_at >=
            options.detail_cache_ttl_seconds)
            item = resource_cache_.erase(item);
        else
            ++item;
    }
    for (auto item = failure_cache_.begin();
         item != failure_cache_.end();) {
        if (now - item->second.fetched_at >=
            options.detail_cache_ttl_seconds)
            item = failure_cache_.erase(item);
        else
            ++item;
    }
    if (options.refresh) {
        resource_cache_.erase(resource);
        failure_cache_.erase(resource);
    }
    const auto cached = resource_cache_.find(resource);
    if (cached != resource_cache_.end()) {
        const int age = static_cast<int>(std::max<std::time_t>(
            0, now - cached->second.fetched_at));
        return {cached->second.document, false, age};
    }
    const auto failure = failure_cache_.find(resource);
    if (failure != failure_cache_.end()) throw Error(failure->second.message);
    try {
        const auto source = fetch_jsn_resource_rows(
            resource, "bi", options.timeout_ms);
        resource_cache_[resource] = {source, std::time(nullptr)};
        return {source, true, 0};
    } catch (const std::exception& error) {
        failure_cache_[resource] = {error.what(), std::time(nullptr)};
        throw;
    }
}

}  // namespace tdx
