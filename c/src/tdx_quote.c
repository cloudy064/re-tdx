/* tdx_quote.c - security identity, 7709 session and 0x0547 decoding.
 *
 * Ported field for field from the verified C++ decoder so both implementations
 * can be diffed against each other on live data. */
#include "tdx_quote.h"

#include "tdx_frame.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tdx_internal.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET tdx_socket;
#define TDX_INVALID_SOCKET_VALUE INVALID_SOCKET
#else
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int tdx_socket;
#define TDX_INVALID_SOCKET_VALUE (-1)
#endif

#define TDX_DEPTH_XOR 0x93u
#define TDX_HANDSHAKE_MIN_BODY 189

/* ------------------------------------------------------------------ */
/* security identity                                                   */
/* ------------------------------------------------------------------ */

static int is_ascii_digits(const char *text, size_t length) {
    size_t index;
    for (index = 0; index < length; ++index)
        if (text[index] < '0' || text[index] > '9')
            return 0;
    return 1;
}

int tdx_code_parse(const char *text, tdx_code *out, tdx_error *err) {
    char working[64];
    char *cursor;
    char *colon;
    char *code;
    int market = -1;
    size_t index;

    if (!text || !out) {
        tdx_error_set(err, "security is null");
        return TDX_ERR;
    }
    if (strlen(text) >= sizeof(working)) {
        tdx_error_set(err, "security text is too long");
        return TDX_ERR;
    }
    for (index = 0; text[index] != '\0'; ++index) {
        char ch = text[index];
        working[index] = (ch >= 'A' && ch <= 'Z') ? (char)(ch - 'A' + 'a') : ch;
    }
    working[index] = '\0';
    cursor = tdx_trim(working);
    colon = strchr(cursor, ':');

    if (colon) {
        *colon = '\0';
        code = colon + 1;
        if (strcmp(cursor, "sz") == 0)
            market = 0;
        else if (strcmp(cursor, "sh") == 0)
            market = 1;
        else if (strcmp(cursor, "bj") == 0)
            market = 2;
        else {
            char *end = NULL;
            long parsed = strtol(cursor, &end, 10);
            if (!end || *end != '\0' || parsed < 0 || parsed > 2) {
                tdx_error_set(err, "invalid market prefix: %s", cursor);
                return TDX_ERR;
            }
            market = (int)parsed;
        }
    } else if (strlen(cursor) == 8 &&
               (strncmp(cursor, "sz", 2) == 0 || strncmp(cursor, "sh", 2) == 0 ||
                strncmp(cursor, "bj", 2) == 0)) {
        market = strncmp(cursor, "sz", 2) == 0 ? 0 : (strncmp(cursor, "sh", 2) == 0 ? 1 : 2);
        code = cursor + 2;
    } else {
        code = cursor;
        if ((strlen(code) == 6 &&
             (strncmp(code, "880", 3) == 0 || strncmp(code, "881", 3) == 0) &&
             is_ascii_digits(code, 6)) ||
            (code[0] == '6' || code[0] == '9'))
            market = 1;
        else if (code[0] == '8')
            market = 2;
        else
            market = 0;
    }

    if (market < 0 || market > 2 || strlen(code) != 6 || !is_ascii_digits(code, 6)) {
        tdx_error_set(err, "invalid quote security: %s", text);
        return TDX_ERR;
    }
    memset(out, 0, sizeof(*out));
    out->market_id = market;
    memcpy(out->code, code, 6);
    out->code[6] = '\0';
    return TDX_OK;
}

void tdx_code_id(const tdx_code *code, char *out, size_t out_size) {
    static const char *prefixes[3] = {"SZ", "SH", "BJ"};
    const char *prefix = "??";
    if (!out || out_size == 0)
        return;
    if (code && code->market_id >= 0 && code->market_id <= 2)
        prefix = prefixes[code->market_id];
    snprintf(out, out_size, "%s%s", prefix, code ? code->code : "");
}

