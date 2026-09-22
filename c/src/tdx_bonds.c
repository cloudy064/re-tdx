/* tdx_bonds.c - bond reference rows: the field mapping over the JSN layer. */
#include "tdx_bonds.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The reference's table, in its own order. */
static const tdx_bond_profile profiles[] = {
    {"list/zqjrz201.jsn", 1, TDX_BOND_SCALE_ISSUE_100M_YUAN},
    {"list/zqdfzfz201.jsn", 1, TDX_BOND_SCALE_CLIENT_MASTER_HIDDEN},
    {"list/zqgsz201.jsn", 1, TDX_BOND_SCALE_CLIENT_MASTER_HIDDEN},
    {"list/zqgz201.jsn", 1, TDX_BOND_SCALE_CLIENT_MASTER_HIDDEN},
    {"list/zqqyz201.jsn", 1, TDX_BOND_SCALE_CLIENT_MASTER_HIDDEN},
    {"list/zqsmz201.jsn", 1, TDX_BOND_SCALE_CLIENT_MASTER_HIDDEN},
    {"list/zqzczczq201.jsn", 1, TDX_BOND_SCALE_CLIENT_MASTER_HIDDEN},
    {"list/zq_dfzfz201_1.jsn", 0, TDX_BOND_SCALE_OUTSTANDING_100M_YUAN},
    {"list/zq_dfzfz201_2.jsn", 0, TDX_BOND_SCALE_OUTSTANDING_100M_YUAN},
    {"list/zq_gsz201_1.jsn", 0, TDX_BOND_SCALE_OUTSTANDING_100M_YUAN},
    {"list/zq_gsz201_2.jsn", 0, TDX_BOND_SCALE_OUTSTANDING_100M_YUAN},
    {"list/zq_gz201_1.jsn", 0, TDX_BOND_SCALE_OUTSTANDING_100M_YUAN},
    {"list/zq_gz201_2.jsn", 0, TDX_BOND_SCALE_OUTSTANDING_100M_YUAN},
    {"list/zq_qyz201_1.jsn", 0, TDX_BOND_SCALE_OUTSTANDING_100M_YUAN},
    {"list/zq_qyz201_2.jsn", 0, TDX_BOND_SCALE_OUTSTANDING_100M_YUAN},
    {"list/zq_smz201_1.jsn", 0, TDX_BOND_SCALE_OUTSTANDING_100M_YUAN},
    {"list/zq_smz201_2.jsn", 0, TDX_BOND_SCALE_OUTSTANDING_100M_YUAN},
    {"list/zq_zczczq201_2.jsn", 0, TDX_BOND_SCALE_OUTSTANDING_100M_YUAN},
    {"list/zq_jrz201_1.jsn", 0, TDX_BOND_SCALE_ISSUE_100M_YUAN},
    {"list/zq_jrz201_2.jsn", 0, TDX_BOND_SCALE_ISSUE_100M_YUAN},
};

tdx_bond_profile tdx_bonds_profile(const char *resource) {
    tdx_bond_profile fallback;
    const char *bare = resource;
    size_t index;

    if (bare) {
        /* The profile table is keyed on the path under the "bi" prefix, so a
         * caller that passes "bi/list/..." or "/list/..." must match too. */
        if (strncmp(bare, "bi/", 3) == 0)
            bare += 3;
        while (*bare == '/' || *bare == '\\')
            bare++;
    }
    for (index = 0; index < sizeof(profiles) / sizeof(profiles[0]); ++index) {
        if (bare && strcmp(profiles[index].resource, bare) == 0)
            return profiles[index];
    }
    fallback.resource = bare ? bare : "";
    fallback.reference_master = 0;
    fallback.scale = TDX_BOND_SCALE_ISSUE_YUAN;
    return fallback;
}

const char *tdx_bonds_market_name(int market_id) {
    switch (market_id) {
    case 0:
        return "sz";
    case 1:
        return "sh";
    case 2:
    case 44:
        return "bj";
    default:
        return NULL; /* the reference spells anything else as its number */
    }
}

int tdx_bonds_market_prefix(int market_id, char *out, size_t capacity, tdx_error *err) {
    const char *name = tdx_bonds_market_name(market_id);
    int written;

    if (!out || capacity == 0) {
        tdx_error_set(err, "a market prefix needs a buffer");
        return TDX_ERR;
    }
    if (name)
        written = snprintf(out, capacity, "%c%c", toupper((unsigned char)name[0]),
                           toupper((unsigned char)name[1]));
    else
        written = snprintf(out, capacity, "M%d:", market_id);
    if (written < 0 || (size_t)written >= capacity) {
        tdx_error_set(err, "the market prefix does not fit in %zu bytes", capacity);
        return TDX_ERR;
    }
    return TDX_OK;
}

