/* test_convertible.c - the convertible-bond overview mapping.
 *
 * The fixture is the real overview resource, reduced: bi/list/kzz_kzzsy200_1.jsn
 * is 200,541 bytes and 314 rows, so the generator keeps the verbatim colheader and
 * the first three rows.  The values asserted below are those three rows.
 *
 * THIS LAYER IS THE OVERVIEW ONLY.  The reference builds its convertible-bond view
 * by joining six documents plus up to two exchangeable-bond projections and then
 * attaching three trigger documents; none of that is here, and the summary says so
 * in a `join_note` field rather than leaving a caller to discover it.
 *
 * One finding is recorded rather than reproduced: the reference binds the ZGDM
 * column to the underlying's NAME, and measured across all 314 live rows it is
 * never a name - 217 are six-digit codes and 97 are empty.  This module keeps the
 * column under `reference_code`, which is what it holds, and asserts that property
 * on the captured rows. */
#include <stdio.h>
#include <string.h>

#include "tdx_convertible.h"
#include "tdx_convertible_json.h"
#include "tdx_jsn.h"
#include "convertible_fixtures.h"
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

static const char *text_of(const tdx_buf *buffer) {
    static char scratch[65536];
    size_t copy = buffer->len < sizeof(scratch) - 1 ? buffer->len : sizeof(scratch) - 1;
    if (copy && buffer->data)
        memcpy(scratch, buffer->data, copy);
    scratch[copy] = '\0';
    return scratch;
}

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

static void test_kind(void) {
    /* The reference decides on the code alone. */
    CHECK(tdx_convertible_kind_of("132001", 6) == TDX_CONVERTIBLE_EXCHANGEABLE,
          "132 is an exchangeable bond");
    CHECK(tdx_convertible_kind_of("110075", 6) == TDX_CONVERTIBLE_BOND,
          "110 is a convertible bond");
    CHECK(tdx_convertible_kind_of("113001", 6) == TDX_CONVERTIBLE_BOND, "113 is convertible");
    CHECK(tdx_convertible_kind_of("132", 3) == TDX_CONVERTIBLE_EXCHANGEABLE, "a bare 132");
    CHECK(tdx_convertible_kind_of("13", 2) == TDX_CONVERTIBLE_BOND, "a truncated code is not");
    CHECK(tdx_convertible_kind_of(NULL, 0) == TDX_CONVERTIBLE_BOND, "no code is not");
}

