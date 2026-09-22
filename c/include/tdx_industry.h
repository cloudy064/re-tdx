/* tdx_industry.h - the industry valuation resource.
 *
 * bi/list/func_gx_hyzt101_1.jsn, measured at 8,970,491 bytes over 5,567 rows of 9 columns.
 * Each row says that ONE STOCK belongs to ONE INDUSTRY, and carries that industry's
 * valuation along with it:
 *
 *   $ZQDM/$SC     the stock
 *   TDXHY         the industry's name
 *   $ZQDM1/$SC1   the industry's index code
 *   hyPE, hyPB    the industry's P/E and P/B, as text
 *   $S_ZQDM       the industry's members, DECLARED as a "market|code" comma list
 *   sszt          a comma-separated list of themes, per row
 *
 * TWO DESCRIPTIONS OF ONE MEMBERSHIP, AND THEY AGREE.  `$S_ZQDM` declares which securities
 * an industry holds; the rows themselves are one stock each, so counting them measures the
 * same thing independently.  Measured over all 110 industries: identical for every one -
 * 880471 has 42 declared and 42 measured, and no industry differs.  Both counts are
 * reported with a flag saying whether they agree, because the interesting case is the one
 * that does not.
 *
 * The industry's own columns are consistent where it matters.  The reference throws on a
 * code whose rows disagree about the market, name or valuation; measured, no industry's
 * rows disagree at all, so that error never fires on this resource.  This reader checks it
 * and reports the codes that disagree rather than refusing the file, since one bad
 * industry should not cost a caller the other 109.
 *
 * `sszt` IS CARRIED, NOT INTERPRETED.  Its values are comma-separated theme names, and the
 * column name is not expanded here - naming it would be a guess.  The array is handed over
 * as it is. */
#ifndef TDX_INDUSTRY_H
#define TDX_INDUSTRY_H

#include <stddef.h>

#include "tdx_bonds.h"
#include "tdx_error.h"
#include "tdx_jsn.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_INDUSTRY_RESOURCE "list/func_gx_hyzt101_1.jsn"
#define TDX_INDUSTRY_ROWS_MAX 8192
#define TDX_INDUSTRY_CODES_MAX 512
#define TDX_INDUSTRY_MEMBERS_MAX 4096
#define TDX_INDUSTRY_TEXT_MAX 2048

typedef struct tdx_industry_row {
    /* The stock this row is about. */
    int stock_market_id;
    char stock_security_id[24];
    tdx_bond_text stock_code;

    /* The industry it belongs to. */
    int industry_market_id;
    char industry_code[16];
    char industry_security_id[24];
    tdx_bond_text industry_name;

    int has_pe;
    double pe;
    int has_pb;
    double pb;

    /* From $S_ZQDM: the securities the row DECLARES for this industry. */
    size_t declared_count;
    /* From sszt: the themes, as the text the resource carries. */
    tdx_bond_text themes;
    size_t theme_count;
} tdx_industry_row;

int tdx_industry_parse(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                       tdx_industry_row *out, size_t capacity, size_t *out_count,
                       size_t *skipped_count, tdx_error *err);

/* One industry, folded from the rows. */
typedef struct tdx_industry {
    char code[16];
    char security_id[24];
    int market_id;
    char name[64];
    int has_pe;
    double pe;
    int has_pb;
    double pb;
    size_t row_count;      /* how many stocks the rows say it holds */
    size_t declared_count; /* what its own $S_ZQDM list says */
    int counts_agree;
    /* Rows for this industry disagreed about the market, name or valuation. */
    int inconsistent;
} tdx_industry;

/* Folds rows into industries, keyed by industry code, ascending.  Inconsistency is
 * recorded rather than refused, so one bad industry does not cost a caller the rest. */
int tdx_industry_catalog(const tdx_industry_row *rows, size_t count, tdx_industry *out,
                         size_t capacity, size_t *out_count, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_INDUSTRY_H */