int tdx_bonds_market_id(const char *text, size_t length, int *out, tdx_error *err) {
    char scratch[16];
    char *stop = NULL;
    long value;
    size_t index;

    if (!out) {
        tdx_error_set(err, "a market id needs an output");
        return TDX_ERR;
    }
    if (!text) {
        tdx_error_set(err, "the bond market field is missing");
        return TDX_ERR;
    }
    /* Trim, because the resource pads its fields. */
    while (length > 0 && isspace((unsigned char)*text)) {
        text++;
        length--;
    }
    while (length > 0 && isspace((unsigned char)text[length - 1]))
        length--;
    if (length == 0 || length >= sizeof(scratch)) {
        tdx_error_set(err, "the bond market field is not a number");
        return TDX_ERR;
    }
    for (index = 0; index < length; ++index)
        scratch[index] = text[index];
    scratch[length] = '\0';
    value = strtol(scratch, &stop, 10);
    if (!stop || *stop != '\0' || value < 0 || value > 255) {
        tdx_error_set(err, "the bond market field is not a market: %s", scratch);
        return TDX_ERR;
    }
    *out = (int)value;
    return TDX_OK;
}

/* --- cell helpers ----------------------------------------------------- */

/* Reads a cell as text, ZERO COPY: the returned pointer is the document's own
 * arena, which is why the header requires the document to outlive the row.  The
 * obvious-looking alternative - appending into a tdx_buf and returning that - is
 * wrong, because the buffer owns a COPY and freeing it would leave the pointer
 * dangling.
 *
 * A cell that is present but not a JSON string is reported as absent for text
 * fields.  The JSN resources carry their values as strings, so this cannot silently
 * lose a value in practice; a caller that needs the JSON form has tdx_jsn_cell_json. */
tdx_bond_text tdx_bonds_cell_text(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                                  size_t row, const char *key) {
    tdx_bond_text result;
    size_t column;

    result.data = NULL;
    result.length = 0;
    result.present = 0;
    if (!doc || !group || !key)
        return result;
    for (column = 0; column < group->column_count; ++column) {
        const char *name = tdx_jsn_column_name(doc, group, column);
        const char *data = NULL;
        size_t length = 0;
        int present = 0;
        if (!name || strcmp(name, key) != 0)
            continue;
        if (tdx_jsn_cell_view(doc, group, row, column, &data, &length, &present, NULL) != TDX_OK)
            return result;
        if (present && data && length > 0) {
            /* The reference TRIMS every text value, and a value that is only
             * whitespace counts as absent.  Both are observable: a resource that
             * pads a field would otherwise give this layer trailing spaces the
             * reference never produces, and a padded empty field would read as
             * present.  This step was missing from the first bond mapping. */
            while (length > 0 && isspace((unsigned char)*data)) {
                data++;
                length--;
            }
            while (length > 0 && isspace((unsigned char)data[length - 1]))
                length--;
            if (length > 0) {
                result.data = data;
                result.length = length;
                result.present = 1;
            }
        }
        return result;
    }
    return result;
}

/* The first of several candidate column names that has a value. */
tdx_bond_text tdx_bonds_first_cell_text(const tdx_jsn_document *doc,
                                        const tdx_jsn_group *group, size_t row,
                                        const char *const *keys, size_t key_count) {
    size_t index;
    for (index = 0; index < key_count; ++index) {
        tdx_bond_text value = tdx_bonds_cell_text(doc, group, row, keys[index]);
        if (value.present)
            return value;
    }
    {
        tdx_bond_text empty;
        empty.data = NULL;
        empty.length = 0;
        empty.present = 0;
        return empty;
    }
}

int tdx_bonds_cell_number(const tdx_jsn_document *doc, const tdx_jsn_group *group, size_t row,
                          const char *key, double *out) {
    tdx_bond_text text = tdx_bonds_cell_text(doc, group, row, key);
    char scratch[64];
    char *stop = NULL;
    double value;

    if (!text.present || text.length == 0 || text.length >= sizeof(scratch))
        return 0;
    memcpy(scratch, text.data, text.length);
    scratch[text.length] = '\0';
    value = strtod(scratch, &stop);
    if (!stop || stop == scratch)
        return 0;
    while (*stop == ' ' || *stop == '\t')
        stop++;
    if (*stop != '\0' || !isfinite(value))
        return 0;
    *out = value;
    return 1;
}

