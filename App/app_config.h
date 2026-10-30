#ifndef LAB_APP_CONFIG_H
#define LAB_APP_CONFIG_H

/* Application policy shared by firmware and portable Host validation.
 * Kernel/IRQ settings stay in FreeRTOSConfig.h; pins/clocks stay in board.c. */
#define APP_TASK_COUNT                    4
#define APP_COMMAND_PRIORITY              4
#define APP_CONTROL_PRIORITY              3
#define APP_MONITOR_PRIORITY              2
#define APP_UART_TX_PRIORITY              1
#define APP_COMMAND_STACK_WORDS           512
#define APP_CONTROL_STACK_WORDS           512
#define APP_MONITOR_STACK_WORDS           512
#define APP_UART_TX_STACK_WORDS           512

#define APP_CONTROL_QUEUE_DEPTH           8
#define APP_RSP_QUEUE_DEPTH               8
#define APP_LOG_QUEUE_DEPTH               12
#define APP_UART_RX_RING_BYTES            256
#define APP_UART_RX_LINE_BUFFER_BYTES     128
#define APP_UART_TX_FRAME_BUFFER_BYTES    320
#define APP_CLI_COMMAND_MAX_BYTES         110

#define APP_LED_DEFAULT_RATE_MS           500
#define APP_LED_MIN_RATE_MS               100
#define APP_LED_MAX_RATE_MS               5000
#define APP_BUTTON_DEBOUNCE_MS            50
#define APP_STATUS_PERIOD_MS              1000
#define APP_RX_NOTIFY_WAIT_MS             50
#define APP_RX_BATCH_BYTES                64
#define APP_RX_YIELD_MS                   1
#define APP_CONTROL_QUEUE_WAIT_MS         10
#define APP_RSP_QUEUE_WAIT_MS             20
#define APP_LOG_QUEUE_WAIT_MS             5
#define APP_UART_TX_TIMEOUT_MS            40

#define APP_CONFIG_TEXT_IMPL(value) #value
#define APP_CONFIG_TEXT(value) APP_CONFIG_TEXT_IMPL(value)

_Static_assert(APP_COMMAND_PRIORITY > APP_CONTROL_PRIORITY &&
               APP_CONTROL_PRIORITY > APP_MONITOR_PRIORITY &&
               APP_MONITOR_PRIORITY > APP_UART_TX_PRIORITY &&
               APP_UART_TX_PRIORITY > 0, "Application priority order must be preserved");
_Static_assert(APP_LED_MIN_RATE_MS > 0 &&
               APP_LED_MIN_RATE_MS <= APP_LED_DEFAULT_RATE_MS &&
               APP_LED_DEFAULT_RATE_MS <= APP_LED_MAX_RATE_MS,
               "Invalid LED rate configuration");
_Static_assert(APP_UART_RX_RING_BYTES > 1 && APP_UART_RX_RING_BYTES <= 65536 &&
               (APP_UART_RX_RING_BYTES & (APP_UART_RX_RING_BYTES - 1)) == 0,
               "RX ring must fit uint16 indices and be a power of two");
/* CMD + space + 10-digit ID + space + command + LF + NUL. */
_Static_assert(APP_CLI_COMMAND_MAX_BYTES + 17 <= APP_UART_RX_LINE_BUFFER_BYTES,
               "CLI command must fit the firmware RX line");
_Static_assert(APP_RX_YIELD_MS > 0 && APP_STATUS_PERIOD_MS > 0 &&
               APP_BUTTON_DEBOUNCE_MS > 0, "Timing periods must be nonzero");
#endif
