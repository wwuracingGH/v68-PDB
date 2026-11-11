/**********************************************************************************
 * config.h
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
#ifndef CONFIG_H
#define CONFIG_H

#define SYS_CLK_HZ      250000000UL   /* SYSCLK = HCLK = PCLK1/2/3 */
#define HSE_CLK_HZ      16000000UL    /* 16 MHz oscillator into PH0 (HSE bypass), FDCAN kernel clock */

void sys_clk_init();

#endif