/* --- mapping ---------------------------------------------------------- */

/* Builds the identity from the columns the profile says to use.  The reference
 * treats an unreadable market as fatal rather than skipping the row, so this does
 * too: a row whose identity cannot be formed cannot be attributed to a security. */
static int set_identity(const tdx_jsn_document *doc, const tdx_jsn_group *group, size_t row,
                        const char *code_key, const char *market_key, int with_prefix,
                        tdx_bond_text *code_out, int *market_id_out, char *market_text,
                        size_t market_size, char *identity, size_t identity_size,
                        tdx_error *err) {
    tdx_bond_text code = tdx_bonds_cell_text(doc, group, row, code_key);
    tdx_bond_text market_text_value = tdx_bonds_cell_text(doc, group, row, market_key);
    char prefix[16];
    int market = -1;
    const char *name;

    *code_out = code;
    market_text[0] = '\0';
    identity[0] = '\0';
    *market_id_out = -1;
    if (!code.present) {
        tdx_error_set(err, "the bond row has no %s column value", code_key);
        return TDX_ERR;
    }
    if (tdx_bonds_market_id(market_text_value.present ? market_text_value.data : "",
                            market_text_value.present ? market_text_value.length : 0, &market,
                            err) != TDX_OK)
        return TDX_ERR;
    if (tdx_bonds_market_prefix(market, prefix, sizeof(prefix), err) != TDX_OK)
        return TDX_ERR;
    name = tdx_bonds_market_name(market);
    snprintf(market_text, market_size, "%s", name ? name : prefix);
    if (with_prefix)
        snprintf(identity, identity_size, "%s%.*s", prefix, (int)code.length, code.data);
    else
        snprintf(identity, identity_size, "%.*s", (int)code.length, code.data);
    *market_id_out = market;
    return TDX_OK;
}