int tdx_price_divisor(const char *code) {
    static const struct {
        const char *prefix;
        int divisor;
    } rules[] = {
        {"10", 100},  {"11", 100},   {"12", 100},  {"204", 100}, {"1318", 100},
        /* Measured: the exchangeable-bond segment needs 100 like the other bond
         * segments, and the table's lack of a "13" rule left it at 1 - a price 100
         * times par.  Verified against the local day files: every 132xxx code that
         * still trades came out at a ratio of about 100 while four control families
         * came out at about 1.  Only "132" is added: the rest of the family has no
         * live code here to measure, so widening it further would be a guess. */
        {"132", 100},
        {"15", 10},  {"16", 10},    {"50", 10},   {"51", 10},   {"52", 10},
        {"53", 10},  {"56", 10},    {"58", 10},
    };
    size_t index;
    if (!code)
        return 1;
    for (index = 0; index < sizeof(rules) / sizeof(rules[0]); ++index) {
        size_t length = strlen(rules[index].prefix);
        if (strncmp(code, rules[index].prefix, length) == 0)
            return rules[index].divisor;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* primitive codecs                                                    */
/* ------------------------------------------------------------------ */

int tdx_varint_decode(const uint8_t *data, size_t size, size_t *offset,
                      int64_t *out, tdx_error *err) {
    uint8_t first;
    uint64_t value;
    int shift;
    uint8_t current;

    if (!data || !offset || !out) {
        tdx_error_set(err, "varint arguments are null");
        return TDX_ERR;
    }
    if (*offset >= size) {
        tdx_error_set(err, "quote varint starts past record end");
        return TDX_ERR;
    }
    first = data[(*offset)++];
    value = (uint64_t)(first & 0x3Fu);
    shift = 6;
    current = first;
    while (current & 0x80u) {
        if (*offset >= size) {
            tdx_error_set(err, "quote varint has no terminator");
            return TDX_ERR;
        }
        current = data[(*offset)++];
        if (shift > 55) {
            tdx_error_set(err, "quote varint is too long");
            return TDX_ERR;
        }
        value += (uint64_t)(current & 0x7Fu) << shift;
        shift += 7;
    }
    if (value > (uint64_t)INT64_MAX) {
        tdx_error_set(err, "quote varint exceeds int64");
        return TDX_ERR;
    }
    *out = (first & 0x40u) ? -(int64_t)value : (int64_t)value;
    return TDX_OK;
}

double tdx_wire_number(uint32_t value) {
    uint8_t raw[4];
    int32_t signed_value;
    int exponent;
    unsigned high_byte;
    unsigned middle_byte;
    unsigned low_byte;
    double base;
    double high;
    double scale;

    if (value == 0)
        return 0.0;
    raw[0] = (uint8_t)(value & 0xFFu);
    raw[1] = (uint8_t)((value >> 8) & 0xFFu);
    raw[2] = (uint8_t)((value >> 16) & 0xFFu);
    raw[3] = (uint8_t)((value >> 24) & 0xFFu);
    memcpy(&signed_value, raw, sizeof(signed_value));

    exponent = signed_value >> 24;
    high_byte = (value >> 16) & 0xFFu;
    middle_byte = (value >> 8) & 0xFFu;
    low_byte = value & 0xFFu;

    base = pow(2.0, (double)(exponent * 2 - 0x7F));
    high = (high_byte > 0x80u)
               ? base * (64.0 + (double)(high_byte & 0x7Fu)) / 64.0
               : base * (double)high_byte / 128.0;
    scale = (high_byte & 0x80u) ? 2.0 : 1.0;
    return base + high + base * (double)middle_byte / 32768.0 * scale +
           base * (double)low_byte / 8388608.0 * scale;
}

/* ------------------------------------------------------------------ */
/* session                                                             */
/* ------------------------------------------------------------------ */

#ifdef _WIN32
static INIT_ONCE tdx_winsock_once = INIT_ONCE_STATIC_INIT;
static int tdx_winsock_status = -1;

static BOOL CALLBACK tdx_winsock_init(PINIT_ONCE once, PVOID parameter, PVOID *context) {
    WSADATA data;
    (void)once;
    (void)parameter;
    (void)context;
    tdx_winsock_status = WSAStartup(MAKEWORD(2, 2), &data);
    return TRUE;
}
#endif

static int tdx_socket_startup(tdx_error *err) {
#ifdef _WIN32
    if (!InitOnceExecuteOnce(&tdx_winsock_once, tdx_winsock_init, NULL, NULL)) {
        tdx_error_set(err, "WSAStartup initialisation failed");
        return TDX_ERR;
    }
    if (tdx_winsock_status != 0) {
        tdx_error_set(err, "WSAStartup failed: %d", tdx_winsock_status);
        return TDX_ERR;
    }
#endif
    (void)err;
    return TDX_OK;
}

static const char *tdx_socket_error_text(char *buffer, size_t size) {
#ifdef _WIN32
    snprintf(buffer, size, "WSA %d", WSAGetLastError());
#else
    snprintf(buffer, size, "%s", strerror(errno));
#endif
    return buffer;
}

static void tdx_socket_close(tdx_socket handle) {
    if (handle == TDX_INVALID_SOCKET_VALUE)
        return;
#ifdef _WIN32
    closesocket(handle);
#else
    close(handle);
#endif
}

static int tdx_send_all(tdx_socket handle, const uint8_t *data, size_t size,
                        tdx_error *err) {
    size_t sent = 0;
    while (sent < size) {
        size_t remaining = size - sent;
        int chunk = (remaining > 0x7FFFFFFFu) ? (int)0x7FFFFFFF : (int)remaining;
        int written;
        if (chunk <= 0)
            chunk = (int)remaining;
#ifdef _WIN32
        written = send(handle, (const char *)(data + sent), chunk, 0);
        if (written == SOCKET_ERROR) {
            char detail[64];
            tdx_error_set(err, "send failed: %s", tdx_socket_error_text(detail, sizeof(detail)));
            return TDX_ERR;
        }
#else
        written = (int)send(handle, data + sent, (size_t)chunk, MSG_NOSIGNAL);
        if (written < 0) {
            char detail[64];
            tdx_error_set(err, "send failed: %s", tdx_socket_error_text(detail, sizeof(detail)));
            return TDX_ERR;
        }
#endif
        if (written == 0) {
            tdx_error_set(err, "server closed connection while sending");
            return TDX_ERR;
        }
        sent += (size_t)written;
    }
    return TDX_OK;
}

static int tdx_receive_exact(tdx_socket handle, uint8_t *buffer, size_t size,
                             tdx_error *err) {
    size_t received = 0;
    while (received < size) {
        size_t remaining = size - received;
        int chunk = (remaining > 0x7FFFFFFFu) ? (int)0x7FFFFFFF : (int)remaining;
        int count;
        if (chunk <= 0)
            chunk = (int)remaining;
#ifdef _WIN32
        count = recv(handle, (char *)(buffer + received), chunk, 0);
        if (count == SOCKET_ERROR) {
            char detail[64];
            tdx_error_set(err, "receive failed: %s",
                          tdx_socket_error_text(detail, sizeof(detail)));
            return TDX_ERR;
        }
#else
        count = (int)recv(handle, buffer + received, (size_t)chunk, 0);
        if (count < 0) {
            char detail[64];
            tdx_error_set(err, "receive failed: %s",
                          tdx_socket_error_text(detail, sizeof(detail)));
            return TDX_ERR;
        }
#endif
        if (count == 0) {
            tdx_error_set(err, "server closed connection after %zu of %zu bytes",
                          received, size);
            return TDX_ERR;
        }
        received += (size_t)count;
    }
    return TDX_OK;
}

int tdx_connection_call(tdx_connection *connection, uint16_t message_type,
                        const void *body, size_t body_size, tdx_buf *out,
                        tdx_error *err) {
    uint32_t message_id;
    tdx_buf request;
    uint8_t header[TDX_RESPONSE_HEADER_SIZE];
    tdx_response_header decoded;
    uint8_t *wire = NULL;
    int result = TDX_ERR;

    if (!connection || !out) {
        tdx_error_set(err, "connection or output buffer is null");
        return TDX_ERR;
    }
    tdx_buf_clear(out);
    if (connection->socket_handle == (intptr_t)TDX_INVALID_SOCKET_VALUE) {
        tdx_error_set(err, "market-data connection is not open");
        return TDX_ERR;
    }
    message_id = connection->next_message_id++;
    tdx_buf_init(&request);
    if (tdx_frame_build_request(message_id, message_type, body, body_size,
                                TDX_REQUEST_PREFIX, &request, err) != TDX_OK)
        return TDX_ERR;

    if (tdx_send_all((tdx_socket)connection->socket_handle, request.data,
                     request.len, err) != TDX_OK)
        goto done;
    if (tdx_receive_exact((tdx_socket)connection->socket_handle, header,
                          sizeof(header), err) != TDX_OK)
        goto done;
    if (tdx_frame_decode_header(header, &decoded, err) != TDX_OK)
        goto done;
    if (decoded.message_id != message_id) {
        tdx_error_set(err, "7709 response message ID mismatch (%u, expected %u)",
                      (unsigned)decoded.message_id, (unsigned)message_id);
        goto done;
    }
    if (decoded.message_type != message_type) {
        tdx_error_set(err, "7709 response command mismatch (0x%04X, expected 0x%04X)",
                      (unsigned)decoded.message_type, (unsigned)message_type);
        goto done;
    }
    if (decoded.wire_size > 0) {
        wire = (uint8_t *)malloc(decoded.wire_size);
        if (!wire) {
            tdx_error_set(err, "out of memory for a %u byte response",
                          (unsigned)decoded.wire_size);
            goto done;
        }
        if (tdx_receive_exact((tdx_socket)connection->socket_handle, wire,
                              decoded.wire_size, err) != TDX_OK)
            goto done;
    }
    if (tdx_frame_decode_body(wire, decoded.wire_size, decoded.decoded_size, out,
                              err) != TDX_OK)
        goto done;
    result = TDX_OK;

done:
    free(wire);
    tdx_buf_free(&request);
    if (result != TDX_OK) {
        /* Any framing or transport fault leaves the stream desynchronised, so
         * the connection is retired instead of being reused. */
        tdx_socket_close((tdx_socket)connection->socket_handle);
        connection->socket_handle = (intptr_t)TDX_INVALID_SOCKET_VALUE;
    }
    return result;
}

static int tdx_connection_connect(tdx_connection *connection, tdx_error *err) {
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    char port_text[8];
    int lookup;
    int last_error = 0;

    if (tdx_socket_startup(err) != TDX_OK)
        return TDX_ERR;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    snprintf(port_text, sizeof(port_text), "%u", (unsigned)connection->endpoint.port);
    lookup = getaddrinfo(connection->endpoint.host, port_text, &hints, &addresses);
    if (lookup != 0) {
        char address_text[80];
        tdx_endpoint_address(&connection->endpoint, address_text, sizeof(address_text));
        tdx_error_set(err, "cannot resolve %s: %s", address_text, gai_strerror(lookup));
        return TDX_ERR;
    }

    for (address = addresses; address; address = address->ai_next) {
        tdx_socket candidate = socket(address->ai_family, address->ai_socktype,
                                      address->ai_protocol);
        if (candidate == TDX_INVALID_SOCKET_VALUE) {
            last_error = 1;
            continue;
        }
#ifdef _WIN32
        {
            DWORD timeout = (DWORD)connection->timeout_ms;
            setsockopt(candidate, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout,
                       sizeof(timeout));
            setsockopt(candidate, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout,
                       sizeof(timeout));
        }
#else
        {
            struct timeval timeout;
            timeout.tv_sec = connection->timeout_ms / 1000;
            timeout.tv_usec = (connection->timeout_ms % 1000) * 1000;
            setsockopt(candidate, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            setsockopt(candidate, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        }
#endif
        if (connect(candidate, address->ai_addr, (int)address->ai_addrlen) == 0) {
            connection->socket_handle = (intptr_t)candidate;
            break;
        }
        last_error = 1;
        tdx_socket_close(candidate);
    }
    freeaddrinfo(addresses);

    if (connection->socket_handle == (intptr_t)TDX_INVALID_SOCKET_VALUE) {
        char address_text[80];
        char detail[64];
        tdx_endpoint_address(&connection->endpoint, address_text, sizeof(address_text));
        tdx_error_set(err, "cannot connect to %s: %s", address_text,
                      last_error ? tdx_socket_error_text(detail, sizeof(detail)) : "no address");
        return TDX_ERR;
    }
    return TDX_OK;
}

int tdx_connection_open(tdx_connection *connection, const tdx_endpoint *endpoint,
                        int timeout_ms, tdx_error *err) {
    tdx_buf body = {0};
    uint8_t handshake = 0x01;
    uint8_t name[128];
    size_t name_length = 0;
    size_t index;

    if (!connection || !endpoint) {
        tdx_error_set(err, "connection or endpoint is null");
        return TDX_ERR;
    }
    if (timeout_ms < 1 || timeout_ms > 600000) {
        tdx_error_set(err, "network timeout must be in 1..600000 ms");
        return TDX_ERR;
    }
    memset(connection, 0, sizeof(*connection));
    connection->endpoint = *endpoint;
    connection->socket_handle = (intptr_t)TDX_INVALID_SOCKET_VALUE;
    connection->next_message_id = 0x01640801u;
    connection->timeout_ms = timeout_ms;

    if (tdx_connection_connect(connection, err) != TDX_OK) {
        tdx_connection_close(connection);
        return TDX_ERR;
    }
    if (tdx_connection_call(connection, TDX_HANDSHAKE_TYPE, &handshake, 1, &body,
                            err) != TDX_OK) {
        tdx_buf_free(&body);
        tdx_connection_close(connection);
        return TDX_ERR;
    }
    if (body.len < TDX_HANDSHAKE_MIN_BODY) {
        tdx_error_set(err, "handshake response is too short: %zu bytes", body.len);
        tdx_buf_free(&body);
        tdx_connection_close(connection);
        return TDX_ERR;
    }
    for (index = 68; index < 152 && index < body.len; ++index)
        if (body.data[index] != 0)
            name[name_length++] = body.data[index];
    tdx_buf_free(&body);
    if (name_length > 0) {
        if (tdx_decode_gb18030(name, name_length, connection->server_name,
                               sizeof(connection->server_name), NULL, err) != TDX_OK) {
            connection->server_name[0] = '\0';
        } else {
            char *trimmed = tdx_trim(connection->server_name);
            if (trimmed != connection->server_name)
                memmove(connection->server_name, trimmed, strlen(trimmed) + 1);
        }
    }
    return TDX_OK;
}

void tdx_connection_close(tdx_connection *connection) {
    if (!connection)
        return;
    tdx_socket_close((tdx_socket)connection->socket_handle);
    connection->socket_handle = (intptr_t)TDX_INVALID_SOCKET_VALUE;
}

/* ------------------------------------------------------------------ */
/* 0x0547                                                              */
/* ------------------------------------------------------------------ */

int tdx_quote_build_depth_request(const tdx_code *codes, size_t count,
                                  tdx_buf *out, tdx_error *err) {
    size_t index;

    if (!out) {
        tdx_error_set(err, "request buffer is null");
        return TDX_ERR;
    }
    tdx_buf_clear(out);
    if (!codes) {
        tdx_error_set(err, "depth request security list is null");
        return TDX_ERR;
    }
    if (count == 0) {
        tdx_error_set(err, "depth request needs at least one security");
        return TDX_ERR;
    }
    if (count > 0xFFFFu) {
        tdx_error_set(err, "depth batch exceeds uint16");
        return TDX_ERR;
    }
    if (tdx_buf_reserve(out, 2 + count * 11, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_u16le(out, (uint16_t)count, err) != TDX_OK) {
        tdx_buf_clear(out);
        return TDX_ERR;
    }
    for (index = 0; index < count; ++index) {
        if (tdx_buf_push(out, (uint8_t)codes[index].market_id, err) != TDX_OK ||
            tdx_buf_append(out, codes[index].code, 6, err) != TDX_OK ||
            tdx_buf_append_zeros(out, 4, err) != TDX_OK) {
            tdx_buf_clear(out);
            return TDX_ERR;
        }
    }
    return TDX_OK;
}

typedef struct record_reader {
    const uint8_t *data;
    size_t size;
    size_t offset;
} record_reader;

static int reader_take(record_reader *reader, size_t count, const uint8_t **out,
                       tdx_error *err) {
    if (reader->offset + count > reader->size) {
        tdx_error_set(err, "depth record is truncated at offset %zu", reader->offset);
        return TDX_ERR;
    }
    if (out)
        *out = reader->data + reader->offset;
    reader->offset += count;
    return TDX_OK;
}

static int reader_varint(record_reader *reader, int64_t *out, tdx_error *err) {
    return tdx_varint_decode(reader->data, reader->size, &reader->offset, out, err);
}

static int reader_u32le(record_reader *reader, uint32_t *out, tdx_error *err) {
    const uint8_t *raw;
    if (reader_take(reader, 4, &raw, err) != TDX_OK)
        return TDX_ERR;
    *out = tdx_u32le(raw);
    return TDX_OK;
}

typedef struct price_fields {
    int64_t current;
    int64_t previous;
    int64_t open;
    int64_t high;
    int64_t low;
} price_fields;

static int decode_prices(record_reader *reader, price_fields *out, tdx_error *err) {
    int64_t current;
    int64_t previous;
    int64_t open;
    int64_t high;
    int64_t low;

    if (reader_varint(reader, &current, err) != TDX_OK ||
        reader_varint(reader, &previous, err) != TDX_OK ||
        reader_varint(reader, &open, err) != TDX_OK ||
        reader_varint(reader, &high, err) != TDX_OK ||
        reader_varint(reader, &low, err) != TDX_OK)
        return TDX_ERR;
    out->current = current * 10;
    out->previous = (previous + current) * 10;
    out->open = (open + current) * 10;
    out->high = (high + current) * 10;
    out->low = (low + current) * 10;
    return TDX_OK;
}

static int parse_depth_record(const uint8_t *record, size_t size, tdx_depth *out,
                              tdx_error *err) {
    record_reader reader;
    price_fields prices;
    int64_t value;
    uint32_t raw32;
    double scale;
    size_t level;

    if (size < 9) {
        tdx_error_set(err, "depth record header is incomplete");
        return TDX_ERR;
    }
    memset(out, 0, sizeof(*out));
    out->security.market_id = record[0];
    memcpy(out->security.code, record + 1, 6);
    out->security.code[6] = '\0';
    if (out->security.market_id < 0 || out->security.market_id > 2 ||
        !is_ascii_digits(out->security.code, 6)) {
        tdx_error_set(err, "depth record carries an invalid security");
        return TDX_ERR;
    }
    out->active = tdx_u16le(record + 7);

    reader.data = record;
    reader.size = size;
    reader.offset = 9;

    if (decode_prices(&reader, &prices, err) != TDX_OK)
        return TDX_ERR;
    if (reader_u32le(&reader, &raw32, err) != TDX_OK)
        return TDX_ERR;
    out->update_time = raw32;
    if (reader_varint(&reader, &out->status, err) != TDX_OK)
        return TDX_ERR;
    if (reader_varint(&reader, &out->total_hand, err) != TDX_OK)
        return TDX_ERR;
    if (reader_varint(&reader, &out->current_hand, err) != TDX_OK)
        return TDX_ERR;
    if (reader_u32le(&reader, &raw32, err) != TDX_OK)
        return TDX_ERR;
    out->amount = tdx_wire_number(raw32);
    if (reader_varint(&reader, &out->inside, err) != TDX_OK)
        return TDX_ERR;
    if (reader_varint(&reader, &out->outside, err) != TDX_OK)
        return TDX_ERR;
    if (reader_varint(&reader, &out->unknown_after_outer, err) != TDX_OK)
        return TDX_ERR;
    out->auction_imbalance_hand = out->unknown_after_outer;
    if (reader_varint(&reader, &value, err) != TDX_OK)
        return TDX_ERR;
    out->open_amount = (double)value * 10.0;

    scale = (double)tdx_price_divisor(out->security.code) * 1000.0;
    out->last = (double)prices.current / scale;
    out->previous = (double)prices.previous / scale;
    out->open = (double)prices.open / scale;
    out->high = (double)prices.high / scale;
    out->low = (double)prices.low / scale;

    for (level = 0; level < TDX_DEPTH_LEVELS; ++level) {
        int64_t buy_delta;
        int64_t sell_delta;
        int64_t buy_volume;
        int64_t sell_volume;
        if (reader_varint(&reader, &buy_delta, err) != TDX_OK)
            return TDX_ERR;
        if (reader_varint(&reader, &sell_delta, err) != TDX_OK)
            return TDX_ERR;
        out->buys[level].price =
            ((double)prices.current + (double)buy_delta * 10.0) / scale;
        if (reader_varint(&reader, &buy_volume, err) != TDX_OK)
            return TDX_ERR;
        out->buys[level].volume_hand = buy_volume;
        out->sells[level].price =
            ((double)prices.current + (double)sell_delta * 10.0) / scale;
        if (reader_varint(&reader, &sell_volume, err) != TDX_OK)
            return TDX_ERR;
        out->sells[level].volume_hand = sell_volume;
    }
    if (reader.offset > reader.size) {
        tdx_error_set(err, "depth record overran its bounds");
        return TDX_ERR;
    }
    out->tail_size = reader.size - reader.offset;
    return TDX_OK;
}

/* Finds the start offset of every record after the first by searching for the
 * seven-byte security key of any requested entry. */
static int split_depth_records(const uint8_t *data, size_t size,
                               const tdx_code *requested, size_t requested_count,
                               size_t count, size_t *starts, tdx_error *err) {
    size_t found = 1;
    size_t search_from = 7;

    if (size < 7) {
        tdx_error_set(err, "depth response record data is too short");
        return TDX_ERR;
    }
    starts[0] = 0;
    while (found < count) {
        size_t best = SIZE_MAX;
        size_t code_index;
        for (code_index = 0; code_index < requested_count; ++code_index) {
            uint8_t marker[7];
            size_t position;
            marker[0] = (uint8_t)requested[code_index].market_id;
            memcpy(marker + 1, requested[code_index].code, 6);
            for (position = search_from; position + sizeof(marker) <= size; ++position) {
                if (memcmp(data + position, marker, sizeof(marker)) == 0) {
                    if (position < best)
                        best = position;
                    break;
                }
            }
        }
        if (best == SIZE_MAX) {
            tdx_error_set(err, "cannot identify depth record boundaries");
            return TDX_ERR;
        }
        starts[found++] = best;
        search_from = best + 7;
    }
    return TDX_OK;
}

int tdx_quote_parse_depth_response(uint8_t *payload, size_t size,
                                   const tdx_code *requested,
                                   size_t requested_count, tdx_depth *out,
                                   size_t out_capacity, size_t *out_count,
                                   tdx_error *err) {
    size_t count;
    size_t starts[TDX_DEPTH_BATCH_MAX];
    size_t index;

    if (!payload || !out || !out_count) {
        tdx_error_set(err, "depth response arguments are null");
        return TDX_ERR;
    }
    if (requested_count == 0) {
        tdx_error_set(err, "depth response needs the requested security list");
        return TDX_ERR;
    }
    if (requested_count > TDX_DEPTH_BATCH_MAX) {
        tdx_error_set(err, "depth parse batch exceeds the %d security limit",
                      TDX_DEPTH_BATCH_MAX);
        return TDX_ERR;
    }
    if (out_capacity < requested_count) {
        tdx_error_set(err, "depth output capacity %zu is smaller than the request",
                      out_capacity);
        return TDX_ERR;
    }
    if (size < 2) {
        tdx_error_set(err, "depth response is shorter than two bytes");
        return TDX_ERR;
    }
    for (index = 0; index < size; ++index)
        payload[index] ^= (uint8_t)TDX_DEPTH_XOR;

    count = tdx_u16le(payload);
    if (count > requested_count) {
        tdx_error_set(err, "depth response count %zu exceeds request count %zu", count,
                      requested_count);
        return TDX_ERR;
    }
    *out_count = 0;
    if (count == 0)
        return TDX_OK;

    if (split_depth_records(payload + 2, size - 2, requested, requested_count, count,
                            starts, err) != TDX_OK)
        return TDX_ERR;

    for (index = 0; index < count; ++index) {
        const size_t begin = starts[index];
        const size_t end = (index + 1 < count) ? starts[index + 1] : size - 2;
        tdx_depth parsed;
        size_t code_index;
        int known = 0;

        if (end <= begin || end > size - 2) {
            tdx_error_set(err, "depth record %zu has an invalid span", index);
            return TDX_ERR;
        }
        if (parse_depth_record(payload + 2 + begin, end - begin, &parsed, err) != TDX_OK)
            return TDX_ERR;
        for (code_index = 0; code_index < requested_count; ++code_index) {
            if (requested[code_index].market_id == parsed.security.market_id &&
                memcmp(requested[code_index].code, parsed.security.code, 6) == 0) {
                known = 1;
                break;
            }
        }
        if (!known) {
            char id[16];
            tdx_code_id(&parsed.security, id, sizeof(id));
            tdx_error_set(err, "depth response contains unrequested security %s", id);
            return TDX_ERR;
        }
        out[*out_count] = parsed;
        *out_count = *out_count + 1;
    }
    return TDX_OK;
}
