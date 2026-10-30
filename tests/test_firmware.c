#include <stdio.h>
#include <string.h>
#include "../tools/protocol.h"
#include "../App/rtos_lab.c"
struct FakeQueue { unsigned cap, count, head, tail; size_t size; unsigned char *data; };
struct FakeMutex { int held; };
static unsigned checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } ++checks; } while(0)
QueueHandle_t xQueueCreate(unsigned n,size_t size) { QueueHandle_t q=calloc(1,sizeof(*q)); q->cap=n;q->size=size;q->data=calloc(n,size);return q; }
int xQueueSend(QueueHandle_t q,const void *p,TickType_t t) { (void)t; if(q->count==q->cap)return 0;memcpy(q->data+q->head*q->size,p,q->size);q->head=(q->head+1)%q->cap;q->count++;return 1; }
int xQueueReceive(QueueHandle_t q,void *p,TickType_t t) { (void)t;if(!q->count)return 0;memcpy(p,q->data+q->tail*q->size,q->size);q->tail=(q->tail+1)%q->cap;q->count--;return 1; }
unsigned uxQueueMessagesWaiting(QueueHandle_t q){return q->count;}
SemaphoreHandle_t xSemaphoreCreateMutex(void){return calloc(1,sizeof(struct FakeMutex));}
int xSemaphoreTake(SemaphoreHandle_t m,TickType_t t){(void)t;configASSERT(!m->held);m->held=1;return 1;}
int xSemaphoreGive(SemaphoreHandle_t m){configASSERT(m->held);m->held=0;return 1;}
TickType_t xTaskGetTickCount(void){return 1234;}
void vTaskDelay(TickType_t t){(void)t;}
void vTaskDelayUntil(TickType_t *p,TickType_t t){*p+=t;}
uint32_t ulTaskNotifyTake(int a,TickType_t t){(void)a;(void)t;return 0;}
void vTaskNotifyGiveFromISR(TaskHandle_t h,BaseType_t *p){(void)h;*p=0;}
int xTaskGetSchedulerState(void){return 0;}
void vTaskStartScheduler(void){}
int xTaskCreate(void (*f)(void *),const char *n,unsigned s,void *a,unsigned p,TaskHandle_t *h){(void)f;(void)n;(void)s;(void)a;(void)p;*h=(void *)1;return 1;}
eTaskState eTaskGetState(TaskHandle_t h){(void)h;return eBlocked;}
unsigned uxTaskPriorityGet(TaskHandle_t h){(void)h;return 1;}
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t h){(void)h;return 128;}
unsigned uxTaskGetNumberOfTasks(void){return 5;}
TaskHandle_t xTaskGetIdleTaskHandle(void){return (void *)1;}
size_t xPortGetFreeHeapSize(void){return 10000;}
size_t xPortGetMinimumEverFreeHeapSize(void){return 9000;}
void Board_EnableRxAndButtonIrq(void){}
void Board_LedSet(int on){(void)on;configASSERT(!state_mutex->held);}
int Board_ButtonPressed(void){return 0;}
int Board_UartSend(const char *s){(void)s;configASSERT(!state_mutex->held);return 1;}
uint32_t Board_RxErrors(void){return 0;}
uint32_t Board_RxOverflow(void){return 0;}
int Board_RxPop(uint8_t *c){(void)c;return 0;}
void Error_Handler(void){abort();}
static int response(const char *text){TxMessage m;return xQueueReceive(rsp_queue,&m,0)&&!strcmp(m.text,text);}
static int TestRates(void)
{
    static const struct { const char *argument, *error; uint32_t value; } cases[] = {
        {"abc", "bad_rate", 0}, {"a", "bad_rate", 0}, {"z", "bad_rate", 0},
        {"100.1", "bad_rate", 0}, {"500.1", "bad_rate", 0}, {"500abc", "bad_rate", 0},
        {"99", "rate_range_100_5000", 0}, {"5001", "rate_range_100_5000", 0},
        {"100", NULL, 100}, {"500", NULL, 500}, {"5000", NULL, 5000},
        {"", "bad_rate", 0}, {" ", "bad_rate", 0}, {" 500", "bad_rate", 0},
        {"500 ", "bad_rate", 0}, {"50 0", "bad_rate", 0},
        {"\t500", "bad_rate", 0}, {"500\t", "bad_rate", 0},
        {"+500", "bad_rate", 0}, {"-500", "bad_rate", 0},
        {"a500", "bad_rate", 0}, {"0x1f4", "bad_rate", 0}, {"1e3", "bad_rate", 0},
        {"0", "rate_range_100_5000", 0}, {"000500", NULL, 500},
        {"4294967295", "rate_range_100_5000", 0},
        {"4294967296", "bad_rate", 0}, {"18446744073709551616", "bad_rate", 0},
        {"999999999999999999999999999999999999", "bad_rate", 0},
        {"5001abc", "bad_rate", 0}
    };
    ControlMessage pending;
    while (xQueueReceive(control_queue,&pending,0)) {}
    LabState saved = GetSnapshot();
    state = (LabState){ MODE_AUTO, 7, 1000, 1 };
    for (size_t i=0; i<sizeof(cases)/sizeof(cases[0]); ++i) {
        char command[128], expected[128];
        snprintf(command,sizeof(command),"CMD 42 RATE %s",cases[i].argument);
        uint32_t errors_before = protocol_errors;
        CHECK(uxQueueMessagesWaiting(control_queue)==0);
        Dispatch(command,0);
        if (cases[i].error) {
            snprintf(expected,sizeof(expected),"RSP 42 ERR %s\n",cases[i].error);
            CHECK(response(expected));
            CHECK(protocol_errors==errors_before+1);
            CHECK(uxQueueMessagesWaiting(control_queue)==0);
            Dispatch("CMD 43 PING",0); CHECK(response("RSP 43 OK PONG\n"));
            Dispatch("CMD 44 STATUS",0);
            TxMessage status;
            CHECK(xQueueReceive(rsp_queue,&status,0));
            CHECK(strstr(status.text,"RSP 44 OK ") && strstr(status.text,"mode=AUTO rate=1000 "));
        } else {
            ControlMessage message;
            CHECK(xQueueReceive(control_queue,&message,0));
            CHECK(message.id==42 && message.op==OP_RATE && message.value==cases[i].value);
            CHECK(uxQueueMessagesWaiting(rsp_queue)==0);
            CHECK(protocol_errors==errors_before);
        }
        LabState after = GetSnapshot();
        CHECK(after.mode==MODE_AUTO && after.rate_ms==1000 && after.led_on==1 && after.button_count==7);
        CHECK(!state_mutex->held);
    }
    Dispatch("CMD 45 RATE",0); CHECK(response("RSP 45 ERR bad_rate\n"));
    CHECK(!uxQueueMessagesWaiting(control_queue));
    state = saved;
    printf("Rate cases: %u (including missing argument); invalid input preserves state and ping/status recovery\n",
           (unsigned)(sizeof(cases)/sizeof(cases[0]))+1);
    return 0;
}
static int TestHelp(void)
{
    TxMessage help;
    Dispatch("CMD 46 HELP",0);
    CHECK(xQueueReceive(rsp_queue,&help,0));
    CHECK(strcmp(help.text,"RSP 46 OK commands=help|ping|status|tasks|queue|led on|led off|led auto|rate N (100~5000 ms)\n")==0);
    CHECK(!strstr(help.text,"led_") && !strstr(help.text,"exit"));
    CHECK(strlen(help.text)<APP_UART_TX_FRAME_BUFFER_BYTES);
    LineParser parser; line_parser_init(&parser);
    char frame[HOST_MAX_UART_LINE+1]; unsigned frames=0;
    for (size_t i=0; help.text[i]; ++i) {
        int result=line_parser_feed(&parser,(unsigned char)help.text[i],frame,sizeof(frame));
        CHECK(result!=LINE_DROPPED);
        if(result==LINE_COMPLETE) frames++;
    }
    uint32_t id;
    CHECK(frames==1 && protocol_classify(frame,&id)==FRAME_RESPONSE && id==46);
    char display[HOST_MAX_UART_LINE+1];
    CHECK(protocol_format_help(strchr(frame+4,' ')+1,display,sizeof(display)));
    CHECK(strcmp(display,"help\nping\nstatus\ntasks\nqueue\nled on\nled off\nled auto\nrate N (100~5000 ms)")==0);
    return 0;
}