int tdx_bonds_normalize(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                        size_t row_in_group, const char *resource, tdx_bond_row *out,
                        tdx_error *err) {
    tdx_bond_profile profile;
    double issue_size = 0.0;
    int has_issue_size;
    static const char *const bond_rating_keys[] = {"ZQXY", "ZQPJ"};
    static const char *const issuer_rating_keys[] = {"ZTXY", "ZTPJ"};
    static const char *const accrual_keys[] = {"QXSJ", "QXRQ"};
    static const char *const maturity_keys[] = {"DQSJ", "DQRQ"};

    if (!doc || !group || !out) {
        tdx_error_set(err, "normalizing a bond row needs a document, a group and an output");
        return TDX_ERR;
    }
    memset(out, 0, sizeof(*out));
    profile = tdx_bonds_profile(resource);
    out->scale = profile.scale;

    /* A reference master names itself in $ZQDM1/$SC1 and its underlying in
     * $ZQDM/$SC; a projection does the opposite. */
    if (set_identity(doc, group, row_in_group, profile.reference_master ? "$ZQDM1" : "$ZQDM",
                     profile.reference_master ? "$SC1" : "$SC", 1, &out->code, &out->market_id,
                     out->market, sizeof(out->market), out->security_id,
                     sizeof(out->security_id), err) != TDX_OK)
        return TDX_ERR;
    out->name = tdx_bonds_cell_text(doc, group, row_in_group, "ZQJC");
    out->name_resolved = out->name.present;

    if (profile.reference_master)
        out->client_instrument_id = tdx_bonds_cell_text(doc, group, row_in_group, "$ZQDM");

    out->bond_type = tdx_bonds_cell_text(doc, group, row_in_group, "ZQLX");
    out->bond_credit_rating =
        tdx_bonds_first_cell_text(doc, group, row_in_group, bond_rating_keys, 2);
    out->issuer_credit_rating =
        tdx_bonds_first_cell_text(doc, group, row_in_group, issuer_rating_keys, 2);
    out->rate_type = tdx_bonds_cell_text(doc, group, row_in_group, "LLLX");
    out->rate_type_flag = tdx_bonds_cell_text(doc, group, row_in_group, "LLLXBZ");
    out->guarantee_status = tdx_bonds_cell_text(doc, group, row_in_group, "SFDB");

    out->accrual_start_date = tdx_bonds_first_cell_text(doc, group, row_in_group, accrual_keys, 2);
    out->maturity_date = tdx_bonds_first_cell_text(doc, group, row_in_group, maturity_keys, 2);
    out->next_coupon_date = tdx_bonds_cell_text(doc, group, row_in_group, "XGFXRQ");
    out->last_coupon_date = tdx_bonds_cell_text(doc, group, row_in_group, "SGFXRQ");
    out->listing_date = tdx_bonds_cell_text(doc, group, row_in_group, "SSRQ");
    out->conversion_start_date = tdx_bonds_cell_text(doc, group, row_in_group, "ZGQSR");
    out->conversion_end_date = tdx_bonds_cell_text(doc, group, row_in_group, "ZGJZR");

    out->has_remaining_years =
        tdx_bonds_cell_number(doc, group, row_in_group, "SYNX", &out->remaining_years);
    out->has_current_coupon_rate_pct =
        tdx_bonds_cell_number(doc, group, row_in_group, "DQLL", &out->current_coupon_rate_pct);
    out->has_coupon_frequency_months =
        tdx_bonds_cell_number(doc, group, row_in_group, "FXPL1", &out->coupon_frequency_months);
    out->has_face_value_yuan =
        tdx_bonds_cell_number(doc, group, row_in_group, "MZ", &out->face_value_yuan);
    out->has_issue_price_yuan =
        tdx_bonds_cell_number(doc, group, row_in_group, "FXJG", &out->issue_price_yuan);
    out->has_remaining_coupon_count =
        tdx_bonds_cell_number(doc, group, row_in_group, "SYFXCS", &out->remaining_coupon_count);
    out->has_conversion_price_yuan =
        tdx_bonds_cell_number(doc, group, row_in_group, "ZGJ", &out->conversion_price_yuan);
    out->has_revision_trigger_pct =
        tdx_bonds_cell_number(doc, group, row_in_group, "XXCFBL", &out->revision_trigger_pct);
    out->has_put_trigger_pct =
        tdx_bonds_cell_number(doc, group, row_in_group, "HSCFBL", &out->put_trigger_pct);
    out->has_call_trigger_pct =
        tdx_bonds_cell_number(doc, group, row_in_group, "QSCFBL", &out->call_trigger_pct);

    /* The size column, under this resource's own semantics. */
    has_issue_size = tdx_bonds_cell_number(doc, group, row_in_group, "GM", &issue_size);
    out->has_source_scale = has_issue_size;
    out->source_scale_raw = has_issue_size ? issue_size : 0.0;
    switch (profile.scale) {
    case TDX_BOND_SCALE_ISSUE_100M_YUAN:
        if (has_issue_size) {
            out->has_issue_size_source_100m = 1;
            out->issue_size_source_100m = issue_size;
            out->has_issue_size_yuan = 1;
            out->issue_size_yuan = issue_size * 100000000.0;
        }
        break;
    case TDX_BOND_SCALE_OUTSTANDING_100M_YUAN:
        if (has_issue_size) {
            out->has_outstanding_balance_source_100m = 1;
            out->outstanding_balance_source_100m = issue_size;
            out->has_outstanding_balance_yuan = 1;
            out->outstanding_balance_yuan = issue_size * 100000000.0;
        }
        break;
    case TDX_BOND_SCALE_CLIENT_MASTER_HIDDEN:
        /* The unit is not recoverable, so neither converted field is filled. */
        break;
    case TDX_BOND_SCALE_ISSUE_YUAN:
    default:
        if (has_issue_size) {
            out->has_issue_size_yuan = 1;
            out->issue_size_yuan = issue_size;
        }
        break;
    }

    /* The underlying is named by the columns the profile did NOT use for identity,
     * so a master has none to report here. */
    if (!profile.reference_master) {
        tdx_bond_text underlying_code = tdx_bonds_cell_text(doc, group, row_in_group, "$ZQDM1");
        tdx_bond_text underlying_market_text = tdx_bonds_cell_text(doc, group, row_in_group, "$SC1");
        if (underlying_code.present && underlying_market_text.present) {
            int underlying_market = -1;
            char prefix[16];
            if (tdx_bonds_market_id(underlying_market_text.data, underlying_market_text.length,
                                    &underlying_market, err) == TDX_OK) {
                const char *name = tdx_bonds_market_name(underlying_market);
                out->has_underlying = 1;
                out->underlying_market_id = underlying_market;
                snprintf(out->underlying_market, sizeof(out->underlying_market), "%s",
                         name ? name : "");
                snprintf(prefix, sizeof(prefix), "%s", "");
                if (tdx_bonds_market_prefix(underlying_market, prefix, sizeof(prefix), err) ==
                    TDX_OK)
                    snprintf(out->underlying_security_id, sizeof(out->underlying_security_id),
                             "%s%.*s", prefix, (int)underlying_code.length, underlying_code.data);
                out->underlying_code = underlying_code;
            }
        }
    }
    return TDX_OK;
}

