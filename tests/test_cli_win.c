/* Capture the actual CLI presentation function without opening COM/TCP. */
#define _WIN32_WINNT 0x0601
#include <stdio.h>
#include <string.h>
static char output[2048];
static int capture_puts(const char *text)
{
    size_t used=strlen(output), length=strlen(text);
    if (used+length+2>sizeof(output)) return EOF;
    memcpy(output+used,text,length);
    output[used+length]='\n'; output[used+length+1]='\0';
    return 0;
}
#define puts capture_puts
#define main cli_program_main
#include "../tools/cli.c"
#undef main
#undef puts
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } ++checks; } while(0)
int main(void)
{
    unsigned checks=0;
    show_response("RSP 77 OK commands=help|ping|status|tasks|queue|led on|led off|led auto|rate N (100~5000 ms)");
    CHECK(!strcmp(output,"help\nping\nstatus\ntasks\nqueue\nled on\nled off\nled auto\nrate N (100~5000 ms)\nexit (Host CLI only)\n"));
    unsigned lines=0; for(const char *p=output;*p;++p) if(*p=='\n') ++lines;
    CHECK(lines==10);
    CHECK(!strstr(output,"led_") && !strchr(output,'|'));
    output[0]=0; show_response("RSP 78 ERR bad_rate"); CHECK(!strcmp(output,"ERR bad_rate\n"));
    output[0]=0; show_response("RSP 79 OK PONG"); CHECK(!strcmp(output,"PONG\n"));
    output[0]=0; show_response("RSP 80 OK uptime_ms=1000 led=OFF");
    CHECK(!strcmp(output,"OK uptime_ms=1000 led=OFF\n"));
    printf("HOST VERIFIED: actual Windows CLI HELP presentation %u checks PASS\n",checks);
    return 0;
}