int main(void)
{
    control_queue=xQueueCreate(APP_CONTROL_QUEUE_DEPTH,sizeof(ControlMessage));
    rsp_queue=xQueueCreate(APP_RSP_QUEUE_DEPTH,sizeof(TxMessage));log_queue=xQueueCreate(APP_LOG_QUEUE_DEPTH,sizeof(TxMessage));
    state_mutex=xSemaphoreCreateMutex();
    Dispatch("CMD 1 PING",0);CHECK(response("RSP 1 OK PONG\n"));
    Dispatch("CMD 0 PING",0);CHECK(response("RSP 0 ERR bad_id\n"));
    Dispatch("CMD 429496729600 PING",0);CHECK(response("RSP 0 ERR bad_id\n"));
    Dispatch("CMD 2 RATE 50",0);CHECK(response("RSP 2 ERR rate_range_100_5000\n"));
    Dispatch("CMD 3 RATE 5000",0);ControlMessage c;CHECK(xQueueReceive(control_queue,&c,0)&&c.id==3&&c.value==5000);
    Dispatch("CMD 4 LED AUTO",1);CHECK(response("RSP 4 ERR line_too_long\n"));
    Dispatch("CMD 5 BAD",0);CHECK(response("RSP 5 ERR unknown_command\n"));
    for(unsigned i=0;i<APP_CONTROL_QUEUE_DEPTH;i++) Dispatch("CMD 6 LED ON",0);
    CHECK(uxQueueMessagesWaiting(control_queue)==APP_CONTROL_QUEUE_DEPTH);
    Dispatch("CMD 7 LED OFF",0);CHECK(response("RSP 7 ERR control_queue_full\n"));CHECK(dropped_cmd==1);
    CHECK(xQueueReceive(control_queue,&c,0));Dispatch("CMD 8 LED OFF",0);CHECK(uxQueueMessagesWaiting(control_queue)==APP_CONTROL_QUEUE_DEPTH);
    for(unsigned i=0;i<APP_RSP_QUEUE_DEPTH+1;i++) { Reply(1,"OK",""); }
    CHECK(dropped_rsp==1);
    TxMessage m;while(xQueueReceive(rsp_queue,&m,0)){}
    Dispatch("CMD 9 PING",0);CHECK(response("RSP 9 OK PONG\n"));
    for(unsigned i=0;i<APP_LOG_QUEUE_DEPTH+1;i++) { Log("EVT mock"); }
    CHECK(dropped_logs==1);
    CHECK(!state_mutex->held);ChangeLed(1,MODE_ON);CHECK(GetSnapshot().led_on==1&&!state_mutex->held);
    Status(10);CHECK(xQueueReceive(rsp_queue,&m,0)&&strstr(m.text,"minheap=9000")&&strstr(m.text,"app_tasks=4"));
    CHECK(protocol_errors==6);CHECK(TaskStateChar(NULL)=='B');
    CHECK(TestRates()==0);
    CHECK(TestHelp()==0);
    printf("HOST VERIFIED: actual firmware dispatch with queue/mutex doubles, %u checks PASS (no scheduler simulation)\n",checks);
    free(control_queue->data);free(control_queue);free(rsp_queue->data);free(rsp_queue);free(log_queue->data);free(log_queue);free(state_mutex);
    return 0;
}
