#ifndef LAB_HOST_PROTOCOL_H
#define LAB_HOST_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#include "../App/app_config.h"

#define HOST_MAX_UART_LINE 512
#define HOST_MAX_CLI_LINE APP_CLI_COMMAND_MAX_BYTES
_Static_assert(HOST_MAX_UART_LINE >= APP_UART_TX_FRAME_BUFFER_BYTES - 2,
               "Host parser must fit firmware output");

typedef struct {
    char data[HOST_MAX_UART_LINE + 1];
    size_t len;
    int dropping;
} LineParser;

enum { LINE_WAIT = 0, LINE_COMPLETE = 1, LINE_DROPPED = -1 };
typedef enum { FRAME_RESPONSE, FRAME_STATUS, FRAME_EVENT, FRAME_OTHER } FrameType;
void line_parser_init(LineParser *p);
int line_parser_feed(LineParser *p, unsigned char ch, char *out, size_t out_cap);
FrameType protocol_classify(const char *line, uint32_t *request_id);
int protocol_normalize_cli(const char *input, char *out, size_t capacity);
/* Convert only an OK commands= payload to display text; UART stays one line. */
int protocol_format_help(const char *payload, char *out, size_t capacity);
int protocol_uptime_ms(const char *line, uint32_t *uptime_ms);
int64_t protocol_clock_skew_ms(uint64_t host_start, uint64_t host_now, uint32_t mcu_start, uint32_t mcu_now);
#endif
