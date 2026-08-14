#pragma once

#include "tdx/state_owned_reform.hpp"

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace tdx::state_owned_detail {

using SecurityMap = std::map<std::pair<int, std::string>, Security>;

enum class ViewKind {
    groups,
    group,
    security,
    restructuring,
    catalog,
};

struct ViewDefinition {
    std::string_view name;
    ViewKind kind;
};

const std::array<ViewDefinition, 5>& view_definitions();
const ViewDefinition* find_view(std::string_view name);
const std::string& restructuring_resource();
const StateOwnedDimension& find_dimension(const std::string& value);

enum class GroupSortKind { count, name, id };
enum class ReformSortKind { date, profit, control, code };

GroupSortKind group_sort_kind(std::string_view name);
ReformSortKind reform_sort_kind(std::string_view name);
bool descending_order(std::string_view name);

const Json* value_ptr(const Json& value, std::string_view key);
std::string text_value(const Json& value, std::string_view key);
std::optional<double> number_value(const Json& value, std::string_view key);
std::optional<double> json_number(const Json& value, std::string_view key);
Json number_json(const std::optional<double>& value);

bool digits(const std::string& value, std::size_t size);
std::string iso_date(const std::string& value);
int market_id(std::string value);
int inferred_market(const std::string& raw, const std::string& code);
std::string market_name(int id);
std::string market_prefix(int id);
Json security_document(int id, const std::string& code,
                       const SecurityMap& securities);

const Json* find_quote(const Json& quote_rows, int market,
                       const std::string& code);
Json quote_metric(const Json* quote, std::string_view key);
Json return_metric(const Json* quote, const std::optional<double>& reference);

std::string now_text();
int bounded(const std::string& text, const std::string& name,
            int minimum, int maximum);
std::filesystem::path native_path(const std::string& value);
const Json& document_for_resource(const Json& documents,
                                  const std::string& resource);
bool json_contains(const Json& value, const std::string& needle);
Json group_reference(const Json& group);
Json quote_source_metadata(const Json& document, bool refreshed);
void append_error(Json& errors, const std::string& resource,
                  const std::string& message);

void sort_groups(Json& rows, GroupSortKind sort, bool descending);
void sort_restructuring(Json& rows, ReformSortKind sort, bool descending);
Json page_rows(const Json& rows, int offset, int limit);

struct QueryOutput {
    Json groups{Json::array()};
    Json details{Json::array()};
    Json restructuring{Json::array()};
    Json errors{Json::array()};
    Json quote_source{Json(nullptr)};
    Json sources{Json::array()};
    Json summary{Json::object()};
    bool master_refreshed{};
    bool detail_refreshed{};
    bool quote_refreshed{};
    int master_age{};
    int detail_age{};
    int quote_age{};
    std::uint64_t matched{};
};

Json compose_response(const StateOwnedReformQuery& options,
                      QueryOutput output);

}  // namespace tdx::state_owned_detail
