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
 *      Configure the PWM for the pump and fan. The pump requires a special
 *      wake up sequence.
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
/* Duty cycle = (CCR/(ARR+1))*/
#define TIM_CLK_HZ      250000000U
#define PWM_PSC         249U                               /* 250 MHz / 250 = 1 MHz tick */
#define TICK_HZ         (TIM_CLK_HZ / (PWM_PSC + 1U))
#define PUMP_PWM_FREQ   100U
#define FAN_PWM_FREQ    1000U
#define FAN_PWM_ARR     (TICK_HZ / FAN_PWM_FREQ - 1U)   /* 999  -> 1 kHz  */
#define PUMP_PWM_ARR    (TICK_HZ / PUMP_PWM_FREQ - 1U)  /* 9999 -> 100 Hz, TIM3 is 16-bit (max 65535) */
/* Duty Cycle: • 0 – 1% • 1 – 7% • 8 – 12% • 13 – 85% • 86 – 97% • 98 – 100% → Stop → Emergency running (about 95% speed) → Stop / Error Reset → Controlled operation from min to max speed → Maximum speed → Emergency running (about 95% speed)
*/

/**********************************************************************************
 * pwm_init
 * 
 * Function:
 *      Initialize PWM for the pump and fan.
 *      Pump:   PB4, TIM3, TIM_CCER_CC1E.
 *      Fan:    PB3, TIM2, TIM_CCER_CC2E.
 **********************************************************************************/
void pwm_init(void){
    /* 1. Clocks for the GPIO port and the timer */
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOBEN;
    RCC->APB1LENR |= RCC_APB1LENR_TIM2EN |RCC_APB1LENR_TIM3EN;

    /* 2. Pins -> alternate function mode (10) */
    /* PB3 fan:  AF1 = TIM2_CH2 */
    GPIOB->MODER  = (GPIOB->MODER  & ~GPIO_MODER_MODE3_Msk)  | (2U << GPIO_MODER_MODE3_Pos);
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~GPIO_AFRL_AFSEL3_Msk) | (1U << GPIO_AFRL_AFSEL3_Pos);
    /* PB4 pump: AF2 = TIM3_CH1 */
    GPIOB->MODER  = (GPIOB->MODER  & ~GPIO_MODER_MODE4_Msk)  | (2U << GPIO_MODER_MODE4_Pos);
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~GPIO_AFRL_AFSEL4_Msk) | (2U << GPIO_AFRL_AFSEL4_Pos);

    /* 3. Frequency and starting duty */
    TIM3->PSC  = PWM_PSC;
    TIM3->ARR  = PUMP_PWM_ARR;
    TIM3->CCR1 = 0;                 /* start at 0% */

    TIM2->PSC  = PWM_PSC;
    TIM2->ARR  = FAN_PWM_ARR;
    TIM2->CCR2 = 0;                 /* start at 0% */

    /* 4. Output, PWM mode 1 (OCxM = 0110), preload on */
    /* Fan: TIM2 CH2 (CH2 bits live in CCMR1 too) */
    TIM2->CCMR1 = (TIM2->CCMR1 & ~(TIM_CCMR1_CC2S_Msk | TIM_CCMR1_OC2M_Msk))
                | TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2PE;

    /* Pump: TIM3 CH1 */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(TIM_CCMR1_CC1S_Msk | TIM_CCMR1_OC1M_Msk))
                | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1PE;


    /* 5. Enable the channel output */
    TIM3->CCER |= TIM_CCER_CC1E;
    TIM2->CCER |= TIM_CCER_CC2E;

    /* 6. ARR preload, load the registers, start counting */
    TIM3->CR1 |= TIM_CR1_ARPE;
    TIM2->CR1 |= TIM_CR1_ARPE;
    TIM3->EGR  = TIM_EGR_UG;        /* copies PSC/ARR/CCR into the active registers */
    TIM2->EGR  = TIM_EGR_UG; 
    TIM3->CR1 |= TIM_CR1_CEN;
    TIM2->CR1 |= TIM_CR1_CEN;

    
}

/**********************************************************************************
 * pwm_set_duty_fan
 * 
 * Function:
 *      Set the fan's duty cycle.
 **********************************************************************************/
void pwm_set_duty_fan(uint8_t percent){
    if (percent > 100) percent = 100;
    TIM2->CCR1 = ((FAN_PWM_ARR + 1U) * percent) / 100U;
}

/**********************************************************************************
 * pwm_set_duty_pump
 * 
 * Function:
 *      Set the pump's duty cycle.
 **********************************************************************************/
void pwm_set_duty_pump(uint8_t percent) {
    if (percent > 100) percent = 100;
    TIM3->CCR1 = ((PUMP_PWM_ARR + 1U) * percent) / 100U;
}

/**********************************************************************************
 * pump_wakeup
 * 
 * Function:
 *      Data sheet specifies that an uninterrupted high pulse of
 *      3ms must be applied to wake up the pump correctly.
 **********************************************************************************/
void pump_wakeup(uint8_t percent) {
    TIM3->CCMR1 = (TIM3->CCMR1 & ~TIM_CCMR1_OC1M_Msk) | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_0;
    vTaskDelay(pdMS_TO_TICKS(5));   /* guarantees >= 4 ms high */
    pwm_set_duty_pump(percent);
    TIM3->CCMR1 = (TIM3->CCMR1 & ~TIM_CCMR1_OC1M_Msk) | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1;
}