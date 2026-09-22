/* tdx_bond_math.c - the valuation arithmetic the pricing view needs. */
#include "tdx_bond_math.h"
#include "tdx_date.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int tdx_bond_civil_days(const char *text, size_t length, long long *out) {
    uint32_t date;
    return tdx_date_parse(text, length, 1, &date) && tdx_date_days(date, out);
}

size_t tdx_bond_csv_pieces(const char *text, size_t length, tdx_bond_piece *out,
                           size_t capacity) {
    size_t count = 0;
    size_t index = 0;

    if (!text || !out || capacity == 0)
        return 0;
    while (index < length) {
        size_t start;
        size_t stop;
        while (index < length && (text[index] == ',' || text[index] == ' ' ||
                                  text[index] == '\t'))
            index++;
        start = index;
        while (index < length && text[index] != ',')
            index++;
        stop = index;
        while (stop > start && (text[stop - 1] == ' ' || text[stop - 1] == '\t'))
            stop--;
        if (stop == start)
            continue;
        if (count >= capacity)
            break;
        out[count].offset = start;
        out[count].length = stop - start;
        count++;
    }
    return count;
}

size_t tdx_bond_csv_numbers(const char *text, size_t length, double *out, size_t capacity) {
    tdx_bond_piece pieces[TDX_BOND_FLOWS_MAX];
    size_t count;
    size_t index;
    size_t written = 0;

    if (!out || capacity == 0)
        return 0;
    count = tdx_bond_csv_pieces(text, length, pieces,
                                capacity < TDX_BOND_FLOWS_MAX ? capacity : TDX_BOND_FLOWS_MAX);
    for (index = 0; index < count; ++index) {
        char scratch[64];
        char *stop = NULL;
        double value;
        if (pieces[index].length >= sizeof(scratch))
            continue;
        memcpy(scratch, text + pieces[index].offset, pieces[index].length);
        scratch[pieces[index].length] = '\0';
        value = strtod(scratch, &stop);
        if (!stop || stop == scratch)
            continue; /* a piece that is not a number is skipped, like the reference */
        out[written++] = value;
    }
    return written;
}

int tdx_bond_accrued_interest(double face, const char *last_coupon, size_t last_length,
                              const char *next_coupon, size_t next_length, const char *as_of,
                              size_t as_of_length, const char *remaining_rates,
                              size_t rates_length, double *out) {
    long long previous = 0;
    long long next = 0;
    long long current = 0;
    double rates[TDX_BOND_FLOWS_MAX];
    size_t rate_count;

    if (!out)
        return 0;
    if (!tdx_bond_civil_days(last_coupon, last_length, &previous) ||
        !tdx_bond_civil_days(next_coupon, next_length, &next) ||
        !tdx_bond_civil_days(as_of, as_of_length, &current))
        return 0;
    rate_count = tdx_bond_csv_numbers(remaining_rates, rates_length, rates, TDX_BOND_FLOWS_MAX);
    if (rate_count == 0)
        return 0;
    /* Outside the current coupon period the formula does not apply, and the reference
     * reports nothing rather than extrapolating. */
    if (current < previous || current > next)
        return 0;
    *out = face * rates[0] * (double)(current - previous) / 365.0;
    return isfinite(*out) ? 1 : 0;
}

size_t tdx_bond_remaining_flows(double face, const char *dates, size_t dates_length,
                                const char *rates, size_t rates_length, const char *as_of,
                                size_t as_of_length, tdx_bond_flow *out, size_t capacity) {
    tdx_bond_piece date_pieces[TDX_BOND_FLOWS_MAX];
    double rate_values[TDX_BOND_FLOWS_MAX];
    size_t date_count;
    size_t rate_count;
    size_t count;
    size_t index;
    size_t written = 0;
    long long current = 0;

    if (!out || capacity == 0)
        return 0;
    if (!tdx_bond_civil_days(as_of, as_of_length, &current))
        return 0;
    date_count = tdx_bond_csv_pieces(dates, dates_length, date_pieces, TDX_BOND_FLOWS_MAX);
    rate_count = tdx_bond_csv_numbers(rates, rates_length, rate_values, TDX_BOND_FLOWS_MAX);
    count = date_count < rate_count ? date_count : rate_count;
    for (index = 0; index < count; ++index) {
        char scratch[16];
        long long date = 0;
        double amount;
        if (date_pieces[index].length >= sizeof(scratch))
            continue;
        memcpy(scratch, dates + date_pieces[index].offset, date_pieces[index].length);
        scratch[date_pieces[index].length] = '\0';
        if (!tdx_bond_civil_days(scratch, date_pieces[index].length, &date))
            continue;
        /* Only flows after the as-of date remain, which is what "remaining" means. */
        if (date <= current)
            continue;
        if (written >= capacity)
            break;
        amount = face * rate_values[index];
        /* The last flow repays the face value as well as paying its coupon. */
        if (index + 1 == count)
            amount += face;
        out[written].years = (double)(date - current) / 365.0;
        out[written].amount = amount;
        written++;
    }
    return written;
}

int tdx_bond_discounted_value(const tdx_bond_flow *flows, size_t count, double annual_rate,
                              double *out) {
    double value = 0.0;
    size_t index;

    if (!out)
        return 0;
    if (!flows || count == 0)
        return 0;
    if (!(annual_rate > -1.0) || !isfinite(annual_rate))
        return 0;
    for (index = 0; index < count; ++index) {
        double factor = pow(1.0 + annual_rate, flows[index].years);
        if (!isfinite(factor) || factor <= 0.0)
            return 0;
        value += flows[index].amount / factor;
    }
    if (!isfinite(value))
        return 0;
    *out = value;
    return 1;
}

int tdx_bond_solve_ytm(const tdx_bond_flow *flows, size_t count, double full_price,
                       double *out) {
    double low = -0.999999;
    double high = 10.0;
    double low_value = 0.0;
    double high_value = 0.0;
    int iteration;

    if (!out)
        return 0;
    if (!flows || count == 0 || !(full_price > 0.0) || !isfinite(full_price))
        return 0;
    /* The interval has to bracket the price.  If it does not, no yield in it solves
     * the equation, and saying so is the honest answer. */
    if (!tdx_bond_discounted_value(flows, count, low, &low_value) ||
        !tdx_bond_discounted_value(flows, count, high, &high_value))
        return 0;
    if (low_value < full_price || high_value > full_price)
        return 0;
    for (iteration = 0; iteration < 160; ++iteration) {
        double middle = (low + high) / 2.0;
        double value = 0.0;
        if (!tdx_bond_discounted_value(flows, count, middle, &value))
            return 0;
        if (value > full_price)
            low = middle;
        else
            high = middle;
    }
    *out = (low + high) / 2.0;
    return 1;
}
