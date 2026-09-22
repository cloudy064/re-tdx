/* tdx_kline.c - 0x052D multi-period K-lines, ported from the C++ tool.
 *
 * Reuses the project's existing varint and wire-number decoders: both are
 * byte-for-byte the same routines the C++ side uses, so there is no reason to
 * write a second copy here. */
#include "tdx_kline.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- period table ----------------------------------------------------- */

int tdx_kline_period_parse(const char *text, tdx_kline_period *out, tdx_error *err) {
    static const struct {
        const char *text;
        const char *name;
        uint16_t id;
        int intraday;
    } table[] = {
        {"time", "time", 7, 1},      {"timeline", "time", 7, 1},
        {"1m", "1m", 7, 1},          {"5m", "5m", 0, 1},
        {"15m", "15m", 1, 1},        {"30m", "30m", 2, 1},
        {"60m", "60m", 3, 1},        {"1h", "60m", 3, 1},
        {"day", "day", 4, 0},        {"1d", "day", 4, 0},
        {"daily", "day", 4, 0},      {"week", "week", 5, 0},
        {"1w", "week", 5, 0},        {"weekly", "week", 5, 0},
        {"month", "month", 6, 0},    {"1mo", "month", 6, 0},
        {"monthly", "month", 6, 0},
    };
    char working[24];
    size_t index;
    size_t length;

    if (!text || !out) {
        tdx_error_set(err, "period needs text and an output");
        return TDX_ERR;
    }
    length = strlen(text);
    if (length == 0 || length >= sizeof(working)) {
        tdx_error_set(err, "unknown K-line period '%s'", text);
        return TDX_ERR;
    }
    /* Trim and fold case without pulling in a helper from another module. */
    {
        size_t start = 0;
        size_t end = length;
        while (start < end && (text[start] == ' ' || text[start] == '\t'))
            start++;
        while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\t'))
            end--;
        if (end == start) {
            tdx_error_set(err, "unknown K-line period '%s'", text);
            return TDX_ERR;
        }
        for (index = 0; index + start < end; ++index) {
            char ch = text[start + index];
            if (ch >= 'A' && ch <= 'Z')
                ch = (char)(ch - 'A' + 'a');
            working[index] = ch;
        }
        working[index] = '\0';
    }
    for (index = 0; index < sizeof(table) / sizeof(table[0]); ++index) {
        if (strcmp(working, table[index].text) == 0) {
            out->name = table[index].name;
            out->id = table[index].id;
            out->intraday = table[index].intraday;
            return TDX_OK;
        }
    }
    tdx_error_set(err,
                  "unknown K-line period '%s'; expected time, 1m, 5m, 15m, 30m, 60m, day, "
                  "week or month",
                  text);
    return TDX_ERR;
}

int tdx_kline_is_block_index_code(const char *code) {
    size_t index;
    if (!code || strlen(code) != 6)
        return 0;
    if (strncmp(code, "880", 3) != 0 && strncmp(code, "881", 3) != 0)
        return 0;
    for (index = 0; index < 6; ++index)
        if (code[index] < '0' || code[index] > '9')
            return 0;
    return 1;
}

int tdx_kline_lc1_date(uint16_t word, int *out_date, tdx_error *err) {
    int year = (int)(word / 2048u) + 2004;
    int remainder = (int)(word % 2048u);
    int month = remainder / 100;
    int day = remainder % 100;
    if (!out_date) {
        tdx_error_set(err, "LC1 date decoding needs an output");
        return TDX_ERR;
    }
    if (year < 2004 || year > 2200 || month < 1 || month > 12 || day < 1 || day > 31) {
        tdx_error_set(err, "invalid LC1 date word %u", (unsigned)word);
        return TDX_ERR;
    }
    *out_date = year * 10000 + month * 100 + day;
    return TDX_OK;
}

/* --- storage ---------------------------------------------------------- */

void tdx_kline_page_init(tdx_kline_page *page) {
    if (!page)
        return;
    memset(page, 0, sizeof(*page));
}

