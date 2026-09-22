/* tdx_serve.c - bounded HTTP handlers with one server-owned lifetime. */
#include "tdx_serve.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "tdx_format.h"
#include "tdx_state.h"
#include "tdx_thread.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET serve_socket;
#define SERVE_INVALID INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
typedef int serve_socket;
#define SERVE_INVALID (-1)
#endif

#define SERVE_REQUEST_MAX 8192
#define SERVE_CODES_MAX 4096

typedef struct serve_connection {
    serve_socket client;
    tdx_hub *hub;
    struct tdx_server *server;
    tdx_thread thread;
    int active;
    int done;
} serve_connection;

struct tdx_server {
    tdx_hub *hub;
    tdx_serve_options options;
    serve_socket listener;
    serve_connection *connections;
    tdx_thread accept_thread;
    tdx_mutex lock;
    tdx_cond cond;
    int started;
    int stopping;
    int joining;
    int stopped;
    int failed;
    int port;
#if defined(_WIN32)
    int winsock_started;
#endif
};

void tdx_serve_options_default(tdx_serve_options *options) {
    if (!options)
        return;
    options->port = 8790;
    options->backlog = 32;
    options->write_timeout_ms = 30000;
    options->read_timeout_ms = 10000;
    options->max_connections = 128;
}

static void close_socket(serve_socket handle) {
    if (handle == SERVE_INVALID)
        return;
#if defined(_WIN32)
    closesocket(handle);
#else
    close(handle);
#endif
}

/* Process signal handling belongs only to the compatibility wrapper. */
static volatile sig_atomic_t g_stop_requested = 0;

static void serve_on_signal(int signal_number) {
    (void)signal_number;
    g_stop_requested = 1;
}

static int server_stopping(tdx_server *server) {
    int stopping;
    tdx_mutex_lock(&server->lock);
    stopping = server->stopping;
    tdx_mutex_unlock(&server->lock);
    return stopping;
}

static void shutdown_socket(serve_socket handle) {
    if (handle == SERVE_INVALID)
        return;
#if defined(_WIN32)
    shutdown(handle, SD_BOTH);
#else
    shutdown(handle, SHUT_RDWR);
#endif
}

static int interrupted(void) {
#if defined(_WIN32)
    return WSAGetLastError() == WSAEINTR;
#else
    return errno == EINTR;
#endif
}

