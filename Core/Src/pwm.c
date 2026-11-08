/**********************************************************************************
 * pwm.c
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
#include "stm32h5xx.h"
#include "config.h"
#include "main.h"
#include "delay.h"
#include "FreeRTOS.h"

#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

/* Frequency = timer clock / ((PSC + 1) × (ARR + 1)) */
#define TIM_CLK_HZ   250000000U
#define PUMP_PWM_FREQ_HZ  150U
#define FAN_PW_FREQ_HZ  1000U
/* PB4 is pump */
/* PB3 is fan */



#define FAN_PWM_ARR     (TIM_CLK_HZ / FAN_PWM_FREQ_HZ - 1U)   /* 9999 */
#define PUMP_PWM_ARR    (TIM_CLK_HZ / PUMP_PWM_FREQ_HZ)
