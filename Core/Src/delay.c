/**********************************************************************************
 * delay.c
 *
 * Cycle-accurate blocking delays using the Cortex-M33 DWT cycle counter.
 * Unsigned subtraction makes the compare safe across counter wrap.
 **********************************************************************************/
#include "delay.h"
#include "config.h"
#include "stm32h533xx.h"

#define CYCLES_PER_US (SYS_CLK_HZ / 1000000UL)

void delay_init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;   /* enable DWT block */
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;             /* start counter */
}

void delay_cycles(uint32_t cycles) {
    uint32_t start = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - start) < cycles);
}

void delay_us(uint32_t us) {
    delay_cycles(us * CYCLES_PER_US);
}

void delay_ms(uint32_t ms) {
    /* Loop in 1 ms chunks so large values don't overflow the 32-bit cycle count */
    while (ms--) delay_cycles(1000UL * CYCLES_PER_US);
}