static int would_block(void) {
#if defined(_WIN32)
    return WSAGetLastError() == WSAEWOULDBLOCK;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

static int set_blocking(serve_socket handle, int blocking) {
#if defined(_WIN32)
    u_long mode = blocking ? 0 : 1;
    return ioctlsocket(handle, FIONBIO, &mode);
#else
    int flags = fcntl(handle, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(handle, F_SETFL, blocking ? flags & ~O_NONBLOCK : flags | O_NONBLOCK);
#endif
}

static int wait_readable(serve_socket handle, int timeout_ms) {
    fd_set read_set;
    struct timeval timeout;
    FD_ZERO(&read_set);
    FD_SET(handle, &read_set);
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    return select((int)handle + 1, &read_set, NULL, NULL, &timeout);
}

static int send_all(serve_socket handle, const char *data, size_t size) {
    size_t sent = 0;
    while (sent < size) {
        size_t remaining = size - sent;
        int chunk = remaining > INT_MAX ? INT_MAX : (int)remaining;
        int flags = 0;
        int written;
#if defined(MSG_NOSIGNAL)
        flags = MSG_NOSIGNAL;
#endif
        written = (int)send(handle, data + sent, chunk, flags);
        if (written < 0 && interrupted())
            continue;
        if (written <= 0)
            return -1;
        sent += (size_t)written;
    }
    return 0;
}

static int send_text(serve_socket handle, const char *text) {
    return send_all(handle, text, strlen(text));
}

static void send_headers(serve_socket handle, int status, const char *reason,
                         const char *content_type, size_t body_size) {
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
             "Access-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n",
             status, reason, content_type, body_size);
    (void)send_text(handle, header);
}

static void send_json(serve_socket handle, int status, const char *reason,
                      const tdx_buf *body) {
    send_headers(handle, status, reason, "application/json; charset=utf-8", body->len);
    if (body->len)
        (void)send_all(handle, (const char *)body->data, body->len);
}

static void send_plain(serve_socket handle, int status, const char *reason,
                       const char *text) {
    send_headers(handle, status, reason, "text/plain; charset=utf-8", strlen(text));
    (void)send_text(handle, text);
}

/* ------------------------------------------------------------------ */
/* request parsing                                                     */
/* ------------------------------------------------------------------ */

static const char *query_value(const char *query, const char *name, char *out,
                               size_t out_size) {
    const size_t name_length = strlen(name);
    const char *cursor = query;
    out[0] = '\0';
    while (cursor && *cursor) {
        const char *equals;
        const char *end;
        size_t piece;
        if (strncmp(cursor, name, name_length) == 0 && cursor[name_length] == '=') {
            cursor += name_length + 1;
            end = strchr(cursor, '&');
            piece = end ? (size_t)(end - cursor) : strlen(cursor);
            if (piece >= out_size)
                piece = out_size - 1;
            memcpy(out, cursor, piece);
            out[piece] = '\0';
            return out;
        }
        equals = strchr(cursor, '&');
        if (!equals)
            break;
        cursor = equals + 1;
    }
    return NULL;
}

static int query_number(const char *query, const char *name, long *value) {
    char text[4096];
    char *end;
    long parsed;
    if (!query_value(query, name, text, sizeof(text)))
        return TDX_OK;
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end || parsed < 0)
        return TDX_ERR;
    *value = parsed;
    return TDX_OK;
}

/* Resolves a comma separated list of codes against the hub universe. */
static int resolve_codes(tdx_hub *hub, const char *text, tdx_code **out,
                         size_t *out_count, tdx_error *err) {
    tdx_code *codes;
    size_t count = 0;
    char *working;
    char *cursor;

    (void)hub;
    working = (char *)malloc(strlen(text) + 1);
    if (!working) {
        tdx_error_set(err, "out of memory for the code list");
        return TDX_ERR;
    }
    strcpy(working, text);
    codes = (tdx_code *)calloc(SERVE_CODES_MAX, sizeof(*codes));
    if (!codes) {
        free(working);
        tdx_error_set(err, "out of memory for %d codes", SERVE_CODES_MAX);
        return TDX_ERR;
    }
    cursor = working;
    for (;;) {
        char *comma = strchr(cursor, ',');
        char *piece;
        if (comma)
            *comma = '\0';
        piece = cursor;
        while (*piece == ' ' || *piece == '\t')
            piece++;
        if (*piece) {
            if (count >= SERVE_CODES_MAX) {
                free(codes);
                free(working);
                tdx_error_set(err, "at most %d codes per request", SERVE_CODES_MAX);
                return TDX_ERR;
            }
            if (tdx_code_parse(piece, &codes[count], err) != TDX_OK) {
                free(codes);
                free(working);
                return TDX_ERR;
            }
            count++;
        }
        if (!comma)
            break;
        cursor = comma + 1;
    }
    free(working);
    if (count == 0) {
        free(codes);
        tdx_error_set(err, "the codes parameter is empty");
        return TDX_ERR;
    }
    *out = codes;
    *out_count = count;
    return TDX_OK;
}

/* ------------------------------------------------------------------ */
/* routes                                                              */
/* ------------------------------------------------------------------ */

static void route_stream(serve_connection *connection, const char *query) {
    tdx_error error;
    tdx_buf event;
    uint64_t subscriber;
    char value[4096];
    long max_events = 0;
    long idle_timeout = 15000;
    long emitted = 0;
    int64_t idle_deadline;

    error.message[0] = '\0';
    if (query_number(query, "max_events", &max_events) != TDX_OK ||
        query_number(query, "wait_timeout_ms", &idle_timeout) != TDX_OK) {
        send_plain(connection->client, 400, "Bad Request", "invalid nonnegative integer parameter\n");
        return;
    }
    if (idle_timeout < 1000)
        idle_timeout = 1000;
    if (idle_timeout > 600000)
        idle_timeout = 600000;

    if (query_value(query, "codes", value, sizeof(value))) {
        tdx_code *codes = NULL;
        size_t count = 0;
        if (resolve_codes(connection->hub, value, &codes, &count, &error) != TDX_OK) {
            send_plain(connection->client, 400, "Bad Request", error.message);
            return;
        }
        subscriber = tdx_hub_subscribe_codes(connection->hub, codes, count, &error);
        free(codes);
    } else {
        subscriber = tdx_hub_subscribe_all(connection->hub, &error);
    }
    if (subscriber == 0) {
        send_plain(connection->client, 503, "Service Unavailable", error.message);
        return;
    }

    (void)send_text(connection->client,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/event-stream; charset=utf-8\r\n"
                    "Cache-Control: no-cache\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "X-Accel-Buffering: no\r\n"
                    "Connection: keep-alive\r\n\r\n"
                    "retry: 2000\n\n");

    tdx_buf_init(&event);
    idle_deadline = tdx_monotonic_ms() + idle_timeout;
    for (;;) {
        if (server_stopping(connection->server) || tdx_hub_is_stopping(connection->hub))
            break;
        if (tdx_hub_next(connection->hub, subscriber, &event, 100,
                          &error) != TDX_OK)
            break;
        if (event.len == 0) {
            if (tdx_monotonic_ms() >= idle_deadline) {
                if (send_text(connection->client, ": keep-alive\n\n") != 0)
                    break;
                idle_deadline = tdx_monotonic_ms() + idle_timeout;
            }
            continue;
        }
        idle_deadline = tdx_monotonic_ms() + idle_timeout;
        {
            char prefix[48];
            int prefix_length = snprintf(prefix, sizeof(prefix), "id: %ld\ndata: ",
                                         emitted + 1);
            if (send_all(connection->client, prefix, (size_t)prefix_length) != 0)
                break;
            if (send_all(connection->client, (const char *)event.data, event.len) != 0)
                break;
            if (send_text(connection->client, "\n\n") != 0)
                break;
        }
        emitted++;
        if (max_events > 0 && emitted >= max_events)
            break;
    }
    tdx_buf_free(&event);
    tdx_hub_unsubscribe(connection->hub, subscriber);
}

static void route_snapshot(serve_connection *connection, const char *query) {
    tdx_error error;
    tdx_buf body;
    char value[4096];

    error.message[0] = '\0';
    tdx_buf_init(&body);
    if (!query_value(query, "codes", value, sizeof(value))) {
        tdx_buf_append_printf(&body, &error,
                              "{\"error\":\"codes_required\",\"message\":"
                              "\"pass codes=sz000001,sh600000\"}");
        send_json(connection->client, 400, "Bad Request", &body);
        tdx_buf_free(&body);
        return;
    }
    {
        tdx_code *codes = NULL;
        size_t count = 0;
        if (resolve_codes(connection->hub, value, &codes, &count, &error) != TDX_OK) {
            send_plain(connection->client, 400, "Bad Request", error.message);
            return;
        }
        if (tdx_hub_snapshot_json(connection->hub, codes, count, &body, &error) !=
            TDX_OK) {
            free(codes);
            send_plain(connection->client, 500, "Internal Server Error", error.message);
            tdx_buf_free(&body);
            return;
        }
        free(codes);
    }
    send_json(connection->client, 200, "OK", &body);
    tdx_buf_free(&body);
}

static void route_status(serve_connection *connection) {
    tdx_buf body;
    tdx_error error;
    error.message[0] = '\0';
    tdx_buf_init(&body);
    if (tdx_hub_status_json(connection->hub, &body, &error) != TDX_OK) {
        send_plain(connection->client, 500, "Internal Server Error", error.message);
        tdx_buf_free(&body);
        return;
    }
    send_json(connection->client, 200, "OK", &body);
    tdx_buf_free(&body);
}

static void route_index(serve_connection *connection) {
    static const char page[] =
        "<!doctype html><meta charset=\"utf-8\"><title>tdx-l1stream</title>"
        "<h1>tdx-l1stream</h1><ul>"
        "<li><code>GET /health</code></li>"
        "<li><code>GET /status</code></li>"
        "<li><code>GET /api/v1/market/snapshot?codes=sz000001</code></li>"
        "<li><code>GET /api/v1/market/stream?codes=sz000001,sh600000</code> "
        "(SSE, change-only; omit codes for the whole universe)</li>"
        "</ul>";
    send_headers(connection->client, 200, "OK", "text/html; charset=utf-8",
                 strlen(page));
    (void)send_text(connection->client, page);
}

static void service_connection_inner(serve_connection *connection);

static void service_connection(void *context) {
    serve_connection *connection = (serve_connection *)context;
    service_connection_inner(connection);
    /* The server owns the slot and thread; this wrapper alone closes its socket. */
    tdx_mutex_lock(&connection->server->lock);
    close_socket(connection->client);
    connection->client = SERVE_INVALID;
    connection->done = 1;
    tdx_cond_broadcast(&connection->server->cond);
    tdx_mutex_unlock(&connection->server->lock);
}

static void service_connection_inner(serve_connection *connection) {
    char request[SERVE_REQUEST_MAX];
    size_t used = 0;
    int header_end = -1;
    char method[8];
    char path[512];
    char query[4096];
    int64_t deadline = tdx_monotonic_ms() + connection->server->options.read_timeout_ms;

    method[0] = '\0';
    path[0] = '\0';
    query[0] = '\0';

    /* One absolute deadline prevents a byte-at-a-time sender extending its life. */
    while (used < sizeof(request) - 1) {
        int64_t remaining = deadline - tdx_monotonic_ms();
        int ready;
        int count;
        if (server_stopping(connection->server))
            return;
        if (remaining <= 0) {
            send_plain(connection->client, 408, "Request Timeout", "header deadline exceeded\n");
            return;
        }
        /* Winsock shutdown need not wake a select already waiting on this
         * socket. Short waits recheck the server stop flag independently. */
        ready = wait_readable(connection->client,
                              remaining > 100 ? 100 : (int)remaining);
        if (ready < 0 && interrupted())
            continue;
        if (ready == 0)
            continue;
        if (ready < 0)
            return;
        count = (int)recv(connection->client, request + used,
                          (int)(sizeof(request) - 1 - used), 0);
        if (count < 0 && interrupted())
            continue;
        if (count <= 0)
            break;
        if (memchr(request + used, '\0', (size_t)count)) {
            send_plain(connection->client, 400, "Bad Request", "NUL in request\n");
            return;
        }
        used += (size_t)count;
        request[used] = '\0';
        {
            char *found = strstr(request, "\r\n\r\n");
            if (found) {
                header_end = (int)(found - request);
                break;
            }
        }
    }
    if (header_end < 0) {
        send_plain(connection->client, 400, "Bad Request", "malformed request\n");
        return;
    }

    {
        char *line_end = strstr(request, "\r\n");
        char *target;
        char *space;
        if (!line_end) {
            send_plain(connection->client, 400, "Bad Request", "malformed request\n");
            return;
        }
        *line_end = '\0';
        space = strchr(request, ' ');
        if (!space) {
            send_plain(connection->client, 400, "Bad Request", "malformed request\n");
            return;
        }
        *space = '\0';
        if (strlen(request) >= sizeof(method)) {
            send_plain(connection->client, 405, "Method Not Allowed", "GET only\n");
            return;
        }
        strcpy(method, request);
        target = space + 1;
        space = strchr(target, ' ');
        if (!space || (strcmp(space + 1, "HTTP/1.1") != 0 &&
                       strcmp(space + 1, "HTTP/1.0") != 0)) {
            send_plain(connection->client, 400, "Bad Request", "invalid request line\n");
            return;
        }
        *space = '\0';
        if (*target != '/') {
            send_plain(connection->client, 400, "Bad Request", "origin-form target required\n");
            return;
        }
        {
            char *mark = strchr(target, '?');
            if (mark) {
                *mark = '\0';
                if (strlen(mark + 1) >= sizeof(query)) {
                    send_plain(connection->client, 414, "URI Too Long", "query too long\n");
                    return;
                }
                strcpy(query, mark + 1);
            }
            if (strlen(target) >= sizeof(path)) {
                send_plain(connection->client, 414, "URI Too Long", "path too long\n");
                return;
            }
            strcpy(path, target);
        }
    }

    if (strcmp(method, "GET") != 0) {
        send_plain(connection->client, 405, "Method Not Allowed", "GET only\n");
    } else if (strcmp(path, "/") == 0) {
        route_index(connection);
    } else if (strcmp(path, "/health") == 0) {
        send_plain(connection->client, 200, "OK", "ok\n");
    } else if (strcmp(path, "/status") == 0) {
        route_status(connection);
    } else if (strcmp(path, "/api/v1/market/snapshot") == 0) {
        route_snapshot(connection, query);
    } else if (strcmp(path, "/api/v1/market/stream") == 0) {
        route_stream(connection, query);
    } else {
        send_plain(connection->client, 404, "Not Found", "unknown route\n");
    }

}

/* ------------------------------------------------------------------ */

static int set_write_timeout(serve_socket client, int milliseconds) {
#if defined(_WIN32)
    DWORD timeout = (DWORD)milliseconds;
    return setsockopt(client, SOL_SOCKET, SO_SNDTIMEO,
                      (const char *)&timeout, sizeof(timeout));
#else
    struct timeval timeout;
    timeout.tv_sec = milliseconds / 1000;
    timeout.tv_usec = (milliseconds % 1000) * 1000;
#if defined(SO_NOSIGPIPE)
    {
        int one = 1;
        if (setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one)) != 0)
            return -1;
    }
#endif
    return setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
}

/* Completed slots are joined before reuse. The active flag is set before
 * thread creation, so neither stop nor admission can miss a pending handler. */
static void server_accept_main(void *context) {
    tdx_server *server = (tdx_server *)context;
    for (;;) {
        serve_socket client;
        serve_connection *slot = NULL;
        size_t index;
        int ready;
        tdx_error error;

        tdx_mutex_lock(&server->lock);
        for (index = 0; index < server->options.max_connections; ++index) {
            serve_connection *candidate = &server->connections[index];
            if (candidate->active && candidate->done) {
                /* A done handler no longer needs the server lock. */
                tdx_thread_join(&candidate->thread);
                candidate->active = 0;
            }
        }
        if (server->stopping) {
            tdx_mutex_unlock(&server->lock);
            break;
        }
        tdx_mutex_unlock(&server->lock);
        ready = wait_readable(server->listener, 100);
        if (ready < 0 && interrupted())
            continue;
        if (ready == 0)
            continue;
        if (ready < 0) {
            tdx_mutex_lock(&server->lock);
            server->failed = 1;
            server->stopping = 1;
            tdx_mutex_unlock(&server->lock);
            break;
        }
        client = accept(server->listener, NULL, NULL);
        if (client == SERVE_INVALID) {
            if (interrupted() || would_block())
                continue;
            tdx_mutex_lock(&server->lock);
            server->failed = 1;
            server->stopping = 1;
            tdx_mutex_unlock(&server->lock);
            break;
        }
#if !defined(_WIN32)
        if (client >= FD_SETSIZE) {
            close_socket(client);
            continue;
        }
#endif
        if (set_blocking(client, 1) != 0 ||
            set_write_timeout(client, server->options.write_timeout_ms) != 0) {
            close_socket(client);
            continue;
        }
        tdx_mutex_lock(&server->lock);
        if (!server->stopping) {
            for (index = 0; index < server->options.max_connections; ++index)
                if (!server->connections[index].active) {
                    slot = &server->connections[index];
                    break;
                }
        }
        if (!slot) {
            tdx_mutex_unlock(&server->lock);
            /* Reject immediately without allocating another thread or blocking
             * the accept loop on a slow peer's error response. */
            close_socket(client);
            continue;
        }
        slot->client = client;
        slot->hub = server->hub;
        slot->server = server;
        slot->done = 0;
        slot->active = 1;
        if (tdx_thread_start(&slot->thread, service_connection, slot, &error) != TDX_OK) {
            close_socket(client);
            slot->client = SERVE_INVALID;
            slot->active = 0;
        }
        tdx_mutex_unlock(&server->lock);
    }
}

int tdx_server_create(tdx_server **out, tdx_hub *hub,
                      const tdx_serve_options *options, tdx_error *err) {
    tdx_serve_options defaults;
    tdx_server *server;
    struct sockaddr_in address;
    int reuse = 1;
#if defined(_WIN32)
    int address_size = (int)sizeof(address);
#else
    socklen_t address_size = sizeof(address);
#endif
    if (!out || !hub) {
        tdx_error_set(err, "server needs an output slot and a hub");
        return TDX_ERR;
    }
    *out = NULL;
    tdx_serve_options_default(&defaults);
    if (options) {
        defaults.port = options->port;
        defaults.backlog = options->backlog;
        defaults.write_timeout_ms = options->write_timeout_ms;
        if (options->read_timeout_ms)
            defaults.read_timeout_ms = options->read_timeout_ms;
        if (options->max_connections)
            defaults.max_connections = options->max_connections;
    }
    if (defaults.port < 0 || defaults.port > 65535 || defaults.backlog < 1 ||
        defaults.write_timeout_ms < 1 || defaults.read_timeout_ms < 1 ||
        defaults.read_timeout_ms > 600000 || defaults.max_connections > 4096) {
        tdx_error_set(err, "invalid server port, backlog, timeout or connection limit");
        return TDX_ERR;
    }
    server = (tdx_server *)calloc(1, sizeof(*server));
    if (!server) {
        tdx_error_set(err, "out of memory for the server");
        return TDX_ERR;
    }
    server->listener = SERVE_INVALID;
    server->hub = hub;
    server->options = defaults;
    if (tdx_mutex_init(&server->lock, err) != TDX_OK ||
        tdx_cond_init(&server->cond, err) != TDX_OK)
        goto failed;
    server->connections = (serve_connection *)calloc(defaults.max_connections,
                                                     sizeof(*server->connections));
    if (!server->connections) {
        tdx_error_set(err, "out of memory for connection slots");
        goto failed;
    }
#if defined(_WIN32)
    {
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            tdx_error_set(err, "cannot initialize Winsock");
            goto failed;
        }
        server->winsock_started = 1;
    }
#endif
    server->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server->listener == SERVE_INVALID) {
        tdx_error_set(err, "cannot create the listening socket");
        goto failed;
    }
#if !defined(_WIN32)
    if (server->listener >= FD_SETSIZE) {
        tdx_error_set(err, "listening descriptor exceeds select capacity");
        goto failed;
    }
