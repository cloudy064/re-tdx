#include "recon_contract_market_data_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace tdx::recon_contract_detail {

namespace {

using SessionContractValidator = void (*)(const std::string& contract_id, const Json& document, Json& result);

void validate_limit_review_current(const std::string& contract_id, const Json& document,
        Json& result) {
    const std::string expected_view = contract_id == "limit-review-current-live"
        ? "current" : contract_id == "limit-review-history-live"
        ? "history" : contract_id == "limit-review-daily-live"
        ? "daily" : "security";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-limit-review-native-v1"),
                  "tdx-market-limit-review-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view", string_is(member(document, "view"), expected_view),
                  expected_view, value_or_null(member(document, "view")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    if (contract_id != "limit-review-security-live") {
        const auto* source_rows = member_path(document, {"counts", "source_rows"});
        add_assertion(result, "source_rows",
                      source_rows && source_rows->is_number() &&
                          source_rows->as_number() >= 1,
                      "number >= 1", value_or_null(source_rows));
        const auto* records = member(document, "records");
        const Json* row = records && records->is_array() &&
                                  !records->as_array().empty()
            ? &records->as_array().front() : nullptr;
        add_assertion(result, "record_present", row != nullptr, true, row != nullptr);
        const auto* raw = row ? member(*row, "raw") : nullptr;
        add_assertion(result, "raw_row_retained", raw && raw->is_object(), "object",
                      value_or_null(raw));
        if (contract_id == "limit-review-current-live") {
            const bool found = source_exists(document, "list/func_zdtfx101_1.jsn");
            add_assertion(result, "current_source", found, true, found);
        } else if (contract_id == "limit-review-history-live") {
            const bool found = source_exists(document, "list/func_zdtfx107_1.jsn");
            add_assertion(result, "history_source", found, true, found);
            const auto* date = row ? member(*row, "date") : nullptr;
            add_assertion(result, "date_yyyymmdd",
                          date && date->is_string() && date->as_string().size() == 8,
                          "YYYYMMDD", value_or_null(date));
            const auto* breadth = row ? member(*row, "limit_up") : nullptr;
            add_assertion(result, "limit_up_breadth",
                          breadth && breadth->is_object(), "object",
                          value_or_null(breadth));
        } else {
            add_assertion(result, "requested_date",
                          string_is(member(document, "date"), "20260731"),
                          "20260731", value_or_null(member(document, "date")));
            const bool up_source = source_exists(document, "zdtfx2/20260731.jsn");
            const bool down_source = source_exists(document, "zdtfx3/20260731.jsn");
            add_assertion(result, "daily_sources", up_source && down_source, true,
                          up_source && down_source);
        }
    } else {
        add_assertion(result, "security_code",
                      string_is(member(document, "code"), "000009"), "000009",
                      value_or_null(member(document, "code")));
        const auto* history = member(document, "history");
        const Json* row = history && history->is_array() &&
                                  !history->as_array().empty()
            ? &history->as_array().front() : nullptr;
        add_assertion(result, "history_present", row != nullptr, true, row != nullptr);
        const auto* raw = row ? member(*row, "raw") : nullptr;
        add_assertion(result, "raw_row_retained", raw && raw->is_object(), "object",
                      value_or_null(raw));
        const bool found = source_exists(document, "zdtfx1/0000009.jsn");
        add_assertion(result, "security_history_source", found, true, found);
    }
}

void validate_session_turnover_a(const std::string& contract_id, const Json& document,
        Json& result) {
    const bool etf = contract_id == "session-turnover-etf-live";
    const std::string expected_universe = etf ? "etf" : "a";
    const std::string expected_resource = etf
        ? "list/func_phcje104_1.jsn" : "list/func_phcje101_1.jsn";
    const std::string amount_field = etf
        ? "opening_turnover_yuan" : "after_hours_turnover_yuan";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-session-turnover-native-v1"),
                  "tdx-market-session-turnover-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "universe",
                  string_is(member(document, "universe"), expected_universe),
                  expected_universe, value_or_null(member(document, "universe")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    const auto* date = member(document, "statistics_date");
    add_assertion(result, "statistics_date",
                  date && date->is_string() && date->as_string().size() == 8,
                  "YYYYMMDD", value_or_null(date));
    const auto* source_rows = member_path(document, {"counts", "source_rows"});
    // The after-hours primary resource is a sparse A-share branch and can
    // legitimately contain only the currently eligible/non-empty rows.
    const double minimum_rows = etf ? 1000.0 : 2500.0;
    add_assertion(result, "market_coverage",
                  source_rows && source_rows->is_number() &&
                      source_rows->as_number() >= minimum_rows,
                  etf ? "number >= 1000" : "number >= 2500",
                  value_or_null(source_rows));
    const auto* records = member(document, "records");
    const Json* first = records && records->is_array() && records->size() >= 2
        ? &records->as_array()[0] : nullptr;
    const Json* second = records && records->is_array() && records->size() >= 2
        ? &records->as_array()[1] : nullptr;
    add_assertion(result, "records_present", first && second, "at least two rows",
                  records ? Json(static_cast<std::uint64_t>(records->size())) : Json(nullptr));
    const auto* first_amount = first ? member(*first, amount_field) : nullptr;
    const auto* second_amount = second ? member(*second, amount_field) : nullptr;
    add_assertion(result, "amount_yuan",
                  first_amount && first_amount->is_number(), "number",
                  value_or_null(first_amount));
    add_assertion(result, "descending_primary_sort",
                  first_amount && second_amount && first_amount->is_number() &&
                      second_amount->is_number() &&
                      first_amount->as_number() >= second_amount->as_number(),
                  true,
                  first_amount && second_amount && first_amount->is_number() &&
                      second_amount->is_number()
                      ? Json(first_amount->as_number() >= second_amount->as_number())
                      : Json(nullptr));
    const auto* raw = first ? member(*first, "raw") : nullptr;
    add_assertion(result, "raw_row_retained", raw && raw->is_object(), "object",
                  value_or_null(raw));
    const auto* source = member(document, "source");
    add_assertion(result, "source_resource",
                  source && string_is(member(*source, "resource"), expected_resource),
                  expected_resource,
                  source ? value_or_null(member(*source, "resource")) : Json(nullptr));
    const auto* attempts = source ? member(*source, "attempts") : nullptr;
    const auto* stale = source ? member(*source, "stale") : nullptr;
    add_assertion(result, "source_health",
                  attempts && attempts->is_number() && attempts->as_number() >= 1 &&
                      stale && stale->is_bool(), true,
                  attempts && stale ? Json(true) : Json(false));
}

struct SessionContract {
    std::string_view id;
    SessionContractValidator validate;
};

constexpr std::array<SessionContract, 6> session_contracts{{
    {"limit-review-current-live", validate_limit_review_current},
    {"limit-review-history-live", validate_limit_review_current},
    {"limit-review-daily-live", validate_limit_review_current},
    {"limit-review-security-live", validate_limit_review_current},
    {"session-turnover-a-live", validate_session_turnover_a},
    {"session-turnover-etf-live", validate_session_turnover_a},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < session_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < session_contracts.size(); ++right)
            if (session_contracts[left].id == session_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_market_data_session_contract(const std::string& contract_id,
    const Json& document, const Json& context, Json& result) {
    (void)context;
    for (const auto& contract : session_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(contract_id, document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail