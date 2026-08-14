#include "recon_contract_market_research_internal.hpp"

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

using EventContractValidator = void (*)(const std::string& contract_id, const Json& document, Json& result);

void validate_commodity_links_commodities(const std::string& contract_id, const Json& document,
        Json& result) {
    const bool list = contract_id == "commodity-links-commodities-live";
    const bool commodity = contract_id == "commodity-links-commodity-live";
    const auto expected_view = list ? "commodities" : commodity ? "commodity" : "theme";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-commodity-links-native-v1"),
                  "tdx-market-commodity-links-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view",
                  string_is(member(document, "view"), expected_view),
                  expected_view, value_or_null(member(document, "view")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    const auto* summary = member(document, "summary");
    const auto* records = member(document, "records");
    const Json* first = records && records->is_array() && records->size()
        ? &records->as_array()[0] : nullptr;
    if (list) {
        const auto* quotes = summary ? member(*summary, "quote_rows") : nullptr;
        const auto* unique = summary ? member(*summary, "unique_commodity_ids") : nullptr;
        add_assertion(result, "commodity_population",
                      quotes && quotes->is_number() && quotes->as_number() >= 200 &&
                          unique && unique->is_number() && unique->as_number() >= 200,
                      "at least 200 quote rows and relation ids", value_or_null(summary));
        const auto* id = first ? member(*first, "commodity_id") : nullptr;
        const auto* quote_id = first ? member(*first, "quote_id") : nullptr;
        const auto* latest = first ? member(*first, "latest_price") : nullptr;
        const auto* date = first ? member(*first, "quote_date") : nullptr;
        const auto* raw = first ? member(*first, "raw") : nullptr;
        add_assertion(result, "typed_quote",
                      id && id->is_string() && !id->as_string().empty() &&
                          quote_id && quote_id->is_string() && latest && latest->is_number() &&
                          date && date->is_string() && date->as_string().size() == 10 &&
                          raw && raw->is_object(),
                      "relation id, unique quote id, price, date and raw row",
                      value_or_null(first));
        bool sorted = true;
        if (records && records->is_array()) {
            for (std::size_t i = 1; i < records->size(); ++i) {
                const auto* previous = member(records->as_array()[i - 1], "quote_date");
                const auto* current = member(records->as_array()[i], "quote_date");
                sorted = sorted && previous && current && previous->is_string() &&
                    current->is_string() && previous->as_string() >= current->as_string();
            }
        }
        add_assertion(result, "descending_quote_date", sorted, true, sorted);
    } else if (commodity) {
        const auto* id = first ? member(*first, "commodity_id") : nullptr;
        const auto* quotes = first ? member(*first, "quotes") : nullptr;
        const auto* stocks = first ? member(*first, "stocks") : nullptr;
        const auto* related = first ? member(*first, "related_securities") : nullptr;
        add_assertion(result, "commodity_relationship",
                      string_is(id, "X100102003") && quotes && quotes->is_array() &&
                          quotes->size() >= 1 && stocks && stocks->is_array() &&
                          stocks->size() >= 1 && related && related->is_array() &&
                          related->size() >= 1,
                      "selected quote(s), stocks and industry/ETF securities",
                      value_or_null(first));
        const Json* stock = stocks && stocks->is_array() && stocks->size()
            ? &stocks->as_array()[0] : nullptr;
        add_assertion(result, "host_quote_boundary",
                      stock && member(*stock, "return_since_reference_pct") &&
                          member(*stock, "return_since_reference_pct")->is_null() &&
                          bool_is(member(*stock, "current_quote_available"), false),
                      "missing host quote retained as null", value_or_null(stock));
    } else {
        const auto* id = first ? member(*first, "theme_id") : nullptr;
        const auto* stocks = first ? member(*first, "stocks") : nullptr;
        const auto* drivers = first ? member(*first, "drivers") : nullptr;
        const auto* latest = first ? member(*first, "latest_driver_date") : nullptr;
        add_assertion(result, "theme_relationship",
                      string_is(id, "70") && stocks && stocks->is_array() &&
                          stocks->size() >= 10 && drivers && drivers->is_array() &&
                          drivers->size() >= 5 && latest && latest->is_string() &&
                          latest->as_string().size() == 10,
                      "theme 70 with stocks, drivers and latest driver date",
                      value_or_null(first));
    }
    const auto* sources = member(document, "sources");
    const std::size_t expected_sources = list ? 1 : 3;
    bool healthy = sources && sources->is_array() && sources->size() == expected_sources;
    if (sources && sources->is_array()) {
        for (const auto& source : sources->as_array())
            healthy = healthy && member(source, "attempts") &&
                member(source, "attempts")->is_number() &&
                member(source, "attempts")->as_number() >= 1 &&
                member(source, "stale") && member(source, "stale")->is_bool();
    }
    bool exact = list || commodity
        ? source_exists(document, "list/func_zjtc103_1.jsn")
        : source_exists(document, "list/func_zjtc101_1.jsn");
    if (commodity)
        exact = exact && source_exists(document, "zjtc4/X100102003.jsn") &&
            source_exists(document, "zjtc5/X100102003.jsn");
    if (!list && !commodity)
        exact = exact && source_exists(document, "zjtc1/70.jsn") &&
            source_exists(document, "zjtc2/70.jsn");
    add_assertion(result, "exact_sources", exact, true, exact);
    add_assertion(result, "source_health", healthy, true, healthy);
}

void validate_announcement_signals_selected(const std::string& contract_id, const Json& document,
        Json& result) {
    const bool selected = contract_id == "announcement-signals-selected-live";
    const bool risks = contract_id == "announcement-signals-risks-live";
    const auto expected_view = selected ? "selected" : risks ? "risks" : "history";
    const auto expected_resource = selected ? "list/func_zxjx101_1.jsn" :
        risks ? "list/func_zxjx103_1.jsn" : "ggjx/0000921.jsn";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-announcement-signals-native-v1"),
                  "tdx-market-announcement-signals-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view", string_is(member(document, "view"), expected_view),
                  expected_view, value_or_null(member(document, "view")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    const auto* records = member(document, "records");
    const Json* first = records && records->is_array() && records->size()
        ? &records->as_array()[0] : nullptr;
    const auto* security = first ? member(*first, "security") : nullptr;
    const auto* date = first ? member(*first, "date") : nullptr;
    const auto* title = first ? member(*first, "title") : nullptr;
    const auto* pdf = first ? member(*first, "pdf_url") : nullptr;
    const auto* direction = first ? member(*first, "direction") : nullptr;
    const auto* type = first ? member(*first, "announcement_type") : nullptr;
    const auto* raw = first ? member(*first, "raw") : nullptr;
    const bool typed = security && security->is_object() &&
        member(*security, "code") && member(*security, "code")->is_string() &&
        member(*security, "code")->as_string().size() == 6 &&
        date && date->is_string() && date->as_string().size() == 10 &&
        title && title->is_string() && !title->as_string().empty() &&
        pdf && pdf->is_string() && pdf->as_string().rfind("http", 0) == 0 &&
        direction && direction->is_string() && type && type->is_string() &&
        raw && raw->is_object();
    add_assertion(result, "typed_announcement", typed,
                  "security/date/title/pdf/direction/type/raw", value_or_null(first));
    bool semantics = false;
    if (first) {
        const auto* recent3 = member(*first, "recent_3d_return_pct");
        const auto* recent10 = member(*first, "recent_10d_return_pct");
        const auto* pre3 = member(*first, "pre_3d_return_pct");
        const auto* post3 = member(*first, "post_3d_return_pct");
        semantics = selected || risks
            ? recent3 && recent3->is_number() && recent10 && recent10->is_number() &&
                pre3 && pre3->is_null() && post3 && post3->is_null()
            : recent3 && recent3->is_null() && recent10 && recent10->is_null() &&
                pre3 && (pre3->is_number() || pre3->is_null()) &&
                post3 && (post3->is_number() || post3->is_null());
    }
    add_assertion(result, "return_semantics", semantics,
                  selected || risks ? "recent returns only" : "pre/post-event returns only",
                  semantics);
    bool sorted = true;
    if (records && records->is_array()) {
        for (std::size_t i = 1; i < records->size(); ++i) {
            const auto* previous = member(records->as_array()[i - 1], "date");
            const auto* current = member(records->as_array()[i], "date");
            sorted = sorted && previous && current && previous->is_string() &&
                current->is_string() && previous->as_string() >= current->as_string();
        }
    }
    add_assertion(result, "descending_date", sorted, true, sorted);
    const auto* sources = member(document, "sources");
    const bool healthy = sources && sources->is_array() && sources->size() == 1 &&
        source_exists(document, expected_resource) &&
        member(sources->as_array()[0], "attempts") &&
        member(sources->as_array()[0], "attempts")->is_number() &&
        member(sources->as_array()[0], "attempts")->as_number() >= 1 &&
        member(sources->as_array()[0], "stale") &&
        member(sources->as_array()[0], "stale")->is_bool();
    add_assertion(result, "exact_healthy_source", healthy, expected_resource, healthy);
}

void validate_reverse_repo_rates(const std::string& contract_id, const Json& document,
        Json& result) {
    const bool quotes = contract_id == "reverse-repo-rates-live";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-reverse-repo-native-v1"),
                  "tdx-market-reverse-repo-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view", string_is(member(document, "view"), "rates"),
                  "rates", value_or_null(member(document, "view")));
    const auto* availability = member(document, "availability");
    const bool available = quotes
        ? string_is(availability, "live") || string_is(availability, "stale-cache")
        : string_is(availability, "schedule-only") || string_is(availability, "stale-cache");
    add_assertion(result, "availability", available,
                  quotes ? "live or stale-cache" : "schedule-only or stale-cache",
                  value_or_null(availability));
    const auto* rows = member_path(document, {"summary", "rows"});
    const auto* quoted = member_path(document, {"summary", "quoted_rows"});
    const auto* schedule_date = member_path(document, {"summary", "schedule_date"});
    const bool population = rows && rows->is_number() && rows->as_number() == 18 &&
        quoted && quoted->is_number() && quoted->as_number() == (quotes ? 18 : 0) &&
        schedule_date && schedule_date->is_string() && schedule_date->as_string().size() == 10;
    add_assertion(result, "repo_population", population,
                  quotes ? "18 rows, 18 quotes" : "18 rows, 0 quotes", population);
    const auto* records = member(document, "records");
    const Json* first = records && records->is_array() && records->size()
        ? &records->as_array()[0] : nullptr;
    const auto* security = first ? member(*first, "security") : nullptr;
    const auto* term = first ? member(*first, "term_days") : nullptr;
    const auto* interest_days = first ? member(*first, "interest_days") : nullptr;
    const auto* fee = first ? member(*first, "fee_yuan") : nullptr;
    const auto* available_date = first ? member(*first, "funds_available_date") : nullptr;
    const auto* withdrawable = first ? member(*first, "funds_withdrawable_date") : nullptr;
    const bool typed = security && security->is_object() &&
        member(*security, "category") && string_is(member(*security, "category"), "repo") &&
        term && term->is_number() && term->as_number() >= 1 &&
        interest_days && interest_days->is_number() &&
        interest_days->as_number() >= term->as_number() &&
        fee && fee->is_number() && available_date && available_date->is_string() &&
        available_date->as_string().size() == 10 && withdrawable &&
        withdrawable->is_string() && withdrawable->as_string().size() == 10;
    add_assertion(result, "typed_schedule", typed,
                  "repo identity, term/interest days, fee and settlement dates",
                  value_or_null(first));
    if (quotes) {
        const auto* rate = first ? member(*first, "annualized_rate_pct") : nullptr;
        const auto* gross = first ? member(*first, "gross_interest_yuan") : nullptr;
        const auto* net = first ? member(*first, "net_interest_yuan") : nullptr;
        const auto* net_rate = first ? member(*first, "net_annualized_rate_pct") : nullptr;
        const bool scale = rate && rate->is_number() && rate->as_number() > 0 &&
            rate->as_number() < 20 && gross && gross->is_number() && net && net->is_number() &&
            net->as_number() < gross->as_number() && net_rate && net_rate->is_number() &&
            net_rate->as_number() < rate->as_number();
        add_assertion(result, "rate_scale_and_net_formula", scale,
                      "annualized repo percent below 20 and net below gross", scale);
        bool sorted = true;
        if (records && records->is_array()) {
            for (std::size_t i = 1; i < records->size(); ++i) {
                const auto* previous = member(records->as_array()[i - 1], "net_annualized_rate_pct");
                const auto* current = member(records->as_array()[i], "net_annualized_rate_pct");
                sorted = sorted && previous && current && previous->is_number() &&
                    current->is_number() && previous->as_number() >= current->as_number();
            }
        }
        add_assertion(result, "descending_net_rate", sorted, true, sorted);
        const auto* command = member_path(document, {"quote_source", "command"});
        const auto* received = member_path(document, {"quote_source", "received"});
        add_assertion(result, "l1_quote_source",
                      string_is(command, "0x054C") && received && received->is_number() &&
                          received->as_number() == 18,
                      "0x054C with 18 snapshots", value_or_null(member(document, "quote_source")));
        const auto* transport = member_path(document, {"quote_source", "transport"});
        const auto* attempts = transport
            ? member(*transport, "connection_attempts") : nullptr;
        const auto* retries = transport
            ? member(*transport, "transient_retries") : nullptr;
        const bool bounded_retry = transport && transport->is_object() &&
            attempts && attempts->is_number() && attempts->as_number() >= 1 &&
            attempts->as_number() <= 9 && retries && retries->is_number() &&
            retries->as_number() >= 0 && retries->as_number() <= 6 &&
            number_is(member(*transport, "max_attempts_per_endpoint"), 3) &&
            string_is(member(*transport, "endpoint_source"),
                      "connect.cfg:hqhost-primary-first") &&
            bool_is(member(*transport, "primary_configured"), true);
        add_assertion(result, "bounded_transport_retry", bounded_retry,
                      "connect.cfg primary-first, attempts 1..9 and retries 0..6",
                      value_or_null(transport));
    } else {
        const auto* rate = first ? member(*first, "annualized_rate_pct") : nullptr;
        const auto* quote_source = member(document, "quote_source");
        add_assertion(result, "calendar_only_boundary",
                      rate && rate->is_null() && quote_source && quote_source->is_null() &&
                          term && term->is_number() && term->as_number() == 1,
                      "null quote fields, null quote source and ascending one-day term",
                      value_or_null(first));
    }
    const auto* sources = member(document, "sources");
    const bool healthy = sources && sources->is_array() && sources->size() == 1 &&
        source_exists(document, "list/func_gznhg100_1.jsn") &&
        member(sources->as_array()[0], "attempts") &&
        member(sources->as_array()[0], "attempts")->is_number() &&
        member(sources->as_array()[0], "attempts")->as_number() >= 1;
    add_assertion(result, "exact_schedule_source", healthy,
                  "list/func_gznhg100_1.jsn", healthy);
}

void validate_tender_offers(const std::string& contract_id, const Json& document,
        Json& result) {
    const bool security_mode = contract_id == "stock-tender-offers-live";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-tender-offers-native-v1"),
                  "tdx-market-tender-offers-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available,
                  "live or stale-cache", value_or_null(availability));
    const auto* records = member(document, "records");
    const Json* first = records && records->is_array() && records->size()
        ? &records->as_array()[0] : nullptr;
    const auto* security = first ? member(*first, "security") : nullptr;
    const auto* announcement = first ? member(*first, "announcement_date") : nullptr;
    const auto* status = first ? member(*first, "status") : nullptr;
    const auto* category = first ? member(*first, "status_category") : nullptr;
    const auto* planned = first ? member(*first, "planned_shares") : nullptr;
    const auto* planned_10k = first ? member(*first, "planned_shares_10k") : nullptr;
    const auto* funds = first ? member(*first, "planned_funds_yuan") : nullptr;
    const auto* funds_10k = first ? member(*first, "planned_funds_10k_yuan") : nullptr;
    const auto* delisting = first ? member(*first, "delisting_flag") : nullptr;
    const auto* purpose = first ? member(*first, "purpose") : nullptr;
    const auto* raw = first ? member(*first, "raw") : nullptr;
    const bool typed = security && security->is_object() &&
        member(*security, "market") && member(*security, "market")->is_string() &&
        member(*security, "code") && member(*security, "code")->is_string() &&
        member(*security, "code")->as_string().size() == 6 && announcement &&
        announcement->is_string() && announcement->as_string().size() == 10 &&
        status && status->is_string() && !status->as_string().empty() && category &&
        category->is_string() && planned && (planned->is_number() || planned->is_null()) &&
        funds && (funds->is_number() || funds->is_null()) && delisting &&
        delisting->is_bool() && purpose && purpose->is_string() && raw && raw->is_object();
    add_assertion(result, "typed_offer", typed,
                  "security, ISO announcement, status, units, delisting, purpose and raw evidence",
                  value_or_null(first));
    const bool unit_projection = planned && planned_10k && funds && funds_10k &&
        planned->is_number() && planned_10k->is_number() && funds->is_number() &&
        funds_10k->is_number() &&
        std::abs(planned->as_number() - planned_10k->as_number() * 10000.0) < 0.001 &&
        std::abs(funds->as_number() - funds_10k->as_number() * 10000.0) < 0.001;
    add_assertion(result, "ten_thousand_unit_projection", unit_projection,
                  "ngs/nzzj source units multiplied by 10,000", unit_projection);
    bool sorted = true;
    if (records && records->is_array()) {
        for (std::size_t i = 1; i < records->size(); ++i) {
            const auto* previous = member(records->as_array()[i - 1], "announcement_date");
            const auto* current = member(records->as_array()[i], "announcement_date");
            sorted = sorted && previous && current && previous->is_string() &&
                current->is_string() && previous->as_string() >= current->as_string();
        }
    }
    add_assertion(result, "descending_announcement_date", sorted, true, sorted);
    if (security_mode) {
        const bool identity = first && security &&
            string_is(member(*security, "market"), "sh") &&
            string_is(member(*security, "code"), "600491") &&
            string_is(member_path(document, {"filters", "market"}), "sh") &&
            string_is(member_path(document, {"filters", "code"}), "600491");
        add_assertion(result, "selected_security", identity,
                      "SH600491 selected in filters and records", identity);
    } else {
        const auto* row_count = member_path(document, {"summary", "rows"});
        const auto* unique = member_path(document, {"summary", "unique_securities"});
        const auto* completed = member_path(document, {"summary", "by_status", "completed"});
        const auto* failed = member_path(document, {"summary", "by_status", "failed"});
        const bool coverage = row_count && row_count->is_number() &&
            row_count->as_number() >= 160 && unique && unique->is_number() &&
            unique->as_number() >= 130 && completed && completed->is_number() &&
            completed->as_number() >= 130 && failed && failed->is_number() &&
            failed->as_number() >= 15;
        add_assertion(result, "historical_coverage", coverage,
                      "160+ rows, 130+ securities, 130+ completed, 15+ failed",
                      coverage);
    }
    const auto* sources = member(document, "sources");
    const bool healthy = sources && sources->is_array() && sources->size() == 1 &&
        source_exists(document, "list/func_yysg101_1.jsn") &&
        member(sources->as_array()[0], "attempts") &&
        member(sources->as_array()[0], "attempts")->is_number() &&
        member(sources->as_array()[0], "attempts")->as_number() >= 1 &&
        member(sources->as_array()[0], "stale") &&
        member(sources->as_array()[0], "stale")->is_bool();
    add_assertion(result, "exact_healthy_source", healthy,
                  "list/func_yysg101_1.jsn", healthy);
}

