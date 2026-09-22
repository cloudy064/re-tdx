/* Real loopback HTTP contracts, malformed input and bounded shutdown. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET test_socket;
#define BAD_SOCKET INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int test_socket;
#define BAD_SOCKET (-1)
#endif

#include "tdx_serve.h"
#include "tdx_thread.h"
#include "tdx_json.h"

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, m); failures++; } } while (0)

static void close_client(test_socket client) {
    if (client == BAD_SOCKET) return;
#if defined(_WIN32)
    closesocket(client);
#else
    close(client);
#endif
}

static int write_client(test_socket client, const char *text) {
    size_t sent = 0, size = strlen(text);
    while (sent < size) {
        int flags = 0;
        int written;
#if defined(MSG_NOSIGNAL)
        flags = MSG_NOSIGNAL;
#endif
        written = (int)send(client, text + sent, (int)(size - sent), flags);
        if (written <= 0) return 0;
        sent += (size_t)written;
    }
    return 1;
}

static test_socket connect_client(int port) {
    test_socket client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in address;
#if defined(_WIN32)
    DWORD timeout = 2000;
#else
    struct timeval timeout = {2, 0};
#endif
    if (client == BAD_SOCKET) return client;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons((uint16_t)port);
    if (connect(client, (struct sockaddr *)&address, sizeof(address)) != 0) {
        close_client(client);
        return BAD_SOCKET;
    }
    return client;
}

static size_t read_response(test_socket client, char *response, size_t capacity) {
    size_t used = 0;
    while (used + 1 < capacity) {
        int got = (int)recv(client, response + used, (int)(capacity - used - 1), 0);
        if (got <= 0) break;
        used += (size_t)got;
    }
    response[used] = '\0';
    return used;
}

static int request(int port, const char *text, char *response, size_t capacity) {
    test_socket client = connect_client(port);
    if (client == BAD_SOCKET) return 0;
    if (!write_client(client, text)) { close_client(client); return 0; }
    read_response(client, response, capacity);
    close_client(client);
    return 1;
}

static int fetch(void *context, const size_t *indices, size_t count,
                 tdx_depth *out, size_t *batches, tdx_error *err) {
    const tdx_code *code = (const tdx_code *)context;
    size_t i;
    (void)indices; (void)err;
    for (i = 0; i < count; ++i) {
        memset(&out[i], 0, sizeof(out[i]));
        out[i].security = *code;
        out[i].last = 12.5;
    }
    *batches = 1;
    return TDX_OK;
}

static tdx_server *start_server(tdx_hub *hub, int read_timeout, size_t connections) {
    tdx_serve_options options;
    tdx_server *server = NULL;
    tdx_error error;
    tdx_serve_options_default(&options);
    options.port = 0;
    options.read_timeout_ms = read_timeout;
    options.write_timeout_ms = 300;
    options.max_connections = connections;
    CHECK(tdx_server_create(&server, hub, &options, &error) == TDX_OK, "server create");
    if (server && tdx_server_start(server, &error) != TDX_OK) {
        CHECK(0, "server start");
        tdx_server_destroy(server);
        return NULL;
    }
    return server;
}

static void test_routes_and_bad_requests(tdx_hub *hub) {
    tdx_server *server = start_server(hub, 200, 8);
    char response[8192];
    int port;
    test_socket client;
    tdx_error error;
    tdx_json_doc doc;
    char *body;
    if (!server) return;
    port = tdx_server_port(server);
    CHECK(request(port, "GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n", response, sizeof(response)), "health request");
    CHECK(strstr(response, "200 OK") && strstr(response, "ok\n"), "health contract");
    CHECK(request(port, "BAD\r\n\r\n", response, sizeof(response)), "malformed request");
    CHECK(strstr(response, "400 Bad Request"), "malformed request rejected without double free");
    client = connect_client(port);
    CHECK(client != BAD_SOCKET, "truncated connection");
    if (client != BAD_SOCKET) {
#if defined(_WIN32)
        shutdown(client, SD_SEND);
#else
        shutdown(client, SHUT_WR);
#endif
        read_response(client, response, sizeof(response));
        CHECK(strstr(response, "400 Bad Request"), "empty half-close safely rejected");
        close_client(client);
    }
    CHECK(request(port, "GET /status HTTP/1.1\r\n\r\n", response, sizeof(response)), "status after malformed clients");
    body = strstr(response, "\r\n\r\n");
    tdx_json_doc_init(&doc);
    CHECK(body && tdx_json_parse((const uint8_t *)(body + 4), strlen(body + 4), &doc, &error) == TDX_OK,
          "status contains complete JSON");
    CHECK(request(port, "GET /api/v1/market/snapshot?codes=sz000001 HTTP/1.1\r\n\r\n", response, sizeof(response)), "snapshot request");
    body = strstr(response, "\r\n\r\n");
    CHECK(strstr(response, "200 OK") && body &&
          tdx_json_parse((const uint8_t *)(body + 4), strlen(body + 4), &doc, &error) == TDX_OK,
          "snapshot contract contains complete JSON");
    CHECK(strstr(response, "observed_at_unix_ms") && strstr(response, "\"stale\":"),
          "HTTP snapshot exposes freshness");
    CHECK(request(port, "GET /api/v1/market/stream?max_events=oops HTTP/1.1\r\n\r\n", response, sizeof(response)), "invalid stream parameter");
    CHECK(strstr(response, "400 Bad Request"), "invalid numeric value is rejected");
    CHECK(request(port, "GET /api/v1/market/stream?codes=sz000001&max_events=1 HTTP/1.1\r\n\r\n", response, sizeof(response)), "SSE request");
    body = strstr(response, "data: ");
    if (body) {
        char *end = strstr(body + 6, "\n\n");
        CHECK(end && tdx_json_parse((const uint8_t *)(body + 6), (size_t)(end - body - 6), &doc, &error) == TDX_OK,
              "SSE sends one complete JSON record");
    } else CHECK(0, "SSE data received");
    tdx_json_doc_free(&doc);
    {
        tdx_server *occupied = NULL;
        tdx_serve_options options;
        tdx_serve_options_default(&options);
        options.port = port;
        CHECK(tdx_server_create(&occupied, hub, &options, &error) == TDX_ERR && !occupied,
              "occupied port creation fails cleanly");
        tdx_server_destroy(occupied);
        CHECK(request(port, "GET /health HTTP/1.1\r\n\r\n", response, sizeof(response)), "original server survives bind failure");
        CHECK(strstr(response, "200 OK"), "bind failure preserves original server");
    }
    client = connect_client(port);
    CHECK(client != BAD_SOCKET, "slow header client");
    if (client != BAD_SOCKET) {
        int64_t started = tdx_monotonic_ms();
        CHECK(write_client(client, "G"), "first partial byte");
        tdx_sleep_ms(80);
        CHECK(write_client(client, "E"), "second partial byte");
        read_response(client, response, sizeof(response));
        CHECK(strstr(response, "408 Request Timeout"), "partial header hits total deadline");
        CHECK(tdx_monotonic_ms() - started < 1500, "header deadline is bounded");
        close_client(client);
    }
    tdx_server_destroy(server);
}

static void stop_server(void *context) { tdx_server_stop((tdx_server *)context); }

static void test_admission_and_stop(tdx_hub *hub) {
    tdx_server *server = start_server(hub, 10000, 2);
    test_socket half, stream, excess;
    char response[4096];
    tdx_thread stop_threads[2] = {{0}, {0}};
    tdx_error error;
    int64_t started;
    int i;
    if (!server) return;
    half = connect_client(tdx_server_port(server));
    CHECK(half != BAD_SOCKET && write_client(half, "GET /status HTTP/1.1\r\n"), "half-open header admitted");
    tdx_sleep_ms(30);
    stream = connect_client(tdx_server_port(server));
    CHECK(stream != BAD_SOCKET && write_client(stream, "GET /api/v1/market/stream HTTP/1.1\r\n\r\n"), "stream admitted");
    if (stream != BAD_SOCKET) {
        int got = (int)recv(stream, response, sizeof(response) - 1, 0);
        if (got > 0) response[got] = '\0'; else response[0] = '\0';
        CHECK(strstr(response, "200 OK"), "stream handler entered before stopping");
    }
    excess = connect_client(tdx_server_port(server));
    CHECK(excess != BAD_SOCKET, "excess TCP connection reaches admission gate");
    if (excess != BAD_SOCKET) {
        int got = (int)recv(excess, response, sizeof(response), 0);
        CHECK(got <= 0, "handler limit rejects excess idle connection");
        close_client(excess);
    }
    started = tdx_monotonic_ms();
    for (i = 0; i < 2; ++i)
        CHECK(tdx_thread_start(&stop_threads[i], stop_server, server, &error) == TDX_OK, "concurrent server stop");
    for (i = 0; i < 2; ++i) tdx_thread_join(&stop_threads[i]);
    CHECK(tdx_monotonic_ms() - started < 1500, "stop drains half-open header and idle SSE promptly");
    close_client(half);
    close_client(stream);
    CHECK(!tdx_hub_is_stopping(hub), "instance stop preserves borrowed hub");
    CHECK(tdx_server_start(server, &error) == TDX_ERR, "stopped server rejects restart");
    tdx_server_destroy(server);
}

int main(void) {
    tdx_hub *hub = NULL;
    tdx_code code = {0};
    tdx_hub_options options;
    tdx_error error;
    strcpy(code.code, "000001");
    tdx_hub_options_default(&options);
    options.interval_ms = 20;
    options.heartbeat_ms = 0;
    CHECK(tdx_hub_create(&hub, &code, 1, &options, fetch, &code, &error) == TDX_OK, "HTTP hub create");
    if (!hub) return 1;
    CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "initial HTTP snapshot");
    CHECK(tdx_hub_start(hub, &error) == TDX_OK, "HTTP resident poller");
    test_routes_and_bad_requests(hub);
    test_admission_and_stop(hub);
    tdx_hub_destroy(hub);
    if (failures) printf("%d HTTP check(s) failed\n", failures);
    else printf("HTTP checks passed\n");
    return failures ? 1 : 0;
}