void tdx_kline_page_free(tdx_kline_page *page) {
    if (!page)
        return;
    free(page->bars);
    memset(page, 0, sizeof(*page));
}

void tdx_kline_series_init(tdx_kline_series *series) {
    if (!series)
        return;
    memset(series, 0, sizeof(*series));
}

void tdx_kline_series_free(tdx_kline_series *series) {
    if (!series)
        return;
    free(series->bars);
    memset(series, 0, sizeof(*series));
}

static int page_push(tdx_kline_page *page, const tdx_kline_bar *bar, tdx_error *err) {
    if (page->count == page->capacity) {
        size_t wanted = page->capacity ? page->capacity * 2 : 64;
        tdx_kline_bar *grown = (tdx_kline_bar *)realloc(page->bars, wanted * sizeof(*grown));
        if (!grown) {
            tdx_error_set(err, "out of memory growing the K-line page to %zu bars", wanted);
            return TDX_ERR;
        }
        page->bars = grown;
        page->capacity = wanted;
    }
    page->bars[page->count++] = *bar;
    return TDX_OK;
}

static int series_append(tdx_kline_series *series, const tdx_kline_bar *bars, size_t count,
                         tdx_error *err) {
    if (series->count + count > series->capacity) {
        size_t wanted = series->capacity ? series->capacity : 256;
        tdx_kline_bar *grown;
        while (wanted < series->count + count)
            wanted *= 2;
        grown = (tdx_kline_bar *)realloc(series->bars, wanted * sizeof(*grown));
        if (!grown) {
            tdx_error_set(err, "out of memory growing the K-line series to %zu bars", wanted);
            return TDX_ERR;
        }
        series->bars = grown;
        series->capacity = wanted;
    }
    memcpy(series->bars + series->count, bars, count * sizeof(*bars));
    series->count += count;
    return TDX_OK;
}

/* --- request ---------------------------------------------------------- */

int tdx_kline_build_request(int market_id, const char *code, uint16_t period_id,
                           uint16_t start, uint16_t count, tdx_buf *out, tdx_error *err) {
    uint8_t body[TDX_KLINE_REQUEST_SIZE];
    size_t length;
    size_t index;

    if (!out) {
        tdx_error_set(err, "0x052D needs an output buffer");
        return TDX_ERR;
    }
    if (market_id < 0 || market_id > 2) {
        tdx_error_set(err, "K-line market must be 0, 1 or 2, got %d", market_id);
        return TDX_ERR;
    }
    if (!code || (length = strlen(code)) == 0 || length > 6) {
        tdx_error_set(err, "K-line code must be 1..6 characters");
        return TDX_ERR;
    }
    for (index = 0; index < length; ++index) {
        char ch = code[index];
        if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= 'a' && ch <= 'z') || ch == ' ')) {
            tdx_error_set(err, "K-line code must be ASCII letters, digits or spaces: %s", code);
            return TDX_ERR;
        }
    }
    if (count < 1 || count > TDX_KLINE_PAGE_SIZE_MAX) {
        tdx_error_set(err, "K-line count must be in 1..%d, got %u",
                      TDX_KLINE_PAGE_SIZE_MAX, (unsigned)count);
        return TDX_ERR;
    }
    if (period_id > 11) {
        tdx_error_set(err, "K-line period id %u is outside the known range",
                      (unsigned)period_id);
        return TDX_ERR;
    }
    memset(body, 0, sizeof(body));
    body[0] = (uint8_t)(market_id & 0xFF);
    body[1] = (uint8_t)((market_id >> 8) & 0xFF);
    memcpy(body + 2, code, length);
    body[8] = (uint8_t)(period_id & 0xFFu);
    body[9] = (uint8_t)((period_id >> 8) & 0xFFu);
    body[10] = (uint8_t)(TDX_KLINE_PERIOD_PARAMETER & 0xFFu);
    body[11] = (uint8_t)((TDX_KLINE_PERIOD_PARAMETER >> 8) & 0xFFu);
    body[12] = (uint8_t)(start & 0xFFu);
    body[13] = (uint8_t)((start >> 8) & 0xFFu);
    body[14] = (uint8_t)(count & 0xFFu);
    body[15] = (uint8_t)((count >> 8) & 0xFFu);
    tdx_buf_clear(out);
    return tdx_buf_append(out, body, sizeof(body), err);
}

