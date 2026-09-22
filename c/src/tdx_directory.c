/* tdx_directory.c - 0x044D/0x044E security directory download.
 *
 * The directory is the trustable universe source: it returns every listed
 * security per market together with the exchange-assigned decimal digits,
 * contract multiple and previous close.  Categories and boards come from the
 * prefix table recovered from the client, never from guesswork. */
#include "tdx_directory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tdx_frame.h"
#include "tdx_internal.h"

#define TDX_CMD_DIRECTORY_LIST 0x044D
#define TDX_CMD_DIRECTORY_COUNT 0x044E

static int starts_with(const char *text, const char *prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

const char *tdx_directory_category(int market_id, const char *code) {
    if (market_id == 1 &&
        (starts_with(code, "000") || starts_with(code, "880") ||
         starts_with(code, "881") || starts_with(code, "999")))
        return "index";
    if (market_id == 0 && starts_with(code, "399"))
        return "index";
    if (market_id == 2 && starts_with(code, "899"))
        return "index";

    if ((market_id == 1 &&
         (starts_with(code, "510") || starts_with(code, "511") ||
          starts_with(code, "512") || starts_with(code, "513") ||
          starts_with(code, "515") || starts_with(code, "516") ||
          starts_with(code, "517") || starts_with(code, "518") ||
          starts_with(code, "520") || starts_with(code, "560") ||
          starts_with(code, "561") || starts_with(code, "562") ||
          starts_with(code, "563") || starts_with(code, "588"))) ||
        (market_id == 0 &&
         (starts_with(code, "158") || starts_with(code, "159"))))
        return "etf";

    if ((market_id == 1 &&
         (starts_with(code, "110") || starts_with(code, "111") ||
          starts_with(code, "113") || starts_with(code, "118") ||
          starts_with(code, "132"))) ||
        (market_id == 0 &&
         (starts_with(code, "123") || starts_with(code, "127") ||
          starts_with(code, "128"))) ||
        (market_id == 2 && starts_with(code, "810")))
        return "convertible_bond";

    if ((market_id == 1 &&
         (starts_with(code, "600") || starts_with(code, "601") ||
          starts_with(code, "603") || starts_with(code, "605") ||
          starts_with(code, "688") || starts_with(code, "689"))) ||
        (market_id == 0 &&
         (starts_with(code, "000") || starts_with(code, "001") ||
          starts_with(code, "002") || starts_with(code, "003") ||
          starts_with(code, "004") || starts_with(code, "300") ||
          starts_with(code, "301"))) ||
        (market_id == 2 && starts_with(code, "92")))
        return "a_share";

    if ((market_id == 1 && starts_with(code, "900")) ||
        (market_id == 0 && starts_with(code, "20")))
        return "b_share";

    if ((market_id == 1 &&
         (starts_with(code, "501") || starts_with(code, "502") ||
          starts_with(code, "505") || starts_with(code, "506"))) ||
        (market_id == 0 && (starts_with(code, "16") || starts_with(code, "18"))))
        return "fund";

    if ((market_id == 1 && starts_with(code, "204")) ||
        (market_id == 0 && starts_with(code, "1318")))
        return "repo";

    if ((market_id == 1 &&
         (starts_with(code, "01") || starts_with(code, "02") ||
          starts_with(code, "10") || starts_with(code, "12"))) ||
        (market_id == 0 && (starts_with(code, "10") || starts_with(code, "11"))) ||
        (market_id == 2 && starts_with(code, "82")))
        return "bond";

    return "unknown";
}

const char *tdx_directory_board(int market_id, const char *code,
                                const char *category) {
    if (!category || strcmp(category, "a_share") != 0)
        return "none";
    if (market_id == 1 &&
        (starts_with(code, "600") || starts_with(code, "601") ||
         starts_with(code, "603") || starts_with(code, "605")))
        return "sse_main_board";
    if (market_id == 1 &&
        (starts_with(code, "688") || starts_with(code, "689")))
        return "sse_star_market";
    if (market_id == 0 &&
        (starts_with(code, "000") || starts_with(code, "001") ||
         starts_with(code, "002") || starts_with(code, "003") ||
         starts_with(code, "004")))
        return "szse_main_board";
    if (market_id == 0 &&
        (starts_with(code, "300") || starts_with(code, "301")))
        return "szse_chinext";
    if (market_id == 2 && starts_with(code, "92"))
        return "bse_listed_stock";
    return "none";
}

int tdx_security_matches_category(const tdx_security *security, const char *category) {
    if (!security || !category || !*category)
        return 1;
    if (strcmp(category, "all") == 0)
        return 1;
    return strcmp(security->category, category) == 0;
}

void tdx_security_list_init(tdx_security_list *list) {
    if (!list)
        return;
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void tdx_security_list_free(tdx_security_list *list) {
    if (!list)
        return;
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static int security_list_reserve(tdx_security_list *list, size_t extra,
                                 tdx_error *err) {
    size_t needed;
    size_t capacity;
    tdx_security *grown;

    if (extra > SIZE_MAX - list->count) {
        tdx_error_set(err, "security list length overflow");
        return TDX_ERR;
    }
    needed = list->count + extra;
    if (needed <= list->capacity)
        return TDX_OK;
    capacity = list->capacity ? list->capacity : 512;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }
    grown = (tdx_security *)realloc(list->items, capacity * sizeof(*grown));
    if (!grown) {
        tdx_error_set(err, "out of memory growing the security list to %zu", capacity);
        return TDX_ERR;
    }
    list->items = grown;
    list->capacity = capacity;
    return TDX_OK;
}

static uint32_t directory_client_date(void) {
    time_t now = time(NULL);
    struct tm local;
#if defined(_WIN32)
    if (localtime_s(&local, &now) != 0)
        return 0;
#else
    if (!localtime_r(&now, &local))
        return 0;
#endif
    return (uint32_t)((local.tm_year + 1900) * 10000 + (local.tm_mon + 1) * 100 +
                      local.tm_mday);
}

static int is_ascii_digits(const char *text, size_t length) {
    size_t index;
    for (index = 0; index < length; ++index)
        if (text[index] < '0' || text[index] > '9')
            return 0;
    return 1;
}

/* Decodes the 16-byte NUL padded GBK name field into UTF-8, tolerating a
 * label the code page cannot represent. */
static void decode_name_field(const uint8_t field[16], char *out, size_t out_size,
                             tdx_error *err) {
    out[0] = '\0';
    {
        size_t name_length = 16;
        while (name_length > 0 && field[name_length - 1] == 0)
            name_length--;
        if (name_length == 0)
            return;
        if (tdx_decode_gb18030(field, name_length, out, out_size, NULL, err) != TDX_OK) {
            out[0] = '\0';
            return;
        }
        {
            char *trimmed = tdx_trim(out);
            if (trimmed != out)
                memmove(out, trimmed, strlen(trimmed) + 1);
        }
    }
}

int tdx_directory_parse_page(const uint8_t *payload, size_t size, int market_id,
                             tdx_security_list *out, size_t *parsed, tdx_error *err) {
    size_t count;
    size_t index;
    size_t offset = 2;

    *parsed = 0;
    if (size < 2) {
        tdx_error_set(err, "security-directory page is too short");
        return TDX_ERR;
    }
    count = tdx_u16le(payload);
    if (size < 2 + count * TDX_DIRECTORY_RECORD_SIZE) {
        tdx_error_set(err, "security-directory page is truncated: expected %zu, got %zu",
                      2 + count * TDX_DIRECTORY_RECORD_SIZE, size);
        return TDX_ERR;
    }
    if (security_list_reserve(out, count, err) != TDX_OK)
        return TDX_ERR;

    for (index = 0; index < count; ++index, offset += TDX_DIRECTORY_RECORD_SIZE) {
        const uint8_t *row = payload + offset;
        tdx_security *item = &out->items[out->count];
        const char *category;

        memset(item, 0, sizeof(*item));
        if (!is_ascii_digits((const char *)row, 6)) {
            tdx_error_set(err, "security-directory page contains an invalid code");
            return TDX_ERR;
        }
        item->market_id = market_id;
        memcpy(item->code, row, 6);
        item->code[6] = '\0';
        item->multiple = tdx_u16le(row + 6);
        decode_name_field(row + 8, item->name, sizeof(item->name), err);
        item->decimal = row[28];
        item->previous_close_price = (double)tdx_f32le(row + 29);
        category = tdx_directory_category(market_id, item->code);
        snprintf(item->category, sizeof(item->category), "%s", category);
        snprintf(item->board, sizeof(item->board), "%s",
                 tdx_directory_board(market_id, item->code, category));
        out->count++;
        (*parsed)++;
    }
    return TDX_OK;
}

int tdx_directory_count(tdx_connection *connection, int market_id, size_t *out,
                        tdx_error *err) {
    tdx_buf request = {0};
    tdx_buf response = {0};
    int result = TDX_ERR;

    if (!connection || !out) {
        tdx_error_set(err, "directory count arguments are null");
        return TDX_ERR;
    }
    tdx_buf_init(&request);
    tdx_buf_init(&response);
    if (tdx_buf_append_u16le(&request, (uint16_t)market_id, err) != TDX_OK)
        goto done;
    if (tdx_buf_append_u32le(&request, directory_client_date(), err) != TDX_OK)
        goto done;
    if (tdx_connection_call(connection, TDX_CMD_DIRECTORY_COUNT, request.data,
                            request.len, &response, err) != TDX_OK)
        goto done;
    if (response.len < 2) {
        tdx_error_set(err, "security-directory count response is too short");
        goto done;
    }
    *out = tdx_u16le(response.data);
    result = TDX_OK;

done:
    tdx_buf_free(&request);
    tdx_buf_free(&response);
    return result;
}

int tdx_directory_page(tdx_connection *connection, int market_id, uint32_t start,
                       uint32_t limit, tdx_security_list *out, tdx_error *err) {
    tdx_buf request = {0};
    tdx_buf response = {0};
    size_t parsed = 0;
    int result = TDX_ERR;

    if (!connection || !out) {
        tdx_error_set(err, "directory page arguments are null");
        return TDX_ERR;
    }
    tdx_buf_init(&request);
    tdx_buf_init(&response);
    if (tdx_buf_append_u16le(&request, (uint16_t)market_id, err) != TDX_OK)
        goto done;
    if (tdx_buf_append_u32le(&request, start, err) != TDX_OK)
        goto done;
    if (tdx_buf_append_u32le(&request, limit, err) != TDX_OK)
        goto done;
    if (tdx_buf_append_u32le(&request, 0, err) != TDX_OK)
        goto done;
    if (tdx_connection_call(connection, TDX_CMD_DIRECTORY_LIST, request.data,
                            request.len, &response, err) != TDX_OK)
        goto done;
    if (tdx_directory_parse_page(response.data, response.len, market_id, out,
                                  &parsed, err) != TDX_OK)
        goto done;
    result = TDX_OK;

done:
    tdx_buf_free(&request);
    tdx_buf_free(&response);
    return result;
}

int tdx_directory_fetch(tdx_connection *connection, int market_id, size_t page_size,
                        tdx_security_list *out, tdx_error *err) {
    size_t reported = 0;
    size_t before;
    uint32_t start = 0;

    if (!connection || !out) {
        tdx_error_set(err, "directory fetch arguments are null");
        return TDX_ERR;
    }
    if (page_size < 1 || page_size > TDX_DIRECTORY_PAGE_MAX) {
        tdx_error_set(err, "directory page size must be in 1..%d",
                      TDX_DIRECTORY_PAGE_MAX);
        return TDX_ERR;
    }
    if (tdx_directory_count(connection, market_id, &reported, err) != TDX_OK)
        return TDX_ERR;
    before = out->count;
    while (out->count - before < reported) {
        size_t remaining = reported - (out->count - before);
        uint32_t requested = (uint32_t)(page_size < remaining ? page_size : remaining);
        size_t page_before = out->count;
        if (tdx_directory_page(connection, market_id, start, requested, out, err) != TDX_OK)
            return TDX_ERR;
        if (out->count == page_before)
            break; /* the peer returned an empty page */
        start += (uint32_t)(out->count - page_before);
    }
    if (out->count - before != reported) {
        tdx_error_set(err, "security-directory download incomplete for market %d: %zu/%zu",
                      market_id, out->count - before, reported);
        return TDX_ERR;
    }
    return TDX_OK;
}
