#pragma once

#include "tdx/common.hpp"
#include "tdx/json.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx {
struct InvestmentQuery;
}

namespace tdx::investment_detail {

inline constexpr std::size_t kPortfolioRecordSize = 192;
inline constexpr std::size_t kTransactionRecordSize = 200;
inline constexpr std::size_t kFeeRuleRecordSize = 79;
inline constexpr char kInvestmentKey[] = "359=dm5h3q543;6jd;aeda";

struct InvestmentPaths {
    std::filesystem::path private_directory;
    std::filesystem::path portfolio_index;
    std::filesystem::path fee_rules;
    std::string private_directory_source;
    std::string fee_rules_source;
};

struct PortfolioRecord {
    std::string name;
    Bytes password;
};

struct PortfolioFileInfo {
    std::filesystem::path path;
    bool lookup_allowed{};
    bool available{};
    bool valid_record_size{};
    bool header_password_readable{};
    bool header_password_matches{};
    std::uintmax_t byte_size{};
    std::size_t record_count{};
    std::size_t transaction_count{};
    std::string diagnostic;
};

struct TransactionRecord {
    std::size_t file_record_index{};
    int market_kind{};
    int date{};
    std::string note;
    bool note_present{};
    int cash_direction{};
    std::string code;
    int type_code{};
    std::int32_t quantity{};
    double unit_price{};
    double fee{};
    double recorded_total{};
    bool unparsed_bytes_present{};
};

struct HoldingRecord {
    int market_kind{};
    std::string code;
    std::int64_t quantity{};
    double average_cost{};
    double realized_profit{};
    double cash_dividends{};
    double fees{};
    std::uint64_t transaction_count{};
    std::uint64_t buy_quantity{};
    std::uint64_t sell_quantity{};
    std::uint64_t bonus_quantity{};
    std::uint64_t transfer_in_quantity{};
    std::uint64_t transfer_out_quantity{};
    int first_date{};
    int last_date{};
};

struct InvestmentLedger {
    std::map<std::pair<int, std::string>, HoldingRecord> holdings;
    std::uint64_t transaction_count{};
    std::uint64_t security_transaction_count{};
    std::uint64_t unknown_type_count{};
    double cash_inflows{};
    double cash_outflows{};
    double net_cash_flow{};
    int last_date{};
};

struct FeeRule {
    std::size_t record_index{};
    int market_kind{};
    std::string prefix;
    std::string market_name;
    double commission_rate{};
    double minimum_commission{};
    double stamp_tax_rate{};
    double transfer_fee_rate{};
    double minimum_transfer_fee{};
    double fixed_fee{};
};

struct FeeEstimate {
    double notional{};
    double commission{};
    double stamp_tax{};
    double transfer_fee{};
    double fixed_fee{};
    double total_fee{};
};

struct ValuationResult {
    Json document;
    std::uint64_t network_requests{};
};

InvestmentPaths resolve_paths(
    const std::filesystem::path& root,
    const std::filesystem::path& private_directory_override,
    const std::filesystem::path& fee_rules_override);

std::vector<PortfolioRecord> parse_portfolio_index(
    const std::filesystem::path& path);

PortfolioFileInfo inspect_portfolio_file(
    const std::filesystem::path& private_directory,
    const PortfolioRecord& portfolio);

std::vector<TransactionRecord> parse_transaction_records(
    const std::filesystem::path& path,
    const PortfolioRecord& portfolio,
    bool include_notes,
    std::size_t offset,
    std::size_t limit);

std::vector<FeeRule> parse_fee_rules(const std::filesystem::path& path);
const FeeRule* select_fee_rule(const std::vector<FeeRule>& rules,
                               int market_kind,
                               const std::string& code);
FeeEstimate estimate_fee(const FeeRule& rule, std::string_view side,
                         double price, std::uint64_t quantity);
Json fee_estimate_document(const FeeRule& rule, std::string_view side,
                           double price, std::uint64_t quantity);
ValuationResult value_investment_ledger(
    const std::filesystem::path& root,
    const InvestmentQuery& query,
    const InvestmentLedger& ledger,
    const std::vector<FeeRule>& fee_rules);

bool regular_file_available(const std::filesystem::path& path);
std::string market_name(int market_kind);
std::string transaction_type_name(int type_code);
std::string transaction_type_label(int type_code);
bool transaction_has_security(int type_code);

void apply_transaction_page(
    InvestmentLedger& ledger,
    const std::vector<TransactionRecord>& transactions);

}  // namespace tdx::investment_detail
