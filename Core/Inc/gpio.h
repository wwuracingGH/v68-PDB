/**********************************************************************************
 * gpio.h
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
#ifndef GPIO_H
#define GPIO_H

void gpio_init();

typedef enum{
    CP_ESTOP,
    BSPD,
    BOTS,
    LAMP_CHECK,
    R_ESTOP,
    LVBB,
    L_ESTOP
} shutdown_t;


#endif