/* --- reply ------------------------------------------------------------ */

/* Intraday means "the record carries an LC1 date word plus a minute of day".
 * Period 7 is the time line; 8 and 13 are not reachable through the period
 * table but the wire treats them the same, so the rule is kept identical to the
 * peer's. */
static int period_is_intraday(uint16_t period_id) {
    return period_id < 4 || period_id == 7 || period_id == 8 || period_id == 13;
}

int tdx_kline_parse(const uint8_t *payload, size_t size, uint16_t period_id, int index_mode,
                   tdx_kline_page *out, tdx_error *err) {
    uint16_t count;
    size_t offset = 2;
    int64_t previous_close_milli = 0;
    int intraday = period_is_intraday(period_id);
    uint16_t record;

    if (!payload || !out) {
        tdx_error_set(err, "0x052D reply needs a body and an output page");
        return TDX_ERR;
    }
    if (size < 2) {
        tdx_error_set(err, "0x052D reply is %zu bytes, expected at least 2", size);
        return TDX_ERR;
    }
    count = tdx_u16le(payload);
    if (size == 2 && count != 0) {
        tdx_error_set(err, "0x052D reply declares %u records but carries none",
                      (unsigned)count);
        return TDX_ERR;
    }
    if (count > TDX_KLINE_PAGE_SIZE_MAX) {
        tdx_error_set(err, "0x052D reply declares %u records, above the %d cap",
                      (unsigned)count, TDX_KLINE_PAGE_SIZE_MAX);
        return TDX_ERR;
    }

    for (record = 0; record < count; ++record) {
        tdx_kline_bar bar;
        int minute_of_day = 15 * 60;
        int64_t open_milli;
        int64_t close_milli;
        int64_t high_milli;
        int64_t low_milli;
        double volume_value;
        double amount_value;

        memset(&bar, 0, sizeof(bar));
        if (offset + 4 > size) {
            tdx_error_set(err, "K-line record %u has no time fields", (unsigned)record + 1);
            return TDX_ERR;
        }
        if (intraday) {
            if (tdx_kline_lc1_date(tdx_u16le(payload + offset), &bar.date, err) != TDX_OK)
                return TDX_ERR;
            minute_of_day = (int)tdx_u16le(payload + offset + 2);
        } else {
            uint32_t raw_date = tdx_u32le(payload + offset);
            int year = (int)(raw_date / 10000u);
            int month = (int)(raw_date / 100u % 100u);
            int day = (int)(raw_date % 100u);
            if (year < 1990 || year > 2200 || month < 1 || month > 12 || day < 1 || day > 31) {
                tdx_error_set(err, "invalid daily K-line date %u", (unsigned)raw_date);
                return TDX_ERR;
            }
            bar.date = (int)raw_date;
        }
        offset += 4;
        if (minute_of_day >= 24 * 60) {
            tdx_error_set(err, "K-line record %u has minute-of-day %d", (unsigned)record + 1,
                          minute_of_day);
            return TDX_ERR;
        }
        bar.hour = minute_of_day / 60;
        bar.minute = minute_of_day % 60;

        if (tdx_varint_decode(payload, size, &offset, &open_milli, err) != TDX_OK)
            return TDX_ERR;
        open_milli += previous_close_milli;
        if (tdx_varint_decode(payload, size, &offset, &close_milli, err) != TDX_OK)
            return TDX_ERR;
        close_milli += open_milli;
        if (tdx_varint_decode(payload, size, &offset, &high_milli, err) != TDX_OK)
            return TDX_ERR;
        high_milli += open_milli;
        if (tdx_varint_decode(payload, size, &offset, &low_milli, err) != TDX_OK)
            return TDX_ERR;
        low_milli += open_milli;
        previous_close_milli = close_milli;

        if (offset + 8 > size) {
            tdx_error_set(err, "K-line record %u has no volume/amount fields",
                          (unsigned)record + 1);
            return TDX_ERR;
        }
        volume_value = tdx_wire_number(tdx_u32le(payload + offset));
        amount_value = tdx_wire_number(tdx_u32le(payload + offset + 4));
        offset += 8;

        bar.open = (double)open_milli / 1000.0;
        bar.close = (double)close_milli / 1000.0;
        bar.high = (double)high_milli / 1000.0;
        bar.low = (double)low_milli / 1000.0;
        bar.amount = amount_value;
        /* Check before converting: a wire float can exceed int64. */
        if (!isfinite(volume_value) || volume_value < 0.0 ||
            volume_value >= (double)INT64_MAX) {
            tdx_error_set(err, "K-line record %u has volume %g outside int64",
                          (unsigned)record + 1, volume_value);
            return TDX_ERR;
        }
        bar.volume = (int64_t)(volume_value + 0.5);
        if (index_mode) {
            if (offset + 4 > size) {
                tdx_error_set(err, "index K-line record %u has no breadth fields",
                              (unsigned)record + 1);
                return TDX_ERR;
            }
            bar.extra_1 = tdx_u16le(payload + offset);
            bar.extra_2 = tdx_u16le(payload + offset + 2);
            bar.has_extras = 1;
            offset += 4;
        }
        if (!isfinite(bar.open) || !isfinite(bar.high) || !isfinite(bar.low) ||
            !isfinite(bar.close) || !isfinite(bar.amount)) {
            tdx_error_set(err, "K-line record %u contains a non-finite value",
                          (unsigned)record + 1);
            return TDX_ERR;
        }
        if (page_push(out, &bar, err) != TDX_OK)
            return TDX_ERR;
    }

    if (offset != size) {
        tdx_error_set(err, "0x052D reply has %zu trailing bytes after %u records", size - offset,
                      (unsigned)count);
        return TDX_ERR;
    }
    return TDX_OK;
}

