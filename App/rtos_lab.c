#include "rtos_lab.h"
#include "lab_time.h"
#include "app_config.h"
#include "board.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(configTICK_RATE_HZ == 1000, "Application time helpers require 1ms ticks");
_Static_assert(APP_COMMAND_PRIORITY < configMAX_PRIORITIES, "Task priority exceeds kernel limit");
typedef enum { MODE_OFF, MODE_ON, MODE_AUTO } LedMode;
typedef enum { OP_LED_OFF, OP_LED_ON, OP_LED_AUTO, OP_RATE } Operation;
typedef struct { uint32_t id; Operation op; uint32_t value; } ControlMessage;
typedef struct { char text[APP_UART_TX_FRAME_BUFFER_BYTES]; } TxMessage;
typedef struct {
    LedMode mode;
    uint32_t button_count;
    uint32_t rate_ms;
    uint8_t led_on;
} LabState;

static QueueHandle_t control_queue, rsp_queue, log_queue;
static SemaphoreHandle_t state_mutex;
static TaskHandle_t command_task, control_task, monitor_task, tx_task;
static LabState state = { MODE_OFF, 0u, APP_LED_DEFAULT_RATE_MS, 0u };
static volatile uint32_t protocol_errors;
static volatile uint32_t tx_failures, dropped_rsp, dropped_logs, dropped_cmd;
static volatile uint32_t auto_missed_periods; /* LED deadlines skipped due to scheduling latency. */
static volatile uint32_t button_notifications;

