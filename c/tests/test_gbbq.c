/* test_gbbq.c - the local encrypted GBBQ rights-and-dividend file.
 *
 * The fixture is five REAL encrypted records sliced out of the live
 * C:\new_tdx\T0002\hq_cache\gbbq file, behind a synthetic header.  So this test
 * drives the actual cipher on actual ciphertext, with nothing invented.
 *
 * What pins the port is a cross-check that no single source could give: the live
 * 0x000F reply and this local file describe the same corporate actions, and when
 * both were run over sz000001, sh600000 and sz000002 every one of the 281 records
 * matched - same date, same category, same four float values, same four share
 * values.  The records asserted here are the ones that carry the reference's own
 * live-verified facts, so the expectations come from outside this module.
 *
 * A wrong cipher would produce garbage identities and fail validation; a wrong
 * table offset would produce plausible-looking nonsense, which is why the values
 * are asserted and not merely the record count. */
#include <stdio.h>
#include <string.h>

#include "tdx_gbbq.h"
#include "gbbq_fixtures.h"

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

#define WANT_SZ000001()                                                              \
    do {                                                                             \
        memset(&wanted, 0, sizeof(wanted));                                          \
        wanted.market_id = 0;                                                        \
        memcpy(wanted.code, "000001", 6);                                            \
        wanted.code[6] = '\0';                                                       \
    } while (0)

static void test_cipher_state(void) {
    uint8_t state[TDX_GBBQ_CIPHER_STATE_BYTES];
    size_t size = 0;
    tdx_error error;
    error.message[0] = '\0';

    CHECK(tdx_gbbq_decode_cipher_state(state, sizeof(state), &size, &error) == TDX_OK,
          "the embedded state decodes: %s", error.message);
    CHECK(size == TDX_GBBQ_CIPHER_STATE_BYTES, "the state is %d bytes, got %zu",
          TDX_GBBQ_CIPHER_STATE_BYTES, size);

    /* The state is only usable if the four tables tile it exactly, which is what
     * the reference's fixed offsets assume: 0x48 + 256 words ends at 0x448, and the
     * last table ends at 0x1048, which is the blob length. */
    CHECK(TDX_GBBQ_CIPHER_STATE_BYTES == 0x1048, "the state is 0x1048 bytes");
    CHECK(0x48 + 256 * 4 == 0x448, "the first table ends where the second begins");
    CHECK(0x448 + 256 * 4 == 0x848, "the second table ends where the third begins");
    CHECK(0x848 + 256 * 4 == 0xC48, "the third table ends where the fourth begins");
    CHECK(0xC48 + 256 * 4 == 0x1048, "the fourth table ends at the end of the state");

    /* A buffer too small must be refused rather than written past. */
    CHECK(tdx_gbbq_decode_cipher_state(state, 16, &size, &error) == TDX_ERR,
          "a short output must be refused");
    CHECK(tdx_gbbq_decode_cipher_state(NULL, sizeof(state), &size, &error) == TDX_ERR,
          "a null output must be refused");
}

static void test_decrypt_captured(void) {
    uint8_t state[TDX_GBBQ_CIPHER_STATE_BYTES];
    uint8_t clear[TDX_GBBQ_RECORD_SIZE];
    size_t size = 0;
    tdx_error error;
    error.message[0] = '\0';

    CHECK(tdx_gbbq_decode_cipher_state(state, sizeof(state), &size, &error) == TDX_OK,
          "state: %s", error.message);

    /* Record 0 is a rights issue from 1990-03-01: a 10-for-1 rights at 3.56. */
    CHECK(tdx_gbbq_decrypt_record(state, gbbq_file + 4, TDX_GBBQ_RECORD_SIZE, clear, &error) ==
              TDX_OK,
          "decrypt: %s", error.message);
    CHECK(clear[0] == 0 && memcmp(clear + 1, "000001", 6) == 0,
          "the first record decrypts to SZ000001");

    /* The clear tail must be copied through untouched. */
    CHECK(memcmp(clear + TDX_GBBQ_ENCRYPTED_SIZE, gbbq_file + 4 + TDX_GBBQ_ENCRYPTED_SIZE,
                 TDX_GBBQ_RECORD_SIZE - TDX_GBBQ_ENCRYPTED_SIZE) == 0,
          "the 5-byte clear tail passes through");

    CHECK(tdx_gbbq_decrypt_record(state, gbbq_file + 4, 28, clear, &error) == TDX_ERR,
          "a 28-byte record must be refused");
    CHECK(tdx_gbbq_decrypt_record(NULL, gbbq_file + 4, TDX_GBBQ_RECORD_SIZE, clear, &error) ==
              TDX_ERR,
          "a null state must be refused");
    CHECK(tdx_gbbq_decrypt_record(state, NULL, TDX_GBBQ_RECORD_SIZE, clear, &error) == TDX_ERR,
          "a null input must be refused");
}