static void test_captured(void) {
    tdx_jsn_document document = {0};
    tdx_convertible_row rows[CONVERTIBLE_FIXTURE_ROWS];

    error.message[0] = '\0';
    tdx_jsn_document_init(&document);
    /* The fixture is already UTF-8, because the parser sees what the GBK
     * conversion produced; the conversion itself is covered by test_jsn. */
    CHECK(tdx_jsn_parse((const uint8_t *)convertible_overview, strlen(convertible_overview),
                        &document, &error) == TDX_OK,
          "the fixture parses: %s", error.message);
    CHECK(document.group_count == 1, "one group");
    CHECK(document.row_count == CONVERTIBLE_FIXTURE_ROWS, "%d rows, got %zu",
          CONVERTIBLE_FIXTURE_ROWS, document.row_count);
    if (document.group_count != 1 || document.row_count != CONVERTIBLE_FIXTURE_ROWS)
        goto done;
    CHECK(document.groups[0].column_count == CONVERTIBLE_OVERVIEW_COLUMNS, "%d columns, got %zu",
          CONVERTIBLE_OVERVIEW_COLUMNS, document.groups[0].column_count);
    CHECK(CONVERTIBLE_OVERVIEW_SOURCE_ROWS == 314,
          "the capture this reduction came from held 314 rows, header says %d",
          CONVERTIBLE_OVERVIEW_SOURCE_ROWS);

    {
        size_t index;
        for (index = 0; index < CONVERTIBLE_FIXTURE_ROWS; ++index)
            CHECK(tdx_convertible_normalize(&document, &document.groups[0], index, &rows[index],
                                            &error) == TDX_OK,
                  "normalize row %zu: %s", index, error.message);
    }

    /* Row 0 is China Southern Airlines' convertible bond, 110075. */
    CHECK(strcmp(rows[0].bond_security_id, "SH110075") == 0, "the bond is SH110075, got %s",
          rows[0].bond_security_id);
    CHECK(strcmp(bond_text(&rows[0].bond_name), "\xe5\x8d\x97\xe8\x88\xaa\xe8\xbd\xac\xe5\x80\xba")
              == 0,
          "the name decodes to the Chinese for Southern Airlines convertible bond, got %s",
          bond_text(&rows[0].bond_name));
    CHECK(rows[0].kind == TDX_CONVERTIBLE_BOND, "110075 is a convertible bond");
    CHECK(rows[0].has_underlying == 1, "it names an underlying");
    CHECK(strcmp(rows[0].underlying_security_id, "SH600029") == 0,
          "the underlying is SH600029, got %s", rows[0].underlying_security_id);
    CHECK(strcmp(bond_text(&rows[0].underlying_code), "600029") == 0, "the underlying code");

    /* The overview terms. */
    CHECK(strcmp(bond_text(&rows[0].bond_rating), "AAA") == 0, "the bond rating is AAA, got %s",
          bond_text(&rows[0].bond_rating));
    CHECK(strcmp(bond_text(&rows[0].issuer_rating), "AAA") == 0, "the issuer rating is AAA");
    CHECK(strcmp(bond_text(&rows[0].listing_date), "20201103") == 0,
          "the listing date is 20201103, got %s", bond_text(&rows[0].listing_date));
    CHECK(strcmp(bond_text(&rows[0].conversion_start_date), "20210421") == 0,
          "the conversion window opens 20210421, got %s",
          bond_text(&rows[0].conversion_start_date));
    CHECK(strcmp(bond_text(&rows[0].maturity_date), "20261015") == 0,
          "it matures 20261015, got %s", bond_text(&rows[0].maturity_date));
    CHECK(rows[0].has_face_value && rows[0].face_value == 100.0, "the face value is 100");
    CHECK(rows[0].has_conversion_price && rows[0].conversion_price > 6.16 &&
              rows[0].conversion_price < 6.18,
          "the conversion price is 6.17, got %f", rows[0].conversion_price);
    CHECK(rows[0].has_issue_size_100m_yuan && rows[0].issue_size_100m_yuan == 160.0,
          "the issue size is 16bn yuan, got %f", rows[0].issue_size_100m_yuan);
    CHECK(rows[0].has_remaining_balance_100m_yuan &&
              rows[0].remaining_balance_100m_yuan > 58.9 &&
              rows[0].remaining_balance_100m_yuan < 59.0,
          "58.96bn yuan remains, got %f", rows[0].remaining_balance_100m_yuan);
    CHECK(rows[0].has_sellback_trigger_ratio_pct && rows[0].sellback_trigger_ratio_pct == 70.0,
          "the sellback trigger is 70 per cent, got %f", rows[0].sellback_trigger_ratio_pct);
    CHECK(rows[0].has_redemption_trigger_ratio_pct &&
              rows[0].redemption_trigger_ratio_pct == 130.0,
          "the redemption trigger is 130 per cent, got %f",
          rows[0].redemption_trigger_ratio_pct);
    /* The column the reference reads for this is not in the overview resource, so
     * the value is absent rather than zero. */
    CHECK(rows[0].has_unpaid_coupon_sum == 0,
          "the unpaid coupon sum is not in the overview, so it is absent, not zero");
    CHECK(rows[0].core_terms_complete == 1, "the core terms are complete");

    /* ZGDM: the reference calls it the underlying's name; live it is a code or
     * empty.  Asserted on the captured rows so the divergence stays visible. */
    {
        size_t index;
        for (index = 0; index < CONVERTIBLE_FIXTURE_ROWS; ++index) {
            const tdx_bond_text *reference = &rows[index].underlying_reference_code;
            if (!reference->present)
                continue;
            if (reference->length == 6) {
                size_t digit;
                int all_digits = 1;
                for (digit = 0; digit < 6; ++digit)
                    if (reference->data[digit] < '0' || reference->data[digit] > '9')
                        all_digits = 0;
                CHECK(all_digits, "row %zu: ZGDM is a six-digit code, got %s", index,
                      bond_text(reference));
            } else {
                CHECK(0, "row %zu: ZGDM is neither empty nor a six-digit code: %s", index,
                      bond_text(reference));
            }
        }
    }

done:
    tdx_jsn_document_free(&document);
}

