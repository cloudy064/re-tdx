/* tdx_gbbq.c - the local encrypted GBBQ file, ported from the C++ reference. */
#include "tdx_gbbq.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gbbq_cipher_state.h"

/* How many 8-byte blocks the encrypted prefix holds. */
#define TDX_GBBQ_BLOCKS (TDX_GBBQ_ENCRYPTED_SIZE / 8)

static int base64_digit(char value) {
    if (value >= 'A' && value <= 'Z')
        return value - 'A';
    if (value >= 'a' && value <= 'z')
        return value - 'a' + 26;
    if (value >= '0' && value <= '9')
        return value - '0' + 52;
    if (value == '+')
        return 62;
    if (value == '/')
        return 63;
    return -1;
}

int tdx_gbbq_decode_cipher_state(uint8_t *out, size_t capacity, size_t *out_size,
                                 tdx_error *err) {
    uint32_t buffer = 0;
    int bits = -8;
    size_t written = 0;
    size_t piece;

    if (out_size)
        *out_size = 0;
    if (!out) {
        tdx_error_set(err, "the cipher state needs an output buffer");
        return TDX_ERR;
    }
    if (capacity < TDX_GBBQ_CIPHER_STATE_BYTES) {
        tdx_error_set(err, "the cipher state needs %d bytes, the output holds %zu",
                      TDX_GBBQ_CIPHER_STATE_BYTES, capacity);
        return TDX_ERR;
    }
    for (piece = 0; piece < TDX_GBBQ_CIPHER_STATE_PIECES; ++piece) {
        const char *text = tdx_gbbq_cipher_state_pieces[piece];
        size_t index;
        for (index = 0; text[index]; ++index) {
            char value = text[index];
            int digit;
            if (value == '=')
                break;
            digit = base64_digit(value);
            if (digit < 0) {
                tdx_error_set(err, "the embedded GBBQ cipher state is not base64");
                return TDX_ERR;
            }
            buffer = (buffer << 6) | (uint32_t)digit;
            bits += 6;
            if (bits >= 0) {
                if (written >= TDX_GBBQ_CIPHER_STATE_BYTES) {
                    tdx_error_set(err,
                                  "the embedded GBBQ cipher state is longer than %d bytes",
                                  TDX_GBBQ_CIPHER_STATE_BYTES);
                    return TDX_ERR;
                }
                out[written++] = (uint8_t)((buffer >> bits) & 0xFFu);
                bits -= 8;
            }
        }
    }
    if (written != TDX_GBBQ_CIPHER_STATE_BYTES) {
        tdx_error_set(err, "the embedded GBBQ cipher state decodes to %zu bytes, expected %d",
                      written, TDX_GBBQ_CIPHER_STATE_BYTES);
        return TDX_ERR;
    }
    if (out_size)
        *out_size = written;
    return TDX_OK;
}

static uint32_t state_word(const uint8_t *state, size_t offset) {
    return tdx_u32le(state + offset);
}

/* One 8-byte block: sixteen rounds, consumed from the round-key area downwards. */
static void decrypt_block(const uint8_t *state, const uint8_t *encrypted, uint8_t *clear) {
    uint32_t current = state_word(state, 0x44) ^ tdx_u32le(encrypted);
    uint32_t previous = tdx_u32le(encrypted + 4);
    int offset;

    for (offset = 0x40; offset >= 4; offset -= 4) {
        uint32_t value = state_word(state, 0x448 + (size_t)((current >> 16) & 0xFFu) * 4);
        uint32_t next_previous;
        value += state_word(state, 0x48 + (size_t)(current >> 24) * 4);
        value ^= state_word(state, 0x848 + (size_t)((current >> 8) & 0xFFu) * 4);
        value += state_word(state, 0xC48 + (size_t)(current & 0xFFu) * 4);
        value ^= state_word(state, (size_t)offset);
        next_previous = current;
        current = previous ^ value;
        previous = next_previous;
    }
    previous ^= state_word(state, 0);
    {
        /* write_u32_le in the reference; kept explicit so the byte order is visible. */
        int shift;
        for (shift = 0; shift < 32; shift += 8) {
            clear[shift / 8] = (uint8_t)((previous >> shift) & 0xFFu);
            clear[4 + shift / 8] = (uint8_t)((current >> shift) & 0xFFu);
        }
    }
}

