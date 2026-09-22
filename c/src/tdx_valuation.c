/* tdx_valuation.c - index valuation: the current table and the PE/PB history. */
#include "tdx_valuation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int valid_code(const char *code, size_t length) {
    size_t index;
    if (!code || length != 6)
        return 0;
    for (index = 0; index < 6; ++index)
        if (code[index] < '0' || code[index] > '9')
            return 0;
    return 1;
}

static tdx_bond_text cell(const tdx_jsn_document *doc, const tdx_jsn_group *group, size_t row,
                          const char *key) {
    return tdx_bonds_cell_text(doc, group, row, key);
}

static int cell_number(const tdx_jsn_document *doc, const tdx_jsn_group *group, size_t row,
                       const char *key, double *out) {
    return tdx_bonds_cell_number(doc, group, row, key, out);
}

/* Copies a text cell into a fixed buffer, NUL-terminated; returns 0 when absent or too
 * long, which for a code means the row cannot be used. */
static int copy_text(const tdx_bond_text *text, char *out, size_t capacity) {
    if (!text->present || !text->data || text->length == 0 || text->length >= capacity) {
        out[0] = '\0';
        return 0;
    }
    memcpy(out, text->data, text->length);
    out[text->length] = '\0';
    return 1;
}

int tdx_valuation_parse_master(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                               tdx_valuation_index *out, size_t capacity, size_t *out_count,
                               size_t *skipped_count, tdx_error *err) {
    size_t stored = 0;
    size_t skipped = 0;
    size_t row;
    static const char *const return_keys[4] = {"jwzf", "jszf", "jezf", "jsszf"};

    if (out_count)
        *out_count = 0;
    if (skipped_count)
        *skipped_count = 0;
    if (!doc || !group || !out) {
        tdx_error_set(err, "parsing the valuation master needs a document, a group and an "
                           "output");
        return TDX_ERR;
    }
    for (row = 0; row < group->row_count; ++row) {
        tdx_valuation_index *item;
        tdx_bond_text code = cell(doc, group, row, "$ZQDM1");
        tdx_bond_text market = cell(doc, group, row, "$SC1");
        tdx_bond_text detail = cell(doc, group, row, "$ZQDM");
        int market_id = -1;
        char prefix[16];
        size_t index;

        if (stored >= capacity) {
            tdx_error_set(err, "the valuation master holds more than %zu rows", capacity);
            return TDX_ERR;
        }
        /* A row that cannot name the index or its detail id cannot be followed up, so it is
         * skipped and counted. */
        if (!valid_code(code.data, code.length) || !valid_code(detail.data, detail.length) ||
            tdx_bonds_market_id(market.data, market.length, &market_id, NULL) != TDX_OK) {
            skipped++;
            continue;
        }
        if (tdx_bonds_market_prefix(market_id, prefix, sizeof(prefix), err) != TDX_OK)
            return TDX_ERR;
        item = &out[stored];
        memset(item, 0, sizeof(*item));
        item->market_id = market_id;
        snprintf(item->security_id, sizeof(item->security_id), "%s%.*s", prefix,
                 (int)code.length, code.data);
        snprintf(item->code, sizeof(item->code), "%.*s", (int)code.length, code.data);
        snprintf(item->detail_id, sizeof(item->detail_id), "%.*s", (int)detail.length,
                 detail.data);
        {
            tdx_bond_text date_text = cell(doc, group, row, "date");
            (void)copy_text(&date_text, item->date, sizeof(item->date));
        }

        item->has_pe = cell_number(doc, group, row, "pe", &item->pe);
        item->has_pe_percentile = cell_number(doc, group, row, "pefws", &item->pe_percentile);
        item->has_pb = cell_number(doc, group, row, "pb", &item->pb);
        item->has_pb_percentile = cell_number(doc, group, row, "pbfws", &item->pb_percentile);
        item->has_dividend_yield = cell_number(doc, group, row, "gxl", &item->dividend_yield);
        item->has_roe = cell_number(doc, group, row, "roe", &item->roe);
        item->has_earnings_yield = cell_number(doc, group, row, "syl", &item->earnings_yield);
        item->label = cell(doc, group, row, "gzsp");
        for (index = 0; index < 4; ++index)
            item->has_return[index] =
                cell_number(doc, group, row, return_keys[index], &item->returns[index]);
        stored++;
    }
    if (out_count)
        *out_count = stored;
    if (skipped_count)
        *skipped_count = skipped;
    return TDX_OK;
}

