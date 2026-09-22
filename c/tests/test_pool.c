/* test_pool.c - deterministic sweep tests against a loopback fake 7709 server.
 *
 * The fake peer speaks just enough of the wire protocol to serve the 0x000D
 * handshake and 0x0547 depth replies, which lets us verify batch partitioning,
 * record ordering, truncation detection and reconnect without touching the
 * public network. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET test_socket;
#define TEST_INVALID INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int test_socket;
#define TEST_INVALID (-1)
#endif

#include "tdx_frame.h"
#include "tdx_pool.h"
#include "tdx_thread.h"

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

#define FAKE_MAX_CODES TDX_DEPTH_BATCH_MAX

typedef struct fake_server {
    int port;
    test_socket listener;
    /* Respond with this many records fewer than requested (0 = exact). */
    int shortfall;
    /* Drop this many 0x0547 connections before answering, to exercise retries. */
    int drop_depth_requests;
    int connections;
    int depth_requests;
    int shortfall_served;
    /* The pool keeps sessions open, so each accepted connection is
     * served on its own thread; the counters need a lock. */
    tdx_mutex lock;
    tdx_cond cond;
    size_t active_connections;
} fake_server;

static void close_socket(test_socket handle) {
    if (handle == TEST_INVALID)
        return;
#ifdef _WIN32
    closesocket(handle);
#else
    close(handle);
#endif
}

static int send_all(test_socket handle, const uint8_t *data, size_t size) {
    size_t sent = 0;
    while (sent < size) {
        int flags = 0;
#if defined(MSG_NOSIGNAL)
        flags = MSG_NOSIGNAL;
#endif
        int written = (int)send(handle, (const char *)(data + sent), (int)(size - sent), flags);
        if (written <= 0)
            return -1;
        sent += (size_t)written;
    }
    return 0;
}

static int recv_exact(test_socket handle, uint8_t *data, size_t size) {
    size_t got = 0;
    while (got < size) {
        int count = (int)recv(handle, (char *)(data + got), (int)(size - got), 0);
        if (count <= 0)
            return -1;
        got += (size_t)count;
    }
    return 0;
}

static void buf_push_varint(uint8_t *buffer, size_t *length, int64_t value) {
    int negative = value < 0;
    uint64_t magnitude = negative ? (uint64_t)(-value) : (uint64_t)value;
    uint8_t first = (uint8_t)(magnitude & 0x3Fu);
    uint64_t remaining = magnitude >> 6;
    if (negative)
        first |= 0x40u;
    if (remaining)
        first |= 0x80u;
    buffer[(*length)++] = first;
    while (remaining) {
        uint8_t byte = (uint8_t)(remaining & 0x7Fu);
        remaining >>= 7;
        if (remaining)
            byte |= 0x80u;
        buffer[(*length)++] = byte;
    }
}

static void buf_push_u32le(uint8_t *buffer, size_t *length, uint32_t value) {
    buffer[(*length)++] = (uint8_t)(value & 0xFFu);
    buffer[(*length)++] = (uint8_t)((value >> 8) & 0xFFu);
    buffer[(*length)++] = (uint8_t)((value >> 16) & 0xFFu);
    buffer[(*length)++] = (uint8_t)((value >> 24) & 0xFFu);
}

