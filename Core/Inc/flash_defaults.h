/**********************************************************************************
 * flash_defaults.h
 *
 * Author:  WWU FSAE
 *          Jackson Newman
 *
 * Last Uptdated: 11/6/26
 *
 * Version: 0.1 v68
 *
 * Function:
 *      Holds the default values of PDB settings.
 **********************************************************************************/
#ifndef FLASH_DEFAULTS_H
#define FLASH_DEFAULTS_H

/* Channel configs: CH_CFG(DAC code, startup delay ms, flags)
 * DAC code 0 = highest limit (~2.8 A), 1023 = lowest (~64 mA) (BE CAREFUL)
 * TODO: real per-channel limits and flags */
#define DEFAULT_LIMIT_CODE          0U
#define DEFAULT_CH1_CONFIG          CH_CFG(DEFAULT_LIMIT_CODE,   0,               CH_CFG_CTRL | CH_CFG_LOGGED)
#define DEFAULT_CH2_CONFIG          CH_CFG(DEFAULT_LIMIT_CODE, 100,               CH_CFG_CTRL | CH_CFG_LOGGED)
#define DEFAULT_CH3_CONFIG          CH_CFG(DEFAULT_LIMIT_CODE, 200,               CH_CFG_CTRL | CH_CFG_LOGGED)
#define DEFAULT_CH4_CONFIG          CH_CFG(DEFAULT_LIMIT_CODE, 450,               CH_CFG_CTRL | CH_CFG_LOGGED)
#define DEFAULT_CH5_CONFIG          CH_CFG(DEFAULT_LIMIT_CODE, 700,               CH_CFG_CTRL | CH_CFG_LOGGED)
#define DEFAULT_CH6_CONFIG          CH_CFG(DEFAULT_LIMIT_CODE, CH_CFG_DELAY_NONE, CH_CFG_CTRL | CH_CFG_LOGGED)
#define DEFAULT_CH7_CONFIG          CH_CFG(DEFAULT_LIMIT_CODE, CH_CFG_DELAY_NONE, CH_CFG_CTRL | CH_CFG_LOGGED)
#define DEFAULT_CH8_CONFIG          CH_CFG(DEFAULT_LIMIT_CODE, CH_CFG_DELAY_NONE, CH_CFG_CTRL | CH_CFG_LOGGED)
#define DEFAULT_CHARGE_CONFIG       CH_CFG(0, CH_CFG_DELAY_NONE, 0)     /* lv_charge.c owns this one */

/* LV charge window 14.0-14.6 V, in ADC2 counts */
#define DEFAULT_LV_THRESH_HIGH      LV_MV_TO_COUNTS(14600UL)            /* 3623 */
#define DEFAULT_LV_THRESH_LOW       LV_MV_TO_COUNTS(14000UL)            /* 3474 */

#define DEFAULT_FAN_PWM             50U
#define DEFAULT_PUMP_PWM            100U

#define DEFAULT_ADC_SAMPLE_US       100U
#define DEFAULT_ADC_OVERSAMPLE      16U
#define DEFAULT_ADC_PRESCALER       4U

#endif
