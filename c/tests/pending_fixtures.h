/* pending_fixtures.h - a captured pending convertible-bond list, GENERATED.
 * Do not hand edit.
 *
 * Produced by output/make_pending_fixtures.py from output/pending_primary.gbk,
 * which output/dbg_jsn_probe.c wrote from the bytes a public node returned for
 * bi/list/dfkzz201_1.jsn (md5 c2df9c63712e1e6331d8f3b0ed647440, 153 rows).
 *
 * The capture is reduced to its VERBATIM colheader plus the FIRST 3 ROWS: a
 * fixture of that size can verify the row mapping, and the set reconciliation
 * is tested against arrays built in the test rather than against a capture,
 * because the interesting cases there are duplicates, empty identities and an
 * exactly equal set, which a live document does not promise to contain.
 *
 * As UTF-8, which is what the parser sees after the GBK conversion. */
#ifndef TDX_TEST_PENDING_FIXTURES_H
#define TDX_TEST_PENDING_FIXTURES_H

/* 16 columns, 3 of 153 rows kept. */
static const char pending_primary[] =
    "[{\"colheader\":[\"$ZQDM\",\"$SC\",\"zzlx\",\"mzgm\",\"fadj\",\"date0\",\"byhq\",\"zgj\",\"gdpsl\",\"sgrq\",\"fxrq\",\"zql\",\""
    "zqr\",\"sgdm\",\"sgmc\",\"fxjg\"],\"data\":[[\"600300\",\"1\",\"可交换债\",\"7.00\",\"董事会预案\",\"20170414\",\"0.432862366710855"
    "73055\",\"3.2252\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\"],[\"000410\",\"0\",\"可交换债\",\"10.00\",\"董事会预案\",\"20170302\",\"0.37749054"
    "103076812152\",\"5.1862\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\"],[\"603099\",\"1\",\"可转债\",\"3.75\",\"董事会预案\",\"20190613\",\"1.376"
    "87489053844620219\",\"38.8452\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\"]]}]"
;

#define PENDING_FIXTURE_ROWS 3
#define PENDING_FIXTURE_COLUMNS 16
#define PENDING_FIXTURE_SOURCE_ROWS 153

#endif /* TDX_TEST_PENDING_FIXTURES_H */