/* One minimal but complete depth record: last price 10.00, five flat levels. */
static size_t build_depth_record(uint8_t *out, int market, const char *code) {
    size_t length = 0;
    size_t level;
    out[length++] = (uint8_t)market;
    memcpy(out + length, code, 6);
    length += 6;
    out[length++] = 0x90;
    out[length++] = 0x06;
    /* prices: current 1000 -> 10.00 with divisor 1 and the x1000 scale */
    buf_push_varint(out, &length, 1000); /* current */
    buf_push_varint(out, &length, 0);    /* previous */
    buf_push_varint(out, &length, 0);    /* open */
    buf_push_varint(out, &length, 0);    /* high */
    buf_push_varint(out, &length, 0);    /* low */
    buf_push_u32le(out, &length, 120000); /* update time */
    buf_push_varint(out, &length, 0);      /* status */
    buf_push_varint(out, &length, 12345);  /* total hand */
    buf_push_varint(out, &length, 7);      /* current hand */
    buf_push_u32le(out, &length, 0);       /* amount */
    buf_push_varint(out, &length, 11);     /* inside */
    buf_push_varint(out, &length, 22);     /* outside */
    buf_push_varint(out, &length, 0);      /* after outer */
    buf_push_varint(out, &length, 33);     /* open amount */
    for (level = 0; level < TDX_DEPTH_LEVELS; ++level) {
        buf_push_varint(out, &length, -(int64_t)level); /* buy delta */
        buf_push_varint(out, &length, (int64_t)level);  /* sell delta */
        buf_push_varint(out, &length, 100 + (int64_t)level); /* buy volume */
        buf_push_varint(out, &length, 200 + (int64_t)level); /* sell volume */
    }
    return length;
}

static int send_response(test_socket client, uint32_t message_id, uint16_t message_type,
                         const uint8_t *body, size_t body_size) {
    uint8_t header[16];
    header[0] = 0xB1;
    header[1] = 0xCB;
    header[2] = 0x74;
    header[3] = 0x00;
    header[4] = 1;
    header[5] = (uint8_t)(message_id & 0xFFu);
    header[6] = (uint8_t)((message_id >> 8) & 0xFFu);
    header[7] = (uint8_t)((message_id >> 16) & 0xFFu);
    header[8] = (uint8_t)((message_id >> 24) & 0xFFu);
    header[9] = 0;
    header[10] = (uint8_t)(message_type & 0xFFu);
    header[11] = (uint8_t)((message_type >> 8) & 0xFFu);
    header[12] = (uint8_t)(body_size & 0xFFu);
    header[13] = (uint8_t)((body_size >> 8) & 0xFFu);
    header[14] = header[12];
    header[15] = header[13];
    if (send_all(client, header, sizeof(header)) != 0)
        return -1;
    if (body_size > 0 && send_all(client, body, body_size) != 0)
        return -1;
    return 0;
}

static void serve_connection(fake_server *server, test_socket client) {
    for (;;) {
        uint8_t header[12];
        uint8_t body[4096];
        uint16_t body_size;
        uint16_t message_type;
        uint32_t message_id;
        if (recv_exact(client, header, sizeof(header)) != 0)
            return;
        message_id = (uint32_t)header[1] | ((uint32_t)header[2] << 8) |
                     ((uint32_t)header[3] << 16) | ((uint32_t)header[4] << 24);
        body_size = (uint16_t)(header[6] | (header[7] << 8));
        message_type = (uint16_t)(header[10] | (header[11] << 8));
        if (body_size < 2)
            return;
        body_size = (uint16_t)(body_size - 2);
        if (body_size > sizeof(body))
            return;
        if (body_size > 0 && recv_exact(client, body, body_size) != 0)
            return;

        if (message_type == TDX_HANDSHAKE_TYPE) {
            uint8_t handshake[200];
            memset(handshake, 0, sizeof(handshake));
            handshake[0] = 1;
            memcpy(handshake + 68, "FAKE-TDX-NODE", 13);
            if (send_response(client, message_id, message_type, handshake,
                              sizeof(handshake)) != 0)
                return;
            continue;
        }
        if (message_type == TDX_CMD_DEPTH) {
            uint8_t payload[2 + FAKE_MAX_CODES * 200];
            size_t payload_size = 0;
            uint16_t requested;
            uint16_t served;
            uint16_t index;
            int drop = 0;
            int shortfall;
            tdx_mutex_lock(&server->lock);
            server->depth_requests++;
            drop = server->depth_requests <= server->drop_depth_requests;
            shortfall = server->shortfall;
            tdx_mutex_unlock(&server->lock);
            if (drop)
                return; /* hang up to exercise reconnect */
            requested = (uint16_t)(body[0] | (body[1] << 8));
            served = requested;
            if (shortfall > 0 && (int)served > shortfall) {
                served = (uint16_t)(served - shortfall);
                tdx_mutex_lock(&server->lock);
                server->shortfall_served++;
                tdx_mutex_unlock(&server->lock);
            }
            payload[payload_size++] = (uint8_t)(served & 0xFFu);
            payload[payload_size++] = (uint8_t)((served >> 8) & 0xFFu);
            for (index = 0; index < served; ++index) {
                const uint8_t *entry = body + 2 + (size_t)index * 11;
                payload_size += build_depth_record(payload + payload_size, entry[0],
                                                   (const char *)(entry + 1));
            }
            for (index = 0; index < payload_size; ++index)
                payload[index] ^= 0x93u;
            if (send_response(client, message_id, message_type, payload, payload_size) != 0)
                return;
            continue;
        }
        return; /* unsupported command */
    }
}

