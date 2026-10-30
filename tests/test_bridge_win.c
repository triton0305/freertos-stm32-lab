/* Exercise the actual Monitor parser/router against real Winsock loopback
 * sockets and a mock UART. No COM port is opened by this test. */
#define main monitor_program_main
#include "../tools/monitor.c"
#undef main
static int checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s (WSA=%d)\n",__LINE__,#x,WSAGetLastError()); return 1; } ++checks; } while(0)
static SOCKET connect_client(unsigned short port)
{
    SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    struct sockaddr_in a={0}; a.sin_family=AF_INET; a.sin_port=htons(port); a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    DWORD timeout=500; setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,(char *)&timeout,sizeof(timeout));
    if(connect(s,(struct sockaddr *)&a,sizeof(a))) return INVALID_SOCKET;
    accept_cli(); return s;
}
static int receive(SOCKET s,const char *expected)
{
    char b[1024]; size_t used=0, length=strlen(expected);
    if(length>=sizeof(b)) return 0;
    while(used<length) {
        int n=recv(s,b+used,(int)(length-used),0);
        if(n<=0) return 0;
        used+=(size_t)n;
    }
    b[used]=0; return strcmp(b,expected)==0;
}
int main(void)
{
    WSADATA ws; CHECK(WSAStartup(MAKEWORD(2,2),&ws)==0);
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&base_tick);
    for(int i=0;i<CLIENT_CAP;i++) clients[i].fd=INVALID_SOCKET;
    for(int i=0;i<REQ_CAP;i++) requests[i].slot=-1;
    mock_uart=1; server=start_server(0); CHECK(server!=INVALID_SOCKET);
    struct sockaddr_in a; int alen=sizeof(a); CHECK(!getsockname(server,(struct sockaddr *)&a,&alen));
    SOCKET first=connect_client(ntohs(a.sin_port)); CHECK(first!=INVALID_SOCKET);
    CHECK(receive(first,"READY FreeRTOS CLI Connected\n"));
    SOCKET second=connect_client(ntohs(a.sin_port)); CHECK(second!=INVALID_SOCKET);
    CHECK(receive(second,"READY FreeRTOS CLI Connected\n"));
    next_req=2147483647U;
    CHECK(send(first,"CMD 7 PI",8,0)==8); read_cli(0); CHECK(!clients[0].in_flight);
    CHECK(send(first,"NG\n",3,0)==3); read_cli(0); CHECK(receive(first,"RSP 7 OK PONG\n"));
    CHECK(next_req==1);
    issue(0,"CMD 8 TIMEOUT"); next_req=clients[0].in_flight; issue(1,"CMD 8 TIMEOUT");
    uint32_t one=clients[0].in_flight,two=clients[1].in_flight;
    CHECK(one && two && one!=two);
    process_frame("EVT button pressed count=1 uptime_ms=500");
    char line[128]; snprintf(line,sizeof(line),"RSP %lu OK second",(unsigned long)two); process_frame(line);
    CHECK(receive(second,"RSP 8 OK second\n"));
    snprintf(line,sizeof(line),"RSP %lu OK first",(unsigned long)one); process_frame(line);
    CHECK(receive(first,"RSP 8 OK first\n"));
    issue(0,"CMD 20 TIMEOUT"); issue(1,"CMD 20 TIMEOUT");
    one=clients[0].in_flight; two=clients[1].in_flight;
    process_frame("STAT uptime_ms=1000 led=OFF");
    process_frame("EVT led OFF ON mode=AUTO rate_ms=500 uptime_ms=1000");
    char help_wire[256], help_line[256];
    snprintf(help_wire,sizeof(help_wire),"RSP %lu OK commands=help|ping|status|tasks|queue|led on|led off|led auto|rate N (100~5000 ms)\n",(unsigned long)one);
    LineParser help_parser; line_parser_init(&help_parser);
    unsigned help_frames=0;
    for(size_t i=0;help_wire[i];++i) {
        if(line_parser_feed(&help_parser,(unsigned char)help_wire[i],help_line,sizeof(help_line))==LINE_COMPLETE) {
            process_frame(help_line); ++help_frames;
        }
    }
    CHECK(help_frames==1);
    CHECK(receive(first,"RSP 20 OK commands=help|ping|status|tasks|queue|led on|led off|led auto|rate N (100~5000 ms)\n"));
    CHECK(!clients[0].in_flight && clients[1].in_flight==two);
    snprintf(line,sizeof(line),"RSP %lu OK PONG",(unsigned long)two); process_frame(line);
    CHECK(receive(second,"RSP 20 OK PONG\n"));
    issue(0,"CMD 9 TIMEOUT");
    issue(0,"CMD 10 PING"); CHECK(receive(first,"ERR request_in_flight\n"));
    for(int i=0;i<REQ_CAP;i++) if(requests[i].slot==0) requests[i].since_ms=monotonic_ms()-REQ_TIMEOUT_MS-1;
    prune_requests(); CHECK(receive(first,"ERR response_timeout\n"));
    issue(0,"CMD 11 PING"); CHECK(receive(first,"RSP 11 OK PONG\n"));
    issue(0,"CMD 12 TIMEOUT"); process_frame("EVT boot STM32F411RE");
    CHECK(receive(first,"ERR mcu_reset\n")); CHECK(!clients[0].in_flight);
    issue(0,"CMD -2 PING"); CHECK(receive(first,"ERR bad_client_frame\n"));
    char large[600]; memset(large,'A',sizeof(large)); large[599]='\n';
    CHECK(send(first,large,sizeof(large),0)==sizeof(large));
    for(int i=0;i<3;i++) read_cli(0);
    CHECK(receive(first,"ERR cli_line_too_long_or_invalid\n"));
    issue(0,"CMD 12 PING"); CHECK(receive(first,"RSP 12 OK PONG\n"));
    issue(0,"CMD 13 TIMEOUT"); one=clients[0].in_flight;
    disconnect_client(0); closesocket(first);
    first=connect_client(ntohs(a.sin_port)); CHECK(receive(first,"READY FreeRTOS CLI Connected\n"));
    snprintf(line,sizeof(line),"RSP %lu OK stale",(unsigned long)one); process_frame(line);
    issue(0,"CMD 1 PING"); CHECK(receive(first,"RSP 1 OK PONG\n"));
    /* A non-reading client must not block the bridge. */
    int small=1024; setsockopt(clients[1].fd,SOL_SOCKET,SO_SNDBUF,(char *)&small,sizeof(small));
    char bulk[4096]; memset(bulk,'x',sizeof(bulk)-1); bulk[sizeof(bulk)-1]=0;
    uint64_t start=monotonic_ms();
    for(int i=0;i<20000 && clients[1].fd!=INVALID_SOCKET;i++) send_text(1,bulk);
    CHECK(clients[1].fd==INVALID_SOCKET); CHECK(monotonic_ms()-start<2000);
    issue(0,"CMD 2 PING"); CHECK(receive(first,"RSP 2 OK PONG\n"));
    disconnect_client(0); char c; CHECK(recv(first,&c,1,0)==0);
    closesocket(first);closesocket(second);closesocket(server);WSACleanup();
    printf("HOST VERIFIED: Windows mock UART / actual TCP bridge %d checks PASS\n",checks);
    return 0;
}