int tdx_gbbq_decrypt_record(const uint8_t *state, const uint8_t *encrypted, size_t size,
                            uint8_t *clear, tdx_error *err) {
    size_t block;

    if (!state || !encrypted || !clear) {
        tdx_error_set(err, "GBBQ decryption needs a state, an input and an output");
        return TDX_ERR;
    }
    if (size != TDX_GBBQ_RECORD_SIZE) {
        tdx_error_set(err, "GBBQ record is %zu bytes, expected %d", size,
                      TDX_GBBQ_RECORD_SIZE);
        return TDX_ERR;
    }
    for (block = 0; block < TDX_GBBQ_BLOCKS; ++block)
        decrypt_block(state, encrypted + block * 8, clear + block * 8);
    /* The tail is clear on the wire and on disk. */
    memcpy(clear + TDX_GBBQ_ENCRYPTED_SIZE, encrypted + TDX_GBBQ_ENCRYPTED_SIZE,
           TDX_GBBQ_RECORD_SIZE - TDX_GBBQ_ENCRYPTED_SIZE);
    return TDX_OK;
}

/* The reference checks the identity, the reserved byte, the date and finiteness
 * before handing the record to the same parser the network path uses. */
static int validate_record(const uint8_t *record, size_t index, tdx_error *err) {
    size_t digit;
    uint32_t raw;

    if (record[0] > 2) {
        tdx_error_set(err, "GBBQ record %zu has an invalid market", index);
        return TDX_ERR;
    }
    for (digit = 1; digit <= 6; ++digit)
        if (record[digit] < '0' || record[digit] > '9') {
            tdx_error_set(err, "GBBQ record %zu has an invalid security code", index);
            return TDX_ERR;
        }
    if (record[7] != 0) {
        tdx_error_set(err, "GBBQ record %zu has a non-zero reserved byte", index);
        return TDX_ERR;
    }
    raw = tdx_u32le(record + 8);
    if (raw != 0) {
        int year = (int)(raw / 10000u);
        int month = (int)(raw / 100u % 100u);
        int day = (int)(raw % 100u);
        if (year < 1980 || year > 2200 || month < 1 || month > 12 || day < 1 || day > 31) {
            tdx_error_set(err, "GBBQ record %zu has an invalid event date %u", index,
                          (unsigned)raw);
            return TDX_ERR;
        }
    }
    return TDX_OK;
}

