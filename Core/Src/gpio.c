/*******************************************************************************
 * gpio.c
 * 
 * Viking v68
 * 10/18/26
 * 
 * Jackson Newman
 * Function:
 *      ...
 ********************************************************************************/

#include "gpio.h"
#include "stm32h5xx.h"
#include "FreeRTOS.h"

/********************************************************************************
 * void gpio_init()
 * 
 * Function:
 *      Initialize GPIO for
 ********************************************************************************/
void gpio_init(){
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIODEN;

    /* Enable MODER set to (01 = output) */
    GPIOC->MODER &= ~(GPIO_MODER_MODE6_Msk);   /* GPIOC PC6 */
    GPIOC->MODER &= ~(GPIO_MODER_MODE7_Msk);   /* GPIOC PC7 */
    GPIOC->MODER &= ~(GPIO_MODER_MODE8_Msk);   /* GPIOC PC8 */
    GPIOC->MODER &= ~(GPIO_MODER_MODE9_Msk);   /* GPIOC PC9 */

    GPIOD->MODER &= ~(GPIO_MODER_MODE12_Msk);  /* GPIO PD12 */
    GPIOD->MODER &= ~(GPIO_MODER_MODE13_Msk);  /* GPIO PD13 */
    GPIOD->MODER &= ~(GPIO_MODER_MODE14_Msk);  /* GPIO PD14 */

    GPIOC->MODER |= (1U << GPIO_MODER_MODE6_Pos);   /* GPIOC PC6 */
    GPIOC->MODER |= (1U << GPIO_MODER_MODE7_Pos);   /* GPIOC PC7 */
    GPIOC->MODER |= (1U << GPIO_MODER_MODE8_Pos);   /* GPIOC PC8 */
    GPIOC->MODER |= (1U << GPIO_MODER_MODE9_Pos);   /* GPIOC PC9 */

    GPIOD->MODER |= (1U << GPIO_MODER_MODE12_Pos);  /* GPIO PD12 */
    GPIOD->MODER |= (1U << GPIO_MODER_MODE13_Pos);  /* GPIO PD13 */
    GPIOD->MODER |= (1U << GPIO_MODER_MODE14_Pos);  /* GPIO PD14 */

    /* Set output type (0 = push-pull) */
    GPIOC->OTYPER &= ~(GPIO_OTYPER_OT6_Msk);   /* GPIOC PC6 */
    GPIOC->OTYPER &= ~(GPIO_OTYPER_OT7_Msk);   /* GPIOC PC7 */
    GPIOC->OTYPER &= ~(GPIO_OTYPER_OT8_Msk);   /* GPIOC PC8 */
    GPIOC->OTYPER &= ~(GPIO_OTYPER_OT9_Msk);   /* GPIOC PC9 */

    GPIOD->OTYPER &= ~(GPIO_OTYPER_OT12_Msk);  /* GPIO PD12 */
    GPIOD->OTYPER &= ~(GPIO_OTYPER_OT13_Msk);  /* GPIO PD13 */
    GPIOD->OTYPER &= ~(GPIO_OTYPER_OT14_Msk);  /* GPIO PD14 */

    /* Set pull-up/pull-down (00 = none) */
    GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPD6_Msk);   /* GPIOC PC6 */
    GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPD7_Msk);   /* GPIOC PC7 */
    GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPD8_Msk);   /* GPIOC PC8 */
    GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPD9_Msk);   /* GPIOC PC9 */

    GPIOD->PUPDR &= ~(GPIO_PUPDR_PUPD12_Msk);  /* GPIO PD12 */
    GPIOD->PUPDR &= ~(GPIO_PUPDR_PUPD13_Msk);  /* GPIO PD13 */
    GPIOD->PUPDR &= ~(GPIO_PUPDR_PUPD14_Msk);  /* GPIO PD14 */

    /* Set output speed (00 = low) */
    GPIOC->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED6_Msk);   /* GPIOC PC6 */
    GPIOC->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED7_Msk);   /* GPIOC PC7 */
    GPIOC->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED8_Msk);   /* GPIOC PC8 */
    GPIOC->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED9_Msk);   /* GPIOC PC9 */

    GPIOD->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED12_Msk);  /* GPIO PD12 */
    GPIOD->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED13_Msk);  /* GPIO PD13 */
    GPIOD->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED14_Msk);  /* GPIO PD14 */
}

/********************************************************************************
 * shutdown_indicator(shutdown_t module)
 * 
 ********************************************************************************/
void shutdown_indicator(shutdown_t module, uint8_t status) {
    switch (module) {
        case CP_ESTOP:
            GPIOC->ODR &= ~(GPIO_ODR_OD6_Msk);
            GPIOC->ODR |= (status << GPIO_ODR_OD6_Pos);
            break;
        case BSPD:
            GPIOC->ODR &= ~(GPIO_ODR_OD7_Msk);
            GPIOC->ODR |= (status << GPIO_ODR_OD7_Pos);
            break;
        case BOTS:
            GPIOC->ODR &= ~(GPIO_ODR_OD8_Msk);
            GPIOC->ODR |= (status << GPIO_ODR_OD8_Pos);
            break;
        case LAMP_CHECK:
            GPIOC->ODR &= ~(GPIO_ODR_OD9_Msk);
            GPIOC->ODR |= (status << GPIO_ODR_OD9_Pos);
            break;
        case R_ESTOP:
            GPIOD->ODR &= ~(GPIO_ODR_OD12_Msk);
            GPIOD->ODR |= (status << GPIO_ODR_OD12_Pos);
            break;
        case LVBB:
            GPIOD->ODR &= ~(GPIO_ODR_OD13_Msk);
            GPIOD->ODR |= (status << GPIO_ODR_OD13_Pos);
            break;
        case L_ESTOP:
            GPIOD->ODR &= ~(GPIO_ODR_OD14_Msk);
            GPIOD->ODR |= (status << GPIO_ODR_OD14_Pos);
            break;
        default:
            break;
    }
    
}

// static const struct { GPIO_TypeDef *port; uint8_t pin; } led_map[] = {
//     [CP_ESTOP]   = { GPIOC, 6 },
//     [BSPD]       = { GPIOC, 7 },
//     [BOTS]       = { GPIOC, 8 },
//     [LAMP_CHECK] = { GPIOC, 9 },
//     [R_ESTOP]    = { GPIOD, 12 },
//     [LVBB]       = { GPIOD, 13 },
//     [L_ESTOP]    = { GPIOD, 14 },
// };

// void shutdown_indicator(shutdown_t module, bool status) {
//     if (module > L_ESTOP) return;
//     led_map[module].port->BSRR = 1U << (led_map[module].pin + (status ? 0 : 16));
// }
