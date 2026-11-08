#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>

/* Call once right after sys_clk_init(). Starts the DWT cycle counter (1 tick = 1 SYSCLK = 4 ns @ 250 MHz) */
void delay_init(void);

/* Blocking, cycle-accurate waits. Safe before HAL_Init/scheduler and inside ISRs.
 * Interrupts can only make them LONGER, never shorter. Max ~17 s per call (32-bit counter @ 250 MHz). */
void delay_cycles(uint32_t cycles);
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);

#endif /* DELAY_H */
