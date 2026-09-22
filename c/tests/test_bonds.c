/* test_bonds.c - the bond reference field mapping.
 *
 * The captured fixture covers the default case: bi/list/zq_tx201.jsn is not in the
 * profile table, so its size column is read as yuan and its identity comes from
 * $ZQDM/$SC.  That is one of the four cases and the least interesting one, so the
 * other three - a client master, an exchange projection and a policy-financial
 * issue - are built here as SYNTHETIC documents, labelled as such, because no
 * capture of them is in this repository.
 *
 * The size semantics are the reason this layer exists at all.  Measured live, the
 * same GM column means three different things:
 *
 *   list/zqgz201.jsn     client master      56000000000  -> no conversion
 *   list/zq_gz201_1.jsn  SH projection             260  -> 26,000,000,000 yuan
 *   list/zq_jrz201_1.jsn policy financial          100  -> 10,000,000,000 yuan
 *
 * so reporting one "size" number would be wrong in at least two of the three.  The
 * tests below pin each reading. */
#include <stdio.h>
#include <string.h>

#include "tdx_bonds.h"
#include "tdx_bonds_json.h"
#include "tdx_jsn.h"
#include "jsn_fixtures.h"
#include "render_check.h"

static int failures = 0;

#define CHECK(condition, ...)                                                        \
    do {                                                                             \
        if (!(condition)) {                                                          \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                              \
            printf(__VA_ARGS__);                                                     \
            printf("\n");                                                            \
            failures++;                                                              \
        }                                                                            \
    } while (0)

static tdx_error error;

/* tdx_buf carries a length, not a terminator, so a comparison must copy. */
static const char *text_of(const tdx_buf *buffer) {
    static char scratch[8192];
    size_t copy = buffer->len < sizeof(scratch) - 1 ? buffer->len : sizeof(scratch) - 1;
    if (copy && buffer->data)
        memcpy(scratch, buffer->data, copy);
    scratch[copy] = '\0';
    return scratch;
}

/* A tdx_bond_text as a C string, for comparison. */
static const char *bond_text(const tdx_bond_text *text) {
    static char scratch[512];
    if (!text->present || !text->data)
        return "(absent)";
    if (text->length >= sizeof(scratch))
        return "(too long)";
    memcpy(scratch, text->data, text->length);
    scratch[text->length] = '\0';
    return scratch;
}

/* Parses a literal as a document.
 *
 * The GBK step is deliberately SKIPPED here: only the wire payload is GBK, and
 * these literals are already UTF-8 (they contain Chinese names).  Passing them
 * through the GBK decoder was a mistake this test made and the decoder caught -
 * correctly, since UTF-8 Chinese is not valid GBK. */
static int open_document(const char *json, tdx_jsn_document *doc) {
    error.message[0] = '\0';
    return tdx_jsn_parse((const uint8_t *)json, strlen(json), doc, &error);
}

/* Every synthetic case needs a parsed document before it can index groups[0]. */
#define REQUIRE_DOCUMENT(json, doc, resource)                                        \
    do {                                                                             \
        if (open_document((json), (doc)) != TDX_OK) {                                \
            CHECK(0, "the synthetic document parses: %s", error.message);            \
            tdx_jsn_document_free(doc);                                              \
            return;                                                                  \
        }                                                                            \
    } while (0)

static void test_profiles(void) {
    tdx_bond_profile profile;

    profile = tdx_bonds_profile("list/zqjrz201.jsn");
    CHECK(profile.reference_master == 1 && profile.scale == TDX_BOND_SCALE_ISSUE_100M_YUAN,
          "the policy-financial master is a master read in 1e8 yuan");
    profile = tdx_bonds_profile("list/zqgz201.jsn");
    CHECK(profile.reference_master == 1 && profile.scale == TDX_BOND_SCALE_CLIENT_MASTER_HIDDEN,
          "the treasury master has a hidden unit");
    profile = tdx_bonds_profile("list/zq_gz201_1.jsn");
    CHECK(profile.reference_master == 0 &&
              profile.scale == TDX_BOND_SCALE_OUTSTANDING_100M_YUAN,
          "an exchange projection is an outstanding balance in 1e8 yuan");
    profile = tdx_bonds_profile("list/zq_jrz201_2.jsn");
    CHECK(profile.reference_master == 0 && profile.scale == TDX_BOND_SCALE_ISSUE_100M_YUAN,
          "the policy-financial projection is an issue size in 1e8 yuan");
    profile = tdx_bonds_profile("list/zq_tx201.jsn");
    CHECK(profile.reference_master == 0 && profile.scale == TDX_BOND_SCALE_ISSUE_YUAN,
          "a resource outside the table defaults to yuan");
    profile = tdx_bonds_profile("list/not_a_resource.jsn");
    CHECK(profile.scale == TDX_BOND_SCALE_ISSUE_YUAN, "and so does an unknown one");
    /* The table is keyed on the path under the prefix, so a caller's spelling with
     * the prefix or a leading slash must still match. */
    profile = tdx_bonds_profile("bi/list/zqgz201.jsn");
    CHECK(profile.reference_master == 1, "a prefixed path matches");
    profile = tdx_bonds_profile("/list/zqgz201.jsn");
    CHECK(profile.reference_master == 1, "a leading slash matches");
}

