/* tdx_limit.h - the price-limit rules the terminal keeps in hqrule.dat.
 *
 * A-share limit prices are not a simple percentage: the rate depends on the board and on
 * whether the stock is under special treatment, and the price is rounded in a way that is
 * not round-half-up everywhere.
 *
 * THE RATES.  The default is 10 per cent.  ChiNext (300/301) and STAR (688/689) are 20,
 * Beijing (920) is 30, and a stock whose NAME looks like special treatment gets 5 - but
 * only once the configured switch date has passed.  That last condition is where the real
 * rule file differs from the reference's defaults: the file on this machine says
 * SZST10Date=99991231 and SHST10Date=99991231, so a switch date that has not arrived keeps
 * special-treatment stocks at 10 per cent, while the defaults of 0 would have put every one
 * of them at 5.
 *
 * THE ROUNDING is the part that is easy to get wrong and impossible to notice, and the
 * reference reconstructs it in three steps that this reproduces exactly:
 *
 *   increment = truncate(rate * previous * 100 + 0.503) / 100
 *   upper     = truncate((previous + increment) * 100 + 0.503) / 100
 *   lower     = truncate((1 - rate) * previous * 100 + 0.503) / 100
 *
 * so the increment is rounded to a cent BEFORE it is added, and the bias of 0.503 is what
 * makes the truncation behave as round-half-up.  Both intermediate results are stored as
 * FLOAT, which is not cosmetic: the stored value is what a comparison against a quote has
 * to match.
 *
 * BEIJING ROUNDS THE OTHER WAY.  Its bias is 0.003 for the upper limit and 0.997 for the
 * lower, which is truncation rather than round-half-up - a different function for one
 * board, not a different constant in the same one.
 *
 * NOT EVERY SECURITY IS PRICE LIMITED.  A board the class table does not list, a previous
 * close that is absent, a name beginning with N (a new listing's first day), or a quote
 * whose flags say so, all yield "no limit" rather than a number. */
#ifndef TDX_LIMIT_H
#define TDX_LIMIT_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_LIMIT_RULE_PATH_MAX 512
/* Long enough for the path the rules came from, not only for "TdxW-defaults". */
#define TDX_LIMIT_SOURCE_MAX 512

typedef struct tdx_limit_rules {
    int sz_st_10_date;
    int sh_st_10_date;
    double chinext_rate;
    double star_rate;
    double beijing_rate;
    /* The file the rules came from, or "TdxW-defaults" when there was none. */
    char source[TDX_LIMIT_SOURCE_MAX];
} tdx_limit_rules;

void tdx_limit_rules_default(tdx_limit_rules *rules);

/* Reads <root>/T0002/hq_cache/hqrule.dat.  A missing file leaves the defaults in place
 * rather than failing: the terminal works without it, and so does this. */
int tdx_limit_rules_load(const char *root, tdx_limit_rules *rules, tdx_error *err);

/* Parses the file's own shape, which is one "[key=value]" per line in sections.  Exposed
 * because the file is small enough to test directly. */
int tdx_limit_rules_parse(const char *text, size_t length, tdx_limit_rules *rules,
                          tdx_error *err);

/* The board class of a security, or -1 when it is not price limited. */
int tdx_limit_security_class(int market_id, const char *code);
/* Whether a name looks like special treatment. */
int tdx_limit_name_is_st(const char *name);

typedef struct tdx_limit_prices {
    int available;
    double upper;
    double lower;
    double rate;
    int security_class;
    char source[TDX_LIMIT_SOURCE_MAX];
} tdx_limit_prices;

/* Computes the limits.  previous_close and as_of_yyyymmdd are as they sound; name may be
 * NULL or empty, in which case no stock is treated as special. */
int tdx_limit_calculate(int market_id, const char *code, const char *name,
                        double previous_close, int as_of_yyyymmdd,
                        const tdx_limit_rules *rules, tdx_limit_prices *out);

/* The two rounding functions, exposed so a test can check them against arithmetic rather
 * than only against a computed limit. */
double tdx_limit_upper(double previous_close, double rate);
double tdx_limit_lower(double previous_close, double rate);
double tdx_limit_beijing(double previous_close, double rate, int upper);

#ifdef __cplusplus
}
#endif

#endif /* TDX_LIMIT_H */
