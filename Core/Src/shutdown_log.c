/**********************************************************************************
 * shutdown_log.c
 *
 * Author:  WWU FSAE 
 *          Jackson Newman
 * 
 * Last Uptdated: 11/7/26
 *
 * Version: 0.1 v68
 *
 * Function:
 *      Shutdown taps will trigger an interrupt, CAN will log when they shutdown
 *      and when the shutdown flag is cleared
 **********************************************************************************/
#include "shutdown_log.h"
#include "main.h"
#include "sys_params.h"

#ifndef SHUTDOWN_IRQ_PRIO
#define SHUTDOWN_IRQ_PRIO   6U
#endif

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;          /* GPIO_PIN_x mask */
} shutdown_pin_t;

/* Pins come from main.h (CubeMX labels) */
static const shutdown_pin_t shutdown_pins[SHUTDOWN_NUM] = {
    [CP_ESTOP]   = { CP_ESTOP_VT_GPIO_Port, CP_ESTOP_VT_Pin },  /* PC6,  EXTI6  */
    [BSPD]       = { BSPD_VT_GPIO_Port,     BSPD_VT_Pin     },  /* PC7,  EXTI7  */
    [BOTS]       = { BOTS_VT_GPIO_Port,     BOTS_VT_Pin     },  /* PC8,  TIM8_CH3 */
    [R_ESTOP]    = { R_ESTOP_VT_GPIO_Port,  R_ESTOP_VT_Pin  },  /* PB8,  EXTI8  */
    [LVBB]       = { LVBB_VT_GPIO_Port,     LVBB_VT_Pin     },  /* PC11, EXTI11 */
    [L_ESTOP]    = { L_ESTOP_VT_GPIO_Port,  L_ESTOP_VT_Pin  },  /* PB14, EXTI14 */
};

/* EXTI0_IRQn..EXTI15_IRQn are consecutive, so line n's IRQ is EXTI0_IRQn + n */
static IRQn_Type exti_irq(uint16_t pin) {
    return (IRQn_Type)(EXTI0_IRQn + __builtin_ctz(pin));
}

/**********************************************************************************
 * logging_gpio_init
 *
 * Function:
 *      Sets every shutdown signal as an input with an interrupt on both edges.
 *      Edges only: read the starting levels with shutdown_read() after this.
 **********************************************************************************/
void logging_gpio_init(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* EXTI signals */
    GPIO_InitTypeDef gpio = {
        .Mode = GPIO_MODE_IT_RISING_FALLING,
        .Pull = GPIO_NOPULL,
    };
    for (uint32_t s = 0; s < SHUTDOWN_NUM; s++) {
        if (s == BOTS) {
            continue;                                   /* on TIM8, below */
        }
        gpio.Pin = shutdown_pins[s].pin;
        HAL_GPIO_Init(shutdown_pins[s].port, &gpio);
        __HAL_GPIO_EXTI_CLEAR_IT(shutdown_pins[s].pin); /* drop edges from before init */
        HAL_NVIC_SetPriority(exti_irq(shutdown_pins[s].pin), SHUTDOWN_IRQ_PRIO, 0);
        HAL_NVIC_EnableIRQ(exti_irq(shutdown_pins[s].pin));
    }

    /* BOTS: PC8 as TIM8_CH3 input capture on both edges */
    __HAL_RCC_TIM8_CLK_ENABLE();
    gpio.Pin       = BOTS_VT_Pin;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Alternate = GPIO_AF3_TIM8;
    HAL_GPIO_Init(BOTS_VT_GPIO_Port, &gpio);

    TIM8->CR1   = (2U << TIM_CR1_CKD_Pos);              /* filter clock fDTS = timer clock / 4 */
    TIM8->PSC   = 0;
    TIM8->ARR   = 0xFFFF;
    TIM8->CCMR2 = (TIM8->CCMR2 & ~(TIM_CCMR2_CC3S_Msk | TIM_CCMR2_IC3F_Msk | TIM_CCMR2_IC3PSC_Msk))
                | (1U   << TIM_CCMR2_CC3S_Pos)          /* CC3 = input from TI3 (PC8) */
                | (0xFU << TIM_CCMR2_IC3F_Pos);         /* 8 samples at fDTS/32: ignores pulses < ~4 us at 250 MHz */
    TIM8->CCER |= TIM_CCER_CC3P | TIM_CCER_CC3NP        /* both edges */
                | TIM_CCER_CC3E;                        /* capture on */
    TIM8->SR    = ~TIM_SR_CC3IF;                        /* clear stale flag (SR bits clear on write 0) */
    TIM8->DIER |= TIM_DIER_CC3IE;
    TIM8->CR1  |= TIM_CR1_CEN;
    HAL_NVIC_SetPriority(TIM8_CC_IRQn, SHUTDOWN_IRQ_PRIO, 0);
    HAL_NVIC_EnableIRQ(TIM8_CC_IRQn);
}

/**********************************************************************************
 * shutdown_read
 *
 * Function:
 *      Current level of a signal: 1 = high, 0 = low (or out of range).
 **********************************************************************************/
uint8_t shutdown_read(shutdown_t sig) {
    if ((uint32_t)sig >= SHUTDOWN_NUM) {
        return 0;
    }
    return (shutdown_pins[sig].port->IDR & shutdown_pins[sig].pin) ? 1U : 0U;
}

/* Default hook does nothing. Define shutdown_changed() elsewhere to log events. */
__attribute__((weak)) void shutdown_changed(shutdown_t sig, uint8_t level) {
    (void)sig;
    (void)level;
}

/**********************************************************************************
 * Interrupt handlers
 *
 *      Handler names are tied to EXTI line = pin number. If a pin moves in main.h,
 *      rename its handler to match.
 **********************************************************************************/
void EXTI6_IRQHandler(void)  { HAL_GPIO_EXTI_IRQHandler(CP_ESTOP_VT_Pin); }
void EXTI7_IRQHandler(void)  { HAL_GPIO_EXTI_IRQHandler(BSPD_VT_Pin);     }
void EXTI8_IRQHandler(void)  { HAL_GPIO_EXTI_IRQHandler(R_ESTOP_VT_Pin);  }
void EXTI11_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(LVBB_VT_Pin);     }
void EXTI14_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(L_ESTOP_VT_Pin);  }

void TIM8_CC_IRQHandler(void) {
    if (TIM8->SR & TIM_SR_CC3IF) {
        (void)TIM8->CCR3;                               /* reading CCR3 clears CC3IF */
        TIM8->SR = ~TIM_SR_CC3OF;                       /* drop overcapture from fast bounce */
        shutdown_changed(BOTS, shutdown_read(BOTS));
    }
}

/* EXTI lines are unique here, so the pin mask alone identifies the signal */
static void exti_edge(uint16_t pin) {
    for (uint32_t s = 0; s < SHUTDOWN_NUM; s++) {
        if (s != BOTS && shutdown_pins[s].pin == pin) {
            shutdown_changed((shutdown_t)s, shutdown_read((shutdown_t)s));
            return;
        }
    }
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)  { exti_edge(GPIO_Pin); }
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin) { exti_edge(GPIO_Pin); }
