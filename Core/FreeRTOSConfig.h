#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H
#include <stdint.h>
extern uint32_t SystemCoreClock;
#define configUSE_PREEMPTION                       1
#define configUSE_TIME_SLICING                     1
#define configUSE_TICKLESS_IDLE                    0
#define configCPU_CLOCK_HZ                         ( SystemCoreClock )
#define configTICK_RATE_HZ                         ( (TickType_t)1000 )
#define configMAX_PRIORITIES                       5
#define configMINIMAL_STACK_SIZE                   128
#define configMAX_TASK_NAME_LEN                    12
#define configTOTAL_HEAP_SIZE                      ( 32 * 1024 )
#define configUSE_16_BIT_TICKS                     0
#define configUSE_MUTEXES                          1
#define configUSE_RECURSIVE_MUTEXES                0
#define configUSE_COUNTING_SEMAPHORES              0
#define configUSE_TASK_NOTIFICATIONS               1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES      1
#define configSUPPORT_DYNAMIC_ALLOCATION           1
#define configSUPPORT_STATIC_ALLOCATION            0
#define configUSE_TIMERS                           0
#define configCHECK_FOR_STACK_OVERFLOW             2
#define configUSE_MALLOC_FAILED_HOOK               1
#define configUSE_IDLE_HOOK                        0
#define configUSE_TICK_HOOK                        0
#define configUSE_TRACE_FACILITY                   1
#define configUSE_STATS_FORMATTING_FUNCTIONS       0
#define configUSE_CO_ROUTINES                      0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION    1
#define configPRIO_BITS                            4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY    15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY            ( 15 << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY       ( 5 << (8 - configPRIO_BITS) )
#define INCLUDE_vTaskDelay                         1
#define INCLUDE_vTaskDelayUntil                    1
#define INCLUDE_uxTaskGetStackHighWaterMark        1
#define INCLUDE_eTaskGetState                      1
#define INCLUDE_vTaskPrioritySet                   0
#define INCLUDE_uxTaskPriorityGet                  1
#define INCLUDE_xTaskGetSchedulerState             1
#define INCLUDE_xTaskGetIdleTaskHandle              1
#define configASSERT(x) do { if ((x) == 0) { __asm volatile("cpsid i"); for (;;) {} } } while (0)
/* The FreeRTOS ARM_CM4F port owns the exception vectors in this opt-in lab. */
#define vPortSVCHandler                            SVC_Handler
#define xPortPendSVHandler                         PendSV_Handler
#define xPortSysTickHandler                        SysTick_Handler
#endif
