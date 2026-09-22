#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define make_dir(p) _mkdir(p)
#define remove_dir(p) _rmdir(p)
#define process_id() _getpid()
#else
#include <sys/stat.h>
#include <unistd.h>
#define make_dir(p) mkdir(p, 0700)
#define remove_dir(p) rmdir(p)
#define process_id() getpid()
#endif
#include "../src/tdx_cache.h"
#include "../src/tdx_zst_day_internal.h"
#include "tdx_thread.h"
#include "tdx_blocks.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %d: %s\n", __LINE__, #c); failures++; } } while (0)

static size_t short_write(void *context, const void *data, size_t size, FILE *file) {
    (void)context;
    return fwrite(data, 1, size / 2, file);
}
static int fail_close(void *context, FILE *file) {
    (void)context;
    fclose(file);
    return EOF;
}
static int fail_replace(void *context, const char *temporary, const char *path) {
    (void)context; (void)temporary; (void)path;
    return -1;
}
static int equals_file(const char *path, const tdx_buf *expected) {
    tdx_buf actual;
    int equal;
    tdx_buf_init(&actual);
    equal = tdx_cache_read(path, &actual, NULL) == TDX_CACHE_FOUND &&
            actual.len == expected->len && !memcmp(actual.data, expected->data, actual.len);
    tdx_buf_free(&actual);
    return equal;
}
typedef struct writer {
    const char *path;
    tdx_buf bytes;
    int failed;
} writer;
static void write_many(void *context) {
    writer *item = (writer *)context;
    int index;
    for (index = 0; index < 20; ++index)
        if (tdx_cache_write(item->path, &item->bytes, NULL) != TDX_OK) item->failed++;
}
static void atomic_tests(const char *path) {
    uint8_t old_bytes[] = "previous valid cache", new_bytes[] = "replacement valid cache";
    tdx_buf old = {old_bytes, sizeof(old_bytes), sizeof(old_bytes)};
    tdx_buf fresh = {new_bytes, sizeof(new_bytes), sizeof(new_bytes)};
    tdx_cache_write_ops ops = {0};
    writer writers[2];
    tdx_thread threads[2] = {{0}};
    int started[2], index;
    CHECK(tdx_cache_write(path, &old, NULL) == TDX_OK);
    ops.write = short_write;
    CHECK(tdx_cache_write_with_ops(path, &fresh, &ops, NULL) == TDX_ERR);
    CHECK(equals_file(path, &old));
    ops.write = NULL; ops.close = fail_close;
    CHECK(tdx_cache_write_with_ops(path, &fresh, &ops, NULL) == TDX_ERR);
    CHECK(equals_file(path, &old));
    ops.close = NULL; ops.replace = fail_replace;
    CHECK(tdx_cache_write_with_ops(path, &fresh, &ops, NULL) == TDX_ERR);
    CHECK(equals_file(path, &old));
    writers[0].path = writers[1].path = path;
    writers[0].bytes = old; writers[1].bytes = fresh;
    writers[0].failed = writers[1].failed = 0;
    for (index = 0; index < 2; ++index) {
        started[index] = tdx_thread_start(&threads[index], write_many, &writers[index], NULL) == TDX_OK;
        CHECK(started[index]);
    }
    for (index = 0; index < 2; ++index) {
        if (started[index]) tdx_thread_join(&threads[index]);
        CHECK(!writers[index].failed);
    }
    CHECK(equals_file(path, &old) || equals_file(path, &fresh));
}