/* --- the history merge ------------------------------------------------ */

/* Reads one history resource into a simple array indexed by date string.  The rows are
 * already in ascending date order in the resources measured, but that is not relied on:
 * this walks them in order and merges, so out-of-order input still joins. */
typedef struct history_point {
    char date[12];
    int has_value;
    double value;
    int has_percentile;
    double percentile;
} history_point;

static int read_history(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                        const char *value_key, const char *percentile_key, history_point *out,
                        size_t capacity, size_t *out_count, tdx_error *err) {
    size_t stored = 0;
    size_t row;

    if (out_count)
        *out_count = 0;
    if (!doc || !group)
        return TDX_OK; /* an absent side is not a failure */
    for (row = 0; row < group->row_count; ++row) {
        history_point *point;
        tdx_bond_text date = cell(doc, group, row, "date");
        char buffer[12];

        if (!copy_text(&date, buffer, sizeof(buffer)))
            continue;
        if (stored >= capacity) {
            tdx_error_set(err, "the history holds more than %zu dates", capacity);
            return TDX_ERR;
        }
        point = &out[stored];
        memset(point, 0, sizeof(*point));
        memcpy(point->date, buffer, sizeof(point->date));
        point->has_value = cell_number(doc, group, row, value_key, &point->value);
        if (percentile_key)
            point->has_percentile =
                cell_number(doc, group, row, percentile_key, &point->percentile);
        stored++;
    }
    if (out_count)
        *out_count = stored;
    return TDX_OK;
}

/* Finds a date in a history, or NULL.  Linear because the arrays are walked once each in
 * the common case and the alternative is a hash table for eight thousand rows. */
static const history_point *find_date(const history_point *points, size_t count,
                                      const char *date) {
    size_t index;
    for (index = 0; index < count; ++index)
        if (strcmp(points[index].date, date) == 0)
            return &points[index];
    return NULL;
}

