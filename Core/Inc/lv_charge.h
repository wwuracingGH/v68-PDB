/**********************************************************************************
 * Project: Viking V68 PDB
 * 
 * main.c
 *
 * Author:  WWU FSAE 
 *          Jackson Newman
 * 
 * Last Uptdated: 11/12/26
 *
 * Version: 0.1 v68
 *
 * Function:
 **********************************************************************************/
#ifndef LV_CHARGE_H
#define LV_CHARGE_H

typedef enum {
    idle,
    init,
    charging
} Charge_State_t;

void lv_charge_init(void);
void LV_Charge_Task(void *params);

extern Charge_State_t lv_charge_state;


#endif