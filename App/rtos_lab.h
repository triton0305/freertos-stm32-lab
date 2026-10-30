#ifndef RTOS_LAB_H
#define RTOS_LAB_H
void Lab_Start(void);
/* Called in interrupt context, never blocks. */
void Lab_UartRxNotifyFromISR(void);
void Lab_ButtonNotifyFromISR(void);
#endif
