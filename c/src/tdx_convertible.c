/* tdx_convertible.c - the convertible-bond overview document. */
#include "tdx_convertible.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

tdx_convertible_kind tdx_convertible_kind_of(const char *code, size_t length) {
    /* The reference decides on the code alone: 132 is an exchangeable bond. */
    if (code && length >= 3 && code[0] == '1' && code[1] == '3' && code[2] == '2')
        return TDX_CONVERTIBLE_EXCHANGEABLE;
    return TDX_CONVERTIBLE_BOND;
}

static int valid_code(const char *code, size_t length) {
    size_t index;
    if (!code || length != 6)
        return 0;
    for (index = 0; index < 6; ++index)
        if (code[index] < '0' || code[index] > '9')
            return 0;
    return 1;
}

/* The identity is built from a code column and a market column; both must be
 * usable or the row cannot be attributed to a security, which the reference treats
 * as fatal. */
static int build_identity(const tdx_jsn_document *doc, const tdx_jsn_group *group, size_t row,
                          const char *code_key, const char *market_key, tdx_bond_text *code_out,
                          int *market_id_out, char *market_text, size_t market_size,
                          char *security_id, size_t security_size, tdx_error *err) {
    tdx_bond_text code = tdx_bonds_cell_text(doc, group, row, code_key);
    tdx_bond_text market_text_value = tdx_bonds_cell_text(doc, group, row, market_key);
    char prefix[16];
    int market = -1;
    const char *name;

    *code_out = code;
    market_text[0] = '\0';
    security_id[0] = '\0';
    *market_id_out = -1;
    if (!code.present) {
        tdx_error_set(err, "the convertible-bond row has no %s value", code_key);
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
    snprintf(security_id, security_size, "%s%.*s", prefix, (int)code.length, code.data);
    *market_id_out = market;
    return TDX_OK;
}

int tdx_convertible_normalize(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                              size_t row_in_group, tdx_convertible_row *out, tdx_error *err) {
    tdx_bond_text stock_code;
    tdx_bond_text stock_market_text;
    int stock_market = -1;

    if (!doc || !group || !out) {
        tdx_error_set(err, "normalizing a convertible-bond row needs a document, a group and an "
                           "output");
        return TDX_ERR;
    }
    memset(out, 0, sizeof(*out));
    if (build_identity(doc, group, row_in_group, "$ZQDM", "$SC", &out->bond_code,
                       &out->bond_market_id, out->bond_market, sizeof(out->bond_market),
                       out->bond_security_id, sizeof(out->bond_security_id), err) != TDX_OK)
        return TDX_ERR;
    out->bond_name = tdx_bonds_cell_text(doc, group, row_in_group, "ZQJC");
    out->kind = tdx_convertible_kind_of(out->bond_code.data, out->bond_code.length);

    /* The underlying is present only when BOTH columns are usable: a code without a
     * market, or a market without a valid six-digit code, is not an underlying. */
    stock_code = tdx_bonds_cell_text(doc, group, row_in_group, "$ZQDM1");
    stock_market_text = tdx_bonds_cell_text(doc, group, row_in_group, "$SC1");
    if (stock_market_text.present &&
        tdx_bonds_market_id(stock_market_text.data, stock_market_text.length, &stock_market,
                            err) == TDX_OK &&
        valid_code(stock_code.data, stock_code.length)) {
        char prefix[16];
        const char *name;
        err->message[0] = '\0';
        if (tdx_bonds_market_prefix(stock_market, prefix, sizeof(prefix), err) != TDX_OK)
            return TDX_ERR;
        name = tdx_bonds_market_name(stock_market);
        out->has_underlying = 1;
        out->underlying_market_id = stock_market;
        snprintf(out->underlying_market, sizeof(out->underlying_market), "%s",
                 name ? name : prefix);
        snprintf(out->underlying_security_id, sizeof(out->underlying_security_id), "%s%.*s",
                 prefix, (int)stock_code.length, stock_code.data);
        out->underlying_code = stock_code;
        out->underlying_reference_code =
            tdx_bonds_cell_text(doc, group, row_in_group, "ZGDM");
        err->message[0] = '\0';
    } else {
        err->message[0] = '\0'; /* an unusable underlying is an absent one, not an error */
    }

    /* The overview's own columns, in the reference's order. */
    out->risk_notice = tdx_bonds_cell_text(doc, group, row_in_group, "FXTS");
    out->listing_date = tdx_bonds_cell_text(doc, group, row_in_group, "SSRQ");
    out->issue_date = tdx_bonds_cell_text(doc, group, row_in_group, "QXRQ");
    out->conversion_start_date = tdx_bonds_cell_text(doc, group, row_in_group, "ZGQSR");
    out->conversion_end_date = tdx_bonds_cell_text(doc, group, row_in_group, "ZGJZR");
    out->maturity_date = tdx_bonds_cell_text(doc, group, row_in_group, "DQRQ");
    out->bond_rating = tdx_bonds_cell_text(doc, group, row_in_group, "ZQPJ");
    out->issuer_rating = tdx_bonds_cell_text(doc, group, row_in_group, "ZTPJ");
    out->current_state = tdx_bonds_cell_text(doc, group, row_in_group, "DQLB");

    out->has_face_value = tdx_bonds_cell_number(doc, group, row_in_group, "MZ", &out->face_value);
    out->has_issue_price =
        tdx_bonds_cell_number(doc, group, row_in_group, "FXJG", &out->issue_price);
    out->has_return_since_listing_pct =
        tdx_bonds_cell_number(doc, group, row_in_group, "ZF1", &out->return_since_listing_pct);
    out->has_return_5d_pct =
        tdx_bonds_cell_number(doc, group, row_in_group, "ZF2", &out->return_5d_pct);
    out->has_return_10d_pct =
        tdx_bonds_cell_number(doc, group, row_in_group, "ZF3", &out->return_10d_pct);
    out->has_issue_size_100m_yuan =
        tdx_bonds_cell_number(doc, group, row_in_group, "FXZE", &out->issue_size_100m_yuan);
    out->has_remaining_balance_100m_yuan = tdx_bonds_cell_number(doc, group, row_in_group, "ZQYE",
                                                                 &out->remaining_balance_100m_yuan);
    out->has_remaining_ratio_pct =
        tdx_bonds_cell_number(doc, group, row_in_group, "YEZB", &out->remaining_ratio_pct);
    out->has_conversion_price =
        tdx_bonds_cell_number(doc, group, row_in_group, "ZGJ", &out->conversion_price);
    out->has_remaining_years =
        tdx_bonds_cell_number(doc, group, row_in_group, "SYNX", &out->remaining_years);
    out->has_maturity_redemption_price = tdx_bonds_cell_number(
        doc, group, row_in_group, "DQSHJ", &out->maturity_redemption_price);
    /* The unpaid coupon sum lives in the coupons document in this resource; the
     * column is read anyway, because the reference reads it from the same place it
     * reads everything else and reports null when it is not there. */
    out->has_unpaid_coupon_sum =
        tdx_bonds_cell_number(doc, group, row_in_group, "LLZH", &out->unpaid_coupon_sum);
    out->has_sellback_trigger_ratio_pct = tdx_bonds_cell_number(
        doc, group, row_in_group, "HSCFBL", &out->sellback_trigger_ratio_pct);
    out->has_redemption_trigger_ratio_pct = tdx_bonds_cell_number(
        doc, group, row_in_group, "QSCFBL", &out->redemption_trigger_ratio_pct);

    /* The reference's completeness test: the three terms without which a
     * convertible-bond row is not usable. */
    out->core_terms_complete = out->has_face_value && out->has_conversion_price &&
                               out->maturity_date.present;
    return TDX_OK;
}
