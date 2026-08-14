#include "investment_internal.hpp"

#include <cmath>

namespace tdx::investment_detail {
namespace {

bool known_type(int type_code) {
    return type_code == 0 || type_code == 1 || type_code == 2 ||
           type_code == 3 || type_code == 4 || type_code == 6 ||
           type_code == 7 || type_code == 9 || type_code == 10 ||
           type_code == 11 || type_code == 12 || type_code == 13;
}

void require_quantity(const TransactionRecord& transaction,
                      const HoldingRecord& holding) {
    if (transaction.quantity <= 0)
        throw Error("security transaction record " +
                    std::to_string(transaction.file_record_index) +
                    " has non-positive quantity");
    if ((transaction.type_code == 1 || transaction.type_code == 12) &&
        transaction.quantity > holding.quantity)
        throw Error("security transaction record " +
                    std::to_string(transaction.file_record_index) +
                    " would make the reconstructed holding negative");
}

void apply_security(HoldingRecord& holding,
                    const TransactionRecord& transaction) {
    require_quantity(transaction, holding);
    ++holding.transaction_count;
    holding.fees += transaction.fee;
    if (holding.first_date == 0) holding.first_date = transaction.date;
    holding.last_date = transaction.date;
    const auto quantity = static_cast<std::int64_t>(transaction.quantity);
    switch (transaction.type_code) {
    case 0:
    case 4:
    case 11: {
        const auto previous = holding.quantity;
        const auto next = previous + quantity;
        holding.average_cost =
            (static_cast<double>(previous) * holding.average_cost +
             static_cast<double>(quantity) * transaction.unit_price +
             transaction.fee) / static_cast<double>(next);
        holding.quantity = next;
        if (transaction.type_code == 11)
            holding.transfer_in_quantity += transaction.quantity;
        else
            holding.buy_quantity += transaction.quantity;
        break;
    }
    case 1: {
        const auto profit = transaction.recorded_total -
            static_cast<double>(quantity) * holding.average_cost;
        holding.realized_profit += static_cast<double>(
            static_cast<float>(profit));
        holding.quantity -= quantity;
        holding.sell_quantity += transaction.quantity;
        if (holding.quantity == 0) holding.average_cost = 0.0;
        break;
    }
    case 2:
        holding.cash_dividends += transaction.recorded_total;
        if (holding.quantity > 0)
            holding.average_cost =
                (static_cast<double>(holding.quantity) * holding.average_cost -
                 transaction.recorded_total) /
                static_cast<double>(holding.quantity);
        break;
    case 3:
    case 13: {
        const auto previous = holding.quantity;
        const auto next = previous + quantity;
        if (previous > 0)
            holding.average_cost =
                static_cast<double>(previous) * holding.average_cost /
                static_cast<double>(next);
        holding.quantity = next;
        holding.bonus_quantity += transaction.quantity;
        break;
    }
    case 12:
        holding.quantity -= quantity;
        holding.transfer_out_quantity += transaction.quantity;
        if (holding.quantity == 0) holding.average_cost = 0.0;
        break;
    default:
        break;
    }
    if (!std::isfinite(holding.average_cost) ||
        !std::isfinite(holding.realized_profit) ||
        !std::isfinite(holding.cash_dividends) ||
        !std::isfinite(holding.fees))
        throw Error("investment holding reconstruction produced a non-finite value");
}

}  // namespace

void apply_transaction_page(
    InvestmentLedger& ledger,
    const std::vector<TransactionRecord>& transactions) {
    for (const auto& transaction : transactions) {
        if (ledger.last_date != 0 && transaction.date < ledger.last_date)
            throw Error("investment transactions are not ordered by YYYYMMDD");
        ledger.last_date = transaction.date;
        ++ledger.transaction_count;
        if (!known_type(transaction.type_code)) ++ledger.unknown_type_count;
        if (transaction.cash_direction > 0)
            ledger.cash_inflows += transaction.recorded_total;
        else if (transaction.cash_direction < 0)
            ledger.cash_outflows += transaction.recorded_total;
        ledger.net_cash_flow +=
            static_cast<double>(transaction.cash_direction) *
            transaction.recorded_total;
        if (!transaction_has_security(transaction.type_code)) continue;
        ++ledger.security_transaction_count;
        auto& holding = ledger.holdings[
            {transaction.market_kind, transaction.code}];
        holding.market_kind = transaction.market_kind;
        holding.code = transaction.code;
        apply_security(holding, transaction);
    }
}

}  // namespace tdx::investment_detail
