#include "protocol.h"
#include <ctype.h>
#include <limits.h>
#include <string.h>

void line_parser_init(LineParser *p) { memset(p, 0, sizeof(*p)); }

int line_parser_feed(LineParser *p, unsigned char ch, char *out, size_t out_cap)
{
    if (ch == '\n') {
        int r = p->dropping ? LINE_DROPPED : (p->len ? LINE_COMPLETE : LINE_WAIT);
        if (r == LINE_COMPLETE) {
            size_t n = p->len;
            if (n && p->data[n - 1] == '\r') n--;
            if (out_cap <= n) r = LINE_DROPPED;
            else { memcpy(out, p->data, n); out[n] = 0; }
        }
        p->len = 0; p->dropping = 0;
        return r;
    }
    if (p->dropping) return LINE_WAIT;
    if (ch < 32 && ch != '\r') { p->dropping = 1; return LINE_WAIT; }
    if (ch > 126) { p->dropping = 1; return LINE_WAIT; }
    if (p->len == HOST_MAX_UART_LINE) { p->dropping = 1; return LINE_WAIT; }
    p->data[p->len++] = (char)ch;
    return LINE_WAIT;
}

FrameType protocol_classify(const char *line, uint32_t *request_id)
{
    if (request_id) *request_id = 0;
    if (strncmp(line, "STAT ", 5) == 0) return FRAME_STATUS;
    if (strncmp(line, "EVT ", 4) == 0) return FRAME_EVENT;
    if (strncmp(line, "RSP ", 4) == 0) {
        const char *p = line + 4;
        uint32_t n = 0;
        if (*p < '0' || *p > '9') return FRAME_OTHER;
        while (*p >= '0' && *p <= '9') {
            unsigned int d = (unsigned int)(*p++ - '0');
            if (n > (UINT32_MAX - d) / 10) return FRAME_OTHER;
            n = n * 10 + d;
        }
        if (!n || *p++ != ' ') return FRAME_OTHER;
        if ((strncmp(p, "OK", 2) == 0 && (p[2] == 0 || p[2] == ' ')) ||
            (strncmp(p, "ERR", 3) == 0 && (p[3] == 0 || p[3] == ' '))) {
            if (request_id) *request_id = n;
            return FRAME_RESPONSE;
        }
    }
    return FRAME_OTHER;
}

int protocol_normalize_cli(const char *input, char *out, size_t capacity)
{
    if (!capacity) return 0;
    size_t n = 0;
    while (*input == ' ' || *input == '\t') ++input;
    for (; *input && *input != '\r' && *input != '\n'; ++input) {
        unsigned char ch = (unsigned char)*input;
        if (ch < 32 || ch > 126 || n >= HOST_MAX_CLI_LINE || n + 1 >= capacity) return 0;
        out[n++] = (char)toupper(ch);
    }
    while (n && out[n-1] == ' ') --n;
    out[n] = 0;
    return n > 0;
}

int protocol_uptime_ms(const char *line, uint32_t *uptime_ms)
{
    const char *p = strstr(line, "uptime_ms=");
    if (!p || (p != line && p[-1] != ' ')) return 0;
    p += 10;
    if (*p < '0' || *p > '9') return 0;
    uint32_t n = 0;
    while (*p >= '0' && *p <= '9') {
        unsigned int d = (unsigned int)(*p++ - '0');
        if (n > (UINT32_MAX - d) / 10) return 0;
        n = n * 10 + d;
    }
    if (*p && *p != ' ') return 0;
    *uptime_ms = n;
    return 1;
}

int protocol_format_help(const char *payload, char *out, size_t capacity)
{
    static const char prefix[] = "OK commands=";
    if (strncmp(payload, prefix, sizeof(prefix) - 1) != 0) return 0;
    const char *commands = payload + sizeof(prefix) - 1;
    size_t length = strlen(commands);
    if (!length || length >= capacity || strpbrk(commands, "\r\n")) return 0;
    for (size_t i = 0; i < length; ++i)
        out[i] = commands[i] == '|' ? '\n' : commands[i];
    out[length] = '\0';
    return 1;
}

int64_t protocol_clock_skew_ms(uint64_t host_start, uint64_t host_now, uint32_t mcu_start, uint32_t mcu_now)
{
    /* 32-bit MCU uptime wraps in ~49.7 days; unsigned subtraction handles it. */
    return (int64_t)(host_now - host_start) - (int64_t)(uint32_t)(mcu_now - mcu_start);
}