static void test_parse_captured(void) {
    static tdx_capital_record records[GBBQ_FIXTURE_COUNT + 4];
    size_t count = 0;
    size_t source_count = 0;
    tdx_error error;
    tdx_code wanted;
    error.message[0] = '\0';
    WANT_SZ000001();

    CHECK(tdx_gbbq_parse(gbbq_file, sizeof(gbbq_file), &wanted, records,
                         GBBQ_FIXTURE_COUNT + 4, &count, &source_count, &error) == TDX_OK,
          "parse: %s", error.message);
    CHECK(source_count == GBBQ_FIXTURE_COUNT, "the header declares %d records, got %zu",
          GBBQ_FIXTURE_COUNT, source_count);
    CHECK(count == GBBQ_FIXTURE_COUNT, "all %d belong to SZ000001, got %zu",
          GBBQ_FIXTURE_COUNT, count);
    if (count != GBBQ_FIXTURE_COUNT)
        return;

    /* Record 0: 1990-03-01, a rights issue at 3.56 per share, 10 for 1. */
    CHECK(records[0].date == 19900301, "record 0 date is 1990-03-01, got %d", records[0].date);
    CHECK(records[0].category == 1, "record 0 is an ex-rights event, got %u",
          (unsigned)records[0].category);
    CHECK(records[0].float_values[1] > 3.55 && records[0].float_values[1] < 3.57,
          "the rights price is 3.56, got %.4f", records[0].float_values[1]);
    CHECK(records[0].float_values[3] > 0.99 && records[0].float_values[3] < 1.01,
          "the rights ratio is 10 for 1, got %.4f", records[0].float_values[3]);

    /* Record 1: the share capital on the listing date. */
    CHECK(records[1].date == 19910403, "record 1 date is the listing date, got %d",
          records[1].date);
    CHECK(records[1].category == 5, "record 1 is a share-capital change");
    CHECK(records[1].share_values[2] > 26499999.0 && records[1].share_values[2] < 26500001.0,
          "it lists 26,500,000 circulating shares, got %.0f", records[1].share_values[2]);
    CHECK(records[1].share_values[3] > 48500170.0 && records[1].share_values[3] < 48500172.0,
          "and 48,500,171 total shares, got %.0f", records[1].share_values[3]);

    /* Record 2 and 3 are the same day: a dividend and the shares it produced. */
    CHECK(records[2].date == 19910502 && records[2].category == 1,
          "record 2 is the 1991-05-02 dividend");
    CHECK(records[2].float_values[0] > 2.99 && records[2].float_values[0] < 3.01,
          "10 for 3 dividend, got %.4f", records[2].float_values[0]);
    CHECK(records[2].float_values[2] > 3.99 && records[2].float_values[2] < 4.01,
          "10 for 4 bonus, got %.4f", records[2].float_values[2]);

    /* THE CHAIN: what record 1 says the company had after its capital change is
     * what record 3 says it had before the next one.  This is the check that would
     * fail on a wrong table offset, because a wrong offset gives plausible numbers
     * that do not chain. */
    CHECK(records[3].date == 19910502 && records[3].category == 2,
          "record 3 is the listed bonus shares");
    CHECK(records[3].share_values[0] == records[1].share_values[2],
          "the circulating chain holds: %.0f then %.0f", records[1].share_values[2],
          records[3].share_values[0]);
    CHECK(records[3].share_values[1] == records[1].share_values[3],
          "the total chain holds: %.0f then %.0f", records[1].share_values[3],
          records[3].share_values[1]);

    /* Record 4 is the fact the C++ reference recorded from live data: Ping An Bank
     * paid 10 for 7.19 in 2024, and the network command returned the same value for
     * this date.  Two independent sources agreeing on one number is the strongest
     * single assertion in this file. */
    CHECK(records[4].date == 20240614, "record 4 is the 2024 dividend, got %d",
          records[4].date);
    CHECK(records[4].category == 1, "record 4 is an ex-dividend event");
    CHECK(records[4].float_values[0] > 7.18 && records[4].float_values[0] < 7.20,
          "10 for 7.19, got %.4f", records[4].float_values[0]);
    CHECK(records[4].share_values[0] > 71899.0 && records[4].share_values[0] < 71901.0,
          "and the wire view agrees at 71900, got %.0f", records[4].share_values[0]);

    /* Every record must name the security that was asked for. */
    {
        size_t index;
        for (index = 0; index < count; ++index)
            CHECK(records[index].security.market_id == 0 &&
                      strcmp(records[index].security.code, "000001") == 0,
                  "record %zu names SZ000001", index);
    }
}

