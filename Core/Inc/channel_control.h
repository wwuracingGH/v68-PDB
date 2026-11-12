/**********************************************************************************
 * Project: Viking V68 PDB
 * 
 * main.c
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
#ifndef CHANNEL_CONTROL_H
#define CHANNEL_CONTROL_H
#include "stm32h533xx.h"
#include "sys_params.h"         /* PDB_Channel_t, PDB_NUM_CHANNELS */

typedef enum {
    PDB_CH1_CTRL = 0,
    PDB_CH2_CTRL = 1,
    PDB_CH3_CTRL = 2,
    PDB_CH4_CTRL = 3,
    PDB_CH5_CTRL = 4,
    PDB_CH6_CTRL = 5,
    PDB_CH7_CTRL = 6,
    PDB_CH8_CTRL = 7,
    PDB_CHARGE_CTRL = 8
} PDB_CTRL_t;

typedef struct {
    GPIO_TypeDef *port;
    uint8_t       pin;
} ch_pin_t;

#define PDB_NUM_OUTPUTS 8U  /* switched outputs CH1-CH8 */
#define PDB_NUM_CTRL    9U  /* control outputs: CH1-CH8 enables + charge path enable (PDB_CTRL_t) */
#define PDB_NUM_DAC     8U  /* DAC53608 outputs, all 8 used (PDB_DAC_t) */
#define VOL_MEM 1U
#define NON_VOL_MEM 0U


uint8_t channel_ctrl_Init(void);
void    PDB_CHANNEL_CTRL_REQ(PDB_CTRL_t ctrl, uint8_t val);
void    PDB_CHANNEL_SHUTDOWN(void);

#endif
