/* tdx_industry.c - the industry valuation resource. */
#include "tdx_industry.h"

#include <math.h>
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

/* Counts the pieces of a list, ignoring empties.
 *
 * THE TWO LISTS USE DIFFERENT SEPARATORS, which the fixture's own values showed: $S_ZQDM
 * separates members with an ASCII comma ("0|000002,0|000014") while sszt separates themes
 * with the ideographic comma U+3001.  Counting them the same way gives every row exactly
 * one theme, which is exactly the kind of plausible number that ships unnoticed. */
static size_t count_pieces(const tdx_bond_text *text, int ideographic) {
    size_t count = 0;
    size_t index;
    int in_piece = 0;

    if (!text->present || !text->data)
        return 0;
    for (index = 0; index < text->length; ++index) {
        unsigned char ch = (unsigned char)text->data[index];
        int separator = ch == ',';
        /* U+3001 is E3 80 81 in UTF-8. */
        if (ideographic && ch == 0xe3 && index + 2 < text->length &&
            (unsigned char)text->data[index + 1] == 0x80 &&
            (unsigned char)text->data[index + 2] == 0x81) {
            separator = 1;
            index += 2;
        }
        if (separator) {
            if (in_piece)
                count++;
            in_piece = 0;
        } else if (ch != ' ' && ch != '\t') {
            in_piece = 1;
        }
    }
    if (in_piece)
        count++;
    return count;
}

int tdx_industry_parse(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                       tdx_industry_row *out, size_t capacity, size_t *out_count,
                       size_t *skipped_count, tdx_error *err) {
    size_t stored = 0;
    size_t skipped = 0;
    size_t row;

    if (out_count)
        *out_count = 0;
    if (skipped_count)
        *skipped_count = 0;
    if (!doc || !group || !out) {
        tdx_error_set(err, "parsing the industry resource needs a document, a group and an "
                           "output");
        return TDX_ERR;
    }
    for (row = 0; row < group->row_count; ++row) {
        tdx_industry_row *item;
        tdx_bond_text stock_code = cell(doc, group, row, "$ZQDM");
        tdx_bond_text stock_market = cell(doc, group, row, "$SC");
        tdx_bond_text industry_code = cell(doc, group, row, "$ZQDM1");
        tdx_bond_text industry_market = cell(doc, group, row, "$SC1");
        int stock_market_id = -1;
        int industry_market_id = -1;
        char prefix[16];

        if (stored >= capacity) {
            tdx_error_set(err, "the industry resource holds more than %zu rows", capacity);
            return TDX_ERR;
        }
        /* A row that cannot name both the stock and the industry says nothing useful, so it
         * is skipped and counted - the reference throws instead, which would cost a caller
         * the whole file over one row. */
        if (!valid_code(stock_code.data, stock_code.length) ||
            !valid_code(industry_code.data, industry_code.length)) {
            skipped++;
            continue;
        }
        if (tdx_bonds_market_id(stock_market.data, stock_market.length, &stock_market_id,
                                NULL) != TDX_OK ||
            tdx_bonds_market_id(industry_market.data, industry_market.length,
                                &industry_market_id, NULL) != TDX_OK) {
            skipped++;
            continue;
        }
        if (tdx_bonds_market_prefix(stock_market_id, prefix, sizeof(prefix), err) != TDX_OK)
            return TDX_ERR;
        item = &out[stored];
        memset(item, 0, sizeof(*item));
        item->stock_market_id = stock_market_id;
        snprintf(item->stock_security_id, sizeof(item->stock_security_id), "%s%.*s", prefix,
                 (int)stock_code.length, stock_code.data);
        item->stock_code = stock_code;
        item->industry_market_id = industry_market_id;
        snprintf(item->industry_code, sizeof(item->industry_code), "%.*s",
                 (int)industry_code.length, industry_code.data);
        if (tdx_bonds_market_prefix(industry_market_id, prefix, sizeof(prefix), err) != TDX_OK)
            return TDX_ERR;
        snprintf(item->industry_security_id, sizeof(item->industry_security_id), "%s%.*s",
                 prefix, (int)industry_code.length, industry_code.data);
        item->industry_name = cell(doc, group, row, "TDXHY");
        item->has_pe = cell_number(doc, group, row, "hyPE", &item->pe);
        item->has_pb = cell_number(doc, group, row, "hyPB", &item->pb);
        {
            tdx_bond_text members = cell(doc, group, row, "$S_ZQDM");
            item->declared_count = count_pieces(&members, 0);
        }
        item->themes = cell(doc, group, row, "sszt");
        item->theme_count = count_pieces(&item->themes, 1);
        stored++;
    }
    if (out_count)
        *out_count = stored;
    if (skipped_count)
        *skipped_count = skipped;
    return TDX_OK;
}

/* --- the catalog ------------------------------------------------------ */

