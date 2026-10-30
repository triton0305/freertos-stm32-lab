#include "board.h"
#include "app_config.h"
#include "rtos_lab.h"
#include "stm32f4xx_hal.h"        /* Standalone HAL, no production include path. */
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

static UART_HandleTypeDef uart2;
static volatile uint8_t rx_ring[APP_UART_RX_RING_BYTES];
static volatile uint16_t rx_head, rx_tail;
static volatile uint32_t rx_errors, rx_overflow;
static int rx_discard;

/* TIM5 is present on F411. TIM6 is NOT. FreeRTOS alone owns SysTick. */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    (void)TickPriority;
    __HAL_RCC_TIM5_CLK_ENABLE();
    TIM5->CR1 = 0;
    TIM5->DIER = 0;
    TIM5->SR = 0;
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    uint32_t timer_clk = ((RCC->CFGR & RCC_CFGR_PPRE1) == 0u) ? pclk1 : (pclk1 * 2u);
    if (timer_clk < 1000000u) return HAL_ERROR;
    TIM5->PSC = (timer_clk / 1000000u) - 1u;
    TIM5->ARR = 999u;
    TIM5->EGR = TIM_EGR_UG;
    TIM5->SR = 0;
    HAL_NVIC_SetPriority(TIM5_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(TIM5_IRQn);
    TIM5->DIER = TIM_DIER_UIE;
    TIM5->CR1 = TIM_CR1_CEN;
    return HAL_OK;
}
void HAL_SuspendTick(void) { TIM5->DIER &= ~TIM_DIER_UIE; }
void HAL_ResumeTick(void)  { TIM5->DIER |= TIM_DIER_UIE; }
void TIM5_IRQHandler(void)
{
    if (TIM5->SR & TIM_SR_UIF) { TIM5->SR = 0; HAL_IncTick(); }
}
void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    /* All four NVIC priority bits are preempt priority (no subpriorities). */
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
}
void HAL_UART_MspInit(UART_HandleTypeDef *h)
{
    if (h->Instance != USART2) return;
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef io = {0};
    io.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    io.Mode = GPIO_MODE_AF_PP;
    io.Pull = GPIO_NOPULL;
    io.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    io.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &io);
}
static void SystemClock_84MHz(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM = 16;
    osc.PLL.PLLN = 336;
    osc.PLL.PLLP = RCC_PLLP_DIV4;
    osc.PLL.PLLQ = 4;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();
    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}
void Board_Init(void)
{
    HAL_Init();
    SystemClock_84MHz();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef io = {0};
    io.Pin = GPIO_PIN_5;
    io.Mode = GPIO_MODE_OUTPUT_PP;
    io.Pull = GPIO_NOPULL;
    io.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &io);
    Board_LedSet(0);
    /* MB1136 B1 shorts PC13 to GND. Both edges wake the debounce consumer. */
    io.Pin = GPIO_PIN_13;
    io.Mode = GPIO_MODE_IT_RISING_FALLING;
    io.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &io);
    uart2.Instance = USART2;
    uart2.Init.BaudRate = 115200;
    uart2.Init.WordLength = UART_WORDLENGTH_8B;
    uart2.Init.StopBits = UART_STOPBITS_1;
    uart2.Init.Parity = UART_PARITY_NONE;
    uart2.Init.Mode = UART_MODE_TX_RX;
    uart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    uart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&uart2) != HAL_OK) Error_Handler();
}
void Board_EnableRxAndButtonIrq(void)
{
    HAL_NVIC_SetPriority(USART2_IRQn, 6, 0); /* >= 5 permits FromISR calls */
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
    USART2->CR1 |= USART_CR1_RXNEIE;
    USART2->CR3 |= USART_CR3_EIE;
}
void Board_LedSet(int on)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
int Board_ButtonPressed(void)
{
    return HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET;
}
int Board_UartSend(const char *s)
{
    /* Finite timeout; only TX Task may call this. No mutex held here. */
    uint16_t len = (uint16_t)strlen(s);
    int ok = HAL_UART_Transmit(&uart2, (uint8_t *)s, len, APP_UART_TX_TIMEOUT_MS) == HAL_OK;
    /* HAL timeout paths may disable RX/error IRQs; preserve the RX owner. */
    if (!ok) { USART2->CR1 |= USART_CR1_RXNEIE; USART2->CR3 |= USART_CR3_EIE; }
    return ok;
}
uint32_t Board_RxErrors(void) { return rx_errors; }
uint32_t Board_RxOverflow(void) { return rx_overflow; }
int Board_RxPop(uint8_t *ch)
{
    taskENTER_CRITICAL();
    uint16_t tail = rx_tail;
    if (tail == rx_head) { taskEXIT_CRITICAL(); return 0; }
    *ch = rx_ring[tail];
    rx_tail = (uint16_t)((tail + 1u) & (APP_UART_RX_RING_BYTES - 1u));
    taskEXIT_CRITICAL();
    return 1;
}
static void RxLost(void)
{
    /* Flush unread backlog and inject an invalid byte. Task drops through the
     * next physically received LF, never executes a suffix after a lost byte.
     * RX pop masks this priority-6 IRQ while touching the ring. */
    rx_tail = rx_head;
    rx_ring[rx_head] = 0;
    rx_head = (uint16_t)((rx_head + 1u) & (APP_UART_RX_RING_BYTES - 1u));
    rx_discard = 1;
    Lab_UartRxNotifyFromISR();
}
void USART2_IRQHandler(void)
{
    uint32_t sr = USART2->SR;
    uint32_t dr = USART2->DR; /* SR then DR clears ORE, FE, NE, PE. */
    if (sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) {
        rx_errors++;
        RxLost();
        return;
    }
    if ((sr & USART_SR_RXNE) != 0u) {
        if (rx_discard) {
            if ((uint8_t)dr != '\n') return;
            rx_discard = 0;
        }
        uint16_t head = rx_head;
        uint16_t next = (uint16_t)((head + 1u) & (APP_UART_RX_RING_BYTES - 1u));
        if (next == rx_tail) { rx_overflow++; RxLost(); }
        else {
            rx_ring[head] = (uint8_t)dr;
            __DMB();
            rx_head = next;
            Lab_UartRxNotifyFromISR();
        }
    }
}
void EXTI15_10_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13); }
void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    if (pin == GPIO_PIN_13) Lab_ButtonNotifyFromISR();
}
void Error_Handler(void)
{
    __disable_irq();
    for (;;) { __NOP(); }
}
/* Port C startup avoids compiling production main.c/interrupt handler sources. */
int main(void)
{
    Board_Init();
    Lab_Start(); /* creates tasks and starts scheduler; should never return */
    Error_Handler();
    return 0;
}