static void test_market_naming(void) {
    char prefix[16];
    int market = -1;

    CHECK(tdx_bonds_market_id("1", 1, &market, &error) == TDX_OK && market == 1,
          "market 1 parses");
    CHECK(strcmp(tdx_bonds_market_name(1), "sh") == 0, "market 1 is sh");
    CHECK(strcmp(tdx_bonds_market_name(0), "sz") == 0, "market 0 is sz");
    CHECK(strcmp(tdx_bonds_market_name(2), "bj") == 0, "market 2 is bj");
    /* 44 is the second Beijing spelling the reference accepts. */
    CHECK(strcmp(tdx_bonds_market_name(44), "bj") == 0, "market 44 is bj too");
    CHECK(tdx_bonds_market_name(7) == NULL, "an unknown market has no name");
    CHECK(tdx_bonds_market_prefix(1, prefix, sizeof(prefix), &error) == TDX_OK &&
              strcmp(prefix, "SH") == 0,
          "market 1 prefixes as SH");
    CHECK(tdx_bonds_market_prefix(44, prefix, sizeof(prefix), &error) == TDX_OK &&
              strcmp(prefix, "BJ") == 0,
          "market 44 prefixes as BJ");
    CHECK(tdx_bonds_market_prefix(7, prefix, sizeof(prefix), &error) == TDX_OK &&
              strcmp(prefix, "M7:") == 0,
          "an unknown market prefixes as M<n>:, got %s", prefix);
    /* Padding is trimmed, and a non-number is refused. */
    CHECK(tdx_bonds_market_id(" 1 ", 3, &market, &error) == TDX_OK && market == 1,
          "padding is trimmed");
    error.message[0] = '\0';
    CHECK(tdx_bonds_market_id("sh", 2, &market, &error) == TDX_ERR,
          "a non-numeric market must be refused");
    CHECK(tdx_bonds_market_id("", 0, &market, &error) == TDX_ERR, "an empty market is refused");
    CHECK(tdx_bonds_market_id("999", 3, &market, &error) == TDX_ERR,
          "a market above 255 is refused");
}

