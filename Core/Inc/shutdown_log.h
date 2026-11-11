/**********************************************************************************
 * shutdown_log.h
 *
 * Author:  WWU FSAE 
 *          Jackson Newman
 * 
 * Last Uptdated: 11/9/26
 *
 * Version: 0.1 v68
 **********************************************************************************/
#ifndef SHUTDOWN_LOG_H
#define SHUTDOWN_LOG_H
#include <stdint.h>

typedef enum{
    CP_ESTOP,
    BSPD,
    BOTS,
    R_ESTOP,
    LVBB,
    L_ESTOP,
    SHUTDOWN_NUM
} shutdown_t;

void    logging_gpio_init(void);
uint8_t shutdown_read(shutdown_t sig);

/* Called from interrupt context on every edge of a signal, with its new level.
 * Weak no-op by default: define your own to log it (keep it short, use FromISR calls). */
void    shutdown_changed(shutdown_t sig, uint8_t level);

#endif