/* Mutex only protects LabState: Control writes; Monitor/Command read. */
static LabState GetSnapshot(void)
{
    LabState s;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    s = state;
    xSemaphoreGive(state_mutex);
    return s;
}
static const char *ModeText(LedMode m) { return m == MODE_AUTO ? "AUTO" : (m == MODE_ON ? "ON" : "OFF"); }
static int Emit(QueueHandle_t q, const char *str, TickType_t wait)
{
    TxMessage msg = {0};
    size_t len = strlen(str);
    if (len > APP_UART_TX_FRAME_BUFFER_BYTES - 2u) {
        taskENTER_CRITICAL();
        if (q == rsp_queue) dropped_rsp++; else dropped_logs++;
        taskEXIT_CRITICAL();
        return 0;
    }
    memcpy(msg.text, str, len);
    msg.text[len] = '\n';
    msg.text[len + 1] = '\0';
    if (xQueueSend(q, &msg, wait) == pdTRUE) return 1;
    taskENTER_CRITICAL();
    if (q == rsp_queue) dropped_rsp++;
    else dropped_logs++;
    taskEXIT_CRITICAL();
    return 0;
}
static void Reply(uint32_t id, const char *status, const char *detail)
{
    if (strcmp(status, "ERR") == 0) { taskENTER_CRITICAL(); protocol_errors++; taskEXIT_CRITICAL(); }
    char out[APP_UART_TX_FRAME_BUFFER_BYTES];
    if (detail && detail[0]) snprintf(out, sizeof(out), "RSP %lu %s %s", (unsigned long)id, status, detail);
    else snprintf(out, sizeof(out), "RSP %lu %s", (unsigned long)id, status);
    /* Never block indefinitely because the PC stopped consuming UART. */
    (void)Emit(rsp_queue, out, pdMS_TO_TICKS(APP_RSP_QUEUE_WAIT_MS));
}
static void Log(const char *str) { (void)Emit(log_queue, str, 0); }
static char TaskStateChar(TaskHandle_t h)
{
    switch (eTaskGetState(h)) {
        case eRunning: return 'X';
        case eReady: return 'R';
        case eBlocked: return 'B';
        case eSuspended: return 'S';
        case eDeleted: return 'D';
        default: return '?';
    }
}
static void Status(uint32_t id)
{
    LabState s = GetSnapshot();
    char buf[APP_UART_TX_FRAME_BUFFER_BYTES];
    snprintf(buf, sizeof(buf), "uptime_ms=%lu led=%s mode=%s rate=%lu buttons=%lu tasks=%lu app_tasks=" APP_CONFIG_TEXT(APP_TASK_COUNT) " heap=%lu minheap=%lu protoerr=%lu rxerr=%lu rxdrop=%lu txerr=%lu drops=%lu btnnotify=%lu",
        (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS), s.led_on ? "ON" : "OFF", ModeText(s.mode),
        (unsigned long)s.rate_ms, (unsigned long)s.button_count, (unsigned long)uxTaskGetNumberOfTasks(),
        (unsigned long)xPortGetFreeHeapSize(), (unsigned long)xPortGetMinimumEverFreeHeapSize(), (unsigned long)protocol_errors, (unsigned long)Board_RxErrors(), (unsigned long)Board_RxOverflow(),
        (unsigned long)tx_failures, (unsigned long)(dropped_rsp + dropped_logs + dropped_cmd),
        (unsigned long)button_notifications);
    Reply(id, "OK", buf);
}
static void Tasks(uint32_t id)
{
    char buf[APP_UART_TX_FRAME_BUFFER_BYTES];
    /* State: X running, R ready, B blocked, S suspended. Watermark unit: words. */
    snprintf(buf, sizeof(buf), "tasks=%lu cmd=%c,%lu,%lu ctrl=%c,%lu,%lu mon=%c,%lu,%lu tx=%c,%lu,%lu idle=%c,%lu,%lu app_tasks=" APP_CONFIG_TEXT(APP_TASK_COUNT) " unit=words",
        (unsigned long)uxTaskGetNumberOfTasks(),
        TaskStateChar(command_task), (unsigned long)uxTaskPriorityGet(command_task), (unsigned long)uxTaskGetStackHighWaterMark(command_task),
        TaskStateChar(control_task), (unsigned long)uxTaskPriorityGet(control_task), (unsigned long)uxTaskGetStackHighWaterMark(control_task),
        TaskStateChar(monitor_task), (unsigned long)uxTaskPriorityGet(monitor_task), (unsigned long)uxTaskGetStackHighWaterMark(monitor_task),
        TaskStateChar(tx_task), (unsigned long)uxTaskPriorityGet(tx_task), (unsigned long)uxTaskGetStackHighWaterMark(tx_task),
        TaskStateChar(xTaskGetIdleTaskHandle()), (unsigned long)uxTaskPriorityGet(xTaskGetIdleTaskHandle()), (unsigned long)uxTaskGetStackHighWaterMark(xTaskGetIdleTaskHandle()));
    Reply(id, "OK", buf);
}
static void Queues(uint32_t id)
{
    char buf[APP_UART_TX_FRAME_BUFFER_BYTES];
    snprintf(buf, sizeof(buf), "cmd=%lu/%u rsp=%lu/%u log=%lu/%u cmddrop=%lu rspdrop=%lu logdrop=%lu rxdrop=%lu",
        (unsigned long)uxQueueMessagesWaiting(control_queue), APP_CONTROL_QUEUE_DEPTH,
        (unsigned long)uxQueueMessagesWaiting(rsp_queue), APP_RSP_QUEUE_DEPTH,
        (unsigned long)uxQueueMessagesWaiting(log_queue), APP_LOG_QUEUE_DEPTH,
        (unsigned long)dropped_cmd, (unsigned long)dropped_rsp, (unsigned long)dropped_logs, (unsigned long)Board_RxOverflow());
    Reply(id, "OK", buf);
}
/* Syntax is checked before range; reject overflow identically on ARM/Host. */
static const char *ParseRate(const char *text, uint32_t *value)
{
    uint32_t number = 0;
    if (*text == '\0') return "bad_rate";
    for (const char *p = text; *p; ++p) {
        if (*p < '0' || *p > '9') return "bad_rate";
        uint32_t digit = (uint32_t)(*p - '0');
        if (number > (UINT32_MAX - digit) / 10u) return "bad_rate";
        number = number * 10u + digit;
    }
    if (number < APP_LED_MIN_RATE_MS || number > APP_LED_MAX_RATE_MS)
        return "rate_range_" APP_CONFIG_TEXT(APP_LED_MIN_RATE_MS) "_" APP_CONFIG_TEXT(APP_LED_MAX_RATE_MS);
    *value = number;
    return NULL;
}