typedef struct fake_connection {
    fake_server *server;
    test_socket client;
} fake_connection;

static void fake_connection_main(void *context) {
    fake_connection *connection = (fake_connection *)context;
    serve_connection(connection->server, connection->client);
    close_socket(connection->client);
    tdx_mutex_lock(&connection->server->lock);
    connection->server->active_connections--;
    tdx_cond_broadcast(&connection->server->cond);
    tdx_mutex_unlock(&connection->server->lock);
    free(connection);
}

/* One thread per accepted peer: a pool holds its sessions open, so a serial
 * accept loop would starve every worker but the first. */
static void fake_server_main(void *context) {
    fake_server *server = (fake_server *)context;
    for (;;) {
        test_socket client = accept(server->listener, NULL, NULL);
        fake_connection *connection;
        tdx_thread thread;
        tdx_error error;
        if (client == TEST_INVALID)
            return;
        tdx_mutex_lock(&server->lock);
        server->connections++;
        tdx_mutex_unlock(&server->lock);
        connection = (fake_connection *)calloc(1, sizeof(*connection));
        if (!connection) {
            close_socket(client);
            continue;
        }
        connection->server = server;
        connection->client = client;
        tdx_mutex_lock(&server->lock);
        server->active_connections++;
        tdx_mutex_unlock(&server->lock);
        error.message[0] = '\0';
        if (tdx_thread_start(&thread, fake_connection_main, connection, &error) != TDX_OK) {
            close_socket(client);
            free(connection);
            tdx_mutex_lock(&server->lock);
            server->active_connections--;
            tdx_mutex_unlock(&server->lock);
            continue;
        }
        tdx_thread_detach(&thread);
    }
}