static void test_normalize_captured(void) {
    tdx_buf utf8;
    tdx_jsn_document document;
    tdx_bond_row row;

    error.message[0] = '\0';
    tdx_buf_init(&utf8);
    tdx_jsn_document_init(&document);
    CHECK(tdx_jsn_gbk_to_utf8(jsn_gbk, sizeof(jsn_gbk), &utf8, &error) == TDX_OK, "convert");
    CHECK(tdx_jsn_parse(utf8.data, utf8.len, &document, &error) == TDX_OK, "parse: %s",
          error.message);
    if (document.group_count != 1)
        goto done;

    CHECK(tdx_bonds_normalize(&document, &document.groups[0], 0, "list/zq_tx201.jsn", &row,
                              &error) == TDX_OK,
          "normalize: %s", error.message);
    /* The identity the resource spells as a code plus a market number. */
    CHECK(row.market_id == 1 && strcmp(row.market, "sh") == 0, "the market is sh");
    CHECK(strcmp(row.security_id, "SH020820") == 0, "the identity is SH020820, got %s",
          row.security_id);
    CHECK(strcmp(bond_text(&row.code), "020820") == 0, "the code is 020820, got %s",
          bond_text(&row.code));
    CHECK(strcmp(bond_text(&row.name), "26\xe8\xb4\xb4\xe5\x80\xba" "39") == 0,
          "the name decodes, got %s", bond_text(&row.name));
    CHECK(row.name_resolved == 1, "the name is marked resolved");
    /* A resource that is not a client master has no client instrument id. */
    CHECK(row.client_instrument_id.present == 0, "a non-master has no client instrument id");

    CHECK(strcmp(bond_text(&row.bond_type), "\xe5\x9b\xbd\xe5\x80\xba") == 0,
          "the bond type is the Chinese for treasury, got %s", bond_text(&row.bond_type));
    CHECK(strcmp(bond_text(&row.rate_type), "\xe8\xb4\xb4\xe7\x8e\xb0") == 0,
          "the rate type is the Chinese for discount, got %s", bond_text(&row.rate_type));
    CHECK(strcmp(bond_text(&row.rate_type_flag), "5") == 0, "the rate type flag is 5");
    CHECK(row.bond_credit_rating.present == 0, "this resource has no credit rating");
    CHECK(strcmp(bond_text(&row.accrual_start_date), "20260625") == 0,
          "the accrual start is 20260625, got %s", bond_text(&row.accrual_start_date));
    CHECK(strcmp(bond_text(&row.maturity_date), "20260924") == 0,
          "the maturity is 20260924, got %s", bond_text(&row.maturity_date));
    CHECK(row.has_remaining_years && row.remaining_years > 0.008 &&
              row.remaining_years < 0.009,
          "remaining years is 0.008219, got %f", row.remaining_years);
    CHECK(row.has_face_value_yuan && row.face_value_yuan == 100.0,
          "the face value is 100, got %f", row.face_value_yuan);
    /* This resource has no GM column, so nothing is claimed about its size. */
    CHECK(row.has_source_scale == 0, "this resource carries no size column");
    CHECK(row.has_issue_size_yuan == 0 && row.has_outstanding_balance_yuan == 0,
          "and so neither converted size is filled");
    CHECK(row.has_underlying == 0, "this resource names no underlying");

done:
    tdx_jsn_document_free(&document);
    tdx_buf_free(&utf8);
}

/* SYNTHETIC: the shape a client master has.  The master names itself in
 * $ZQDM1/$SC1 and its underlying in $ZQDM/$SC, and its unit is not recoverable. */
static void test_normalize_master(void) {
    static const char *json =
        "[{\"colheader\":[\"$ZQDM1\",\"$SC1\",\"$ZQDM\",\"$SC\",\"ZQJC\",\"GM\",\"ZQLX\"],"
        "\"data\":[[\"010107\",\"1\",\"600000\",\"1\",\"20国债07\",\"56000000000\","
        "\"国债\"]]}]";
    tdx_jsn_document document;
    tdx_bond_row row;

    tdx_jsn_document_init(&document);
    REQUIRE_DOCUMENT(json, &document, "list/zqgz201.jsn");
    CHECK(tdx_bonds_normalize(&document, &document.groups[0], 0, "list/zqgz201.jsn", &row,
                              &error) == TDX_OK,
          "normalize: %s", error.message);
    CHECK(strcmp(row.security_id, "SH010107") == 0,
          "the master takes its identity from $ZQDM1/$SC1, got %s", row.security_id);
    CHECK(strcmp(bond_text(&row.client_instrument_id), "600000") == 0,
          "and reports $ZQDM as the client instrument id, got %s",
          bond_text(&row.client_instrument_id));
    CHECK(strcmp(bond_text(&row.name), "20\xe5\x9b\xbd\xe5\x80\xba" "07") == 0,
          "the name decodes, got %s", bond_text(&row.name));
    /* THE POINT: the hidden unit is not converted, so neither converted field is
     * filled and the raw value is all that is claimed. */
    CHECK(row.scale == TDX_BOND_SCALE_CLIENT_MASTER_HIDDEN, "the scale is the hidden one");
    CHECK(row.has_source_scale == 1 && row.source_scale_raw == 56000000000.0,
          "the raw size survives, got %f", row.source_scale_raw);
    CHECK(row.has_issue_size_yuan == 0 && row.has_outstanding_balance_yuan == 0,
          "and neither converted size is invented");
    tdx_jsn_document_free(&document);
}

/* SYNTHETIC: an exchange projection, whose size is an OUTSTANDING balance and which
 * names its underlying in $ZQDM1/$SC1. */
