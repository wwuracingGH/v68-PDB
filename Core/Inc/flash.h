/**********************************************************************************
 * flash.h
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
#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>
#include "channel_control.h"

#define PDB_NUM_CH           11U
#define PDB_CUR_CH_SENSE_NUM  6U
#define PDB_STARTUP_CH_NUM    5U        /* TODO*/
#define SETTINGS_MAGIC    0xDEADAF00u   /* "PDB1" */
#define SETTINGS_VERSION  1u

/* Specify startup channel order and delay */
typedef struct {
    uint8_t  channel[PDB_STARTUP_CH_NUM];       /* power-up order */
    uint16_t delay_ms[PDB_STARTUP_CH_NUM - 1];
} PDB_STARTUP_ORDER_t;

/* Set Current limit for current tripping channels, and general transient channel thresholds */
/* TODO SOME CHANNELS USE VOLT */
typedef struct {
    uint16_t current_limit_mA[PDB_CUR_CH_SENSE_NUM];
    uint16_t transients[PDB_NUM_CH];
} PDB_CUR_LIMIT_t;

/* ADC settings */
typedef struct {
    uint32_t sample_us;
    uint16_t oversample_ratio;
    uint8_t  prescaler;
    uint8_t  reserved;
} PDB_ADC_SETTINGS_t;

/* Fan and pump speed*/
typedef struct {
    uint16_t fan_speed;
    uint16_t pump_speed;
} PDB_COOLING_t;

/* Settings */
typedef struct __attribute__((aligned(16))) {
    uint32_t            magic;
    uint16_t            version;
    uint16_t            size;
    PDB_STARTUP_ORDER_t startup;
    PDB_CUR_LIMIT_t     cur_limit;
    PDB_ADC_SETTINGS_t  adc;
    PDB_COOLING_t       cooling;
    uint32_t            crc;            /* keep last */
} PDB_SETTINGS_t;

extern PDB_SETTINGS_t pdb_settings;
void settings_load(void);
int  settings_save(void);

#endif