int tdx_gbbq_parse(const uint8_t *data, size_t size, const tdx_code *wanted,
                   tdx_capital_record *out, size_t capacity, size_t *out_count,
                   size_t *source_count, tdx_error *err) {
    uint8_t state[TDX_GBBQ_CIPHER_STATE_BYTES];
    uint8_t clear[TDX_GBBQ_RECORD_SIZE];
    size_t state_size = 0;
    size_t count;
    size_t expected;
    size_t index;
    size_t found = 0;

    if (out_count)
        *out_count = 0;
    if (source_count)
        *source_count = 0;
    if (!data || !out) {
        tdx_error_set(err, "GBBQ parsing needs data and an output");
        return TDX_ERR;
    }
    if (!wanted) {
        tdx_error_set(err, "GBBQ parsing needs the security to look for");
        return TDX_ERR;
    }
    if (size < TDX_GBBQ_HEADER_SIZE) {
        tdx_error_set(err, "the GBBQ file is %zu bytes, shorter than its 4-byte header", size);
        return TDX_ERR;
    }
    count = (size_t)tdx_u32le(data);
    if (count > TDX_GBBQ_RECORD_LIMIT) {
        tdx_error_set(err, "the GBBQ header declares %zu records, above the safety limit",
                      count);
        return TDX_ERR;
    }
    expected = TDX_GBBQ_HEADER_SIZE + count * TDX_GBBQ_RECORD_SIZE;
    if (size != expected) {
        tdx_error_set(err, "GBBQ file length mismatch: %zu bytes for %zu records, expected %zu",
                      size, count, expected);
        return TDX_ERR;
    }
    if (tdx_gbbq_decode_cipher_state(state, sizeof(state), &state_size, err) != TDX_OK)
        return TDX_ERR;

    for (index = 0; index < count; ++index) {
        if (tdx_gbbq_decrypt_record(state, data + TDX_GBBQ_HEADER_SIZE +
                                               index * TDX_GBBQ_RECORD_SIZE,
                                    TDX_GBBQ_RECORD_SIZE, clear, err) != TDX_OK)
            return TDX_ERR;
        if (validate_record(clear, index, err) != TDX_OK)
            return TDX_ERR;
        if (clear[0] != wanted->market_id || memcmp(clear + 1, wanted->code, 6) != 0)
            continue;
        if (found >= capacity) {
            tdx_error_set(err, "GBBQ holds more than %zu records for %s%s", capacity,
                          wanted->market_id == 1 ? "SH" : wanted->market_id == 2 ? "BJ" : "SZ",
                          wanted->code);
            return TDX_ERR;
        }
        /* The decrypted record is exactly an 0x000F record, so the same parser
         * decodes it.  Reusing it is the point: the two sources are then compared
         * field for field without a second interpretation of the same bytes. */
        if (tdx_capital_parse_record(clear, TDX_GBBQ_RECORD_SIZE, &out[found], err) != TDX_OK) {
            tdx_error_set(err, "GBBQ record %zu: %s", index, err->message);
            return TDX_ERR;
        }
        found++;
    }
    if (out_count)
        *out_count = found;
    if (source_count)
        *source_count = count;
    return TDX_OK;
}

int tdx_gbbq_default_path(const char *root, char *out, size_t capacity, tdx_error *err) {
    int written;

    if (!out || capacity == 0) {
        tdx_error_set(err, "the default GBBQ path needs an output buffer");
        return TDX_ERR;
    }
    if (!root || !*root) {
        tdx_error_set(err, "the default GBBQ path needs a TDX root");
        return TDX_ERR;
    }
    written = snprintf(out, capacity, "%s\\T0002\\hq_cache\\gbbq", root);
    if (written < 0 || (size_t)written >= capacity) {
        tdx_error_set(err, "the default GBBQ path does not fit in %zu bytes", capacity);
        return TDX_ERR;
    }
    return TDX_OK;
}

int tdx_gbbq_load(const char *path, const char *root, const tdx_code *wanted,
                  tdx_capital_record *out, size_t capacity, size_t *out_count,
                  size_t *source_count, tdx_error *err) {
    char resolved[1024];
    FILE *stream;
    uint8_t *data = NULL;
    long length;
    int result = TDX_ERR;

    if (path && *path) {
        if (strlen(path) >= sizeof(resolved)) {
            tdx_error_set(err, "the GBBQ path is too long");
            return TDX_ERR;
        }
        memcpy(resolved, path, strlen(path) + 1);
    } else if (tdx_gbbq_default_path(root, resolved, sizeof(resolved), err) != TDX_OK) {
        return TDX_ERR;
    }
    stream = fopen(resolved, "rb");
    if (!stream) {
        tdx_error_set(err, "the GBBQ file is unavailable: %s", resolved);
        return TDX_ERR;
    }
    if (fseek(stream, 0, SEEK_END) != 0 || (length = ftell(stream)) < 0 ||
        fseek(stream, 0, SEEK_SET) != 0) {
        tdx_error_set(err, "cannot size the GBBQ file: %s", resolved);
        fclose(stream);
        return TDX_ERR;
    }
    data = (uint8_t *)malloc(length > 0 ? (size_t)length : 1);
    if (!data) {
        tdx_error_set(err, "out of memory for a %ld-byte GBBQ file", length);
        fclose(stream);
        return TDX_ERR;
    }
    if (fread(data, 1, (size_t)length, stream) != (size_t)length) {
        tdx_error_set(err, "cannot read the GBBQ file: %s", resolved);
        goto done;
    }
    result = tdx_gbbq_parse(data, (size_t)length, wanted, out, capacity, out_count,
                            source_count, err);

done:
    free(data);
    fclose(stream);
    return result;
}
