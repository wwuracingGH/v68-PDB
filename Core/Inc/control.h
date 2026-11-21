/**********************************************************************************
 * sys_params
 *
 * Author:  WWU FSAE 
 *          Jackson Newman
 * 
 * Last Uptdated: 11/9/26
 *
 * Version: 0.1 v68
 * 
 * Function:
 *      System control and state.
 **********************************************************************************/
#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>
typedef struct {
    uint8_t lv_charge;
    
} PDB_STATE_t;
extern PDB_STATE_t PDB_state;

#endif