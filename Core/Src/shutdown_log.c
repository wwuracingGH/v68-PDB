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
    [BOTS]       = { BOTS_VT_GPIO_Port,     BOTS_VT_Pin     },  /* PD2,  EXTI2  */
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
    __HAL_RCC_GPIOD_CLK_ENABLE();                       /* BOTS on PD2 */

    /* Every signal is on its own EXTI line (2, 6, 7, 8, 11, 14), both edges */
    GPIO_InitTypeDef gpio = {
        .Mode = GPIO_MODE_IT_RISING_FALLING,
        .Pull = GPIO_NOPULL,
    };
    for (uint32_t s = 0; s < SHUTDOWN_NUM; s++) {
        gpio.Pin = shutdown_pins[s].pin;
        HAL_GPIO_Init(shutdown_pins[s].port, &gpio);
        __HAL_GPIO_EXTI_CLEAR_IT(shutdown_pins[s].pin); /* drop edges from before init */
        HAL_NVIC_SetPriority(exti_irq(shutdown_pins[s].pin), SHUTDOWN_IRQ_PRIO, 0);
        HAL_NVIC_EnableIRQ(exti_irq(shutdown_pins[s].pin));
    }
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

shutdown_changed(shutdown_t sig, uint8_t level) {
    
}

/**********************************************************************************
 * Interrupt handlers
 *
 *      Handler names are tied to EXTI line = pin number. If a pin moves in main.h,
 *      rename its handler to match.
 **********************************************************************************/
void EXTI2_IRQHandler(void)  { HAL_GPIO_EXTI_IRQHandler(BOTS_VT_Pin);     }
void EXTI6_IRQHandler(void)  { HAL_GPIO_EXTI_IRQHandler(CP_ESTOP_VT_Pin); }
void EXTI7_IRQHandler(void)  { HAL_GPIO_EXTI_IRQHandler(BSPD_VT_Pin);     }
void EXTI8_IRQHandler(void)  { HAL_GPIO_EXTI_IRQHandler(R_ESTOP_VT_Pin);  }
void EXTI11_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(LVBB_VT_Pin);     }
void EXTI14_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(L_ESTOP_VT_Pin);  }

/* EXTI lines are unique here, so the pin mask alone identifies the signal */
static void exti_edge(uint16_t pin) {
    for (uint32_t s = 0; s < SHUTDOWN_NUM; s++) {
        if (shutdown_pins[s].pin == pin) {
            shutdown_changed((shutdown_t)s, shutdown_read((shutdown_t)s));
            return;
        }
    }
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)  { exti_edge(GPIO_Pin); }
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin) { exti_edge(GPIO_Pin); }