static void test_normalize_projection(void) {
    static const char *json =
        "[{\"colheader\":[\"$ZQDM\",\"$SC\",\"ZQJC\",\"GM\",\"$ZQDM1\",\"$SC1\"],"
        "\"data\":[[\"019427\",\"1\",\"25国债15\",\"260\",\"600519\",\"1\"]]}]";
    tdx_jsn_document document;
    tdx_bond_row row;

    tdx_jsn_document_init(&document);
    REQUIRE_DOCUMENT(json, &document, "list/zq_gz201_1.jsn");
    CHECK(tdx_bonds_normalize(&document, &document.groups[0], 0, "list/zq_gz201_1.jsn", &row,
                              &error) == TDX_OK,
          "normalize: %s", error.message);
    CHECK(strcmp(row.security_id, "SH019427") == 0,
          "a projection takes its identity from $ZQDM/$SC, got %s", row.security_id);
    CHECK(row.client_instrument_id.present == 0, "a projection has no client instrument id");
    CHECK(row.scale == TDX_BOND_SCALE_OUTSTANDING_100M_YUAN, "the scale is outstanding 1e8");
    CHECK(row.has_outstanding_balance_source_100m &&
              row.outstanding_balance_source_100m == 260.0,
          "the raw 260 is the 1e8-yuan figure");
    CHECK(row.has_outstanding_balance_yuan && row.outstanding_balance_yuan == 26000000000.0,
          "and it converts to 26bn yuan, got %f", row.outstanding_balance_yuan);
    CHECK(row.has_issue_size_yuan == 0, "an outstanding balance is NOT an issue size");
    CHECK(row.has_underlying == 1, "the underlying is present");
    CHECK(strcmp(row.underlying_security_id, "SH600519") == 0,
          "the underlying is SH600519, got %s", row.underlying_security_id);
    CHECK(strcmp(bond_text(&row.underlying_code), "600519") == 0, "the underlying code");
    tdx_jsn_document_free(&document);
}

/* SYNTHETIC: a policy-financial issue, whose size really is an issue size in 1e8. */
static void test_normalize_policy_financial(void) {
    static const char *json =
        "[{\"colheader\":[\"$ZQDM\",\"$SC\",\"ZQJC\",\"GM\"],"
        "\"data\":[[\"018014\",\"1\",\"25国开14\",\"100\"]]}]";
    tdx_jsn_document document;
    tdx_bond_row row;

    tdx_jsn_document_init(&document);
    REQUIRE_DOCUMENT(json, &document, "list/zq_jrz201_1.jsn");
    CHECK(tdx_bonds_normalize(&document, &document.groups[0], 0, "list/zq_jrz201_1.jsn", &row,
                              &error) == TDX_OK,
          "normalize: %s", error.message);
    CHECK(row.scale == TDX_BOND_SCALE_ISSUE_100M_YUAN, "the scale is issue 1e8");
    CHECK(row.has_issue_size_source_100m && row.issue_size_source_100m == 100.0,
          "the raw 100 is the 1e8 figure");
    CHECK(row.has_issue_size_yuan && row.issue_size_yuan == 10000000000.0,
          "and it converts to 10bn yuan, got %f", row.issue_size_yuan);
    CHECK(row.has_outstanding_balance_yuan == 0, "an issue size is NOT an outstanding balance");
    tdx_jsn_document_free(&document);
}

