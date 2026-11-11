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
 *      System-wide channel list and interrupt priorities
 **********************************************************************************/
#ifndef SYS_PARAMS_H
#define SYS_PARAMS_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
typedef struct {
    uint8_t lv_charge;


} PDB_STATE_t;
extern PDB_STATE_t PDB_state;

/******************************** Channels ******************************************/

/* Every measured PDB channel, used as the index for per-channel settings
 * (e.g. pdb_settings.cur_limit.transients[]). LV charge voltage is not in this list:
 * it is on ADC2 and handled by lv_charge.c */
typedef enum {
    PDB_CH1      = 0,
    PDB_CH2      = 1,
    PDB_CH3      = 2,
    PDB_CH4      = 3,
    PDB_CH5      = 4,
    PDB_CH6      = 5,
    PDB_CH7      = 6,
    PDB_CH8      = 7,
    PDB_BUS_CUR  = 8,
    PDB_BUS_VOLT = 9,
    PDB_NUM_CHANNELS            /* count (10), keep last */
} PDB_Channel_t;

/******************************** ADC **************************************************/
#define ADC_QUANT4          4U
#define ADC_VREF_MV         3300UL
#define ADC_FULL_SCALE      4095UL
/* Macro for general ADC counts */
#define ADC_MV_TO_COUNTS(mv)  (((mv) * ADC_FULL_SCALE) / (ADC_VREF_MV))

/****************************** LV Charge *********************************************/
extern TaskHandle_t lv_charge_task_handle;
#define LV_CHARGE_DIV       5UL     /* charge voltage / PA6 voltage */
/* Macro for LV mv */
#define LV_MV_TO_COUNTS(mv)  (((mv) * ADC_FULL_SCALE) / (LV_CHARGE_DIV * ADC_VREF_MV))

/**************************** Function PRIO *********************************************/
#define SHUTDOWN_IRQ_PRIO   6U     /* Shutdown log priority */
#define LV_CHARGE_PRIO      6U




#endif