void validate_exchange_supervision_current(const std::string& contract_id, const Json& document,
        Json& result) {
    const bool current_view = contract_id == "exchange-supervision-current-live";
    const auto expected_view = current_view ? "current" : "history";
    const auto expected_resource = current_view
        ? "list/func_jysjk101_1.jsn" : "list/func_jysjk102_1.jsn";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-exchange-supervision-native-v1"),
                  "tdx-market-exchange-supervision-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view", string_is(member(document, "view"), expected_view),
                  expected_view, value_or_null(member(document, "view")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
        string_is(availability, "stale-cache") ||
        (current_view && string_is(availability, "records-only"));
    add_assertion(result, "availability", available,
                  current_view ? "live, records-only or stale-cache" : "live or stale-cache",
                  value_or_null(availability));
    const auto* records = member(document, "records");
    const Json* first = records && records->is_array() && records->size()
        ? &records->as_array()[0] : nullptr;
    const auto* security = first ? member(*first, "security") : nullptr;
    const auto* start = first ? member(*first, "start_date") : nullptr;
    const auto* end = first ? member(*first, "end_date") : nullptr;
    const auto* start_price = first ? member(*first, "start_price") : nullptr;
    const auto* raw = first ? member(*first, "raw") : nullptr;
    const bool typed = security && security->is_object() &&
        member(*security, "code") && member(*security, "code")->is_string() &&
        member(*security, "code")->as_string().size() == 6 &&
        start && start->is_string() && start->as_string().size() == 10 &&
        end && end->is_string() && end->as_string().size() == 10 &&
        start_price && start_price->is_number() && raw && raw->is_object();
    add_assertion(result, "typed_period", typed,
                  "security, ISO start/end dates, start price and raw evidence",
                  value_or_null(first));
    bool semantics = false;
    if (first) {
        const auto* last = member(*first, "last_price");
        const auto* end_price = member(*first, "end_price");
        const auto* since = member(*first, "since_start_return_pct");
        const auto* period = member(*first, "period_return_pct");
        semantics = current_view
            ? last && (last->is_number() || last->is_null()) &&
                since && (since->is_number() || since->is_null()) &&
                end_price && end_price->is_null() && period && period->is_null()
            : last && last->is_null() && since && since->is_null() &&
                end_price && end_price->is_number() && period && period->is_number();
    }
    add_assertion(result, "return_semantics", semantics,
                  current_view ? "live/start return only" : "end/start period return only",
                  semantics);
    bool sorted = true;
    if (records && records->is_array()) {
        for (std::size_t i = 1; i < records->size(); ++i) {
            const auto* previous = member(records->as_array()[i - 1], "end_date");
            const auto* next = member(records->as_array()[i], "end_date");
            sorted = sorted && previous && next && previous->is_string() &&
                next->is_string() && previous->as_string() >= next->as_string();
        }
    }
    add_assertion(result, "descending_end_date", sorted, true, sorted);
    const auto* sources = member(document, "sources");
    const bool healthy = sources && sources->is_array() && sources->size() == 1 &&
        source_exists(document, expected_resource) &&
        member(sources->as_array()[0], "attempts") &&
        member(sources->as_array()[0], "attempts")->is_number() &&
        member(sources->as_array()[0], "attempts")->as_number() >= 1 &&
        member(sources->as_array()[0], "stale") &&
        member(sources->as_array()[0], "stale")->is_bool();
    add_assertion(result, "exact_healthy_source", healthy, expected_resource, healthy);
    if (current_view) {
        const auto* command = member_path(document, {"quote_source", "command"});
        const bool quote_boundary = string_is(availability, "records-only") ||
            string_is(availability, "stale-cache") || string_is(command, "0x054C");
        add_assertion(result, "quote_boundary", quote_boundary,
                      "0x054C or explicit degraded availability",
                      value_or_null(member(document, "quote_source")));
    }
}

struct EventContract {
    std::string_view id;
    EventContractValidator validate;
};

constexpr std::array<EventContract, 12> event_contracts{{
    {"commodity-links-commodities-live", validate_commodity_links_commodities},
    {"commodity-links-commodity-live", validate_commodity_links_commodities},
    {"commodity-links-theme-live", validate_commodity_links_commodities},
    {"announcement-signals-selected-live", validate_announcement_signals_selected},
    {"announcement-signals-risks-live", validate_announcement_signals_selected},
    {"announcement-signals-history-live", validate_announcement_signals_selected},
    {"reverse-repo-rates-live", validate_reverse_repo_rates},
    {"reverse-repo-calendar-live", validate_reverse_repo_rates},
    {"tender-offers-live", validate_tender_offers},
    {"stock-tender-offers-live", validate_tender_offers},
    {"exchange-supervision-current-live", validate_exchange_supervision_current},
    {"exchange-supervision-history-live", validate_exchange_supervision_current},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < event_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < event_contracts.size(); ++right)
            if (event_contracts[left].id == event_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_market_research_event_contract(const std::string& contract_id,
    const Json& document, const Json& context, Json& result) {
    (void)context;
    for (const auto& contract : event_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(contract_id, document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail