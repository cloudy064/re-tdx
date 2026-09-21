/* tdx_serve.c - blocking accept loop, one thread per connection. */
#include "tdx_serve.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#include <unistd.h>
typedef int serve_socket;
#define SERVE_INVALID (-1)
#endif

#define SERVE_REQUEST_MAX 8192
#define SERVE_CODES_MAX 4096

typedef struct serve_connection {
    serve_socket client;
    tdx_hub *hub;
    const tdx_code *universe;
    size_t universe_size;
    int write_timeout_ms;
} serve_connection;

void tdx_serve_options_default(tdx_serve_options *options) {
    if (!options)
        return;
    options->port = 8790;
    options->backlog = 32;
    options->write_timeout_ms = 30000;
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

/* Shutdown plumbing.  The signal handler only touches a flag and the
 * listening socket; closing it is what pulls a blocked accept() out. */
static serve_socket g_listener = SERVE_INVALID;
static volatile sig_atomic_t g_stopping = 0;
static size_t g_active_connections = 0;
static tdx_mutex g_connection_lock;

static void serve_on_signal(int signal_number) {
    (void)signal_number;
    g_stopping = 1;
    if (g_listener != SERVE_INVALID)
        close_socket(g_listener);
    g_listener = SERVE_INVALID;
}

static int send_all(serve_socket handle, const char *data, size_t size) {
    size_t sent = 0;
    while (sent < size) {
        int written = (int)send(handle, data + sent, (int)(size - sent), 0);
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
    tdx_buf codes_buffer;
    uint64_t subscriber;
    char value[4096];
    long max_events = 0;
    long idle_timeout = 15000;
    long emitted = 0;

    error.message[0] = '\0';
    if (query_value(query, "max_events", value, sizeof(value)))
        max_events = strtol(value, NULL, 10);
    if (query_value(query, "wait_timeout_ms", value, sizeof(value)))
        idle_timeout = strtol(value, NULL, 10);
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
    tdx_buf_init(&codes_buffer);
    for (;;) {
        if (g_stopping || tdx_hub_is_stopping(connection->hub))
            break;
        if (tdx_hub_next(connection->hub, subscriber, &event, (int)idle_timeout,
                         &error) != TDX_OK)
            break;
        if (event.len == 0) {
            if (send_text(connection->client, ": keep-alive\n\n") != 0)
                break;
            continue;
        }
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
    tdx_buf_free(&codes_buffer);
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
    tdx_mutex_lock(&g_connection_lock);
    g_active_connections++;
    tdx_mutex_unlock(&g_connection_lock);
    service_connection_inner(connection);
    close_socket(connection->client);
    free(connection);
    tdx_mutex_lock(&g_connection_lock);
    g_active_connections--;
    tdx_mutex_unlock(&g_connection_lock);
}

static void service_connection_inner(serve_connection *connection) {
    char request[SERVE_REQUEST_MAX];
    size_t used = 0;
    int header_end = -1;
    char method[8];
    char path[512];
    char query[4096];

    method[0] = '\0';
    path[0] = '\0';
    query[0] = '\0';

    /* Read once; a small GET always arrives in one or two segments. */
    while (used < sizeof(request) - 1) {
        int count = (int)recv(connection->client, request + used,
                              (int)(sizeof(request) - 1 - used), 0);
        if (count <= 0)
            break;
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
        close_socket(connection->client);
        free(connection);
        return;
    }

    {
        char *line_end = strstr(request, "\r\n");
        char *target;
        char *space;
        if (!line_end) {
            send_plain(connection->client, 400, "Bad Request", "malformed request\n");
            close_socket(connection->client);
            free(connection);
            return;
        }
        *line_end = '\0';
        space = strchr(request, ' ');
        if (!space) {
            send_plain(connection->client, 400, "Bad Request", "malformed request\n");
            close_socket(connection->client);
            free(connection);
            return;
        }
        *space = '\0';
        snprintf(method, sizeof(method), "%.7s", request);
        target = space + 1;
        space = strchr(target, ' ');
        if (space)
            *space = '\0';
        {
            char *mark = strchr(target, '?');
            if (mark) {
                *mark = '\0';
                snprintf(query, sizeof(query), "%.4095s", mark + 1);
            }
            snprintf(path, sizeof(path), "%.511s", target);
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

int tdx_serve_run(tdx_hub *hub, const tdx_code *universe, size_t universe_size,
                  const tdx_serve_options *options, tdx_error *err) {
    tdx_serve_options defaults;
    serve_socket listener;
    struct sockaddr_in address;
    int reuse = 1;

    if (!hub || !options) {
        tdx_error_set(err, "serve needs a hub and options");
        return TDX_ERR;
    }
    tdx_serve_options_default(&defaults);
    if (options->port < 1 || options->port > 65535) {
        tdx_error_set(err, "serve port must be in 1..65535");
        return TDX_ERR;
    }

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == SERVE_INVALID) {
        tdx_error_set(err, "cannot create the listening socket");
        return TDX_ERR;
    }
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons((uint16_t)options->port);
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, options->backlog) != 0) {
        close_socket(listener);
        tdx_error_set(err, "cannot bind 127.0.0.1:%d", options->port);
        return TDX_ERR;
    }

    if (tdx_mutex_init(&g_connection_lock, err) != TDX_OK) {
        close_socket(listener);
        return TDX_ERR;
    }
    g_listener = listener;
    signal(SIGINT, serve_on_signal);
    signal(SIGTERM, serve_on_signal);
    printf("press Ctrl+C to stop\n");
    printf("tdx-l1stream listening on http://127.0.0.1:%d/ (universe %zu)\n",
           options->port, universe_size);
    printf("  GET /api/v1/market/stream            SSE for the whole universe\n");
    printf("  GET /api/v1/market/stream?codes=...  SSE for selected securities\n");
    printf("  GET /api/v1/market/snapshot?codes=...\n");
    printf("  GET /status   GET /health\n");
    fflush(stdout);

    for (;;) {
        serve_socket client;
        serve_connection *connection;
        tdx_thread thread;
        tdx_error thread_error;

        if (g_stopping)
            break;
        client = accept(g_listener, NULL, NULL);
        if (client == SERVE_INVALID)
            break;
        {
            int timeout = options->write_timeout_ms;
            setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout,
                       sizeof(timeout));
        }
        connection = (serve_connection *)calloc(1, sizeof(*connection));
        if (!connection) {
            close_socket(client);
            continue;
        }
        connection->client = client;
        connection->hub = hub;
        connection->universe = universe;
        connection->universe_size = universe_size;
        connection->write_timeout_ms = options->write_timeout_ms;

        thread_error.message[0] = '\0';
        if (tdx_thread_start(&thread, service_connection, connection,
                             &thread_error) != TDX_OK) {
            close_socket(client);
            free(connection);
            continue;
        }
        /* Detached: the handler owns the connection and frees it. */
        tdx_thread_detach(&thread);
    }
    if (g_listener != SERVE_INVALID) {
        close_socket(g_listener);
        g_listener = SERVE_INVALID;
    }
    /* Stop the poller and wake every blocked subscriber, then let the
     * in-flight handlers return before the caller frees the hub. */
    tdx_hub_stop(hub);
    {
        int waited_ms = 0;
        for (;;) {
            size_t active;
            tdx_mutex_lock(&g_connection_lock);
            active = g_active_connections;
            tdx_mutex_unlock(&g_connection_lock);
            if (active == 0)
                break;
            if (waited_ms >= 5000) {
                fprintf(stderr,
                        "warning: %zu connection(s) still open, exiting anyway\n",
                        active);
                break;
            }
            tdx_sleep_ms(20);
            waited_ms += 20;
        }
    }
    tdx_mutex_destroy(&g_connection_lock);
    printf("stopped\\n");
    fflush(stdout);
    return TDX_OK;
}