void tdx_kline_sort(tdx_kline_bar *bars, size_t count) {
    size_t index;
    if (!bars || count < 2)
        return;
    /* Insertion sort keeps the order stable without pulling in qsort's
     * comparator indirection; pages hold at most a few thousand bars. */
    for (index = 1; index < count; ++index) {
        tdx_kline_bar key = bars[index];
        size_t slot = index;
        while (slot > 0) {
            const tdx_kline_bar *previous = &bars[slot - 1];
            int earlier = previous->date < key.date ||
                          (previous->date == key.date &&
                           (previous->hour < key.hour ||
                            (previous->hour == key.hour && previous->minute <= key.minute)));
            if (earlier)
                break;
            bars[slot] = bars[slot - 1];
            slot--;
        }
        bars[slot] = key;
    }
}

/* --- session-bound ---------------------------------------------------- */

int tdx_kline_fetch(tdx_connection *connection, int market_id, const char *code,
                    const tdx_kline_period *period, int index_mode, uint16_t start,
                    uint16_t page_size, size_t max_pages, tdx_kline_series *out,
                    tdx_error *err) {
    tdx_kline_page *pages = NULL;
    size_t page_count = 0;
    size_t page_capacity = 0;
    size_t page_index;
    int result = TDX_ERR;

    if (!connection || !period || !out) {
        tdx_error_set(err, "K-line fetch needs a session, a period and an output");
        return TDX_ERR;
    }
    if (page_size < 1 || page_size > TDX_KLINE_PAGE_SIZE_MAX) {
        tdx_error_set(err, "K-line page size must be in 1..%d", TDX_KLINE_PAGE_SIZE_MAX);
        return TDX_ERR;
    }
    if (max_pages < 1 || max_pages > TDX_KLINE_PAGES_MAX) {
        tdx_error_set(err, "K-line pages must be in 1..%d", TDX_KLINE_PAGES_MAX);
        return TDX_ERR;
    }
    if ((long)start + ((long)max_pages - 1) * (long)page_size > TDX_KLINE_START_MAX) {
        tdx_error_set(err, "the last K-line page start would exceed %d", TDX_KLINE_START_MAX);
        return TDX_ERR;
    }

    for (page_index = 0; page_index < max_pages; ++page_index) {
        tdx_kline_page page;
        tdx_buf request = {0};
        tdx_buf response = {0};
        uint16_t current_start = (uint16_t)(start + (int)page_index * (int)page_size);
        int parsed;

        tdx_kline_page_init(&page);
        tdx_buf_init(&request);
        tdx_buf_init(&response);
        if (tdx_kline_build_request(market_id, code, period->id, current_start, page_size,
                                    &request, err) != TDX_OK)
            parsed = TDX_ERR;
        else if (tdx_connection_call(connection, TDX_CMD_KLINE, request.data, request.len,
                                     &response, err) != TDX_OK)
            parsed = TDX_ERR;
        else
            parsed = tdx_kline_parse(response.data, response.len, period->id, index_mode, &page,
                                     err);
        tdx_buf_free(&request);
        tdx_buf_free(&response);
        if (parsed != TDX_OK) {
            tdx_kline_page_free(&page);
            goto done;
        }
        if (page_count == page_capacity) {
            size_t wanted = page_capacity ? page_capacity * 2 : 4;
            tdx_kline_page *grown = (tdx_kline_page *)realloc(pages, wanted * sizeof(*grown));
            if (!grown) {
                tdx_error_set(err, "out of memory growing the K-line page list to %zu", wanted);
                tdx_kline_page_free(&page);
                goto done;
            }
            pages = grown;
            page_capacity = wanted;
        }
        pages[page_count++] = page;
        if (page.count < page_size) {
            out->reached_end = 1;
            break;
        }
    }

    out->pages = page_count;
    out->page_size = page_size;
    out->period_id = period->id;
    out->index_mode = index_mode;
    for (page_index = 0; page_index < page_count; ++page_index)
        if (series_append(out, pages[page_index].bars, pages[page_index].count, err) != TDX_OK)
            goto done;
    /* The server sends the newest bars first, so chronological order is only
     * meaningful after the whole walk. */
    tdx_kline_sort(out->bars, out->count);
    result = TDX_OK;

done:
    if (pages) {
        size_t index;
        for (index = 0; index < page_count; ++index)
            tdx_kline_page_free(&pages[index]);
        free(pages);
    }
    return result;
}