typedef struct fetch_fixture { tdx_buf *payload; int calls; } fetch_fixture;
static int fixture_fetch(void *context, const tdx_endpoint_pool *pool, int timeout_ms,
    const char *path, tdx_buf *payload, tdx_file_info *info, char *endpoint,
    size_t endpoint_size, tdx_error *err) {
    fetch_fixture *fixture = (fetch_fixture *)context;
    (void)pool; (void)timeout_ms; (void)path; (void)info; (void)endpoint; (void)endpoint_size;
    fixture->calls++;
    tdx_buf_clear(payload);
    return tdx_buf_append(payload, fixture->payload->data, fixture->payload->len, err);
}
static void u32(uint8_t *bytes, uint32_t value) {
    unsigned index;
    for (index = 0; index < 4; ++index) bytes[index] = (uint8_t)(value >> (index * 8));
}
static void day_tests(const char *directory) {
    uint8_t stream[] = {3,'0','1','0','0','0','6','2','3',2,'0','T','9','3','0','0','0',4,
                       3,'0','1','0','0','0','6','2','3',2,'0','T','9','3','0','0','1',4};
    uint8_t image[256] = {0}, corrupt[] = "not an image";
    uLongf compressed = sizeof(image) - 24;
    tdx_buf valid = {image, 0, sizeof(image)}, invalid = {corrupt, sizeof(corrupt), sizeof(corrupt)};
    tdx_zst_day_options options = {0};
    tdx_zst_day_result result;
    fetch_fixture fixture = {&valid, 0};
    char path[256];
    tdx_error err = {{0}};
    tdx_buf missing;
    tdx_buf_init(&missing);
    CHECK(compress2(image + 24, &compressed, stream, sizeof(stream), Z_BEST_SPEED) == Z_OK);
    u32(image + 8, (uint32_t)compressed); u32(image + 16, sizeof(stream));
    valid.len = 24 + compressed;
    memcpy(options.security.code, "000623", 7);
    memcpy(options.date, "20260612", 9);
    options.cache_dir = directory;
    options.quiet = 1;
    options.limit = 1;
    options.out = tmpfile();
    CHECK(options.out != NULL);
    if (!options.out) return;
    CHECK(tdx_zst_day_build_paths(&options.security, options.date, directory, NULL, 0,
                                  path, sizeof(path), &err) == TDX_OK);
    CHECK(tdx_cache_read(path, &missing, &err) == TDX_CACHE_MISSING);
    CHECK(tdx_cache_write(path, &invalid, &err) == TDX_OK);
    CHECK(tdx_zst_day_run_with_fetch(NULL, 0, &options, &result, fixture_fetch, &fixture, &err) == TDX_OK);
    CHECK(fixture.calls == 1 && result.downloaded && result.records == 2 && result.emitted == 1);
    CHECK(equals_file(path, &valid));
    CHECK(tdx_zst_day_run_with_fetch(NULL, 0, &options, &result, fixture_fetch, &fixture, NULL) == TDX_OK);
    CHECK(fixture.calls == 1 && !result.downloaded);
    options.refresh = 1;
    fixture.payload = &invalid;
    CHECK(tdx_zst_day_run_with_fetch(NULL, 0, &options, &result, fixture_fetch, &fixture, &err) == TDX_ERR);
    CHECK(equals_file(path, &valid));
    /* A valid container for another security must not replace a good entry. */
    memcpy(options.security.code, "000624", 7);
    fixture.payload = &valid;
    CHECK(tdx_zst_day_run_with_fetch(NULL, 0, &options, &result, fixture_fetch, &fixture, &err) == TDX_ERR);
    CHECK(equals_file(path, &valid));
    CHECK(tdx_cache_read(directory, &missing, &err) == TDX_CACHE_ERROR);
    fclose(options.out);
    tdx_buf_free(&missing);
    remove(path);
}

typedef struct block_loader { const char *root; int failed; } block_loader;
static void load_many(void *context) {
    block_loader *loader = (block_loader *)context;
    unsigned index;
    for (index = 0; index < 40; ++index) {
        tdx_block blocks[4];
        tdx_block_member members[1];
        tdx_blocks_load_report report;
        size_t block_count = 0, member_count = 0;
        if (tdx_blocks_load(loader->root, blocks, 4, &block_count, members, 1, &member_count,
                            NULL, 0, NULL, &report, NULL) != TDX_OK ||
            block_count != 1 || !report.industry_catalog_read || member_count != 0)
            loader->failed++;
    }
}
static void blocks_tests(const char *directory) {
    char parent[200], cache[220], catalog[240], unreadable[240];
    uint8_t contents[] = "sample|880301|2|1|1|T01\n";
    tdx_buf bytes = {contents, sizeof(contents) - 1, sizeof(contents)};
    tdx_thread threads[2] = {{0}};
    block_loader loaders[2] = {{directory, 0}, {directory, 0}};
    int started[2], index;
    snprintf(parent, sizeof(parent), "%s/T0002", directory);
    snprintf(cache, sizeof(cache), "%s/hq_cache", parent);
    snprintf(catalog, sizeof(catalog), "%s/tdxzs3.cfg", cache);
    snprintf(unreadable, sizeof(unreadable), "%s/tdxhy.cfg", cache);
    CHECK(make_dir(parent) == 0);
    CHECK(make_dir(cache) == 0);
    CHECK(tdx_cache_write(catalog, &bytes, NULL) == TDX_OK);
    for (index = 0; index < 2; ++index) {
        started[index] = tdx_thread_start(&threads[index], load_many, &loaders[index], NULL) == TDX_OK;
        CHECK(started[index]);
    }
    for (index = 0; index < 2; ++index) {
        if (started[index]) tdx_thread_join(&threads[index]);
        CHECK(!loaders[index].failed);
    }
    /* Existing but unreadable as a regular file must not mean an optional file is absent. */
    CHECK(make_dir(unreadable) == 0);
    {
        tdx_block blocks[4];
        tdx_block_member members[1];
        size_t block_count, member_count;
        CHECK(tdx_blocks_load(directory, blocks, 4, &block_count, members, 1, &member_count,
                              NULL, 0, NULL, NULL, NULL) == TDX_ERR);
    }
    CHECK(remove_dir(unreadable) == 0);
    remove(catalog);
    CHECK(remove_dir(cache) == 0);
    CHECK(remove_dir(parent) == 0);
}
int main(void) {
    char directory[128], path[180];
    snprintf(directory, sizeof(directory), "cache-test-%ld", (long)process_id());
    CHECK(make_dir(directory) == 0);
    if (failures) return 1;
    snprintf(path, sizeof(path), "%s/entry.img", directory);
    atomic_tests(path);
    day_tests(directory);
    blocks_tests(directory);
    remove(path);
    CHECK(remove_dir(directory) == 0); /* catches leaked .part files too */
    printf("cache checks: %d failures\n", failures);
    return failures ? 1 : 0;
}