/* --- coupon schedule -------------------------------------------------- */

/* Splits on commas, trimming each piece.  Returns how many pieces the source holds,
 * so a caller can tell "the list is empty" from "the caller's buffer is too small". */
static size_t split_count(const char *text, size_t length) {
    size_t count = 0;
    size_t index;
    int in_piece = 0;

    if (!text || length == 0)
        return 0;
    for (index = 0; index < length; ++index) {
        if (text[index] == ',') {
            in_piece = 0;
        } else if (!isspace((unsigned char)text[index]) && !in_piece) {
            in_piece = 1;
            count++;
        }
    }
    return count;
}

static void split_piece(const char *text, size_t length, size_t ordinal, const char **out,
                        size_t *out_length) {
    size_t seen = 0;
    size_t index = 0;

    *out = NULL;
    *out_length = 0;
    while (index < length) {
        size_t start;
        size_t stop;
        while (index < length && (text[index] == ',' || isspace((unsigned char)text[index])))
            index++;
        start = index;
        while (index < length && text[index] != ',')
            index++;
        stop = index;
        while (stop > start && isspace((unsigned char)text[stop - 1]))
            stop--;
        if (stop > start) {
            if (seen == ordinal) {
                *out = text + start;
                *out_length = stop - start;
                return;
            }
            seen++;
        }
    }
}

int tdx_bonds_coupon_schedule(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                              size_t row_in_group, const char *dates_key, const char *rates_key,
                              tdx_bond_coupon *out, size_t capacity, size_t *out_count,
                              size_t *list_length, tdx_error *err) {
    tdx_bond_text dates = tdx_bonds_cell_text(doc, group, row_in_group, dates_key);
    tdx_bond_text rates = tdx_bonds_cell_text(doc, group, row_in_group, rates_key);
    size_t total;
    size_t stored = 0;
    size_t index;

    if (out_count)
        *out_count = 0;
    if (list_length)
        *list_length = 0;
    if (!doc || !group || !out) {
        tdx_error_set(err, "a coupon schedule needs a document, a group and an output");
        return TDX_ERR;
    }
    total = split_count(dates.data, dates.length);
    if (list_length)
        *list_length = total;
    if (total == 0)
        return TDX_OK;
    if (total > capacity) {
        tdx_error_set(err,
                      "the coupon list holds %zu entries and the output holds %zu; a truncated "
                      "schedule would silently drop coupons",
                      total, capacity);
        return TDX_ERR;
    }
    for (index = 0; index < total; ++index) {
        const char *date = NULL;
        size_t date_length = 0;
        const char *rate = NULL;
        size_t rate_length = 0;

        split_piece(dates.data, dates.length, index, &date, &date_length);
        out[stored].date.data = date;
        out[stored].date.length = date_length;
        out[stored].date.present = date != NULL;
        out[stored].has_rate = 0;
        out[stored].rate_pct = 0.0;
        if (index < split_count(rates.data, rates.length)) {
            char scratch[64];
            char *stop = NULL;
            double value;
            split_piece(rates.data, rates.length, index, &rate, &rate_length);
            if (rate && rate_length > 0 && rate_length < sizeof(scratch)) {
                memcpy(scratch, rate, rate_length);
                scratch[rate_length] = '\0';
                value = strtod(scratch, &stop);
                if (stop && stop != scratch && isfinite(value)) {
                    out[stored].has_rate = 1;
                    /* The resource writes some rates as fractions (0.035) and some
                     * as percents (3.5); a magnitude of at most one can only be the
                     * fraction form, so that is what it is read as. */
                    out[stored].rate_pct = fabs(value) <= 1.0 ? value * 100.0 : value;
                }
            }
        }
        stored++;
    }
    if (out_count)
        *out_count = stored;
    return TDX_OK;
}
