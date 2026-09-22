/* tdx_bond_math.h - the valuation arithmetic the convertible-bond pricing view needs.
 *
 * This is separate from any protocol because none of it is protocol: it is the
 * prospectus arithmetic the reference applies to a row once the row has been read.
 * Ported exactly, including the two places where it refuses to answer:
 *
 *   accrued interest   IA = face * rate * t / 365
 *   cash flows         one per remaining coupon date, the last one also repaying the
 *                      face value
 *   discounted value   sum of amount / (1 + rate)^years
 *   yield to maturity  bisection on that, which is why the solver can be checked by
 *                      its own fixed point: discounting the flows at the yield it
 *                      returns must reproduce the price it was given.
 *
 * Two details matter more than they look:
 *
 *   * `civil_days` is days from 1970-01-01 and is only defined for a real calendar
 *     date.  Every date in the chain goes through it, so a malformed date makes the
 *     whole calculation absent rather than producing a plausible number from a
 *     misread one.
 *   * the time fraction is days/365 throughout - not ACT/365 in the bond-market sense
 *     with a settlement convention, just the table's own rule, so it is reproduced
 *     rather than improved.
 *
 * A guard that lets an infinity through is the failure that looks like data, so every
 * division here is checked and the result is either finite or absent. */
#ifndef TDX_BOND_MATH_H
#define TDX_BOND_MATH_H

#include <stddef.h>

#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_BOND_FLOWS_MAX 64

/* Days from 1970-01-01 for a YYYYMMDD or YYYY-MM-DD date, or 0 when the text is not a
 * real date. */
int tdx_bond_civil_days(const char *text, size_t length, long long *out);

/* A comma-separated list read as numbers.  Pieces that are not numbers are skipped,
 * and 0 is returned when none is, which is how the reference treats them. */
size_t tdx_bond_csv_numbers(const char *text, size_t length, double *out, size_t capacity);

/* A comma-separated list read as trimmed text pieces.  Each piece is reported as an
 * offset and length into the source, so nothing is copied. */
typedef struct tdx_bond_piece {
    size_t offset;
    size_t length;
} tdx_bond_piece;

size_t tdx_bond_csv_pieces(const char *text, size_t length, tdx_bond_piece *out,
                           size_t capacity);

/* One future payment. */
typedef struct tdx_bond_flow {
    double years;
    double amount;
} tdx_bond_flow;

/* --- the calculation -------------------------------------------------- */

/* IA = face * first_remaining_rate * (as_of - last_coupon) / 365, and only when the
 * as-of date lies inside the current coupon period. */
int tdx_bond_accrued_interest(double face, const char *last_coupon, size_t last_length,
                              const char *next_coupon, size_t next_length, const char *as_of,
                              size_t as_of_length, const char *remaining_rates,
                              size_t rates_length, double *out);

/* Fills out with the remaining flows: one per date after the as-of date, paired with
 * the same index of the rate list, the last one also repaying the face value.  Returns
 * how many were written. */
size_t tdx_bond_remaining_flows(double face, const char *dates, size_t dates_length,
                                const char *rates, size_t rates_length, const char *as_of,
                                size_t as_of_length, tdx_bond_flow *out, size_t capacity);

/* The sum of amount / (1 + annual_rate)^years, or absent when the rate is at or below
 * -100 per cent or any factor is not a positive finite number. */
int tdx_bond_discounted_value(const tdx_bond_flow *flows, size_t count, double annual_rate,
                              double *out);

/* Bisection on the discounted value, on the interval the reference uses.  Absent when
 * the price is not bracketed by the interval's ends, which is the honest answer for a
 * price no non-negative yield can produce. */
int tdx_bond_solve_ytm(const tdx_bond_flow *flows, size_t count, double full_price,
                       double *out);

#ifdef __cplusplus
}
#endif

#endif /* TDX_BOND_MATH_H */
