/* render_check.h - asserts that a rendered line is JSON, not merely brace-balanced.
 *
 * A brace count cannot tell whether a line parses.  Two real bugs proved that within one
 * round: a Windows path reached the JSON through a raw %s and produced "\U", which is not
 * a JSON escape at all, and a CRC was printed as a bare %08x, so "member_crc":9c4fb1bf
 * could not be parsed either since a JSON number may not begin with a letter.  Both lines
 * were perfectly balanced.
 *
 * So every rendering test asks the project's own parser.  It is header-only on purpose:
 * a test binary already links the core library, and a shared object file would need a
 * CMake entry for what is three lines of glue.
 *
 * Usage, next to the brace check:
 *
 *     char reason[192];
 *     CHECK(render_parses(text, reason, sizeof(reason)),
 *           "and it parses as JSON: %s\n    %s", reason, text);
 *
 * The helper is silent about the text, so the caller can print it in its own message. */
#ifndef TDX_TEST_RENDER_CHECK_H
#define TDX_TEST_RENDER_CHECK_H

#include <stdio.h>
#include <string.h>

#include "tdx_json.h"

/* Returns 1 when the text is a JSON document this project can read, and 0 with a reason
 * written into message otherwise. */
static inline int render_parses(const char *text, char *message, size_t capacity) {
    tdx_json_doc doc;
    tdx_error error;
    int result;

    if (message && capacity)
        message[0] = '\0';
    if (!text) {
        if (message && capacity)
            snprintf(message, capacity, "there is no text to parse");
        return 0;
    }
    error.message[0] = '\0';
    tdx_json_doc_init(&doc);
    result = tdx_json_parse((const uint8_t *)text, strlen(text), &doc, &error);
    tdx_json_doc_free(&doc);
    if (result != TDX_OK) {
        if (message && capacity)
            snprintf(message, capacity, "%s", error.message);
        return 0;
    }
    /* A document that parses but leaves the text unused would mean the parser stopped
     * early and the rest of the line is garbage. */
    if (message && capacity)
        message[0] = '\0';
    return 1;
}

#endif /* TDX_TEST_RENDER_CHECK_H */
