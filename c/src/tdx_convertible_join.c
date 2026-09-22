/* tdx_convertible_join.c - the convertible-bond six-document join. */
#include "tdx_convertible_join.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The row of a document that carries this bond, or -1.  The rule the reference
 * states is exactly "same market, same code", so that is what this compares. */
static long find_row(const tdx_jsn_document *doc, int market, const char *code,
                     size_t code_length) {
    const tdx_json_node *root;
    const tdx_json_node *group;
    const tdx_json_node *data;
    size_t row;

    if (!doc || !code)
        return -1;
    root = tdx_json_root(&doc->json);
    group = tdx_json_at(&doc->json, root, 0);
    if (!group)
        return -1;
    data = tdx_json_member(&doc->json, group, "data");
    if (!data)
        return -1;
    for (row = 0; row < data->child_count; ++row) {
        tdx_bond_text row_code = tdx_bonds_cell_text(doc, &doc->groups[0], row, "$ZQDM");
        tdx_bond_text row_market = tdx_bonds_cell_text(doc, &doc->groups[0], row, "$SC");
        int row_market_id = -1;
        if (!row_code.present || row_code.length != code_length ||
            memcmp(row_code.data, code, code_length) != 0)
            continue;
        if (!row_market.present)
            continue;
        if (tdx_bonds_market_id(row_market.data, row_market.length, &row_market_id, NULL) !=
            TDX_OK)
            continue;
        if (row_market_id == market)
            return (long)row;
    }
    return -1;
}

/* A row from one document, with the document it came from. */
typedef struct {
    const tdx_jsn_document *doc;
    const tdx_jsn_group *group;
    long row;
} source_row;

static source_row locate(const tdx_jsn_document *doc, int market, const char *code,
                         size_t code_length) {
    source_row result;
    result.doc = doc;
    result.group = NULL;
    result.row = -1;
    if (!doc || doc->group_count == 0)
        return result;
    result.row = find_row(doc, market, code, code_length);
    if (result.row >= 0)
        result.group = &doc->groups[0];
    return result;
}

/* Text and number reads that tolerate an absent document or an absent key. */
static tdx_bond_text row_text(const source_row *source, const char *key) {
    tdx_bond_text empty;
    empty.data = NULL;
    empty.length = 0;
    empty.present = 0;
    if (source->row < 0 || !source->doc || !source->group)
        return empty;
    return tdx_bonds_cell_text(source->doc, source->group, (size_t)source->row, key);
}

static int row_number(const source_row *source, const char *key, double *out) {
    if (source->row < 0 || !source->doc || !source->group)
        return 0;
    return tdx_bonds_cell_number(source->doc, source->group, (size_t)source->row, key, out);
}

static void read_trigger(const source_row *source, const char *start_key, const char *price_key,
                         const char *history_count_key, const char *history_dates_key,
                         const char *available_days_key, tdx_convertible_trigger *out) {
    memset(out, 0, sizeof(*out));
    out->condition = row_text(source, "CFSJTJ");
    out->has_price_ratio_pct = row_number(source, "CFJGBL", &out->price_ratio_pct);
    out->start_date = row_text(source, start_key);
    out->has_trigger_price = row_number(source, price_key, &out->trigger_price);
    out->has_conversion_price = row_number(source, "ZGJG", &out->conversion_price);
    out->current_days = row_text(source, "CFJD");
    out->has_current_ratio_pct = row_number(source, "CFJDBL", &out->current_ratio_pct);
    out->status = row_text(source, "CFQK");
    out->has_history_count = row_number(source, history_count_key, &out->history_count);
    out->history_dates = row_text(source, history_dates_key);
    out->has_available_days = row_number(source, available_days_key, &out->available_days);
}

