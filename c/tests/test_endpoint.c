/* test_endpoint.c - endpoint parsing and connect.cfg HQHOST discovery.
 *
 * The discovery path had no coverage at all, which is how a case-sensitive key
 * lookup survived: connect.cfg spells its keys HostNum / IPAddressNN / PortNN,
 * the lookup used strcmp against lower-case names, every entry was missed and
 * the process quietly ran on a single compiled-in node.  These checks pin the
 * INI contract so that cannot come back unnoticed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define test_mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define test_mkdir(path) mkdir(path, 0700)
#endif

#include "tdx_endpoint.h"

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

/* --- parsing ----------------------------------------------------------- */

static void test_endpoint_parse(void) {
    tdx_endpoint endpoint;
    tdx_error error;
    error.message[0] = '\0';

    CHECK(tdx_endpoint_parse("110.41.147.114", &endpoint, &error) == TDX_OK, "host: %s",
          error.message);
    CHECK(strcmp(endpoint.host, "110.41.147.114") == 0, "host %s", endpoint.host);
    CHECK(endpoint.port == 7709, "a bare host must default to 7709, got %u",
          (unsigned)endpoint.port);

    CHECK(tdx_endpoint_parse("110.41.147.114:7712", &endpoint, &error) == TDX_OK, "host:port");
    CHECK(endpoint.port == 7712, "port %u", (unsigned)endpoint.port);

    CHECK(tdx_endpoint_parse("[2001:db8::1]:7709", &endpoint, &error) == TDX_OK, "ipv6");
    CHECK(strcmp(endpoint.host, "2001:db8::1") == 0, "ipv6 host %s", endpoint.host);
    CHECK(endpoint.port == 7709, "ipv6 port %u", (unsigned)endpoint.port);

    CHECK(tdx_endpoint_parse("", &endpoint, &error) == TDX_ERR, "an empty endpoint must fail");
    CHECK(tdx_endpoint_parse("host:0", &endpoint, &error) == TDX_ERR, "port 0 must fail");
    CHECK(tdx_endpoint_parse("host:70000", &endpoint, &error) == TDX_ERR,
          "port 70000 must fail");
    CHECK(tdx_endpoint_parse("host:abc", &endpoint, &error) == TDX_ERR,
          "a non-numeric port must fail");
}

/* --- connect.cfg discovery -------------------------------------------- */

#define FIXTURE_DIR "tdx_endpoint_fixture"

/* connect.cfg is GBK.  The label below is 通达信 in GBK, which the loader must
 * decode rather than pass through raw. */
static const char *fixture_text =
    "[USER]\r\n"
    "UserName=\r\n"
    "SavePass=1\r\n"
    "[HQHOST]\r\n"
    "HostNum=3\r\n"
    "PrimaryHost=2\r\n"
    "HostName01=\xCD\xA8\xB4\xEF\xD0\xC5\r\n"
    "IPAddress01=10.0.0.1\r\n"
    "Port01=7709\r\n"
    "HostName02=second\r\n"
    "IPAddress02=10.0.0.2\r\n"
    "Port02=7710\r\n"
    "HostName03=third\r\n"
    "IPAddress03=10.0.0.3\r\n"
    "Port03=7711\r\n";

static int write_fixture(const char *text) {
    char path[256];
    FILE *file;
    snprintf(path, sizeof(path), "%s/connect.cfg", FIXTURE_DIR);
    file = fopen(path, "wb");
    if (!file)
        return -1;
    fwrite(text, 1, strlen(text), file);
    fclose(file);
    return 0;
}

static void remove_fixture(void) {
    char path[256];
    snprintf(path, sizeof(path), "%s/connect.cfg", FIXTURE_DIR);
    remove(path);
}