int tdx_valuation_merge_history(const tdx_jsn_document *pe_doc, const tdx_jsn_group *pe_group,
                                const tdx_jsn_document *pb_doc, const tdx_jsn_group *pb_group,
                                tdx_valuation_point *out, size_t capacity,
                                tdx_valuation_merge *merge, tdx_error *err) {
    history_point *pe = NULL;
    history_point *pb = NULL;
    size_t pe_count = 0;
    size_t pb_count = 0;
    size_t written = 0;
    size_t index;
    int status = TDX_ERR;

    if (merge)
        memset(merge, 0, sizeof(*merge));
    if (!out) {
        tdx_error_set(err, "merging the history needs an output");
        return TDX_ERR;
    }
    pe = (history_point *)calloc(TDX_VALUATION_HISTORY_MAX, sizeof(*pe));
    pb = (history_point *)calloc(TDX_VALUATION_HISTORY_MAX, sizeof(*pb));
    if (!pe || !pb) {
        tdx_error_set(err, "out of memory for the history buffers");
        goto done;
    }
    if (read_history(pe_doc, pe_group, "pe", "pebfw", pe, TDX_VALUATION_HISTORY_MAX, &pe_count,
                     err) != TDX_OK)
        goto done;
    if (read_history(pb_doc, pb_group, "pb", "pbbfw", pb, TDX_VALUATION_HISTORY_MAX, &pb_count,
                     err) != TDX_OK)
        goto done;

    /* The PE side sets the order, because it is the series a reader would name first; a
     * date only the PB side carries is appended so nothing is lost. */
    for (index = 0; index < pe_count; ++index) {
        const history_point *other = find_date(pb, pb_count, pe[index].date);
        tdx_valuation_point *point;
        if (written >= capacity) {
            tdx_error_set(err, "the merged history holds more than %zu dates", capacity);
            goto done;
        }
        point = &out[written];
        memset(point, 0, sizeof(*point));
        memcpy(point->date, pe[index].date, sizeof(point->date));
        point->has_pe = pe[index].has_value;
        point->pe = pe[index].value;
        point->has_pe_percentile = pe[index].has_percentile;
        point->pe_percentile = pe[index].percentile;
        if (other) {
            point->has_pb = other->has_value;
            point->pb = other->value;
            point->has_pb_percentile = other->has_percentile;
            point->pb_percentile = other->percentile;
            if (merge)
                merge->both_sides++;
        } else if (merge) {
            merge->pe_only++;
        }
        written++;
    }
    for (index = 0; index < pb_count; ++index) {
        tdx_valuation_point *point;
        if (find_date(pe, pe_count, pb[index].date))
            continue;
        if (written >= capacity) {
            tdx_error_set(err, "the merged history holds more than %zu dates", capacity);
            goto done;
        }
        point = &out[written];
        memset(point, 0, sizeof(*point));
        memcpy(point->date, pb[index].date, sizeof(point->date));
        point->has_pb = pb[index].has_value;
        point->pb = pb[index].value;
        point->has_pb_percentile = pb[index].has_percentile;
        point->pb_percentile = pb[index].percentile;
        if (merge)
            merge->pb_only++;
        written++;
    }
    /* Ascending by date, so the merged series reads in order whatever order it arrived in. */
    for (index = 1; index < written; ++index) {
        tdx_valuation_point key = out[index];
        size_t position = index;
        while (position > 0 && strcmp(out[position - 1].date, key.date) > 0) {
            out[position] = out[position - 1];
            position--;
        }
        out[position] = key;
    }
    if (merge) {
        merge->points = written;
        merge->complete = merge->pe_only == 0 && merge->pb_only == 0 && written > 0;
    }
    status = TDX_OK;

done:
    free(pe);
    free(pb);
    return status;
}

int tdx_valuation_parse_funds(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                              tdx_valuation_fund *out, size_t capacity, size_t *out_count,
                              size_t *skipped_count, tdx_error *err) {
    size_t stored = 0;
    size_t skipped = 0;
    size_t row;

    if (out_count)
        *out_count = 0;
    if (skipped_count)
        *skipped_count = 0;
    if (!doc || !group || !out)
        return TDX_OK; /* an absent funds resource is not a failure */
    for (row = 0; row < group->row_count; ++row) {
        tdx_valuation_fund *item;
        tdx_bond_text code = cell(doc, group, row, "$ZQDM");
        tdx_bond_text market = cell(doc, group, row, "$SC");
        int market_id = -1;
        char prefix[16];

        if (stored >= capacity) {
            tdx_error_set(err, "the funds resource holds more than %zu rows", capacity);
            return TDX_ERR;
        }
        if (!valid_code(code.data, code.length) ||
            tdx_bonds_market_id(market.data, market.length, &market_id, NULL) != TDX_OK) {
            skipped++;
            continue;
        }
        if (tdx_bonds_market_prefix(market_id, prefix, sizeof(prefix), err) != TDX_OK)
            return TDX_ERR;
        item = &out[stored];
        memset(item, 0, sizeof(*item));
        item->market_id = market_id;
        snprintf(item->security_id, sizeof(item->security_id), "%s%.*s", prefix,
                 (int)code.length, code.data);
        item->has_unit_nav = cell_number(doc, group, row, "dwjz", &item->unit_nav);
        item->has_premium_pct = cell_number(doc, group, row, "yjl", &item->premium_pct);
        item->has_size_yuan = cell_number(doc, group, row, "zxfe", &item->size_yuan);
        item->fund_type = cell(doc, group, row, "jjlx");
        stored++;
    }
    if (out_count)
        *out_count = stored;
    if (skipped_count)
        *skipped_count = skipped;
    return TDX_OK;
}