static void test_identity_rejects(void) {
    tdx_jsn_document document = {0};
    tdx_convertible_row row;

    /* No bond code: the row cannot be attributed to a security. */
    {
        static const char *json =
            "[{\"colheader\":[\"$ZQDM\",\"$SC\",\"ZQJC\"],\"data\":[[\"\",\"1\",\"x\"]]}]";
        error.message[0] = '\0';
        tdx_jsn_document_init(&document);
        if (tdx_jsn_parse((const uint8_t *)json, strlen(json), &document, &error) != TDX_OK) {
            CHECK(0, "parses: %s", error.message);
            tdx_jsn_document_free(&document);
            return;
        }
        CHECK(tdx_convertible_normalize(&document, &document.groups[0], 0, &row, &error) ==
                  TDX_ERR,
              "a row without a bond code must be refused");
        CHECK(strstr(error.message, "$ZQDM") != NULL, "the error names the column: %s",
              error.message);
        tdx_jsn_document_free(&document);
    }
    /* A market that is not a number. */
    {
        static const char *json =
            "[{\"colheader\":[\"$ZQDM\",\"$SC\"],\"data\":[[\"110075\",\"sh\"]]}]";
        error.message[0] = '\0';
        tdx_jsn_document_init(&document);
        if (tdx_jsn_parse((const uint8_t *)json, strlen(json), &document, &error) != TDX_OK) {
            CHECK(0, "parses: %s", error.message);
            tdx_jsn_document_free(&document);
            return;
        }
        CHECK(tdx_convertible_normalize(&document, &document.groups[0], 0, &row, &error) ==
                  TDX_ERR,
              "a non-numeric market must be refused");
        tdx_jsn_document_free(&document);
    }
    /* An unusable underlying is an ABSENT underlying, not an error: the bond is
     * still a bond. */
    {
        static const char *json =
            "[{\"colheader\":[\"$ZQDM\",\"$SC\",\"$ZQDM1\",\"$SC1\"],"
            "\"data\":[[\"110075\",\"1\",\"60002\",\"1\"]]}]";
        error.message[0] = '\0';
        tdx_jsn_document_init(&document);
        if (tdx_jsn_parse((const uint8_t *)json, strlen(json), &document, &error) != TDX_OK) {
            CHECK(0, "parses: %s", error.message);
            tdx_jsn_document_free(&document);
            return;
        }
        CHECK(tdx_convertible_normalize(&document, &document.groups[0], 0, &row, &error) == TDX_OK,
              "a short underlying code is tolerated: %s", error.message);
        /* A five-digit underlying code is not a security. */
        CHECK(row.has_underlying == 0, "and leaves the underlying absent");
        tdx_jsn_document_free(&document);
    }
}

static void test_rendering(void) {
    tdx_jsn_document document = {0};
    tdx_convertible_row row;
    tdx_buf line = {0};

    error.message[0] = '\0';
    tdx_jsn_document_init(&document);
    tdx_buf_init(&line);
    if (tdx_jsn_parse((const uint8_t *)convertible_overview, strlen(convertible_overview),
                      &document, &error) != TDX_OK ||
        tdx_convertible_normalize(&document, &document.groups[0], 0, &row, &error) != TDX_OK) {
        CHECK(0, "prepare: %s", error.message);
        goto done;
    }
    CHECK(tdx_convertible_format(&line, &row, "list/kzz_kzzsy201_1.jsn", 0, 0, &error) == TDX_OK,
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
        /* This check is why the missing overview brace was found: the renderer had
         * opened the overview object and never closed it. */
        CHECK(depth == 0 && !in_string, "the rendered row is balanced, depth %d in_string %d",
              depth, in_string);
    {
        /* Balanced is not the same as parseable: this assertion is what caught a Windows
         * path reaching the JSON through a raw %s, and a CRC printed without quotes. */
        char reason[192];
        CHECK(render_parses(text, reason, sizeof(reason)),
              "and it parses as JSON: %s\n    %s", reason, text);
    }
        CHECK(strstr(text, "\"security_id\":\"SH110075\"") != NULL, "the bond identity renders");
        CHECK(strstr(text, "\"instrument_type\":\"convertible-bond\"") != NULL,
              "the instrument type renders");
        CHECK(strstr(text, "\"underlying\":{\"market\":\"sh\"") != NULL,
              "the underlying renders");
        CHECK(strstr(text, "\"reference_code\":\"190075\"") != NULL,
              "ZGDM renders under what it holds");
        CHECK(strstr(text, "\"core_terms_complete\":true") != NULL, "and the completeness flag");
    }

    tdx_buf_clear(&line);
    CHECK(tdx_convertible_format_summary(&line, 314, 314, 0, 314, "list/kzz_kzzsy201_1.jsn",
                                         "1.2.3.4:7709", &error) == TDX_OK,
          "summary: %s", error.message);
    CHECK(strstr(text_of(&line), "\"type\":\"convertible_bond_summary\"") != NULL,
          "the summary type");
    CHECK(strstr(text_of(&line), "\"rows_core_terms_complete\":314") != NULL,
          "the completeness count");
    /* The summary states plainly that this is not the join. */
    CHECK(strstr(text_of(&line), "\"documents_joined\":1") != NULL, "one document was joined");
    CHECK(strstr(text_of(&line), "overview only") != NULL, "and the note says so");

    tdx_buf_clear(&line);
    CHECK(tdx_convertible_format_summary(&line, 0, 0, 0, 0, "x", NULL, &error) == TDX_OK,
          "an empty summary renders");
    CHECK(strstr(text_of(&line), "\"endpoint\":null") != NULL, "a missing endpoint is null");

done:
    tdx_jsn_document_free(&document);
    tdx_buf_free(&line);
}

int main(void) {
    test_kind();
    test_captured();
    test_identity_rejects();
    test_rendering();

    if (failures) {
        printf("%d convertible check(s) failed\n", failures);
        return 1;
    }
    printf("convertible checks passed\n");
    return 0;
}
