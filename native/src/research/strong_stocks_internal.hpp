#pragma once

#include "tdx/strong_stocks.hpp"

#include <filesystem>
#include <optional>
#include <string_view>

namespace tdx::detail::strong_stocks {

enum class View { intervals, security, detail, catalog };
enum class SortFamily { interval, detail, none };

struct ViewDefinition {
    View view;
    std::string_view name;
    std::string_view label;
    std::string_view resource;
    std::string_view fields;
    std::string_view default_sort;
    std::string_view default_order;
    SortFamily sort_family;
};

const ViewDefinition& view_definition(std::string_view name);
bool valid_view(std::string_view name);
bool valid_sort(SortFamily family, std::string_view name);
std::string_view main_resource();
Json catalog_rows();

const Json* value_ptr(const Json& value, std::string_view key);
std::string text_value(const Json& value, std::string_view key);
std::optional<double> number_value(const Json& value, std::string_view key);
Json number_json(const std::optional<double>& value);
std::optional<double> json_number(const Json& value, std::string_view key);
std::string json_text(const Json& value, std::string_view key);
bool digits(const std::string& value, std::size_t size);
std::string iso_date(const std::string& value);
std::string compact_date(std::string value, const std::string& name);
int market_id(std::string value);
std::string market_name(int id);
Json security_document(int id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);
std::optional<int> positive_integer_prefix(const std::string& value,
                                            const std::string& suffix);
bool json_contains(const Json& value, const std::string& needle);
std::string now_text();
int bounded(const std::string& text, const std::string& name,
            int minimum, int maximum);
std::filesystem::path native_path(const std::string& value);
Json interval_summary(const Json& rows);
Json detail_summary(const Json& rows, const Json& interval);
std::optional<double> sort_number(const Json& row, const std::string& sort);

}  // namespace tdx::detail::strong_stocks

