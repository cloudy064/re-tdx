/* tdx_subscription.c - convertible-bond subscription events. */
#include "tdx_subscription.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define TDX_SUBSCRIPTION_FACE_VALUE 100.0

int tdx_subscription_conversion_value(double underlying_close, double conversion_price,
                                      double *out) {
    if (!out)
        return 0;
    /* A zero conversion price is not a price, and dividing by it would produce an
     * infinity that a caller would then print as a number. */
    if (!(conversion_price != 0.0) || !isfinite(underlying_close) ||
        !isfinite(conversion_price))
        return 0;
    *out = underlying_close * TDX_SUBSCRIPTION_FACE_VALUE / conversion_price;
    return isfinite(*out) ? 1 : 0;
}

int tdx_subscription_premium(double bond_close, double conversion_value, double *out) {
    if (!out)
        return 0;
    if (!(conversion_value != 0.0) || !isfinite(bond_close) || !isfinite(conversion_value))
        return 0;
    *out = (bond_close - conversion_value) * 100.0 / conversion_value;
    return isfinite(*out) ? 1 : 0;
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

static tdx_bond_text cell(const tdx_jsn_document *doc, const tdx_jsn_group *group, size_t row,
                          const char *key) {
    return tdx_bonds_cell_text(doc, group, row, key);
}

/* Builds a market/code identity.  Returns 0 when either half is unusable, which for
 * an identity column is a skip rather than an error. */
static int build_identity(const tdx_jsn_document *doc, const tdx_jsn_group *group, size_t row,
                          const char *code_key, const char *market_key, tdx_bond_text *code_out,
                          int *market_out, char *market_text, size_t market_size,
                          char *security_id, size_t security_size) {
    tdx_bond_text code = cell(doc, group, row, code_key);
    tdx_bond_text market_value = cell(doc, group, row, market_key);
    char prefix[16];
    int market = -1;
    const char *name;

    *code_out = code;
    market_text[0] = '\0';
    security_id[0] = '\0';
    *market_out = -1;
    if (!valid_code(code.data, code.length))
        return 0;
    if (tdx_bonds_market_id(market_value.data, market_value.length, &market, NULL) != TDX_OK)
        return 0;
    if (tdx_bonds_market_prefix(market, prefix, sizeof(prefix), NULL) != TDX_OK)
        return 0;
    name = tdx_bonds_market_name(market);
    snprintf(market_text, market_size, "%s", name ? name : prefix);
    snprintf(security_id, security_size, "%s%.*s", prefix, (int)code.length, code.data);
    *market_out = market;
    return 1;
}

int tdx_subscription_normalize(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                               tdx_subscription_row *out, size_t capacity, size_t *out_count,
                               size_t *skipped_count, tdx_error *err) {
    size_t stored = 0;
    size_t skipped = 0;
    size_t row;

    if (out_count)
        *out_count = 0;
    if (skipped_count)
        *skipped_count = 0;
    if (!doc || !group || !out) {
        tdx_error_set(err, "normalizing subscriptions needs a document, a group and an output");
        return TDX_ERR;
    }
    for (row = 0; row < group->row_count; ++row) {
        tdx_subscription_row *item;

        if (stored >= capacity) {
            tdx_error_set(err, "the subscription list holds more than %zu rows", capacity);
            return TDX_ERR;
        }
        item = &out[stored];
        memset(item, 0, sizeof(*item));
        /* BOTH identities have to be readable: an event that cannot be attached to a
         * bond and an underlying is not an event. */
        if (!build_identity(doc, group, row, "$ZQDM", "$SC", &item->bond_code,
                            &item->bond_market_id, item->bond_market,
                            sizeof(item->bond_market), item->bond_security_id,
                            sizeof(item->bond_security_id))) {
            skipped++;
            continue;
        }
        if (!build_identity(doc, group, row, "$ZQDM1", "$SC1", &item->stock_code,
                            &item->stock_market_id, item->stock_market,
                            sizeof(item->stock_market), item->stock_security_id,
                            sizeof(item->stock_security_id))) {
            skipped++;
            continue;
        }
        item->bond_name = cell(doc, group, row, "ZQJC");
        item->subscription_date = cell(doc, group, row, "sgrq");
        item->subscription_code = cell(doc, group, row, "sgdm");
        item->has_subscription_limit_10k_yuan =
            tdx_bonds_cell_number(doc, group, row, "sgsx", &item->subscription_limit_10k_yuan);
        item->conversion_start_date = cell(doc, group, row, "zgr");
        item->has_underlying_close_yuan =
            tdx_bonds_cell_number(doc, group, row, "zxj", &item->underlying_close_yuan);
        item->has_conversion_price_yuan =
            tdx_bonds_cell_number(doc, group, row, "zgj", &item->conversion_price_yuan);
        item->has_bond_close_yuan =
            tdx_bonds_cell_number(doc, group, row, "zxsp", &item->bond_close_yuan);
        item->has_issue_size_100m_yuan =
            tdx_bonds_cell_number(doc, group, row, "fxzs", &item->issue_size_100m_yuan);
        item->lottery_date = cell(doc, group, row, "zqr");
        item->has_lottery_rate_pct =
            tdx_bonds_cell_number(doc, group, row, "zql", &item->lottery_rate_pct);
        item->listing_date = cell(doc, group, row, "ssrq");
        item->listed = item->listing_date.present;

        /* The two derived metrics, each only when its inputs allow it. */
        if (item->has_underlying_close_yuan && item->has_conversion_price_yuan)
            item->has_conversion_value_yuan =
                tdx_subscription_conversion_value(item->underlying_close_yuan,
                                                  item->conversion_price_yuan,
                                                  &item->conversion_value_yuan);
        if (item->has_bond_close_yuan && item->has_conversion_value_yuan)
            item->has_conversion_premium_pct =
                tdx_subscription_premium(item->bond_close_yuan, item->conversion_value_yuan,
                                         &item->conversion_premium_pct);

        /* The event id, built the reference's way so the same event seen twice can be
         * recognised.  A missing date leaves an id ending in a colon, which is what the
         * reference produces too. */
        snprintf(item->event_id, sizeof(item->event_id), "convertible-subscription:%d:%.*s:%.*s",
                 item->bond_market_id, (int)item->bond_code.length, item->bond_code.data,
                 (int)item->subscription_date.length,
                 item->subscription_date.data ? item->subscription_date.data : "");
        stored++;
    }
    if (out_count)
        *out_count = stored;
    if (skipped_count)
        *skipped_count = skipped;
    return TDX_OK;
}
