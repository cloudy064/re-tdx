#pragma once

#include "tdx/common.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tdx {

struct ProfessionalFinanceRecord {
    int market_id{};
    std::string code;
    std::vector<std::optional<double>> fields;
};

struct ProfessionalFinanceData {
    std::uint32_t report_date{};
    std::size_t field_count{};
    std::map<std::pair<int, std::string>, ProfessionalFinanceRecord> records;
    std::string source;
};

struct ProfessionalTradingRecord {
    int id{};
    std::uint32_t date{};
    std::optional<double> value1;
    std::optional<double> value2;
};

ProfessionalFinanceData parse_professional_finance_data(
    const Bytes& payload, std::string source = {});
std::vector<ProfessionalTradingRecord> parse_professional_trading_data(
    const Bytes& payload);

std::vector<std::optional<double>> professional_trading_series(
    const std::vector<ProfessionalTradingRecord>& records,
    int id, int field, int type, const std::vector<std::uint32_t>& dates);

// TCalc's GPJYONE/BKJYONE/SCJYONE wrappers use a full YYYYMMDD for an exact
// lookup, while values below 10000 select the Nth record from the end (zero is
// the latest record).  The year/MMDD overload preserves the wrapper's
// two-digit-year conversion before applying that host rule.
std::optional<double> professional_trading_one(
    const std::vector<ProfessionalTradingRecord>& records,
    int id, int field, int year, int mmdd);

// FINONE has different partial-date semantics: (N,0) means N years back,
// (0,N<=300) means N quarters back, and (0,MMDD) chooses the latest occurrence
// of that month/day.  Records must be report-period/value pairs.
std::optional<double> professional_finance_one(
    const std::vector<std::pair<std::uint32_t, std::optional<double>>>& records,
    int year, int mmdd);

// FINANCE(43/44) are published directly in the professional finance package.
// Centralizing the mapping preserves TongDaXin's exceptional/negative-base semantics.
std::optional<double> professional_finance_growth_value(
    const ProfessionalFinanceRecord& record, int finance_id);

std::vector<ProfessionalTradingRecord> fetch_professional_stock_trading_data(
    const std::string& market, const std::string& code,
    const std::filesystem::path& cache_directory = {},
    int timeout_ms = 15000, bool refresh = false);
std::vector<ProfessionalTradingRecord> fetch_professional_market_trading_data(
    const std::filesystem::path& cache_directory = {},
    int timeout_ms = 15000, bool refresh = false);
ProfessionalFinanceData fetch_professional_finance_data(
    std::uint32_t report_date,
    const std::filesystem::path& cache_directory = {},
    int timeout_ms = 30000, bool refresh = false);
ProfessionalFinanceData fetch_latest_professional_finance_data_for_security(
    int market_id, const std::string& code,
    const std::filesystem::path& cache_directory = {},
    int timeout_ms = 30000, bool refresh = false);

Json professional_trading_document(
    const std::vector<ProfessionalTradingRecord>& records,
    std::string kind, std::string security_id,
    const std::vector<int>& fields = {}, std::uint32_t from = 0,
    std::uint32_t to = 99999999, std::size_t limit = 5000,
    bool history = false);
Json professional_finance_document(
    const ProfessionalFinanceData& data, int market_id,
    const std::string& code, const std::vector<int>& fields = {});
Json fetch_professional_catalog_document(int timeout_ms = 15000);
Json fetch_professional_finance_series_document(
    int market_id, const std::string& code,
    const std::vector<int>& fields = {},
    std::uint32_t from = 0, std::uint32_t to = 99999999,
    std::size_t maximum_periods = 8,
    const std::filesystem::path& cache_directory = {},
    int timeout_ms = 30000, bool refresh = false);

int command_market_professional(const std::vector<std::string>& args);

}  // namespace tdx