#endif
#if defined(_WIN32)
    setsockopt(server->listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
#else
    setsockopt(server->listener, SOL_SOCKET, SO_REUSEADDR,
#endif
               (const char *)&reuse, sizeof(reuse));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons((uint16_t)defaults.port);
    if (bind(server->listener, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(server->listener, defaults.backlog) != 0 ||
        getsockname(server->listener, (struct sockaddr *)&address, &address_size) != 0 ||
        set_blocking(server->listener, 0) != 0) {
        tdx_error_set(err, "cannot bind 127.0.0.1:%d", defaults.port);
        goto failed;
    }
    server->port = (int)ntohs(address.sin_port);
    *out = server;
    return TDX_OK;

failed:
    tdx_server_destroy(server);
    return TDX_ERR;
}

int tdx_server_start(tdx_server *server, tdx_error *err) {
    if (!server) {
        tdx_error_set(err, "server is null");
        return TDX_ERR;
    }
    tdx_mutex_lock(&server->lock);
    if (server->stopping || server->stopped) {
        tdx_mutex_unlock(&server->lock);
        tdx_error_set(err, "server is stopped");
        return TDX_ERR;
    }
    if (!server->started) {
        if (tdx_thread_start(&server->accept_thread, server_accept_main, server, err) != TDX_OK) {
            tdx_mutex_unlock(&server->lock);
            return TDX_ERR;
        }
        server->started = 1;
    }
    tdx_mutex_unlock(&server->lock);
    return TDX_OK;
}

int tdx_server_port(const tdx_server *server) {
    return server ? server->port : 0;
}

void tdx_server_stop(tdx_server *server) {
    size_t index;
    if (!server)
        return;
    tdx_mutex_lock(&server->lock);
    while (server->joining)
        tdx_cond_wait(&server->cond, &server->lock, -1);
    if (server->stopped) {
        tdx_mutex_unlock(&server->lock);
        return;
    }
    server->joining = 1;
    server->stopping = 1;
    if (server->connections)
        for (index = 0; index < server->options.max_connections; ++index)
            if (server->connections[index].active)
                shutdown_socket(server->connections[index].client);
    tdx_mutex_unlock(&server->lock);
    if (server->started)
        tdx_thread_join(&server->accept_thread);
    /* No more admissions or slot reuse after the accept thread joins. */
    if (server->connections)
        for (index = 0; index < server->options.max_connections; ++index)
            if (server->connections[index].active)
                tdx_thread_join(&server->connections[index].thread);
    close_socket(server->listener);
    server->listener = SERVE_INVALID;
    tdx_mutex_lock(&server->lock);
    server->started = 0;
    server->stopped = 1;
    server->joining = 0;
    tdx_cond_broadcast(&server->cond);
    tdx_mutex_unlock(&server->lock);
}

void tdx_server_destroy(tdx_server *server) {
    if (!server)
        return;
    tdx_server_stop(server);
    free(server->connections);
    tdx_cond_destroy(&server->cond);
    tdx_mutex_destroy(&server->lock);
#if defined(_WIN32)
    if (server->winsock_started)
        WSACleanup();
#endif
    free(server);
}

int tdx_serve_run(tdx_hub *hub, const tdx_code *universe, size_t universe_size,
                  const tdx_serve_options *options, tdx_error *err) {
    tdx_server *server = NULL;
    void (*old_int)(int);
    void (*old_term)(int);
    int failed;
    (void)universe;
    if (tdx_server_create(&server, hub, options, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_server_start(server, err) != TDX_OK) {
        tdx_server_destroy(server);
        return TDX_ERR;
    }
    g_stop_requested = 0;
    old_int = signal(SIGINT, serve_on_signal);
    old_term = signal(SIGTERM, serve_on_signal);
    printf("tdx-l1stream listening on http://127.0.0.1:%d/ (universe %zu)\n",
           tdx_server_port(server), universe_size);
    printf("press Ctrl+C to stop\n");
    fflush(stdout);
    while (!g_stop_requested && !server_stopping(server))
        tdx_sleep_ms(50);
    tdx_server_stop(server);
    failed = server->failed;
    tdx_server_destroy(server);
    tdx_hub_stop(hub);
    if (old_int != SIG_ERR)
        signal(SIGINT, old_int);
    if (old_term != SIG_ERR)
        signal(SIGTERM, old_term);
    if (failed) {
        tdx_error_set(err, "HTTP listener failed");
        return TDX_ERR;
    }
    return TDX_OK;
}
