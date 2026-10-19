/*****************************************************************
 * gpio.h
 * 
 * Viking v68
 * 10/18/26
 * 
 * Jackson Newman
 *
 ******************************************************************/
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