/* CMD <positive-id> <keyword...>; one RSP line completes each query. */
static void Dispatch(const char *line, int too_long)
{
    uint32_t id = 0;
    if (strncmp(line, "CMD ", 4) != 0) { Reply(0, "ERR", "bad_frame"); return; }
    const char *p = line + 4;
    if (*p < '0' || *p > '9') { Reply(0, "ERR", "bad_id"); return; }
    char *end = 0;
    unsigned long parsed = strtoul(p, &end, 10);
    if (parsed == 0 || parsed > 2147483647UL || end == p || *end != ' ') { Reply(0, "ERR", "bad_id"); return; }
    id = (uint32_t)parsed;
    if (too_long) { Reply(id, "ERR", "line_too_long"); return; }
    const char *cmd = end + 1;
    if (strcmp(cmd, "PING") == 0) { Reply(id, "OK", "PONG"); return; }
    if (strcmp(cmd, "HELP") == 0) {
        Reply(id, "OK", "commands=help|ping|status|tasks|queue|led on|led off|led auto|rate N ("
              APP_CONFIG_TEXT(APP_LED_MIN_RATE_MS) "~" APP_CONFIG_TEXT(APP_LED_MAX_RATE_MS) " ms)");
        return;
    }
    if (strcmp(cmd, "STATUS") == 0) { Status(id); return; }
    if (strcmp(cmd, "TASKS") == 0) { Tasks(id); return; }
    if (strcmp(cmd, "QUEUE") == 0) { Queues(id); return; }
    ControlMessage m = {id, OP_LED_OFF, 0};
    if      (strcmp(cmd, "LED OFF") == 0) m.op = OP_LED_OFF;
    else if (strcmp(cmd, "LED ON") == 0) m.op = OP_LED_ON;
    else if (strcmp(cmd, "LED AUTO") == 0) m.op = OP_LED_AUTO;
    else if (strcmp(cmd, "RATE") == 0 || strncmp(cmd, "RATE ", 5) == 0) {
        const char *error = ParseRate(cmd + (cmd[4] ? 5 : 4), &m.value);
        if (error) { Reply(id, "ERR", error); return; }
        m.op = OP_RATE;
    } else { Reply(id, "ERR", "unknown_command"); return; }
    /* Control queue is truly used; overflow returns an explicit ERR. */
    if (xQueueSend(control_queue, &m, 0) != pdPASS) {
        dropped_cmd++;
        Reply(id, "ERR", "control_queue_full");
    }
}
static void CommandTask(void *arg)
{
    (void)arg;
    char line[APP_UART_RX_LINE_BUFFER_BYTES] = {0};
    size_t used = 0;
    int overflowing = 0;
    uint32_t last_ring_overflow = Board_RxOverflow();
    uint32_t last_uart_error = Board_RxErrors();
    for (;;) {
        /* IRQ notifies on bytes, but our SPSC ring retains them even if notifications coalesce. */
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(APP_RX_NOTIFY_WAIT_MS));
        uint8_t ch;
        unsigned batch = 0;
        while (Board_RxPop(&ch)) {
            if (++batch == APP_RX_BATCH_BYTES) { batch = 0; vTaskDelay(pdMS_TO_TICKS(APP_RX_YIELD_MS)); }
            /* A dropped UART byte invalidates the entire current line. */
            uint32_t overflow_now = Board_RxOverflow();
            if (overflow_now != last_ring_overflow) {
                overflowing = 1;
                last_ring_overflow = overflow_now;
            }
            uint32_t error_now = Board_RxErrors();
            if (error_now != last_uart_error) {
                overflowing = 1;
                last_uart_error = error_now;
            }
            if (ch == '\n') {
                line[used] = '\0';
                if (used || overflowing) Dispatch(line, overflowing);
                used = 0; overflowing = 0;
            } else if (ch != '\r' && !overflowing) {
                if (ch < 32u || ch > 126u) { overflowing = 1; }
                else if (used < sizeof(line) - 1u) line[used++] = (char)ch;
                else overflowing = 1;
            }
        }
    }
}
static void ChangeLed(int next, LedMode mode)
{
    LabState before;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    before = state;
    state.led_on = (uint8_t)(next != 0);
    state.mode = mode;
    xSemaphoreGive(state_mutex);
    Board_LedSet(next);
    if (before.led_on != (uint8_t)(next != 0)) {
        char e[128];
        snprintf(e, sizeof(e), "EVT led %s %s mode=%s rate_ms=%lu uptime_ms=%lu",
            before.led_on ? "ON" : "OFF", next ? "ON" : "OFF", ModeText(mode), (unsigned long)before.rate_ms,
            (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS));
        Log(e);
    }
}
static void ControlTask(void *arg)
{
    (void)arg;
    LabDebounce button = {0};
    TickType_t next_toggle = xTaskGetTickCount();
    ControlMessage msg;
    for (;;) {
        if (xQueueReceive(control_queue, &msg, pdMS_TO_TICKS(APP_CONTROL_QUEUE_WAIT_MS)) == pdTRUE) {
            LabState old = GetSnapshot();
            if (msg.op == OP_LED_OFF) ChangeLed(0, MODE_OFF);
            else if (msg.op == OP_LED_ON) ChangeLed(1, MODE_ON);
            else if (msg.op == OP_LED_AUTO) { ChangeLed(0, MODE_AUTO); next_toggle = xTaskGetTickCount() + pdMS_TO_TICKS(old.rate_ms); }
            else if (msg.op == OP_RATE) {
                xSemaphoreTake(state_mutex, portMAX_DELAY);
                state.rate_ms = msg.value;
                xSemaphoreGive(state_mutex);
                next_toggle = xTaskGetTickCount() + pdMS_TO_TICKS(msg.value);
            }
            LabState current = GetSnapshot();
            char config[128];
            snprintf(config, sizeof(config), "EVT config mode=%s rate_ms=%lu uptime_ms=%lu",
                ModeText(current.mode), (unsigned long)current.rate_ms, (unsigned long)xTaskGetTickCount());
            Log(config);
            char detail[32];
            snprintf(detail, sizeof(detail), "rate_ms=%lu", (unsigned long)current.rate_ms);
            Reply(msg.id, "OK", detail);
        }
        /* IRQ notification coalesces edges; sampled level must remain stable. */
        button_notifications += ulTaskNotifyTake(pdTRUE, 0);
        if (lab_debounce(&button, xTaskGetTickCount(), Board_ButtonPressed())) {
            xSemaphoreTake(state_mutex, portMAX_DELAY);
            uint32_t count = ++state.button_count;
            xSemaphoreGive(state_mutex);
            char event[100];
            snprintf(event, sizeof(event), "EVT button pressed count=%lu uptime_ms=%lu",
                (unsigned long)count, (unsigned long)xTaskGetTickCount());
            Log(event);
        }
        LabState s = GetSnapshot();
        TickType_t now = xTaskGetTickCount();
        if (s.mode == MODE_AUTO && lab_due(now, next_toggle)) {
            ChangeLed(!s.led_on, MODE_AUTO);
            /* Advance from scheduled deadline, not from actual execution time.
             * This preserves phase without doing burst catch-up toggles.
             * Missed periods are counted rather than silently compressed. */
            TickType_t period = pdMS_TO_TICKS(s.rate_ms);
            auto_missed_periods += lab_advance(now, &next_toggle, period);
        }
    }
}
static void MonitorTask(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    Log("EVT boot STM32F411RE");
    Log("EVT scheduler started");
    for (;;) {
        /* Fixed periodic schedule, not vTaskDelay drift. */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(APP_STATUS_PERIOD_MS));
        LabState s = GetSnapshot();
        char out[APP_UART_TX_FRAME_BUFFER_BYTES];
        /* last_wake is now the scheduled (not actual) release time. */
        int32_t jitter_ticks = (int32_t)(xTaskGetTickCount() - last_wake);
        snprintf(out, sizeof(out), "STAT uptime_ms=%lu led=%s mode=%s buttons=%lu tasks=%lu queue=%lu heap=%lu rxerr=%lu drops=%lu jitter_ms=%ld missed=%lu",
            (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS), s.led_on ? "ON" : "OFF",
            ModeText(s.mode), (unsigned long)s.button_count, (unsigned long)uxTaskGetNumberOfTasks(),
            (unsigned long)uxQueueMessagesWaiting(control_queue), (unsigned long)xPortGetFreeHeapSize(),
            (unsigned long)Board_RxErrors(), (unsigned long)(dropped_rsp + dropped_logs + dropped_cmd + Board_RxOverflow()),
            (long)(jitter_ticks * portTICK_PERIOD_MS), (unsigned long)auto_missed_periods);
        Log(out);
        /* If execution missed whole releases, skip those slots; never emit a
         * burst of historical STATUS frames pretending time passed again. */
        TickType_t late = xTaskGetTickCount() - last_wake;
        last_wake += (late / pdMS_TO_TICKS(APP_STATUS_PERIOD_MS)) * pdMS_TO_TICKS(APP_STATUS_PERIOD_MS);
    }
}
static void TxTask(void *arg)
{
    (void)arg;
    TxMessage msg;
    for (;;) {
        /* Responses outrank monitor/status frames; low lane drops if saturated. */
        if (xQueueReceive(rsp_queue, &msg, 0) == pdTRUE ||
            xQueueReceive(log_queue, &msg, pdMS_TO_TICKS(APP_LOG_QUEUE_WAIT_MS)) == pdTRUE) {
            if (!Board_UartSend(msg.text)) tx_failures++;
        }
    }
}
void Lab_UartRxNotifyFromISR(void)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        BaseType_t higher = pdFALSE;
        vTaskNotifyGiveFromISR(command_task, &higher);
        portYIELD_FROM_ISR(higher);
    }
}
void Lab_ButtonNotifyFromISR(void)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        BaseType_t higher = pdFALSE;
        vTaskNotifyGiveFromISR(control_task, &higher);
        portYIELD_FROM_ISR(higher);
    }
}
void vApplicationMallocFailedHook(void) { taskDISABLE_INTERRUPTS(); for (;;) {} }
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task; (void)name;
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}
void Lab_Start(void)
{
    control_queue = xQueueCreate(APP_CONTROL_QUEUE_DEPTH, sizeof(ControlMessage));
    rsp_queue = xQueueCreate(APP_RSP_QUEUE_DEPTH, sizeof(TxMessage));
    log_queue = xQueueCreate(APP_LOG_QUEUE_DEPTH, sizeof(TxMessage));
    state_mutex = xSemaphoreCreateMutex();
    configASSERT(control_queue && rsp_queue && log_queue && state_mutex);
    /* Stack depth is WORDS, not bytes. Priorities: RX 4 > Control 3 > Monitor 2 > TX 1. */
    configASSERT(xTaskCreate(CommandTask, "Command", APP_COMMAND_STACK_WORDS, NULL, APP_COMMAND_PRIORITY, &command_task) == pdPASS);
    configASSERT(xTaskCreate(ControlTask, "Control", APP_CONTROL_STACK_WORDS, NULL, APP_CONTROL_PRIORITY, &control_task) == pdPASS);
    configASSERT(xTaskCreate(TxTask, "UartTx", APP_UART_TX_STACK_WORDS, NULL, APP_UART_TX_PRIORITY, &tx_task) == pdPASS);
    configASSERT(xTaskCreate(MonitorTask, "Monitor", APP_MONITOR_STACK_WORDS, NULL, APP_MONITOR_PRIORITY, &monitor_task) == pdPASS);
    Board_EnableRxAndButtonIrq();
    vTaskStartScheduler();
    Error_Handler();
}
