#ifndef LAB_BOARD_H
#define LAB_BOARD_H
#include <stdint.h>
void Board_Init(void);
void Board_EnableRxAndButtonIrq(void);
void Board_LedSet(int on);
int Board_ButtonPressed(void);
int Board_UartSend(const char *text);
uint32_t Board_RxErrors(void);
uint32_t Board_RxOverflow(void);
int Board_RxPop(uint8_t *ch);
void Error_Handler(void);
#endif