static void test_parse_rejects(void) {
    static tdx_capital_record records[GBBQ_FIXTURE_COUNT + 4];
    size_t count = 0;
    size_t source_count = 0;
    tdx_error error;
    tdx_code wanted;
    tdx_code other;
    uint8_t damaged[sizeof(gbbq_file)];
    error.message[0] = '\0';
    WANT_SZ000001();

    CHECK(tdx_gbbq_parse(gbbq_file, 3, &wanted, records, GBBQ_FIXTURE_COUNT + 4, &count,
                         &source_count, &error) == TDX_ERR,
          "a file shorter than its header must be refused");
    CHECK(tdx_gbbq_parse(gbbq_file, sizeof(gbbq_file) - 1, &wanted, records,
                         GBBQ_FIXTURE_COUNT + 4, &count, &source_count, &error) == TDX_ERR,
          "a file one byte short must be refused");
    CHECK(strstr(error.message, "length mismatch") != NULL, "the error must say why: %s",
          error.message);
    CHECK(tdx_gbbq_parse(gbbq_file, sizeof(gbbq_file), &wanted, records, 2, &count,
                         &source_count, &error) == TDX_ERR,
          "a file with more records than the output holds must be refused");
    CHECK(tdx_gbbq_parse(gbbq_file, sizeof(gbbq_file), NULL, records, GBBQ_FIXTURE_COUNT + 4,
                         &count, &source_count, &error) == TDX_ERR,
          "a missing security must be refused");

    /* A header claiming more records than the safety limit. */
    memcpy(damaged, gbbq_file, sizeof(gbbq_file));
    damaged[0] = 0xFF;
    damaged[1] = 0xFF;
    damaged[2] = 0xFF;
    damaged[3] = 0x7F;
    error.message[0] = '\0';
    CHECK(tdx_gbbq_parse(damaged, sizeof(damaged), &wanted, records, GBBQ_FIXTURE_COUNT + 4,
                         &count, &source_count, &error) == TDX_ERR,
          "an absurd record count must be refused");
    CHECK(strstr(error.message, "safety limit") != NULL, "the error must say why: %s",
          error.message);

    /* A security the file does not hold: valid parse, zero records. */
    memset(&other, 0, sizeof(other));
    other.market_id = 1;
    memcpy(other.code, "999999", 6);
    other.code[6] = '\0';
    CHECK(tdx_gbbq_parse(gbbq_file, sizeof(gbbq_file), &other, records,
                         GBBQ_FIXTURE_COUNT + 4, &count, &source_count, &error) == TDX_OK,
          "an absent security parses to nothing: %s", error.message);
    CHECK(count == 0, "an absent security yields zero records, got %zu", count);
    CHECK(source_count == GBBQ_FIXTURE_COUNT, "the source count still reports the file");

    /* Corrupting ciphertext must never be invisible.
     *
     * This container has NO integrity check - no MAC, no digest, just a length and
     * a per-record shape test - so an edit can in principle decrypt to a different
     * record that still passes validation.  Asserting "corruption is always
     * rejected" would therefore be asserting something the format does not
     * promise.  What IS promised, and what this checks, is that corruption is never
     * silent: either validation refuses the file, or the decoded values change. */
    {
        static tdx_capital_record intact[GBBQ_FIXTURE_COUNT + 4];
        size_t intact_count = 0;
        size_t offset;
        size_t refused = 0;
        size_t visible = 0;

        CHECK(tdx_gbbq_parse(gbbq_file, sizeof(gbbq_file), &wanted, intact,
                             GBBQ_FIXTURE_COUNT + 4, &intact_count, &source_count,
                             &error) == TDX_OK,
              "the intact file parses first: %s", error.message);

        for (offset = 4; offset < 4 + TDX_GBBQ_ENCRYPTED_SIZE; ++offset) {
            size_t damaged_count = 0;
            size_t record;
            int differs = 0;
            memcpy(damaged, gbbq_file, sizeof(gbbq_file));
            damaged[offset] ^= 0x5A;
            error.message[0] = '\0';
            if (tdx_gbbq_parse(damaged, sizeof(damaged), &wanted, records,
                               GBBQ_FIXTURE_COUNT + 4, &damaged_count, &source_count,
                               &error) != TDX_OK) {
                refused++;
                continue;
            }
            /* It parsed.  Then it must not have parsed to the same thing. */
            if (damaged_count != intact_count) {
                visible++;
                continue;
            }
            for (record = 0; record < intact_count; ++record) {
                if (records[record].date != intact[record].date ||
                    records[record].category != intact[record].category ||
                    memcmp(records[record].float_values, intact[record].float_values,
                           sizeof(intact[record].float_values)) != 0 ||
                    memcmp(records[record].share_values, intact[record].share_values,
                           sizeof(intact[record].share_values)) != 0)
                    differs = 1;
            }
            if (differs)
                visible++;
            else
                CHECK(0, "flipping byte %zu changed nothing at all", offset);
        }
        CHECK(refused + visible == TDX_GBBQ_ENCRYPTED_SIZE,
              "every one of the %d encrypted bytes was accounted for: %zu refused, %zu "
              "visible as changed values",
              TDX_GBBQ_ENCRYPTED_SIZE, refused, visible);
        CHECK(refused > 0, "at least some corruption is refused outright");
    }
}

static void test_default_path(void) {
    char path[256];
    tdx_error error;
    error.message[0] = '\0';

    CHECK(tdx_gbbq_default_path("C:\\new_tdx", path, sizeof(path), &error) == TDX_OK,
          "default path: %s", error.message);
    CHECK(strcmp(path, "C:\\new_tdx\\T0002\\hq_cache\\gbbq") == 0, "the default path: %s", path);
    CHECK(tdx_gbbq_default_path(NULL, path, sizeof(path), &error) == TDX_ERR,
          "a null root must be refused");
    CHECK(tdx_gbbq_default_path("C:\\new_tdx", path, 8, &error) == TDX_ERR,
          "a short buffer must be refused");
}

int main(void) {
    test_cipher_state();
    test_decrypt_captured();
    test_parse_captured();
    test_parse_rejects();
    test_default_path();

    if (failures) {
        printf("%d gbbq check(s) failed\n", failures);
        return 1;
    }
    printf("gbbq checks passed\n");
    return 0;
}
