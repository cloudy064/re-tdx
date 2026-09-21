/* tdx_directory.h - 0x044D/0x044E server security directory.
 *
 * The directory is the trustable universe source: it returns every listed
 * security per market with its exchange-assigned decimal digits, contract
 * multiple and previous close.  Categories are derived from the exchange
 * prefix table recovered from the client, not guessed. */
#ifndef TDX_DIRECTORY_H
#define TDX_DIRECTORY_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_error.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_DIRECTORY_RECORD_SIZE 37
#define TDX_DIRECTORY_PAGE_MAX 1600
#define TDX_DIRECTORY_NAME_MAX 72
#define TDX_DIRECTORY_CATEGORY_MAX 24
#define TDX_DIRECTORY_BOARD_MAX 24

typedef struct tdx_security {
    int market_id;
    char code[8];
    char name[TDX_DIRECTORY_NAME_MAX];
    uint16_t multiple;
    uint8_t decimal;
    double previous_close_price;
    char category[TDX_DIRECTORY_CATEGORY_MAX];
    char board[TDX_DIRECTORY_BOARD_MAX];
} tdx_security;

typedef struct tdx_security_list {
    tdx_security *items;
    size_t count;
    size_t capacity;
} tdx_security_list;

void tdx_security_list_init(tdx_security_list *list);
void tdx_security_list_free(tdx_security_list *list);

/* Exchange prefix classification, ported from the verified client table. */
const char *tdx_directory_category(int market_id, const char *code);
const char *tdx_directory_board(int market_id, const char *code,
                                const char *category);
/* True when category is "all" or exactly the record category. */
int tdx_security_matches_category(const tdx_security *security, const char *category);

/* 0x044E: number of securities the server reports for one market. */
int tdx_directory_count(tdx_connection *connection, int market_id, size_t *out,
                        tdx_error *err);

/* 0x044D: one page appended to out. */
int tdx_directory_page(tdx_connection *connection, int market_id, uint32_t start,
                       uint32_t limit, tdx_security_list *out, tdx_error *err);

/* Decodes one 0x044D page body into out, appending to it. */
int tdx_directory_parse_page(const uint8_t *payload, size_t size,
                             int market_id, tdx_security_list *out,
                             size_t *parsed, tdx_error *err);

/* Downloads a whole market page by page.  Fails when the received total does
 * not match the reported count, so a silently truncated directory cannot be
 * mistaken for a complete universe. */
int tdx_directory_fetch(tdx_connection *connection, int market_id, size_t page_size,
                        tdx_security_list *out, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_DIRECTORY_H */
