#pragma once

#include "tdx/json.hpp"
#include "tdx/stats.hpp"

#include <map>
#include <set>
#include <string>

namespace tdx {

struct FinanceEventValue {
    int days{};
    std::string event_date;
    std::string resource;
};

// FINANCE(90/91) use an inclusive natural-day counter: an event published
// today is 1, yesterday is 2, and no matching event is 0.
std::map<int, FinanceEventValue> finance_event_values_from_documents(
    const Json& documents,
    const std::string& market,
    const std::string& code,
    const std::set<int>& finance_ids,
    const std::string& today);

std::map<int, FinanceEventValue> finance_event_values_from_stats(
    const TdxStatsResource& resource,
    const std::string& market,
    const std::string& code,
    const std::set<int>& finance_ids,
    const std::string& today);

std::map<int, FinanceEventValue> fetch_finance_event_values(
    const std::string& market,
    const std::string& code,
    const std::set<int>& finance_ids,
    int timeout_ms = 15000);

std::string finance_event_today();

}  // namespace tdx