static void test_coupon_schedule(void) {
    static const char *json =
        "[{\"colheader\":[\"$ZQDM\",\"$SC\",\"FXRQXL\",\"FXLLXL\",\"SYFXRQXL\",\"SYFXLLXL\"],"
        "\"data\":[[\"010107\",\"1\",\"20250101,20260101,20270101\",\"0.035,0.04,3.5\","
        "\"20260101,20270101\",\"0.04,3.5\"]]}]";
    tdx_jsn_document document;
    tdx_bond_coupon entries[8];
    size_t count = 0;
    size_t list_length = 0;

    tdx_jsn_document_init(&document);
    REQUIRE_DOCUMENT(json, &document, "schedule");
    CHECK(tdx_bonds_coupon_schedule(&document, &document.groups[0], 0, "FXRQXL", "FXLLXL",
                                    entries, 8, &count, &list_length, &error) == TDX_OK,
          "schedule: %s", error.message);
    CHECK(count == 3 && list_length == 3, "three coupons, got %zu", count);
    if (count == 3) {
        CHECK(strcmp(bond_text(&entries[0].date), "20250101") == 0, "the first date");
        /* THE RULE: a magnitude of at most one is a fraction, so 0.035 is 3.5 per
         * cent; 3.5 is already a percent and stays. */
        CHECK(entries[0].has_rate && entries[0].rate_pct > 3.49 && entries[0].rate_pct < 3.51,
              "0.035 reads as 3.5 per cent, got %f", entries[0].rate_pct);
        CHECK(entries[1].has_rate && entries[1].rate_pct > 3.99 && entries[1].rate_pct < 4.01,
              "0.04 reads as 4 per cent, got %f", entries[1].rate_pct);
        CHECK(entries[2].has_rate && entries[2].rate_pct > 3.49 && entries[2].rate_pct < 3.51,
              "3.5 stays 3.5 per cent, got %f", entries[2].rate_pct);
    }

    /* Fewer rates than dates leaves the extra dates without a rate. */
    {
        static const char *shorter =
            "[{\"colheader\":[\"FXRQXL\",\"FXLLXL\"],\"data\":[[\"20250101,20260101\","
            "\"0.035\"]]}]";
        tdx_jsn_document other;
        tdx_jsn_document_init(&other);
        if (open_document(shorter, &other) != TDX_OK) {
            CHECK(0, "the shorter list parses: %s", error.message);
            tdx_jsn_document_free(&other);
            goto after_short;
        }
        CHECK(tdx_bonds_coupon_schedule(&other, &other.groups[0], 0, "FXRQXL", "FXLLXL", entries,
                                        8, &count, &list_length, &error) == TDX_OK,
              "schedule: %s", error.message);
        CHECK(count == 2, "two dates are kept, got %zu", count);
        if (count == 2) {
            CHECK(entries[0].has_rate == 1, "the first has a rate");
            CHECK(entries[1].has_rate == 0, "the second has none rather than a wrong one");
        }
        tdx_jsn_document_free(&other);
    }
after_short:

    /* An empty list is empty, not an error. */
    {
        static const char *empty =
            "[{\"colheader\":[\"$ZQDM\"],\"data\":[[\"010107\"]]}]";
        tdx_jsn_document other;
        tdx_jsn_document_init(&other);
        if (open_document(empty, &other) != TDX_OK) {
            CHECK(0, "the empty case parses: %s", error.message);
            tdx_jsn_document_free(&other);
            goto after_empty;
        }
        CHECK(tdx_bonds_coupon_schedule(&other, &other.groups[0], 0, "FXRQXL", "FXLLXL", entries,
                                        8, &count, &list_length, &error) == TDX_OK,
              "an absent schedule is not an error: %s", error.message);
        CHECK(count == 0 && list_length == 0, "and holds nothing");
        tdx_jsn_document_free(&other);
    }
after_empty:

    /* A schedule longer than the caller's buffer is REFUSED, because truncating it
     * would silently drop coupons. */
    CHECK(tdx_bonds_coupon_schedule(&document, &document.groups[0], 0, "FXRQXL", "FXLLXL",
                                    entries, 2, &count, &list_length, &error) == TDX_ERR,
          "a truncated schedule must be refused");
    CHECK(strstr(error.message, "truncated") != NULL, "the error says why: %s", error.message);
    tdx_jsn_document_free(&document);
}

static void test_identity_rejects(void) {
    tdx_jsn_document document;
    tdx_bond_row row;

    /* A row with no code column value cannot be attributed to a security. */
    {
        static const char *json =
            "[{\"colheader\":[\"$ZQDM\",\"$SC\",\"ZQJC\"],\"data\":[[\"\",\"1\",\"x\"]]}]";
        tdx_jsn_document_init(&document);
        if (open_document(json, &document) != TDX_OK) {
            CHECK(0, "parses: %s", error.message);
            tdx_jsn_document_free(&document);
            return;
        }
        error.message[0] = '\0';
        CHECK(tdx_bonds_normalize(&document, &document.groups[0], 0, "list/x.jsn", &row,
                                  &error) == TDX_ERR,
              "a row without a code must be refused");
        CHECK(strstr(error.message, "$ZQDM") != NULL, "the error names the column: %s",
              error.message);
        tdx_jsn_document_free(&document);
    }
    /* A market that is not a number. */
    {
        static const char *json =
            "[{\"colheader\":[\"$ZQDM\",\"$SC\"],\"data\":[[\"010107\",\"sh\"]]}]";
        tdx_jsn_document_init(&document);
        if (open_document(json, &document) != TDX_OK) {
            CHECK(0, "parses: %s", error.message);
            tdx_jsn_document_free(&document);
            return;
        }
        error.message[0] = '\0';
        CHECK(tdx_bonds_normalize(&document, &document.groups[0], 0, "list/x.jsn", &row,
                                  &error) == TDX_ERR,
              "a non-numeric market must be refused");
        CHECK(strstr(error.message, "market") != NULL, "the error says why: %s", error.message);
        tdx_jsn_document_free(&document);
    }
}

