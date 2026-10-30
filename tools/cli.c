/* Terminal 2 Windows-native CLI: does not open serial / COM.
 * Build with Windows CMake host/ target, linking ws2_32.
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
#include <string.h>
#include "protocol.h"

static int send_all(SOCKET sock, const char *s)
{
    size_t len=strlen(s);
    while(len) {
        int n=send(sock,s,(int)len,0);
        if(n<=0) return 0;
        s+=n; len-=n;
    }
    return 1;
}
static int read_line(SOCKET sock, char *line, size_t cap)
{
    size_t n=0; int overflow=0;
    for(;;) {
        char ch;
        int rc=recv(sock,&ch,1,0);
        if(rc<=0) return 0;
        if(ch=='\n') {
            if (overflow) return -1;
            if (n && line[n-1]=='\r') n--;
            line[n]=0;
            return 1;
        }
        if(n+1>=cap) overflow=1;
        else if(!overflow) line[n++]=ch;
    }
}
static void show_response(const char *s)
{
    uint32_t id;
    if(protocol_classify(s,&id)==FRAME_RESPONSE) {
        const char *p=strchr(s+4,' ');
        if(p) {
            char help[HOST_MAX_UART_LINE + 1];
            if (protocol_format_help(p+1, help, sizeof(help))) {
                puts(help);
                puts("exit (Host CLI only)");
                return;
            }
            if(strcmp(p+1,"OK PONG")==0) {puts("PONG");return;}
            puts(p+1); return;
        }
    }
    puts(s);
}
static HANDLE response_ready;
static volatile LONG expected_id;
static DWORD WINAPI input_thread(LPVOID argument)
{
    SOCKET sock=*(SOCKET *)argument;
    char input[256], command[HOST_MAX_CLI_LINE+1];
    uint32_t sequence=1;
    for(;;) {
        fputs("rtos> ",stdout);fflush(stdout);
        if(!fgets(input,sizeof(input),stdin)) {puts("");break;}
        if(!strchr(input,'\n') && !feof(stdin)) {
            int c;while((c=getchar())!='\n' && c!=EOF){}
            puts("ERR cli_line_too_long");continue;
        }
        if(!protocol_normalize_cli(input,command,sizeof(command))) {puts("ERR invalid_command");continue;}
        if(strcmp(command,"EXIT")==0 || strcmp(command,"QUIT")==0) break;
        char wire[HOST_MAX_CLI_LINE+32];
        InterlockedExchange(&expected_id,(LONG)sequence++);
        if (sequence>2147483647U) sequence=1;
        ResetEvent(response_ready);
        snprintf(wire,sizeof(wire),"CMD %lu %s\n",(unsigned long)expected_id,command);
        if(!send_all(sock,wire)) {puts("[ERROR] Monitor disconnected during send");break;}
        if (WaitForSingleObject(response_ready,7000)!=WAIT_OBJECT_0) {
            puts("[ERROR] response timeout"); break;
        }
    }
    shutdown(sock,SD_BOTH);
    return 0;
}

int main(int argc, char **argv)
{
    const char *host="127.0.0.1";
    unsigned long port=19006;
    for(int i=1;i<argc;i++) {
        if(i+1<argc && strcmp(argv[i],"--host")==0) host=argv[++i];
        else if(i+1<argc && strcmp(argv[i],"--port")==0) port=strtoul(argv[++i],NULL,10);
        else {fprintf(stderr,"Usage: cli.exe [--host 127.0.0.1] [--port 19006]\n");return 2;}
    }
    if(strcmp(host,"127.0.0.1")!=0 || port==0 || port>65535) {
        fprintf(stderr,"[ERROR] Local loopback only: --host 127.0.0.1\n");return 2;
    }
    WSADATA ws;
    if(WSAStartup(MAKEWORD(2,2),&ws)) return 1;
    SOCKET sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(sock==INVALID_SOCKET) {WSACleanup();return 1;}
    struct sockaddr_in a; memset(&a,0,sizeof(a));
    a.sin_family=AF_INET; a.sin_addr.s_addr=htonl(INADDR_LOOPBACK); a.sin_port=htons((unsigned short)port);
    DWORD wait=6500;
    setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(const char *)&wait,sizeof(wait));
    setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,(const char *)&wait,sizeof(wait));
    if(connect(sock,(const struct sockaddr *)&a,sizeof(a))==SOCKET_ERROR) {
        fprintf(stderr,"[ERROR] Monitor not connected at 127.0.0.1:%lu (Winsock %d). Start monitor.exe first.\n",port,WSAGetLastError());
        closesocket(sock); WSACleanup(); return 1;
    }
    char response[HOST_MAX_UART_LINE+64];
    if(read_line(sock,response,sizeof(response))!=1) {
        fprintf(stderr,"[ERROR] Monitor closed during handshake\n");closesocket(sock);WSACleanup();return 1;
    }
    puts(response);
    puts("Commands: help, ping, status, tasks, queue, led on, led off, led auto, rate 500, exit");
    response_ready=CreateEventA(NULL,TRUE,FALSE,NULL);
    if (!response_ready) { closesocket(sock); WSACleanup(); return 1; }
    /* Main owns receive, so idle console input cannot hide a disconnect. */
    DWORD no_timeout=0;
    setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(const char *)&no_timeout,sizeof(no_timeout));
    HANDLE input=CreateThread(NULL,0,input_thread,&sock,0,NULL);
    if (!input) { CloseHandle(response_ready); closesocket(sock); WSACleanup(); return 1; }
    while (read_line(sock,response,sizeof(response))==1) {
        uint32_t id=0;
        if (protocol_classify(response,&id)==FRAME_RESPONSE && id!=(uint32_t)InterlockedCompareExchange(&expected_id,0,0)) {
            puts("[ERROR] mismatched response id"); break;
        }
        show_response(response);
        SetEvent(response_ready);
    }
    puts("[CLI] connection closed");
    fflush(stdout);
    /* A console reader may still be waiting; process exit ends it. */
    CloseHandle(input);
    shutdown(sock,SD_BOTH);closesocket(sock);WSACleanup();return 0;
}
