#ifndef FAKE_FREERTOS_H
#define FAKE_FREERTOS_H
/* Host queue/parser test double only; never included by ARM build. */
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
typedef uint32_t TickType_t;
typedef unsigned UBaseType_t;
typedef int BaseType_t;
typedef void *TaskHandle_t;
typedef struct FakeQueue *QueueHandle_t;
typedef struct FakeMutex *SemaphoreHandle_t;
typedef enum {eRunning,eReady,eBlocked,eSuspended,eDeleted,eInvalid} eTaskState;
#define configTICK_RATE_HZ 1000
#define configMAX_PRIORITIES 5
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(x) ((TickType_t)(x))
#define taskSCHEDULER_NOT_STARTED 0
#define taskSCHEDULER_RUNNING 1
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#define taskDISABLE_INTERRUPTS() ((void)0)
#define portYIELD_FROM_ISR(x) ((void)(x))
#define configASSERT(x) do { if (!(x)) abort(); } while (0)
QueueHandle_t xQueueCreate(unsigned,size_t);
int xQueueSend(QueueHandle_t,const void *,TickType_t);
int xQueueReceive(QueueHandle_t,void *,TickType_t);
unsigned uxQueueMessagesWaiting(QueueHandle_t);
SemaphoreHandle_t xSemaphoreCreateMutex(void);
int xSemaphoreTake(SemaphoreHandle_t,TickType_t);
int xSemaphoreGive(SemaphoreHandle_t);
TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t);
void vTaskDelayUntil(TickType_t *,TickType_t);
uint32_t ulTaskNotifyTake(int,TickType_t);
void vTaskNotifyGiveFromISR(TaskHandle_t,BaseType_t *);
int xTaskGetSchedulerState(void);
void vTaskStartScheduler(void);
int xTaskCreate(void (*)(void *),const char *,unsigned,void *,unsigned,TaskHandle_t *);
eTaskState eTaskGetState(TaskHandle_t);
unsigned uxTaskPriorityGet(TaskHandle_t);
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t);
unsigned uxTaskGetNumberOfTasks(void);
TaskHandle_t xTaskGetIdleTaskHandle(void);
size_t xPortGetFreeHeapSize(void);
size_t xPortGetMinimumEverFreeHeapSize(void);
#endif