static void test_pool_load_parses_mixed_case_keys(void) {
    tdx_endpoint_pool pool;
    tdx_error error;
    char address[80];
    error.message[0] = '\0';

    test_mkdir(FIXTURE_DIR);
    if (write_fixture(fixture_text) != 0) {
        printf("skip: cannot write the connect.cfg fixture\n");
        return;
    }
    if (tdx_endpoint_pool_load(FIXTURE_DIR, TDX_ENDPOINT_POOL_MAX, &pool, &error) != TDX_OK) {
        CHECK(0, "pool load failed: %s", error.message);
        remove_fixture();
        return;
    }
    CHECK(pool.count == 3, "three hosts were declared, got %zu", pool.count);
    CHECK(pool.configured_count == 3, "configured %zu", pool.configured_count);
    CHECK(pool.primary_configured == 1, "PrimaryHost was present");
    CHECK(strcmp(pool.source, "connect.cfg:hqhost-primary-first") == 0, "source %s",
          pool.source);
    if (pool.count == 3) {
        /* PrimaryHost=2 puts the second declared host first, then wraps. */
        tdx_endpoint_address(&pool.items[0], address, sizeof(address));
        CHECK(strcmp(address, "10.0.0.3:7711") == 0, "primary-first order starts at %s",
              address);
        tdx_endpoint_address(&pool.items[1], address, sizeof(address));
        CHECK(strcmp(address, "10.0.0.1:7709") == 0, "second entry is %s", address);
        tdx_endpoint_address(&pool.items[2], address, sizeof(address));
        CHECK(strcmp(address, "10.0.0.2:7710") == 0, "third entry is %s", address);
        CHECK(strcmp(pool.items[1].name, "\xe9\x80\x9a\xe8\xbe\xbe\xe4\xbf\xa1") == 0,
              "the GBK label must decode, got '%s'", pool.items[1].name);
    }

    /* A smaller limit keeps the rotation and still starts at the primary. */
    CHECK(tdx_endpoint_pool_load(FIXTURE_DIR, 2, &pool, &error) == TDX_OK, "pool load: %s",
          error.message);
    CHECK(pool.count == 2, "the limit must be honoured, got %zu", pool.count);
    if (pool.count == 2) {
        tdx_endpoint_address(&pool.items[0], address, sizeof(address));
        CHECK(strcmp(address, "10.0.0.3:7711") == 0, "limited order starts at %s", address);
    }
    remove_fixture();
}

static void test_pool_load_falls_back(void) {
    tdx_endpoint_pool pool;
    tdx_error error;
    error.message[0] = '\0';

    remove_fixture();
    CHECK(tdx_endpoint_pool_load(FIXTURE_DIR, 4, &pool, &error) == TDX_OK,
          "a missing connect.cfg must not fail the load");
    CHECK(pool.count == 1, "the fallback is a single node, got %zu", pool.count);
    CHECK(strcmp(pool.source, "compiled-default") == 0, "source %s", pool.source);

    CHECK(tdx_endpoint_pool_load(NULL, 4, &pool, &error) == TDX_OK,
          "no root must not fail the load");
    CHECK(pool.count == 1 && strcmp(pool.source, "compiled-default") == 0,
          "an absent root falls back too");

    /* A file without HostNum is not usable either. */
    test_mkdir(FIXTURE_DIR);
    if (write_fixture("[HQHOST]\r\nIPAddress01=10.0.0.1\r\n") == 0) {
        CHECK(tdx_endpoint_pool_load(FIXTURE_DIR, 4, &pool, &error) == TDX_OK, "load");
        CHECK(pool.count == 1 && strcmp(pool.source, "compiled-default") == 0,
              "a file without HostNum must fall back, source %s", pool.source);
        remove_fixture();
    }
    /* HostNum declaring more hosts than the file carries is fine. */
    if (write_fixture("[HQHOST]\r\nHostNum=9\r\nIPAddress01=10.0.0.1\r\nPort01=7709\r\n") == 0) {
        CHECK(tdx_endpoint_pool_load(FIXTURE_DIR, 4, &pool, &error) == TDX_OK, "load");
        CHECK(pool.count == 1, "only the present host is usable, got %zu", pool.count);
        CHECK(strcmp(pool.source, "connect.cfg:hqhost-primary-first") == 0, "source %s",
              pool.source);
        remove_fixture();
    }

    CHECK(tdx_endpoint_pool_load(FIXTURE_DIR, 0, &pool, &error) == TDX_ERR,
          "a zero limit must be rejected");
    CHECK(tdx_endpoint_pool_load(FIXTURE_DIR, TDX_ENDPOINT_POOL_MAX + 1, &pool, &error) ==
              TDX_ERR,
          "a limit above the pool capacity must be rejected");
}

int main(void) {
    test_endpoint_parse();
    test_pool_load_parses_mixed_case_keys();
    test_pool_load_falls_back();

    if (failures) {
        printf("%d endpoint check(s) failed\n", failures);
        return 1;
    }
    printf("endpoint checks passed\n");
    return 0;
}