int tdx_kline_fetch_from_pool(const tdx_endpoint_pool *pool, int timeout_ms, int market_id,
                              const char *code, const tdx_kline_period *period,
                              int index_mode, uint16_t start, uint16_t page_size,
                              size_t max_pages, tdx_kline_series *out, char *endpoint_used,
                              size_t endpoint_used_size, tdx_error *err) {
    tdx_connection connection;
    size_t attempt;
    int result = TDX_ERR;

    if (!pool || !pool->count || !out) {
        tdx_error_set(err, "K-line fetch needs an endpoint pool and a series");
        return TDX_ERR;
    }
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;
    for (attempt = 0; attempt < pool->count; ++attempt) {
        char address[80];
        tdx_error step;
        int ok;

        tdx_endpoint_address(&pool->items[attempt], address, sizeof(address));
        step.message[0] = '\0';
        if (tdx_connection_open(&connection, &pool->items[attempt], timeout_ms, &step) != TDX_OK)
            continue;
        ok = tdx_kline_fetch(&connection, market_id, code, period, index_mode, start, page_size,
                             max_pages, out, &step) == TDX_OK;
        tdx_connection_close(&connection);
        if (!ok) {
            tdx_kline_series_free(out);
            tdx_kline_series_init(out);
            *err = step;
            continue;
        }
        if (endpoint_used && endpoint_used_size)
            snprintf(endpoint_used, endpoint_used_size, "%s", address);
        result = TDX_OK;
        break;
    }
    return result;
}
