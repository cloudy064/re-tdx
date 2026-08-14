#include "state_owned_reform_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <vector>

namespace tdx {
namespace {

bool is_security(const Json& row, int market, const std::string& code) {
    const auto& security = row.at("security");
    return static_cast<int>(security.at("market_id").as_number()) == market &&
        security.at("code").as_string() == code;
}

void add_catalog_rows(state_owned_detail::QueryOutput& output) {
    for (const auto& dimension : state_owned_dimensions()) {
        Json row = Json::object();
        row["dimension"] = dimension.name;
        row["label"] = dimension.label;
        row["resource"] = dimension.resource;
        row["unit_id"] = dimension.unit_id;
        output.groups.push_back(std::move(row));
    }
    Json row = Json::object();
    row["dimension"] = "restructuring";
    row["label"] = "重组预期";
    row["resource"] = state_owned_detail::restructuring_resource();
    row["unit_id"] = "6";
    output.groups.push_back(std::move(row));
    output.matched = output.groups.size();
}

}  // namespace

Json StateOwnedReformService::query(const StateOwnedReformQuery& input) {
    using namespace state_owned_detail;
    StateOwnedReformQuery options = input;
    options.view = lower_ascii(trim(options.view));
    options.dimension = lower_ascii(trim(options.dimension));
    options.group_id = trim(options.group_id);
    options.market = lower_ascii(trim(options.market));
    options.code = trim(options.code);
    options.query = trim(options.query);
    options.sort = lower_ascii(trim(options.sort));
    options.order = lower_ascii(trim(options.order));
    const auto* view = find_view(options.view);
    if (!view)
        throw Error("view must be groups, group, security, restructuring, or catalog");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    int selected_market = -1;
    if (!options.market.empty()) {
        selected_market = market_id(options.market);
        options.market = market_name(selected_market);
        if (!digits(options.code, 6)) throw Error("code must contain six digits");
    }
    if (view->kind == ViewKind::group && options.group_id.empty())
        throw Error("group view requires group-id");
    if (view->kind == ViewKind::security && selected_market < 0)
        throw Error("security view requires market and code");
    if (options.offset < 0 || options.offset > 1000000 ||
        options.limit < 1 || options.limit > 5000 ||
        options.detail_limit < 1 || options.detail_limit > 5000)
        throw Error("state-owned reform pagination is invalid");
    if (options.master_cache_ttl_seconds < 0 ||
        options.master_cache_ttl_seconds > 86400 ||
        options.detail_cache_ttl_seconds < 0 ||
        options.detail_cache_ttl_seconds > 86400 ||
        options.quote_cache_ttl_seconds < 0 ||
        options.quote_cache_ttl_seconds > 3600 ||
        options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("state-owned reform cache or timeout value is invalid");

    QueryOutput output;
    if (view->kind == ViewKind::catalog) {
        add_catalog_rows(output);
        return compose_response(options, std::move(output));
    }

    auto master = fetch_master(options);
    output.master_refreshed = master.refreshed;
    output.master_age = master.age_seconds;
    output.sources = master.document.at("sources");
    output.summary = master.document.at("summary");
    const auto needle = lower_ascii(options.query);

    switch (view->kind) {
    case ViewKind::groups: {
        const auto& dimension = find_dimension(options.dimension);
        for (const auto& group :
             master.document.at("groups").at(dimension.name).as_array()) {
            if (needle.empty() || json_contains(group, needle))
                output.groups.push_back(group);
        }
        sort_groups(output.groups, group_sort_kind(options.sort),
                    descending_order(options.order));
        output.matched = output.groups.size();
        output.groups = page_rows(output.groups, options.offset, options.limit);
        break;
    }
    case ViewKind::group: {
        const Json* selected = nullptr;
        for (const auto& dimension : state_owned_dimensions()) {
            for (const auto& group :
                 master.document.at("groups").at(dimension.name).as_array()) {
                if (group.at("group_id").as_string() == options.group_id)
                    selected = &group;
            }
        }
        if (!selected) break;
        output.groups.push_back(*selected);
        output.matched = 1;
        if (!options.include_details) break;
        const auto resource = selected->at("detail_resource").as_string();
        try {
            auto detail = fetch_resource(resource, options);
            output.detail_refreshed = detail.refreshed;
            output.detail_age = detail.age_seconds;
            output.sources.push_back(jsn_source_metadata(detail.document));
            Json quotes = Json::array();
            if (options.include_quotes) {
                std::vector<std::string> requested;
                for (const auto& raw : detail.document.at("rows").as_array()) {
                    const auto code = text_value(raw, "$ZQDM");
                    if (!digits(code, 6)) continue;
                    requested.push_back(market_name(inferred_market(
                        text_value(raw, "$SC"), code)) + ":" + code);
                }
                try {
                    auto quote = fetch_quotes(requested, options);
                    output.quote_refreshed = quote.refreshed;
                    output.quote_age = quote.age_seconds;
                    quotes = quote.document.at("records");
                    output.quote_source = quote_source_metadata(
                        quote.document, quote.refreshed);
                } catch (const std::exception& error) {
                    append_error(output.errors, "public-l1-snapshot", error.what());
                }
            }
            output.details = normalize_state_owned_details(
                detail.document.at("rows"), quotes, blocks_.securities);
            if (output.details.size() >
                static_cast<std::size_t>(options.detail_limit)) {
                output.details.as_array().resize(
                    static_cast<std::size_t>(options.detail_limit));
            }
        } catch (const std::exception& error) {
            append_error(output.errors, resource, error.what());
        }
        break;
    }
    case ViewKind::security: {
        for (const auto& dimension : state_owned_dimensions()) {
            for (const auto& group :
                 master.document.at("groups").at(dimension.name).as_array()) {
                const auto& members = group.at("members").as_array();
                const bool member = std::any_of(
                    members.begin(), members.end(), [&](const Json& security) {
                        return static_cast<int>(
                            security.at("market_id").as_number()) ==
                                selected_market &&
                            security.at("code").as_string() == options.code;
                    });
                if (!member) continue;
                output.groups.push_back(group_reference(group));
                if (!options.include_details) continue;
                const auto resource = group.at("detail_resource").as_string();
                try {
                    auto detail = fetch_resource(resource, options);
                    output.detail_refreshed =
                        output.detail_refreshed || detail.refreshed;
                    output.detail_age = std::max(
                        output.detail_age, detail.age_seconds);
                    output.sources.push_back(jsn_source_metadata(detail.document));
                    auto normalized = normalize_state_owned_details(
                        detail.document.at("rows"), Json::array(),
                        blocks_.securities);
                    for (auto& row : normalized.as_array()) {
                        if (!is_security(row, selected_market, options.code))
                            continue;
                        row["group"] = group_reference(group);
                        output.details.push_back(std::move(row));
                    }
                } catch (const std::exception& error) {
                    append_error(output.errors, resource, error.what());
                }
            }
        }
        for (const auto& row :
             master.document.at("restructuring").as_array()) {
            if (is_security(row, selected_market, options.code))
                output.restructuring.push_back(row);
        }
        output.matched = output.groups.size() + output.restructuring.size();
        if (options.include_quotes &&
            (output.details.size() || output.restructuring.size())) {
            try {
                auto quote = fetch_quotes(
                    {options.market + ":" + options.code}, options);
                output.quote_refreshed = quote.refreshed;
                output.quote_age = quote.age_seconds;
                output.quote_source = quote_source_metadata(
                    quote.document, quote.refreshed);
                const auto quotes = quote.document.at("records");
                for (auto& row : output.details.as_array()) {
                    Json one = Json::array();
                    one.push_back(row.at("raw"));
                    auto normalized = normalize_state_owned_details(
                        one, quotes, blocks_.securities);
                    if (normalized.size()) {
                        normalized.as_array()[0]["group"] = row.at("group");
                        row = normalized.as_array()[0];
                    }
                }
                Json raw = Json::array();
                for (const auto& row : output.restructuring.as_array())
                    raw.push_back(row.at("raw"));
                output.restructuring = normalize_state_owned_restructuring(
                    raw, quotes, blocks_.securities);
            } catch (const std::exception& error) {
                append_error(output.errors, "public-l1-snapshot", error.what());
            }
        }
        break;
    }
    case ViewKind::restructuring: {
        for (const auto& row :
             master.document.at("restructuring").as_array()) {
            const auto& security = row.at("security");
            if (selected_market >= 0 && static_cast<int>(
                    security.at("market_id").as_number()) != selected_market)
                continue;
            if (!options.code.empty() &&
                security.at("code").as_string() != options.code)
                continue;
            if (!needle.empty() && !json_contains(row, needle)) continue;
            output.restructuring.push_back(row);
        }
        sort_restructuring(output.restructuring,
            reform_sort_kind(options.sort), descending_order(options.order));
        output.matched = output.restructuring.size();
        output.restructuring = page_rows(
            output.restructuring, options.offset, options.limit);
        if (!options.include_quotes || !output.restructuring.size()) break;
        std::vector<std::string> requested;
        Json raw = Json::array();
        for (const auto& row : output.restructuring.as_array()) {
            const auto& security = row.at("security");
            requested.push_back(security.at("market").as_string() + ":" +
                                security.at("code").as_string());
            raw.push_back(row.at("raw"));
        }
        try {
            auto quote = fetch_quotes(requested, options);
            output.quote_refreshed = quote.refreshed;
            output.quote_age = quote.age_seconds;
            output.quote_source = quote_source_metadata(
                quote.document, quote.refreshed);
            output.restructuring = normalize_state_owned_restructuring(
                raw, quote.document.at("records"), blocks_.securities);
        } catch (const std::exception& error) {
            append_error(output.errors, "public-l1-snapshot", error.what());
        }
        break;
    }
    case ViewKind::catalog:
        break;
    }
    return compose_response(options, std::move(output));
}

}  // namespace tdx
