/* Single-threaded Win32 COM + localhost-only TCP bridge. No pySerial, no extra service.
 * Build: cmake -S host -B build/host-win && cmake --build build/host-win --config Release
 * Terminal 1: monitor.exe --serial COM7 --baud 115200 --port 19006
 */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "protocol.h"
#include "timing.h"

#define CLIENT_CAP 8
#define REQ_CAP 64
#define REQ_TIMEOUT_MS 5000ULL

typedef struct {
    SOCKET fd;
    char input[HOST_MAX_CLI_LINE + 32];
    size_t used;
    int dropping;
    uint32_t in_flight;
} Client;
typedef struct { uint32_t id, client_id; int slot; uint64_t since_ms; } Request;
static volatile LONG running = 1;
static Client clients[CLIENT_CAP];
static Request requests[REQ_CAP];
static HANDLE com_handle = INVALID_HANDLE_VALUE;
static SOCKET server = INVALID_SOCKET;
static uint32_t next_req = 1;
static LineParser parser;
static LARGE_INTEGER frequency, base_tick;
static int mock_uart;
static uint64_t stop_after_ms;
static Timing status_timing, led_timing;
static uint32_t led_target = APP_LED_DEFAULT_RATE_MS;
static void process_frame(const char *line);

static uint64_t monotonic_ms(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (uint64_t)(((long double)(t.QuadPart - base_tick.QuadPart) * 1000.0L) / (long double)frequency.QuadPart);
}
static BOOL WINAPI on_console(DWORD type)
{
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
        InterlockedExchange(&running, 0);
        return TRUE;
    }
    return FALSE;
}
static void win_error(const char *where)
{
    fprintf(stderr, "[ERROR] %s: Win32 error %lu\n", where, (unsigned long)GetLastError());
}
static void sock_error(const char *where)
{
    fprintf(stderr, "[ERROR] %s: Winsock error %d\n", where, WSAGetLastError());
}
static void disconnect_client(int slot);
static void send_text(int slot, const char *s)
{
    SOCKET fd = clients[slot].fd;
    if (fd == INVALID_SOCKET) return;
    size_t len = strlen(s);
    while (len) {
        int n = send(fd, s, (int)len, 0);
        if (n <= 0) {
            if (n < 0 && WSAGetLastError() == WSAEWOULDBLOCK)
                fprintf(stderr, "[WARN] CLI socket too slow, disconnecting client %d\n", slot);
            disconnect_client(slot);
            return;
        }
        s += n; len -= (size_t)n;
    }
}
static void disconnect_client(int slot)
{
    if (clients[slot].fd != INVALID_SOCKET) {
        shutdown(clients[slot].fd, SD_BOTH);
        closesocket(clients[slot].fd);
        clients[slot].fd = INVALID_SOCKET;
    }
    clients[slot].in_flight = 0;
    clients[slot].used = 0;
    clients[slot].dropping = 0;
    for (int i = 0; i < REQ_CAP; i++) if (requests[i].slot == slot) requests[i].slot = -1;
}
static HANDLE open_serial(const char *name, unsigned long baud)
{
    if (strncmp(name, "COM", 3) != 0 || strlen(name) > 6) {
        fprintf(stderr, "[ERROR] Pass the STM32 ST-LINK VCP COM number, e.g. COM7\n");
        return INVALID_HANDLE_VALUE;
    }
    for (const char *p = name + 3; *p; ++p) if (*p < '0' || *p > '9') return INVALID_HANDLE_VALUE;
    char path[32];
    snprintf(path, sizeof(path), "\\\\.\\%s", name);
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { win_error("Cannot open STM32 COM port; check Windows Device Manager / exclusive access"); return h; }
    DCB dcb; memset(&dcb, 0, sizeof(dcb)); dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) { win_error("GetCommState"); CloseHandle(h); return INVALID_HANDLE_VALUE; }
    dcb.BaudRate = baud; dcb.ByteSize = 8; dcb.Parity = NOPARITY; dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE; dcb.fOutxCtsFlow = FALSE; dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE; dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fInX = FALSE; dcb.fOutX = FALSE; dcb.fAbortOnError = FALSE;
    if (!SetCommState(h, &dcb)) { win_error("SetCommState"); CloseHandle(h); return INVALID_HANDLE_VALUE; }
    COMMTIMEOUTS t = {0};
    t.ReadIntervalTimeout = MAXDWORD;
    t.ReadTotalTimeoutMultiplier = 0;
    t.ReadTotalTimeoutConstant = 20; /* UART poll no longer than ~20ms without bytes */
    t.WriteTotalTimeoutMultiplier = 0;
    t.WriteTotalTimeoutConstant = 200;
    if (!SetCommTimeouts(h, &t)) { win_error("SetCommTimeouts"); CloseHandle(h); return INVALID_HANDLE_VALUE; }
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return h;
}
static int write_serial(const char *s)
{
    if (mock_uart) {
        unsigned long id; char cmd[128], response[180];
        if (sscanf(s, "CMD %lu %127[^\n]", &id, cmd) != 2) return 0;
        if (!strcmp(cmd,"TIMEOUT")) return 1;
        snprintf(response,sizeof(response),"RSP %lu %s",id,
            !strcmp(cmd,"PING") ? "OK PONG" : "ERR mock_command");
        process_frame(response);
        return 1;
    }
    DWORD written = 0;
    DWORD len = (DWORD)strlen(s);
    if (!WriteFile(com_handle, s, len, &written, NULL) || written != len) {
        win_error("UART write");
        return 0;
    }
    return 1;
}
static void issue(int slot, const char *line)
{
    unsigned long local_id=0; char *end=NULL;
    if (strncmp(line,"CMD ",4)!=0 || line[4]<'0' || line[4]>'9') { send_text(slot,"ERR bad_client_frame\n"); return; }
    local_id=strtoul(line+4,&end,10);
    if (!local_id || local_id>2147483647UL || *end!=' ') { send_text(slot,"ERR bad_client_id\n"); return; }
    line=end+1;
    char command[HOST_MAX_CLI_LINE + 1];
    if (!protocol_normalize_cli(line, command, sizeof(command))) {
        send_text(slot, "ERR invalid_or_oversized_command\n"); return;
    }
    if (clients[slot].in_flight) { send_text(slot, "ERR request_in_flight\n"); return; }
    int req_slot = -1;
    for (int i = 0; i < REQ_CAP; i++) if (requests[i].slot == -1) { req_slot = i; break; }
    if (req_slot < 0) { send_text(slot, "ERR too_many_pending\n"); return; }
    uint32_t id;
    int collision;
    do {
        id = next_req++;
        if (next_req > 2147483647U) next_req = 1;
        collision = 0;
        for (int i=0;i<REQ_CAP;i++)
            if (requests[i].slot>=0 && requests[i].id==id) collision=1;
    } while (collision);
    char wire[HOST_MAX_CLI_LINE + 40];
    int n = snprintf(wire, sizeof(wire), "CMD %lu %s\n", (unsigned long)id, command);
    if (n <= 0 || n >= (int)sizeof(wire) || (size_t)n > APP_UART_RX_LINE_BUFFER_BYTES - 1) {
        send_text(slot, "ERR command_too_long_for_STM32\n"); return;
    }
    requests[req_slot].slot = slot;
    requests[req_slot].id = id;
    requests[req_slot].client_id = (uint32_t)local_id;
    requests[req_slot].since_ms = monotonic_ms();
    clients[slot].in_flight = id;
    if (!write_serial(wire)) {
        requests[req_slot].slot = -1;
        clients[slot].in_flight = 0;
        send_text(slot, "ERR uart_write_failure\n");
        InterlockedExchange(&running, 0);
    }
}
static void process_frame(const char *line)
{
    uint32_t id, mcu_ms;
    FrameType typ = protocol_classify(line, &id);
    uint64_t host = monotonic_ms();
    if (typ == FRAME_RESPONSE) {
        for (int i=0;i<REQ_CAP;i++) {
            if (requests[i].slot >= 0 && requests[i].id == id) {
                int slot = requests[i].slot;
                requests[i].slot = -1;
                if (clients[slot].fd != INVALID_SOCKET && clients[slot].in_flight == id) {
                    clients[slot].in_flight = 0;
                    char outgoing[HOST_MAX_UART_LINE + 2];
                    snprintf(outgoing, sizeof(outgoing), "RSP %lu %s\n",
                        (unsigned long)requests[i].client_id, strchr(line+4,' ')+1);
                    send_text(slot, outgoing);
                }
                return;
            }
        }
        printf("[ORPHAN RESPONSE] %s\n", line);
    } else if (typ == FRAME_STATUS) {
        printf("[STATUS host_ms=%llu] %s\n", (unsigned long long)host, line + 5);
        if (protocol_uptime_ms(line, &mcu_ms)) {
            if (timing_sample(&status_timing,host,mcu_ms)<0)
                fprintf(stderr,"[TIMING] MCU reset/discontinuity; new window\n");
            timing_print("STATUS",&status_timing,APP_STATUS_PERIOD_MS);
        }
    } else if (typ == FRAME_EVENT) {
        printf("[EVENT host_ms=%llu] %s\n", (unsigned long long)host, line + 4);
        if (strncmp(line,"EVT boot ",9)==0) {
            memset(&status_timing,0,sizeof(status_timing));
            memset(&led_timing,0,sizeof(led_timing));
            for (int i=0;i<REQ_CAP;i++) if (requests[i].slot>=0) {
                int slot=requests[i].slot;
                requests[i].slot=-1;
                clients[slot].in_flight=0;
                send_text(slot,"ERR mcu_reset\n");
            }
        }
        if (strncmp(line,"EVT config ",11)==0) memset(&led_timing,0,sizeof(led_timing));
        if (strncmp(line,"EVT led ",8)==0) {
            if (strstr(line,"mode=AUTO") && protocol_uptime_ms(line,&mcu_ms)) {
                const char *rate=strstr(line,"rate_ms=");
                uint32_t target=rate ? (uint32_t)strtoul(rate+8,NULL,10) : APP_LED_DEFAULT_RATE_MS;
                if (target!=led_target) memset(&led_timing,0,sizeof(led_timing));
                led_target=target;
                timing_sample(&led_timing,host,mcu_ms);
                timing_print("LED",&led_timing,led_target);
            } else memset(&led_timing,0,sizeof(led_timing));
        }
    } else {
        printf("[UART] %s\n", line);
    }
    fflush(stdout);
}
static void read_serial(void)
{
    if (mock_uart) { Sleep(5); return; }
    unsigned char bytes[128]; DWORD n = 0;
    if (!ReadFile(com_handle, bytes, sizeof(bytes), &n, NULL)) {
        DWORD errors = 0;
        COMSTAT stat;
        if (!ClearCommError(com_handle, &errors, &stat)) {
            win_error("UART disconnected"); InterlockedExchange(&running, 0); return;
        }
        fprintf(stderr,"[ERROR] UART read error, ClearCommError flags=0x%lx\n", (unsigned long)errors);
        InterlockedExchange(&running, 0); return;
    }
    char line[HOST_MAX_UART_LINE + 1];
    for (DWORD i=0;i<n;i++) {
        int outcome = line_parser_feed(&parser, bytes[i], line, sizeof(line));
        if (outcome == LINE_COMPLETE) process_frame(line);
        else if (outcome == LINE_DROPPED) fprintf(stderr,"[WARN] corrupt/oversized UART line discarded until LF\n");
    }
}
static void prune_requests(void)
{
    uint64_t now = monotonic_ms();
    for (int i=0;i<REQ_CAP;i++) {
        if (requests[i].slot >= 0 && now - requests[i].since_ms > REQ_TIMEOUT_MS) {
            int slot = requests[i].slot;
            if (clients[slot].fd != INVALID_SOCKET && clients[slot].in_flight == requests[i].id) {
                clients[slot].in_flight = 0;
                send_text(slot, "ERR response_timeout\n");
            }
            requests[i].slot = -1;
        }
    }
}
static void accept_cli(void)
{
    SOCKET fd = accept(server, NULL, NULL);
    if (fd == INVALID_SOCKET) return;
    int slot = -1;
    for (int i=0;i<CLIENT_CAP;i++) if (clients[i].fd == INVALID_SOCKET) { slot=i; break; }
    if (slot<0) { send(fd, "ERR too_many_clients\n", 21, 0); closesocket(fd); return; }
    u_long one = 1;
    ioctlsocket(fd, FIONBIO, &one);
    clients[slot].fd=fd;
    clients[slot].in_flight=0;
    clients[slot].used=0;
    clients[slot].dropping=0;
    send_text(slot, "READY FreeRTOS CLI Connected\n");
}
static void read_cli(int slot)
{
    char in[256];
    int count=recv(clients[slot].fd,in,sizeof(in),0);
    if (count<=0) {
        if (count<0 && WSAGetLastError()==WSAEWOULDBLOCK) return;
        disconnect_client(slot); return;
    }
    Client *c=&clients[slot];
    for (int i=0;i<count && c->fd!=INVALID_SOCKET;i++) {
        unsigned char ch=(unsigned char)in[i];
        if (ch=='\n') {
            if (c->dropping) send_text(slot,"ERR cli_line_too_long_or_invalid\n");
            else { c->input[c->used]=0; issue(slot,c->input); }
            c->used=0; c->dropping=0;
        } else if (!c->dropping) {
            if (ch=='\r') continue;
            if (ch<32 || ch>126 || c->used>=sizeof(c->input)-1) c->dropping=1;
            else c->input[c->used++]=(char)ch;
        }
    }
}
static SOCKET start_server(unsigned short port)
{
    SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (s==INVALID_SOCKET) {sock_error("socket");return s;}
    struct sockaddr_in a; memset(&a,0,sizeof(a));
    a.sin_family=AF_INET; a.sin_port=htons(port); a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if (bind(s,(const struct sockaddr *)&a,sizeof(a))==SOCKET_ERROR || listen(s,CLIENT_CAP)==SOCKET_ERROR) {
        sock_error("bind/listen 127.0.0.1 (port in use?)"); closesocket(s); return INVALID_SOCKET;
    }
    u_long one=1; ioctlsocket(s,FIONBIO,&one);
    return s;
}
int main(int argc,char **argv)
{
    const char *name=NULL;
    unsigned long baud=115200, port=19006;
    for (int i=1;i<argc;i++) {
        if (strcmp(argv[i],"--mock")==0) mock_uart=1;
        else if (i+1<argc && strcmp(argv[i],"--duration-ms")==0) stop_after_ms=strtoul(argv[++i],NULL,10);
        else if (i+1<argc && strcmp(argv[i],"--serial")==0) name=argv[++i];
        else if (i+1<argc && strcmp(argv[i],"--baud")==0) baud=strtoul(argv[++i],NULL,10);
        else if (i+1<argc && strcmp(argv[i],"--port")==0) port=strtoul(argv[++i],NULL,10);
        else {fprintf(stderr,"Usage: monitor.exe --serial COMx [--baud 115200] [--port 19006]\n");return 2;}
    }
    if ((!name && !mock_uart) || baud<1200 || baud>1000000 || port==0 || port>65535) {
        fprintf(stderr,"Usage: monitor.exe --serial COMx [--baud 115200] [--port 19006]\n");return 2;
    }
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&base_tick);

    line_parser_init(&parser);
    for(int i=0;i<CLIENT_CAP;i++) clients[i].fd=INVALID_SOCKET;
    for(int i=0;i<REQ_CAP;i++) requests[i].slot=-1;
    SetConsoleCtrlHandler(on_console, TRUE);
    WSADATA ws;
    if (WSAStartup(MAKEWORD(2,2),&ws)) { fprintf(stderr,"[ERROR] WSAStartup\n"); return 1; }
    if (!mock_uart) com_handle=open_serial(name,baud);
    if (!mock_uart && com_handle==INVALID_HANDLE_VALUE) {WSACleanup();return 1;}
    server=start_server((unsigned short)port);
    if (server==INVALID_SOCKET) {CloseHandle(com_handle);WSACleanup();return 1;}
    printf("[MONITOR] STM32 VCP=%s %lu 8N1 | 127.0.0.1:%lu | Ctrl+C to stop\n",mock_uart ? "MOCK_NO_HARDWARE" : name,baud,port);
    printf("[MONITOR] Independent PC timing uses QueryPerformanceCounter (ms since launch)\n");
    while (InterlockedCompareExchange(&running,1,1)) {
        if (stop_after_ms && monotonic_ms() >= stop_after_ms) break;
        fd_set reads; FD_ZERO(&reads); FD_SET(server,&reads);
        for (int i=0;i<CLIENT_CAP;i++) if (clients[i].fd!=INVALID_SOCKET) FD_SET(clients[i].fd,&reads);
        struct timeval tv={0,0};
        int n=select(0,&reads,NULL,NULL,&tv);
        if (n==SOCKET_ERROR) {sock_error("select");break;}
        if (FD_ISSET(server,&reads)) accept_cli();
        for (int i=0;i<CLIENT_CAP;i++) if(clients[i].fd!=INVALID_SOCKET && FD_ISSET(clients[i].fd,&reads)) read_cli(i);
        read_serial(); /* configured 20ms timeout, not busy-loop */
        prune_requests();
    }
    printf("[MONITOR] stopped, closing CLI connections and COM port\n");
    for(int i=0;i<CLIENT_CAP;i++) disconnect_client(i);
    closesocket(server); CloseHandle(com_handle); WSACleanup();
    return 0;
}
