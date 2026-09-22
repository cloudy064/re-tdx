/* tdx_valuation.h - index valuation: the current table and the PE/PB history.
 *
 * Three resources, measured:
 *
 *   list/func_zsgz101_1.jsn    2,407 bytes, 11 rows   the indices, with their metrics
 *   zsgz3/<detail>.jsn       101,898 bytes, 3,093 rows the PE history
 *   zsgz4/<detail>.jsn        98,524 bytes, 3,093 rows the PB history
 *   zsgz1/<detail>.jsn           497 bytes, 6 rows     the funds that track the index
 *
 * Note where the detail resources live: the master row's own $ZQDM column is a DETAIL ID,
 * and the three resources are fetched as "<id>.jsn" under the same root with no "list/" in
 * between.  The first attempt added one and got three empty answers.
 *
 * THE TWO HISTORIES JOIN ON THEIR DATE, and they have the same row count - 3,093 each for
 * the index measured - so the join is checkable rather than assumed: how many dates both
 * sides carry, and which side, if either, has a date the other lacks.  Both counts are
 * reported.  The merged point carries the PE, its percentile, the PB and its percentile.
 *
 * The master's metrics are text in the resource and are parsed as numbers, except the
 * valuation LABEL, which is text and stays text: it is a judgement the server makes
 * ("估值适中" and the like), not a number this reader could derive. */
#ifndef TDX_VALUATION_H
#define TDX_VALUATION_H

#include <stddef.h>

#include "tdx_bonds.h"
#include "tdx_error.h"
#include "tdx_jsn.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_VALUATION_MASTER_RESOURCE "list/func_zsgz101_1.jsn"
#define TDX_VALUATION_INDICES_MAX 256
#define TDX_VALUATION_HISTORY_MAX 8192
#define TDX_VALUATION_FUNDS_MAX 512
#define TDX_VALUATION_LABEL_MAX 32

typedef struct tdx_valuation_index {
    int market_id;
    char security_id[24];
    char code[16];
    /* The id that names the detail resources. */
    char detail_id[16];
    char date[12];

    int has_pe;
    double pe;
    int has_pe_percentile;
    double pe_percentile;
    int has_pb;
    double pb;
    int has_pb_percentile;
    double pb_percentile;
    int has_dividend_yield;
    double dividend_yield;
    int has_roe;
    double roe;
    int has_earnings_yield;
    double earnings_yield;
    /* The server's own word for the valuation; text, because it is a judgement. */
    tdx_bond_text label;

    double returns[4]; /* 5, 10, 20 and 30 days */
    int has_return[4];
} tdx_valuation_index;

int tdx_valuation_parse_master(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                               tdx_valuation_index *out, size_t capacity, size_t *out_count,
                               size_t *skipped_count, tdx_error *err);

/* One date of the merged history.  A field is absent when that resource did not carry the
 * date, which is why every one has a has_ flag. */
typedef struct tdx_valuation_point {
    char date[12];
    int has_pe;
    double pe;
    int has_pe_percentile;
    double pe_percentile;
    int has_pb;
    double pb;
    int has_pb_percentile;
    double pb_percentile;
} tdx_valuation_point;

/* What the join found, so a caller can see whether the two halves really describe one
 * series rather than taking it on trust. */
typedef struct tdx_valuation_merge {
    size_t points;
    size_t both_sides;
    size_t pe_only;
    size_t pb_only;
    /* True when every point carries both halves. */
    int complete;
} tdx_valuation_merge;

/* Merges the PE and the PB history by date.  Either may be NULL, in which case its half is
 * simply absent from every point. */
int tdx_valuation_merge_history(const tdx_jsn_document *pe_doc, const tdx_jsn_group *pe_group,
                                const tdx_jsn_document *pb_doc, const tdx_jsn_group *pb_group,
                                tdx_valuation_point *out, size_t capacity,
                                tdx_valuation_merge *merge, tdx_error *err);

typedef struct tdx_valuation_fund {
    int market_id;
    char security_id[24];
    int has_unit_nav;
    double unit_nav;
    int has_premium_pct;
    double premium_pct;
    int has_size_yuan;
    double size_yuan;
    tdx_bond_text fund_type;
} tdx_valuation_fund;

int tdx_valuation_parse_funds(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                              tdx_valuation_fund *out, size_t capacity, size_t *out_count,
                              size_t *skipped_count, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_VALUATION_H */