static void test_rendering(void) {
    tdx_buf utf8;
    tdx_buf line;
    tdx_jsn_document document;
    tdx_bond_row row;

    error.message[0] = '\0';
    tdx_buf_init(&utf8);
    tdx_buf_init(&line);
    tdx_jsn_document_init(&document);
    CHECK(tdx_jsn_gbk_to_utf8(jsn_gbk, sizeof(jsn_gbk), &utf8, &error) == TDX_OK, "convert");
    CHECK(tdx_jsn_parse(utf8.data, utf8.len, &document, &error) == TDX_OK, "parse");
    CHECK(tdx_bonds_normalize(&document, &document.groups[0], 0, "list/zq_tx201.jsn", &row,
                              &error) == TDX_OK,
          "normalize");
    CHECK(tdx_bonds_format(&line, &row, "list/zq_tx201.jsn", 0, 0, &error) == TDX_OK,
          "render: %s", error.message);
    {
        const char *text = text_of(&line);
        int depth = 0;
        int in_string = 0;
        int escaped = 0;
        const char *cursor;
        for (cursor = text; *cursor; ++cursor) {
            if (in_string) {
                if (escaped)
                    escaped = 0;
                else if (*cursor == '\\')
                    escaped = 1;
                else if (*cursor == '"')
                    in_string = 0;
                continue;
            }
            if (*cursor == '"')
                in_string = 1;
            else if (*cursor == '{')
                depth++;
            else if (*cursor == '}') {
                depth--;
                CHECK(depth >= 0, "the object must not close early");
            }
        }
        CHECK(depth == 0 && !in_string, "the rendered row is balanced, depth %d", depth);
    {
        /* Balanced is not the same as parseable: this assertion is what caught a Windows
         * path reaching the JSON through a raw %s, and a CRC printed without quotes. */
        char reason[192];
        CHECK(render_parses(text, reason, sizeof(reason)),
              "and it parses as JSON: %s\n    %s", reason, text);
    }
        CHECK(strstr(text, "\"security_id\":\"SH020820\"") != NULL, "the identity renders");
        CHECK(strstr(text, "\"scale\":\"issue-size-yuan\"") != NULL, "the scale renders");
        CHECK(strstr(text, "\"face_value_yuan\":100.000000") != NULL, "the face value renders");
        CHECK(strstr(text, "\"underlying\":null") != NULL, "an absent underlying renders null");
    }

    tdx_buf_clear(&line);
    CHECK(tdx_bonds_format_summary(&line, 42, 42, 0, 0, "list/zq_tx201.jsn", "issue-size-yuan",
                                   "1.2.3.4:7709", &error) == TDX_OK,
          "summary: %s", error.message);
    CHECK(strstr(text_of(&line), "\"type\":\"bond_summary\"") != NULL, "the summary type");
    CHECK(strstr(text_of(&line), "\"rows\":42") != NULL, "the row count");
    CHECK(strstr(text_of(&line), "\"endpoint\":\"1.2.3.4:7709\"") != NULL, "the endpoint");

    tdx_buf_clear(&line);
    CHECK(tdx_bonds_format_summary(&line, 0, 0, 0, 0, "x", NULL, NULL, &error) == TDX_OK,
          "an empty summary renders");
    CHECK(strstr(text_of(&line), "\"endpoint\":null") != NULL, "a missing endpoint is null");
    CHECK(strstr(text_of(&line), "\"scale\":\"\"") != NULL, "a missing scale is empty");

    tdx_jsn_document_free(&document);
    tdx_buf_free(&utf8);
    tdx_buf_free(&line);
}

int main(void) {
    test_profiles();
    test_market_naming();
    test_normalize_captured();
    test_normalize_master();
    test_normalize_projection();
    test_normalize_policy_financial();
    test_coupon_schedule();
    test_identity_rejects();
    test_rendering();

    if (failures) {
        printf("%d bond check(s) failed\n", failures);
        return 1;
    }
    printf("bond checks passed\n");
    return 0;
}
