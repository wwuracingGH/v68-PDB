/**********************************************************************************
 * pwm.h
 *
 * Author:  WWU FSAE 
 *          Jackson Newman
 * 
 * Last Uptdated: 11/7/26
 * 
 * Version: 0.1 v68
 * 
 * Function:
 **********************************************************************************/
 #ifndef PWM_H
 #define PWM_H
 #include <stdint.h>
 
typedef struct {
    uint8_t PUMP_STOP[2];
    uint8_t PUMP_EMERGENCY_RUNNING[2];
    uint8_t PUMP_CONTROLLED_OP[2];
    uint8_t PUMP_MAXIMUM_SPEED[2];
    uint8_t PUMP_EMERGENCY_RUNNING[2];
} Pump_Interface_t;

void pwm_init(void);
void pwm_set_duty_fan(uint8_t percent);
void pwm_set_duty_pump(uint8_t percent);

 #endif