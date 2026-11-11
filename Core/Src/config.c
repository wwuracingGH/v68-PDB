/**********************************************************************************
 * config.c
 *
 * Author:  WWU FSAE 
 *          Jackson Newman
 * 
 * Last Uptdated: 11/6/26
 *
 * Version: 0.1 v68
 *
 * Function:
 **********************************************************************************/

#include "config.h"
#include "stm32h5xx.h"
#include "FreeRTOS.h"

#define HSI_HZ   64000000UL
#define PLL1_M   16U
#define PLL1_N   125U
#define PLL1_P   2U
#define PLL1_Q   2U
#define PLL1_R   2U
/* SYSCLK = ((HSI/M)*N)/P */
/********************************************************************************
 * void sys_clk_init()
 *
 * Function:
 *      Run SYSCLK at 250 MHz from HSI through PLL1, and start the 16 MHz HSE
 *      oscillator (bypass, PH0) that clocks FDCAN. Call first thing in main, before any
 *      peripheral init or starting the scheduler.
 ********************************************************************************/
void sys_clk_init(){
    /* Core voltage to VOS0 (11), required above 200 MHz. Must happen before raising the clock */
    PWR->VOSCR = (PWR->VOSCR & ~PWR_VOSCR_VOS_Msk) | (3U << PWR_VOSCR_VOS_Pos);
    while (!(PWR->VOSSR & PWR_VOSSR_VOSRDY));

    /* Flash: 5 wait states + WRHIGHFREQ 2 for 210-250 MHz at VOS0 (RM0481 Table 48), prefetch on.
     * Clear the fields first, reset value is LATENCY = 3, WRHIGHFREQ = 1 */
    FLASH->ACR = (FLASH->ACR & ~(FLASH_ACR_LATENCY_Msk | FLASH_ACR_WRHIGHFREQ_Msk))
               | (5U << FLASH_ACR_LATENCY_Pos)
               | (2U << FLASH_ACR_WRHIGHFREQ_Pos)
               | FLASH_ACR_PRFTEN;
    while ((FLASH->ACR & FLASH_ACR_LATENCY_Msk) != (5U << FLASH_ACR_LATENCY_Pos));

    /* Set HSIDIV /1 */
    RCC->CR &= ~RCC_CR_HSIDIV_Msk;
    while (!(RCC->CR & RCC_CR_HSIDIVF));

    /* HSE: 16 MHz oscillator module driving PH0 only, so bypass mode (PH1 is free GPIO).
     * HSEEXT = 1: digital bypass. The oscillator has an HCMOS output (full-swing square wave).
     * (HSEEXT = 0, analog bypass, would only be for a sine / clipped-sine output.)
     * HSEBYP and HSEEXT must be set while HSEON = 0. Clocks FDCAN1/2 directly, see below.
     * Hangs here if the oscillator isn't running */
    RCC->CR |= RCC_CR_HSEBYP | RCC_CR_HSEEXT;
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    /* PLL1 source/range/prescaler. Only writable while PLL1 is off.
     * 
     * VCOSEL left 0 = wide VCO range (128-560 MHz) */
    RCC->PLL1CFGR = (1U << RCC_PLL1CFGR_PLL1SRC_Pos)      /* 01 = HSI */
                  | (2U << RCC_PLL1CFGR_PLL1RGE_Pos)
                  | (PLL1_M << RCC_PLL1CFGR_PLL1M_Pos)
                  | RCC_PLL1CFGR_PLL1PEN
                  | RCC_PLL1CFGR_PLL1QEN;

    /* N, P, Q, R are written as (value - 1) */
    /* PLLxM is the pre - divider 00 = 1-2M, 01 = 2-4M, 10 = 4-8M, 11= 8-16 */
    /* Fvco = Fref*DIVN */
    /* FPLL_y_CK = ( Fvco/( DIVy + 1 ) ) */
    /* (64M/16)*125 */
    /* R, Q, P are seperate outputs with seperate dividers */
    RCC->PLL1DIVR = ((PLL1_N - 1U) << RCC_PLL1DIVR_PLL1N_Pos)
                  | ((PLL1_P - 1U) << RCC_PLL1DIVR_PLL1P_Pos)
                  | ((PLL1_Q - 1U) << RCC_PLL1DIVR_PLL1Q_Pos)
                  | ((PLL1_R - 1U) << RCC_PLL1DIVR_PLL1R_Pos);

    /* Turn PLL1 ON*/
    RCC->CR |= RCC_CR_PLL1ON;
    while (!(RCC->CR & RCC_CR_PLL1RDY));    /* Wait until its ready */

    /* AHB and APB1/2/3 prescalers /1, all buses at 250 MHz (max for VOS0) */
    RCC->CFGR2 &= ~(RCC_CFGR2_HPRE_Msk | RCC_CFGR2_PPRE1_Msk | RCC_CFGR2_PPRE2_Msk | RCC_CFGR2_PPRE3_Msk);

    /* SYSCLK source = PLL1 P (11), wait until hardware reports the switch */
    RCC->CFGR1 = (RCC->CFGR1 & ~RCC_CFGR1_SW_Msk) | (3U << RCC_CFGR1_SW_Pos);
    while ((RCC->CFGR1 & RCC_CFGR1_SWS_Msk) != (3U << RCC_CFGR1_SWS_Pos));

    /* FDCAN kernel clock = HSE (00): exact 16 MHz, divides evenly into 250k and 1M (can.h) */
    RCC->CCIPR5 &= ~RCC_CCIPR5_FDCANSEL_Msk;

    /* Keep CMSIS SystemCoreClock in sync with the new clock */
    SystemCoreClockUpdate();
}

