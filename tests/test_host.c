#include "../tools/protocol.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int count;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } count++; } while (0)
int main(void)
{
    LineParser p; char out[HOST_MAX_UART_LINE + 1]; uint32_t id = 0, ms;
    line_parser_init(&p);
    const char *a = "STAT uptime_ms=1000 led=OFF\n";
    int result = LINE_WAIT;
    for (size_t i=0; i<strlen(a); i++) result = line_parser_feed(&p, (unsigned char)a[i], out, sizeof(out));
    CHECK(result == LINE_COMPLETE);
    CHECK(strcmp(out, "STAT uptime_ms=1000 led=OFF") == 0);
    CHECK(protocol_classify(out, &id) == FRAME_STATUS);
    CHECK(protocol_uptime_ms(out, &ms) && ms == 1000);
    CHECK(protocol_classify("RSP 123 OK PONG", &id) == FRAME_RESPONSE && id == 123);
    CHECK(protocol_classify("RSP 34 ERR line_too_long", &id) == FRAME_RESPONSE && id == 34);
    CHECK(protocol_classify("RSP 2abc OK", &id) == FRAME_OTHER);
    CHECK(protocol_classify("RSP 0 OK", &id) == FRAME_OTHER);
    CHECK(protocol_classify("RSP 4294967296 OK", &id) == FRAME_OTHER);
    CHECK(protocol_classify("RSP -1 OK", &id) == FRAME_OTHER);
    CHECK(protocol_classify("RSP 1 OKAY", &id) == FRAME_OTHER);
    CHECK(protocol_classify("EVT led OFF ON uptime_ms=500", &id) == FRAME_EVENT);
    CHECK(protocol_uptime_ms("EVT led OFF ON uptime_ms=500", &ms) && ms == 500);
    char cmd[HOST_MAX_CLI_LINE + 1];
    CHECK(protocol_normalize_cli(" led auto \r\n", cmd, sizeof(cmd)) && strcmp(cmd,"LED AUTO")==0);
    CHECK(!protocol_normalize_cli("", cmd, sizeof(cmd)));
    CHECK(!protocol_normalize_cli("led \x01on", cmd, sizeof(cmd)));
    CHECK(!protocol_normalize_cli("ping", cmd, 0));
    CHECK(protocol_normalize_cli(" rate 500 \r\n",cmd,sizeof(cmd)) && !strcmp(cmd,"RATE 500"));
    CHECK(protocol_normalize_cli("rate  500",cmd,sizeof(cmd)) && !strcmp(cmd,"RATE  500"));
    CHECK(protocol_normalize_cli("rate ",cmd,sizeof(cmd)) && !strcmp(cmd,"RATE"));
    char help[256];
    CHECK(protocol_format_help("OK commands=led on|rate N (100~5000 ms)",help,sizeof(help)));
    CHECK(!strcmp(help,"led on\nrate N (100~5000 ms)"));
    CHECK(!protocol_format_help("ERR commands=led on",help,sizeof(help)));
    CHECK(!protocol_format_help("OK PONG",help,sizeof(help)));
    CHECK(!protocol_format_help("OK commands=",help,sizeof(help)));
    CHECK(!protocol_format_help("OK commands=help\nexit",help,sizeof(help)));
    CHECK(!protocol_format_help("OK commands=help",help,4));
    CHECK(!protocol_format_help("OK commands=help",help,0));
    CHECK(!protocol_uptime_ms("STAT uptime_ms=4294967296", &ms));
    CHECK(!protocol_uptime_ms("STAT notuptime_ms=1", &ms));
    CHECK(!protocol_uptime_ms("STAT uptime_ms=1oops", &ms));
    CHECK(protocol_clock_skew_ms(10000,20000,1000,11000) == 0);
    CHECK(protocol_clock_skew_ms(10000,11000,1000,31000) == -29000); /* catches 30s-in-1s */
    CHECK(protocol_clock_skew_ms(0,1000,UINT32_MAX-499,500) == 0); /* wrap */
    line_parser_init(&p);
    for (size_t i=0;i<HOST_MAX_UART_LINE+2;i++) line_parser_feed(&p, 'x', out, sizeof(out));
    CHECK(line_parser_feed(&p, '\n', out, sizeof(out)) == LINE_DROPPED);
    const char *recover="RSP 2 OK\n";
    for (size_t i=0;i<strlen(recover);i++) result=line_parser_feed(&p,(unsigned char)recover[i],out,sizeof(out));
    CHECK(result==LINE_COMPLETE && strcmp(out,"RSP 2 OK")==0);
    CHECK(line_parser_feed(&p, 0, out, sizeof(out))==LINE_WAIT);
    CHECK(line_parser_feed(&p, '\n', out, sizeof(out))==LINE_DROPPED);
    CHECK(line_parser_feed(&p, 'a', out, sizeof(out))==LINE_WAIT);
    CHECK(line_parser_feed(&p, '\n', out, 1)==LINE_DROPPED);
    puts("HOST VERIFIED: portable C protocol/timing assertions PASS");
    printf("Assertions: %d\n", count);
    return 0;
}
