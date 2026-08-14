#include "formula_engine_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>

namespace tdx::formula_engine_detail {

long long calendar_day_number(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned adjusted_month = month > 2 ? month - 3 : month + 9;
    const unsigned doy = (153 * adjusted_month + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<long long>(era) * 146097 + doe;
}

int period_at(const Series& value, std::size_t index, int minimum) {
    if (index >= value.size() || !std::isfinite(value[index])) return minimum;
    const auto number = static_cast<long long>(value[index]);
    return static_cast<int>(std::max<long long>(
        minimum, std::min<long long>(number, 1000000)));
}

void require_arity(const std::string& name, const std::vector<Series>& args,
                   std::size_t minimum, std::size_t maximum) {
    if (args.size() < minimum || args.size() > maximum)
        throw Error(name + " expects " + std::to_string(minimum) +
                    (minimum == maximum
                         ? ""
                         : ".." + std::to_string(maximum)) +
                    " arguments");
}

}  // namespace tdx::formula_engine_detail