int tdx_convertible_join(const tdx_convertible_documents *documents, const tdx_code *identity,
                         tdx_convertible_row *out, tdx_convertible_extra *extra,
                         tdx_convertible_join_flags *flags, tdx_error *err) {
    source_row overview;
    source_row progress;
    source_row coupons;
    source_row sellback;
    source_row redemption;
    source_row revision;
    size_t index;

    if (!documents || !identity || !out || !extra || !flags) {
        tdx_error_set(err, "the convertible-bond join needs the documents, an identity and outputs");
        return TDX_ERR;
    }
    memset(extra, 0, sizeof(*extra));
    memset(flags, 0, sizeof(*flags));

    if (identity->market_id < 0 || identity->market_id > 255) {
        tdx_error_set(err, "the convertible-bond identity has no market");
        return TDX_ERR;
    }
    overview = locate(documents->overview, identity->market_id, identity->code,
                      strlen(identity->code));
    progress = locate(documents->progress, identity->market_id, identity->code,
                      strlen(identity->code));
    coupons = locate(documents->coupons, identity->market_id, identity->code,
                     strlen(identity->code));
    sellback = locate(documents->sellback, identity->market_id, identity->code,
                      strlen(identity->code));
    redemption = locate(documents->redemption, identity->market_id, identity->code,
                        strlen(identity->code));
    revision = locate(documents->revision, identity->market_id, identity->code,
                      strlen(identity->code));

    flags->from_overview = overview.row >= 0;
    flags->from_progress = progress.row >= 0;
    flags->from_coupons = coupons.row >= 0;
    flags->from_sellback = sellback.row >= 0;
    flags->from_redemption = redemption.row >= 0;
    flags->from_revision = revision.row >= 0;
    if (overview.row < 0 && progress.row < 0 && coupons.row < 0 && sellback.row < 0 &&
        redemption.row < 0 && revision.row < 0) {
        tdx_error_set(err, "the bond %d/%s appears in none of the six documents",
                      identity->market_id, identity->code);
        return TDX_ERR;
    }

    /* The overview mapping is the same one the overview-only layer uses, so it is
     * called rather than restated.  When the overview has no such row, that layer
     * would refuse the identity, so the fields are simply left absent instead. */
    memset(out, 0, sizeof(*out));
    if (overview.row >= 0) {
        if (tdx_convertible_normalize(documents->overview, overview.group,
                                      (size_t)overview.row, out, err) != TDX_OK)
            return TDX_ERR;
    } else {
        /* Build the identity by hand, since there is no overview row to read it
         * from: the bond is known to exist because another document names it. */
        char prefix[16];
        const char *name;
        if (tdx_bonds_market_prefix(identity->market_id, prefix, sizeof(prefix), err) != TDX_OK)
            return TDX_ERR;
        name = tdx_bonds_market_name(identity->market_id);
        out->bond_market_id = identity->market_id;
        snprintf(out->bond_market, sizeof(out->bond_market), "%s", name ? name : prefix);
        snprintf(out->bond_security_id, sizeof(out->bond_security_id), "%s%s", prefix,
                 identity->code);
        out->bond_code.data = identity->code;
        out->bond_code.length = strlen(identity->code);
        out->bond_code.present = 1;
        out->kind = tdx_convertible_kind_of(identity->code, strlen(identity->code));
    }
    /* The name can also come from a document other than the overview. */
    if (!out->bond_name.present) {
        static const char *const name_keys[] = {"ZQJC"};
        out->bond_name = tdx_bonds_first_cell_text(progress.doc, progress.group,
                                                   progress.row >= 0 ? (size_t)progress.row : 0,
                                                   name_keys, 1);
    }

    /* progress */
    extra->has_issue_size_100m_yuan =
        row_number(&progress, "FXZL", &extra->issue_size_100m_yuan);
    extra->has_remaining_balance_100m_yuan =
        row_number(&progress, "ZQYE", &extra->remaining_balance_100m_yuan);
    extra->has_conversion_progress_pct =
        row_number(&progress, "ZGJD", &extra->conversion_progress_pct);
    extra->has_redeemed_amount_100m_yuan =
        row_number(&progress, "YSHME", &extra->redeemed_amount_100m_yuan);
    extra->has_sellback_amount_100m_yuan =
        row_number(&progress, "YHSME", &extra->sellback_amount_100m_yuan);
    extra->has_maturity_progress_pct =
        row_number(&progress, "DQJD", &extra->maturity_progress_pct);

    /* coupons: the term, six annual rates and the compensation rate. */
    extra->has_term_years = row_number(&coupons, "FXQX", &extra->term_years);
    for (index = 0; index < 6; ++index) {
        char key[16];
        snprintf(key, sizeof(key), "PMLL_%zu", index + 1);
        extra->has_rate[index] = row_number(&coupons, key, &extra->rates_pct[index]);
    }
    extra->has_compensation_rate_pct =
        row_number(&coupons, "BCLL", &extra->compensation_rate_pct);

    /* The two comma lists come from the overview, which is the only document that
     * carries them. */
    extra->payment_dates = row_text(&overview, "FXRQXL");
    extra->payment_rates = row_text(&overview, "FXLLXL");

    read_trigger(&sellback, "HSQSRQ", "HSJG", "YCFCS", "YHSRQ", "HSCFSYTS", &extra->sellback);
    read_trigger(&redemption, "SHQSRQ", "SHJG", "YCFCS", "YSHRQ", "SHCFSYTS",
                 &extra->redemption);
    read_trigger(&revision, "XZQSRQ", "CFJG", "ZGJTZCS", "YXZRQ", "XZCFSYTS", &extra->revision);
    return TDX_OK;
}

