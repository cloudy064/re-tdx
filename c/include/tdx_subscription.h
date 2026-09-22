/* tdx_subscription.h - convertible-bond subscription events.
 *
 * The subscription list: bonds that are being or have been offered, with the two
 * standard metrics a convertible-bond trader reads first, both DERIVED here because
 * the resource does not carry them:
 *
 *     conversion_value_yuan = underlying_close_yuan * 100 / conversion_price_yuan
 *     conversion_premium_pct = (bond_close_yuan - conversion_value_yuan) * 100
 *                              / conversion_value_yuan
 *
 * The 100 is the face value in yuan, which is why the conversion value is what one
 * bond is worth if converted.  Both divisions are guarded: a zero conversion price or
 * a zero conversion value leaves the field ABSENT rather than producing an infinity
 * or a zero that would read as a real number.  That distinction is the whole reason
 * the fields carry has_* flags.
 *
 * Unlike the pending list, a row here is skipped only when the BOND or the STOCK
 * cannot be identified - both codes and both markets have to be readable - because an
 * event that cannot be attached to a bond and an underlying is not an event.  The
 * reference skips exactly those rows and so does this.
 *
 * A row also carries an event id built the reference's way,
 * "convertible-subscription:<market>:<code>:<subscription date>", so that the same
 * event seen twice can be recognised. */
#ifndef TDX_SUBSCRIPTION_H
#define TDX_SUBSCRIPTION_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bonds.h"
#include "tdx_error.h"
#include "tdx_jsn.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_SUBSCRIPTION_RESOURCE "list/func_kkzss101_1.jsn"
#define TDX_SUBSCRIPTION_ROWS_MAX 4096
#define TDX_SUBSCRIPTION_EVENT_ID_MAX 64

typedef struct tdx_subscription_row {
    /* The bond being offered. */
    int bond_market_id;
    char bond_market[16];
    char bond_security_id[24];
    tdx_bond_text bond_code;
    tdx_bond_text bond_name;

    /* The stock it will convert into. */
    int stock_market_id;
    char stock_market[16];
    char stock_security_id[24];
    tdx_bond_text stock_code;

    tdx_bond_text subscription_date;   /* sgrq */
    tdx_bond_text subscription_code;   /* sgdm */
    int has_subscription_limit_10k_yuan;
    double subscription_limit_10k_yuan; /* sgsx */
    tdx_bond_text conversion_start_date; /* zgr */

    int has_underlying_close_yuan;
    double underlying_close_yuan;      /* zxj */
    int has_conversion_price_yuan;
    double conversion_price_yuan;      /* zgj */
    int has_bond_close_yuan;
    double bond_close_yuan;            /* zxsp */

    /* Derived.  Absent when the inputs are, or when a divisor is zero. */
    int has_conversion_value_yuan;
    double conversion_value_yuan;
    int has_conversion_premium_pct;
    double conversion_premium_pct;

    int has_issue_size_100m_yuan;
    double issue_size_100m_yuan;       /* fxzs */
    tdx_bond_text lottery_date;        /* zqr */
    int has_lottery_rate_pct;
    double lottery_rate_pct;           /* zql */
    tdx_bond_text listing_date;        /* ssrq */
    /* The reference sets this from the listing date being non-empty, which is how the
     * resource distinguishes an offered bond from a listed one. */
    int listed;

    char event_id[TDX_SUBSCRIPTION_EVENT_ID_MAX];
} tdx_subscription_row;

/* Maps one document.  A row whose bond or stock identity cannot be read is skipped
 * and counted. */
int tdx_subscription_normalize(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                               tdx_subscription_row *out, size_t capacity, size_t *out_count,
                               size_t *skipped_count, tdx_error *err);

/* The two derived metrics, exposed so a test can check them against inputs it chooses
 * rather than only against captured ones.  Each returns 0 when the value is not
 * defined, which is how absence is expressed. */
int tdx_subscription_conversion_value(double underlying_close, double conversion_price,
                                      double *out);
int tdx_subscription_premium(double bond_close, double conversion_value, double *out);

#ifdef __cplusplus
}
#endif

#endif /* TDX_SUBSCRIPTION_H */