static int start_fake_server(fake_server *server, tdx_thread *thread, tdx_error *err) {
    struct sockaddr_in address;
#if defined(_WIN32)
    int address_size = (int)sizeof(address);
#else
    socklen_t address_size = sizeof(address);
#endif
    memset(server, 0, sizeof(*server));
    if (tdx_mutex_init(&server->lock, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_cond_init(&server->cond, err) != TDX_OK) {
        tdx_mutex_destroy(&server->lock);
        return TDX_ERR;
    }
    server->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server->listener == TEST_INVALID) {
        tdx_error_set(err, "cannot create the fake server socket");
        return TDX_ERR;
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(server->listener, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(server->listener, 8) != 0) {
        tdx_error_set(err, "cannot bind the fake server socket");
        close_socket(server->listener);
        return TDX_ERR;
    }
    if (getsockname(server->listener, (struct sockaddr *)&address, &address_size) != 0) {
        tdx_error_set(err, "cannot read the fake server port");
        close_socket(server->listener);
        return TDX_ERR;
    }
    server->port = (int)ntohs(address.sin_port);
    if (tdx_thread_start(thread, fake_server_main, server, err) != TDX_OK) {
        close_socket(server->listener);
        return TDX_ERR;
    }
    return TDX_OK;
}

static void stop_fake_server(fake_server *server, tdx_thread *thread) {
    /* Stop accepting first so no new connection takes the lock we are
     * about to destroy. */
#if defined(_WIN32)
    shutdown(server->listener, SD_BOTH);
#else
    shutdown(server->listener, SHUT_RDWR);
#endif
    close_socket(server->listener);
    tdx_thread_join(thread);
    server->listener = TEST_INVALID;
    tdx_mutex_lock(&server->lock);
    while (server->active_connections)
        tdx_cond_wait(&server->cond, &server->lock, 1000);
    tdx_mutex_unlock(&server->lock);
    tdx_cond_destroy(&server->cond);
    tdx_mutex_destroy(&server->lock);
}

static int server_port(const fake_server *server) { return server->port; }

/* ------------------------------------------------------------------ */

static void fill_codes(tdx_code *codes, size_t count) {
    size_t index;
    for (index = 0; index < count; ++index) {
        codes[index].market_id = (int)(index % 2);
        snprintf(codes[index].code, sizeof(codes[index].code), "%06u",
                 (unsigned)(100000 + index));
    }
}

static void test_sweep_single_worker(void) {
    fake_server server;
    tdx_thread thread;
    tdx_endpoint_pool pool;
    tdx_sweep_options options;
    tdx_sweep_stats stats;
    tdx_code codes[25];
    tdx_depth *records;
    tdx_error error;

    error.message[0] = '\0';
    if (start_fake_server(&server, &thread, &error) != TDX_OK) {
        printf("FAIL cannot start the fake server: %s\n", error.message);
        failures++;
        return;
    }
    memset(&pool, 0, sizeof(pool));
    snprintf(pool.items[0].host, sizeof(pool.items[0].host), "%s", "127.0.0.1");
    pool.items[0].port = (uint16_t)server_port(&server);
    pool.count = 1;
    tdx_sweep_options_default(&options);
    options.connections = 1;
    options.batch_size = 10;

    records = (tdx_depth *)calloc(25, sizeof(*records));
    fill_codes(codes, 25);
    CHECK(tdx_sweep(codes, 25, &pool, &options, records, 25, &stats, &error) == TDX_OK,
          "sweep failed: %s", error.message);
    CHECK(stats.records == 25, "sweep reported %zu records", stats.records);
    CHECK(stats.batches == 3, "sweep used %zu batches, expected 3", stats.batches);
    CHECK(stats.failed_batches == 0, "sweep reported %zu failed batches",
          stats.failed_batches);
    CHECK(stats.connections_opened == 1, "sweep opened %zu connections",
          stats.connections_opened);
    CHECK(stats.upstream_requests == 3, "sweep sent %zu requests",
          stats.upstream_requests);
    {
        size_t index;
        int order_ok = 1;
        for (index = 0; index < 25; ++index) {
            if (strcmp(records[index].security.code, codes[index].code) != 0)
                order_ok = 0;
        }
        CHECK(order_ok, "records did not stay in universe order");
    }
    CHECK(records[0].outside == 22, "decoded outside is %lld",
          (long long)records[0].outside);
    CHECK(records[24].sells[4].volume_hand == 204, "decoded sell5 volume is %lld",
          (long long)records[24].sells[4].volume_hand);

    free(records);
    stop_fake_server(&server, &thread);
}

static void test_sweep_parallel_workers(void) {
    fake_server server;
    tdx_thread thread;
    tdx_endpoint_pool pool;
    tdx_sweep_options options;
    tdx_sweep_stats stats;
    tdx_code codes[200];
    tdx_depth *records;
    tdx_error error;
    size_t index;
    int order_ok = 1;

    error.message[0] = '\0';
    if (start_fake_server(&server, &thread, &error) != TDX_OK) {
        printf("FAIL cannot start the fake server: %s\n", error.message);
        failures++;
        return;
    }
    memset(&pool, 0, sizeof(pool));
    snprintf(pool.items[0].host, sizeof(pool.items[0].host), "%s", "127.0.0.1");
    pool.items[0].port = (uint16_t)server_port(&server);
    pool.count = 1;
    tdx_sweep_options_default(&options);
    options.connections = 4;
    options.batch_size = 32;

    records = (tdx_depth *)calloc(200, sizeof(*records));
    fill_codes(codes, 200);
    CHECK(tdx_sweep(codes, 200, &pool, &options, records, 200, &stats, &error) == TDX_OK,
          "parallel sweep failed: %s", error.message);
    CHECK(stats.records == 200, "parallel sweep reported %zu records", stats.records);
    CHECK(stats.batches == 7, "parallel sweep used %zu batches", stats.batches);
    CHECK(stats.connections_opened == 4, "parallel sweep opened %zu connections",
          stats.connections_opened);
    for (index = 0; index < 200; ++index)
        if (strcmp(records[index].security.code, codes[index].code) != 0)
            order_ok = 0;
    CHECK(order_ok, "parallel records did not stay in universe order");
    CHECK(server.connections == 4, "fake server saw %d connections",
          server.connections);

    free(records);
    stop_fake_server(&server, &thread);
}

static void test_sweep_detects_short_response(void) {
    fake_server server;
    tdx_thread thread;
    tdx_endpoint_pool pool;
    tdx_sweep_options options;
    tdx_sweep_stats stats;
    tdx_code codes[20];
    tdx_depth *records;
    tdx_error error;

    error.message[0] = '\0';
    if (start_fake_server(&server, &thread, &error) != TDX_OK) {
        printf("FAIL cannot start the fake server: %s\n", error.message);
        failures++;
        return;
    }
    server.shortfall = 1;
    memset(&pool, 0, sizeof(pool));
    snprintf(pool.items[0].host, sizeof(pool.items[0].host), "%s", "127.0.0.1");
    pool.items[0].port = (uint16_t)server_port(&server);
    pool.count = 1;
    tdx_sweep_options_default(&options);
    options.connections = 1;
    options.batch_size = 20;
    options.max_attempts = 1;

    records = (tdx_depth *)calloc(20, sizeof(*records));
    fill_codes(codes, 20);
    CHECK(tdx_sweep(codes, 20, &pool, &options, records, 20, &stats, &error) == TDX_ERR,
          "a truncated response must fail the sweep");
    CHECK(stats.records < 20, "truncated sweep still reported %zu records",
          stats.records);
    CHECK(stats.failed_batches >= 1, "truncated sweep reported no failed batch");

    free(records);
    stop_fake_server(&server, &thread);
}

static void test_sweep_recovers_from_dropped_connection(void) {
    fake_server server;
    tdx_thread thread;
    tdx_endpoint_pool pool;
    tdx_sweep_options options;
    tdx_sweep_stats stats;
    tdx_code codes[10];
    tdx_depth *records;
    tdx_error error;

    error.message[0] = '\0';
    if (start_fake_server(&server, &thread, &error) != TDX_OK) {
        printf("FAIL cannot start the fake server: %s\n", error.message);
        failures++;
        return;
    }
    server.drop_depth_requests = 1; /* first attempt is dropped */
    memset(&pool, 0, sizeof(pool));
    snprintf(pool.items[0].host, sizeof(pool.items[0].host), "%s", "127.0.0.1");
    pool.items[0].port = (uint16_t)server_port(&server);
    pool.count = 1;
    tdx_sweep_options_default(&options);
    options.connections = 1;
    options.batch_size = 10;
    options.max_attempts = 3;

    records = (tdx_depth *)calloc(10, sizeof(*records));
    fill_codes(codes, 10);
    CHECK(tdx_sweep(codes, 10, &pool, &options, records, 10, &stats, &error) == TDX_OK,
          "sweep should recover from one dropped request: %s", error.message);
    CHECK(stats.retries >= 1, "sweep reported %zu retries", stats.retries);
    CHECK(stats.connections_opened >= 2, "sweep opened %zu connections",
          stats.connections_opened);

    free(records);
    stop_fake_server(&server, &thread);
}

static void test_sweep_argument_validation(void) {
    tdx_endpoint_pool pool;
    tdx_sweep_options options;
    tdx_sweep_stats stats;
    tdx_code codes[4];
    tdx_depth records[4];
    tdx_error error;

    error.message[0] = '\0';
    memset(&pool, 0, sizeof(pool));
    snprintf(pool.items[0].host, sizeof(pool.items[0].host), "%s", "127.0.0.1");
    pool.items[0].port = 7709;
    pool.count = 1;
    tdx_sweep_options_default(&options);
    fill_codes(codes, 4);

    options.connections = 0;
    CHECK(tdx_sweep(codes, 4, &pool, &options, records, 4, &stats, &error) == TDX_ERR,
          "zero connections must be rejected");
    options.connections = 4;
    options.batch_size = 101;
    CHECK(tdx_sweep(codes, 4, &pool, &options, records, 4, &stats, &error) == TDX_ERR,
          "an oversized batch must be rejected");
    options.batch_size = 4;
    CHECK(tdx_sweep(codes, 4, &pool, &options, records, 3, &stats, &error) == TDX_ERR,
          "an undersized output buffer must be rejected");
    pool.count = 0;
    CHECK(tdx_sweep(codes, 4, &pool, &options, records, 4, &stats, &error) == TDX_ERR,
          "an empty endpoint pool must be rejected");
}

static void test_persistent_pool_reuse_and_recovery(void) {
    fake_server server;
    tdx_thread thread = {0};
    tdx_endpoint_pool endpoints = {0};
    tdx_sweep_options options;
    tdx_sweep_stats stats;
    tdx_pool *pool = NULL;
    tdx_code codes[10];
    tdx_depth records[10];
    tdx_error error;
    if (start_fake_server(&server, &thread, &error) != TDX_OK) {
        CHECK(0, "persistent fake server: %s", error.message);
        return;
    }
    strcpy(endpoints.items[0].host, "127.0.0.1");
    endpoints.items[0].port = (uint16_t)server.port;
    endpoints.count = 1;
    tdx_sweep_options_default(&options);
    options.connections = 1;
    options.batch_size = 10;
    options.max_attempts = 1;
    options.timeout_ms = 1000;
    fill_codes(codes, 10);
    CHECK(tdx_pool_create(&pool, &endpoints, &options, &error) == TDX_OK,
          "persistent pool creation: %s", error.message);
    if (pool) {
        CHECK(tdx_pool_run(pool, codes, 10, records, 10, &stats, &error) == TDX_OK,
              "first resident round: %s", error.message);
        CHECK(stats.connections_opened == 1, "first round opens one session");
        CHECK(tdx_pool_run(pool, codes, 10, records, 10, &stats, &error) == TDX_OK,
              "second resident round: %s", error.message);
        CHECK(stats.connections_opened == 0 && stats.records == 10,
              "second round reuses the existing session and resets counters");
        tdx_mutex_lock(&server.lock);
        server.shortfall = 1;
        tdx_mutex_unlock(&server.lock);
        CHECK(tdx_pool_run(pool, codes, 10, records, 10, &stats, &error) == TDX_ERR,
              "resident short round is rejected");
        CHECK(stats.failed_batches == 1, "failed resident round reports one batch");
        tdx_mutex_lock(&server.lock);
        server.shortfall = 0;
        tdx_mutex_unlock(&server.lock);
        CHECK(tdx_pool_run(pool, codes, 10, records, 10, &stats, &error) == TDX_OK,
              "resident pool recovers after failure: %s", error.message);
        CHECK(stats.connections_opened == 1 && stats.records == 10,
              "recovery opens a clean session and returns all records");
        CHECK(tdx_pool_run(pool, codes, 10, records, 10, &stats, &error) == TDX_OK &&
              stats.connections_opened == 0 && stats.failed_batches == 0,
              "recovered session is reusable in the next generation");
        tdx_pool_destroy(pool);
    }
    stop_fake_server(&server, &thread);
}

int main(void) {
#ifdef _WIN32
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
#endif
    test_sweep_argument_validation();
    test_sweep_single_worker();
    test_sweep_parallel_workers();
    test_sweep_detects_short_response();
    test_sweep_recovers_from_dropped_connection();
    test_persistent_pool_reuse_and_recovery();
    if (failures) {
        printf("%d pool check(s) failed\n", failures);
        return 1;
    }
    printf("pool checks passed\n");
    return 0;
}