/* --- the key union ---------------------------------------------------- */

typedef struct {
    int market;
    char code[8];
} key_entry;

static int key_compare(const void *left, const void *right) {
    const key_entry *a = (const key_entry *)left;
    const key_entry *b = (const key_entry *)right;
    if (a->market != b->market)
        return a->market < b->market ? -1 : 1;
    return strcmp(a->code, b->code);
}

static int add_keys(const tdx_jsn_document *doc, key_entry *entries, size_t capacity,
                    size_t *count) {
    size_t row;

    if (!doc || doc->group_count == 0)
        return TDX_OK;
    for (row = 0; row < doc->groups[0].row_count; ++row) {
        tdx_bond_text code = tdx_bonds_cell_text(doc, &doc->groups[0], row, "$ZQDM");
        tdx_bond_text market_text = tdx_bonds_cell_text(doc, &doc->groups[0], row, "$SC");
        int market = -1;
        if (!code.present || code.length != 6 || !market_text.present)
            continue;
        if (tdx_bonds_market_id(market_text.data, market_text.length, &market, NULL) != TDX_OK)
            continue;
        if (*count >= capacity)
            return TDX_ERR; /* reported by the caller as a capacity problem */
        entries[*count].market = market;
        memcpy(entries[*count].code, code.data, 6);
        entries[*count].code[6] = '\0';
        (*count)++;
    }
    return TDX_OK;
}

int tdx_convertible_keys(const tdx_convertible_documents *documents, tdx_code *out,
                         size_t capacity, size_t *out_count, size_t *union_count,
                         tdx_error *err) {
    key_entry *entries;
    size_t count = 0;
    size_t unique = 0;
    size_t index;
    size_t room;
    const tdx_jsn_document *all[6];
    size_t doc_index;

    if (out_count)
        *out_count = 0;
    if (union_count)
        *union_count = 0;
    if (!documents || !out) {
        tdx_error_set(err, "the key union needs the documents and an output");
        return TDX_ERR;
    }
    all[0] = documents->overview;
    all[1] = documents->progress;
    all[2] = documents->coupons;
    all[3] = documents->sellback;
    all[4] = documents->redemption;
    all[5] = documents->revision;
    room = 0;
    for (doc_index = 0; doc_index < 6; ++doc_index) {
        if (!all[doc_index] || all[doc_index]->group_count == 0)
            continue;
        room += all[doc_index]->groups[0].row_count;
    }
    if (room == 0)
        return TDX_OK;
    entries = (key_entry *)malloc(room * sizeof(*entries));
    if (!entries) {
        tdx_error_set(err, "out of memory for %zu bond keys", room);
        return TDX_ERR;
    }
    for (doc_index = 0; doc_index < 6; ++doc_index) {
        if (add_keys(all[doc_index], entries, room, &count) != TDX_OK) {
            free(entries);
            tdx_error_set(err, "the bond keys do not fit in %zu entries", room);
            return TDX_ERR;
        }
    }
    qsort(entries, count, sizeof(*entries), key_compare);
    for (index = 0; index < count; ++index) {
        if (index > 0 && entries[index].market == entries[index - 1].market &&
            strcmp(entries[index].code, entries[index - 1].code) == 0)
            continue;
        if (unique < capacity) {
            out[unique].market_id = entries[index].market;
            memcpy(out[unique].code, entries[index].code, 7);
        }
        unique++;
    }
    free(entries);
    if (union_count)
        *union_count = unique;
    if (out_count)
        *out_count = unique < capacity ? unique : capacity;
    if (unique > capacity) {
        /* A partial fill is reported rather than silently truncated: the count says
         * how many the union really holds. */
        tdx_error_set(err, "the bond union holds %zu keys and the output holds %zu", unique,
                      capacity);
        return TDX_ERR;
    }
    return TDX_OK;
}