typedef struct industry_member_set {
    char ids[TDX_INDUSTRY_MEMBERS_MAX][24];
    size_t count;
} industry_member_set;

/* The array is read, not written; pre-C23 pedantic warns when char (*)[24] is passed
 * where const char (*)[24] is expected, so the parameter follows the caller. */
static int contains_id(char (*ids)[24], size_t count, const char *id) {
    size_t index;
    for (index = 0; index < count; ++index)
        if (strcmp(ids[index], id) == 0)
            return 1;
    return 0;
}

static int text_equals(const char *left, const tdx_bond_text *right) {
    if (!right->present || !right->data)
        return left[0] == '\0';
    return strlen(left) == right->length && memcmp(left, right->data, right->length) == 0;
}

int tdx_industry_catalog(const tdx_industry_row *rows, size_t count, tdx_industry *out,
                         size_t capacity, size_t *out_count, tdx_error *err) {
    industry_member_set *members = NULL;
    size_t written = 0;
    size_t index;
    int status = TDX_ERR;

    if (out_count)
        *out_count = 0;
    if (!out) {
        tdx_error_set(err, "the industry catalog needs an output");
        return TDX_ERR;
    }
    members = (industry_member_set *)calloc(capacity ? capacity : 1, sizeof(*members));
    if (!members) {
        tdx_error_set(err, "out of memory for the industry membership sets");
        return TDX_ERR;
    }
    for (index = 0; index < count; ++index) {
        size_t scan;
        tdx_industry *slot = NULL;
        for (scan = 0; scan < written; ++scan)
            if (strcmp(out[scan].code, rows[index].industry_code) == 0) {
                slot = &out[scan];
                break;
            }
        if (!slot) {
            if (written >= capacity) {
                tdx_error_set(err, "the resource names more than %zu industries", capacity);
                goto done;
            }
            slot = &out[written];
            memset(slot, 0, sizeof(*slot));
            snprintf(slot->code, sizeof(slot->code), "%s", rows[index].industry_code);
            snprintf(slot->security_id, sizeof(slot->security_id), "%s",
                     rows[index].industry_security_id);
            slot->market_id = rows[index].industry_market_id;
            if (rows[index].industry_name.present && rows[index].industry_name.length &&
                rows[index].industry_name.length < sizeof(slot->name))
                memcpy(slot->name, rows[index].industry_name.data,
                       rows[index].industry_name.length);
            slot->name[sizeof(slot->name) - 1] = '\0';
            slot->has_pe = rows[index].has_pe;
            slot->pe = rows[index].pe;
            slot->has_pb = rows[index].has_pb;
            slot->pb = rows[index].pb;
            members[written].count = 0;
            /* First wins: every row of an industry carries the same declared list, so the
             * only question is what happens if one does not - and a value that stays put
             * is easier to reason about than one that follows the last row. */
            slot->declared_count = rows[index].declared_count;
            written++;
        } else {
            /* A disagreement is RECORDED, not refused: one industry with a stale row should
             * not cost the caller the rest.  Measured, no industry in this resource
             * disagrees at all, so the flag is false everywhere today - which is the point
             * of keeping it, since the interesting case is the one that appears later. */
            if (slot->market_id != rows[index].industry_market_id ||
                !text_equals(slot->name, &rows[index].industry_name) ||
                slot->has_pe != rows[index].has_pe ||
                (slot->has_pe && slot->pe != rows[index].pe) ||
                slot->has_pb != rows[index].has_pb ||
                (slot->has_pb && slot->pb != rows[index].pb))
                slot->inconsistent = 1;
        }
        /* One stock counted once, however many rows name it. */
        {
            size_t slot_index = (size_t)(slot - out);
            if (slot_index < capacity &&
                !contains_id(members[slot_index].ids, members[slot_index].count,
                             rows[index].stock_security_id)) {
                if (members[slot_index].count >= TDX_INDUSTRY_MEMBERS_MAX) {
                    tdx_error_set(err, "industry %s holds more than %d members",
                                  slot->code, TDX_INDUSTRY_MEMBERS_MAX);
                    goto done;
                }
                snprintf(members[slot_index].ids[members[slot_index].count], 24, "%s",
                         rows[index].stock_security_id);
                members[slot_index].count++;
            }
        }
    }
    /* Ascending by code, and the two counts compared once everything is folded. */
    for (index = 1; index < written; ++index) {
        tdx_industry key = out[index];
        industry_member_set key_members = members[index];
        size_t position = index;
        while (position > 0 && strcmp(out[position - 1].code, key.code) > 0) {
            out[position] = out[position - 1];
            members[position] = members[position - 1];
            position--;
        }
        out[position] = key;
        members[position] = key_members;
    }
    for (index = 0; index < written; ++index) {
        out[index].row_count = members[index].count;
        out[index].counts_agree = members[index].count == out[index].declared_count;
    }
    if (out_count)
        *out_count = written;
    status = TDX_OK;

done:
    free(members);
    return status;
}
