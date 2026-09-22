/* limits_fixtures.h - a live 0x0452 page, GENERATED. Do not hand edit.
 *
 * Produced by c/tools/generators/make_limits_fixtures.py from
 * output/limits_probe_evidence.txt, which output/dbg_limits_probe.c wrote from
 * the bytes a public node returned for a request from index 0.
 *
 * The page holds ONE record, because that is what this command answers: the
 * server returns a single row per request and the client advances by explicit
 * index.  A fixture with several records would not be a capture of anything.
 *
 * The record is SZ000010: up 1.74, down 1.42.  Its midpoint, 1.58, is the
 * previous close the 0x054C command reported for the same security at the same
 * time, which is what makes it a readable sample rather than a plausible one. */
#ifndef TDX_TEST_LIMITS_FIXTURES_H
#define TDX_TEST_LIMITS_FIXTURES_H

#include <stdint.h>

/* 15 bytes: a 2-byte count + 1 record of 13. */
static const uint8_t limits_page[] = {
    0x01, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x52, 0xb8, 0xde, 0x3f, 0x8f, 0xc2, 0xb5,
    0x3f,
};

#define LIMITS_PAGE_COUNT 1

#endif /* TDX_TEST_LIMITS_FIXTURES_H */
