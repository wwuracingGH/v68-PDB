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
 * 
 **********************************************************************************/
#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>
#include "sys_params.h"
#include "channel_control.h"        /* PDB_CTRL_t, PDB_NUM_CTRL, PDB_NUM_OUTPUTS */

#define SETTINGS_MAGIC          0xDEADAF00u
#define SETTINGS_VERSION        2u          /* bump on ANY change to PDB_SETTINGS_t */

/**********************************************************************************
 * Parameter IDs for CAN PARAM_CHANGE / PARAM_REQUEST (same message format as the VCU)
 *
 * Channel IDs: high nibble = field, low nibble = PDB_CTRL_t index (CH1 = 0 ... CHARGE = 8)
 *   0x0n cur limit     DAC code 0-1023, 0 = highest limit
 *   0x1n startup delay ms after PDB_STARTUP begins, CH_CFG_DELAY_NONE = never
 *   0x2n controllable  0/1
 *   0x3n logged        0/1
 * Charge slot (n = 8): lv_charge.c owns it, limit and delay are unused
 * magic, version and crc are not params
 **********************************************************************************/
#define FLASH_PARAMID_CH_INDEX(id)      ((id) & 0x0FU)      /* -> PDB_CTRL_t */
#define FLASH_PARAMID_CH_FIELD(id)      ((id) & 0xF0U)      /* -> FLASH_PARAMID_*_BASE */

#define FLASH_PARAMID_LIMIT_BASE        0x00U
#define FLASH_PARAMID_CH1_LIMIT         0x00U
#define FLASH_PARAMID_CH2_LIMIT         0x01U
#define FLASH_PARAMID_CH3_LIMIT         0x02U
#define FLASH_PARAMID_CH4_LIMIT         0x03U
#define FLASH_PARAMID_CH5_LIMIT         0x04U
#define FLASH_PARAMID_CH6_LIMIT         0x05U
#define FLASH_PARAMID_CH7_LIMIT         0x06U
#define FLASH_PARAMID_CH8_LIMIT         0x07U
#define FLASH_PARAMID_CHARGE_LIMIT      0x08U

#define FLASH_PARAMID_DELAY_BASE        0x10U
#define FLASH_PARAMID_CH1_DELAY         0x10U
#define FLASH_PARAMID_CH2_DELAY         0x11U
#define FLASH_PARAMID_CH3_DELAY         0x12U
#define FLASH_PARAMID_CH4_DELAY         0x13U
#define FLASH_PARAMID_CH5_DELAY         0x14U
#define FLASH_PARAMID_CH6_DELAY         0x15U
#define FLASH_PARAMID_CH7_DELAY         0x16U
#define FLASH_PARAMID_CH8_DELAY         0x17U
#define FLASH_PARAMID_CHARGE_DELAY      0x18U

#define FLASH_PARAMID_CTRL_BASE         0x20U
#define FLASH_PARAMID_CH1_CTRL          0x20U
#define FLASH_PARAMID_CH2_CTRL          0x21U
#define FLASH_PARAMID_CH3_CTRL          0x22U
#define FLASH_PARAMID_CH4_CTRL          0x23U
#define FLASH_PARAMID_CH5_CTRL          0x24U
#define FLASH_PARAMID_CH6_CTRL          0x25U
#define FLASH_PARAMID_CH7_CTRL          0x26U
#define FLASH_PARAMID_CH8_CTRL          0x27U
#define FLASH_PARAMID_CHARGE_CTRL       0x28U

#define FLASH_PARAMID_LOGGED_BASE       0x30U
#define FLASH_PARAMID_CH1_LOGGED        0x30U
#define FLASH_PARAMID_CH2_LOGGED        0x31U
#define FLASH_PARAMID_CH3_LOGGED        0x32U
#define FLASH_PARAMID_CH4_LOGGED        0x33U
#define FLASH_PARAMID_CH5_LOGGED        0x34U
#define FLASH_PARAMID_CH6_LOGGED        0x35U
#define FLASH_PARAMID_CH7_LOGGED        0x36U
#define FLASH_PARAMID_CH8_LOGGED        0x37U
#define FLASH_PARAMID_CHARGE_LOGGED     0x38U

/* LV charge window, ADC2 counts 0-4095 */
#define FLASH_PARAMID_LV_THRESH_HIGH    0x40U
#define FLASH_PARAMID_LV_THRESH_LOW     0x41U

/* PWM, 0-65535 */
#define FLASH_PARAMID_FAN_PWM           0x50U
#define FLASH_PARAMID_PUMP_PWM          0x51U

/* ADC */
#define FLASH_PARAMID_ADC_SAMPLE_US     0x60U
#define FLASH_PARAMID_ADC_OVERSAMPLE    0x61U
#define FLASH_PARAMID_ADC_PRESCALER     0x62U

/********************************************************************************************
 * Channel config word, one per PDB_CTRL_t (CH1-CH8 + LV charge)
 *
 * | B31-B28 spare | B27 logged | B26 controllable | B25-B10 startup delay (ms) | B9-B0 cur limit |
 *
 * cur limit: raw DAC53608 code from the programmer. 0 = highest limit (~2.8 A), 1023 = lowest
 * delay:     ms after PDB_STARTUP begins, so the order comes from the delays.
 *            CH_CFG_DELAY_NONE = PDB_STARTUP never turns it on
 ********************************************************************************************/
#define CH_CFG_LIMIT_Pos        0U
#define CH_CFG_LIMIT_Msk        (0x3FFUL << CH_CFG_LIMIT_Pos)
#define CH_CFG_DELAY_Pos        10U
#define CH_CFG_DELAY_Msk        (0xFFFFUL << CH_CFG_DELAY_Pos)
#define CH_CFG_CTRL_Pos         26U
#define CH_CFG_CTRL             (1UL << CH_CFG_CTRL_Pos)
#define CH_CFG_LOGGED_Pos       27U
#define CH_CFG_LOGGED           (1UL << CH_CFG_LOGGED_Pos)

#define CH_CFG_DELAY_NONE       0xFFFFU

/* Build a config word: CH_CFG(DAC code, delay ms, CH_CFG_CTRL | CH_CFG_LOGGED) */
#define CH_CFG(limit, delay_ms, flags) ((((uint32_t)(limit)    << CH_CFG_LIMIT_Pos) & CH_CFG_LIMIT_Msk) |\
                                        (((uint32_t)(delay_ms) << CH_CFG_DELAY_Pos) & CH_CFG_DELAY_Msk) |\
                                        (uint32_t)(flags))

/* LV_CHARGE_THRESHS: | B31-B24 spare | B23-B12 high | B11-B0 low |, ADC2 counts from the programmer */
#define LV_THRESH_LOW_Pos       0U
#define LV_THRESH_LOW_Msk       (0xFFFUL << LV_THRESH_LOW_Pos)
#define LV_THRESH_HIGH_Pos      12U
#define LV_THRESH_HIGH_Msk      (0xFFFUL << LV_THRESH_HIGH_Pos)
#define LV_THRESHS(high, low) ((((uint32_t)(high) << LV_THRESH_HIGH_Pos) & LV_THRESH_HIGH_Msk) |\
                              (((uint32_t)(low)  << LV_THRESH_LOW_Pos)  & LV_THRESH_LOW_Msk))

/* PWM_CONFIG: | B31-B16 pump | B15-B0 fan | */
#define PWM_FAN_Pos             0U
#define PWM_FAN_Msk             (0xFFFFUL << PWM_FAN_Pos)
#define PWM_PUMP_Pos            16U
#define PWM_PUMP_Msk            (0xFFFFUL << PWM_PUMP_Pos)
#define PWM_CFG(pump, fan) ((((uint32_t)(pump) << PWM_PUMP_Pos) & PWM_PUMP_Msk) |\
                            (((uint32_t)(fan)  << PWM_FAN_Pos)  & PWM_FAN_Msk))

/* ADC settings */
typedef struct {
    uint32_t sample_us;
    uint16_t oversample_ratio;
    uint8_t  prescaler;
    uint8_t  reserved;
} PDB_ADC_SETTINGS_t;

/* Same 9 words by name or by PDB_CTRL_t index */
typedef union {
    struct {
        uint32_t PDB_CH1_CONFIG;
        uint32_t PDB_CH2_CONFIG;
        uint32_t PDB_CH3_CONFIG;
        uint32_t PDB_CH4_CONFIG;
        uint32_t PDB_CH5_CONFIG;
        uint32_t PDB_CH6_CONFIG;
        uint32_t PDB_CH7_CONFIG;
        uint32_t PDB_CH8_CONFIG;
        uint32_t PDB_CHARGE_CONFIG;
    };
    uint32_t ch[PDB_NUM_CTRL];
} PDB_CONFIGS_t;
_Static_assert(sizeof(PDB_CONFIGS_t) == PDB_NUM_CTRL * sizeof(uint32_t), "PDB_CONFIGS_t must be 9 packed u32s");

/**********************************************************************************
 * PDB_SETTINGS_t, what sits in flash
 *
 * magic:             SETTINGS_MAGIC
 * version:           SETTINGS_VERSION
 * LV_CHARGE_THRESHS: | B23-B12 High V thresh | B11-B0 Low V thresh |
 * PWM_CONFIG:        | B31-B16 PUMP PWM | B15-B0 FAN PWM |
 * channel_config:    one CH_CFG word per PDB_CTRL_t
 * crc:               CRC32 of everything before it. Keep last
 **********************************************************************************/
typedef struct __attribute__((aligned(16))) {
    uint32_t            magic;
    uint32_t            version;
    uint32_t            LV_CHARGE_THRESHS;
    uint32_t            PWM_CONFIG;
    PDB_CONFIGS_t       channel_config;
    PDB_ADC_SETTINGS_t  adc;
    uint32_t            crc;
} PDB_SETTINGS_t;
_Static_assert(sizeof(PDB_SETTINGS_t) % 16U == 0U, "flash programs 16 bytes at a time");

/* Unpacked copy of pdb_settings settings_decode() rebuilds it */
typedef struct {
    uint16_t limit[PDB_NUM_CTRL];           /* DAC code */
    uint16_t delay_ms[PDB_NUM_CTRL];        /* CH_CFG_DELAY_NONE = not started by PDB_STARTUP */
    uint8_t  controllable[PDB_NUM_CTRL];
    uint8_t  logged[PDB_NUM_CTRL];
    uint16_t lv_thresh_high;
    uint16_t lv_thresh_low;
    uint16_t fan_pwm;
    uint16_t pump_pwm;
} PDB_RUNTIME_t;

extern PDB_SETTINGS_t pdb_settings;
extern PDB_RUNTIME_t  pdb_rt;

void settings_load(void);           /* flash -> pdb_settings (defaults if bad) -> pdb_rt */
void settings_decode(void);         /* pdb_settings -> pdb_rt, call after changing pdb_settings */
void settings_reset_defaults(void);
int  settings_save(void);

#endif


/* Generated DBC code
BO_ 773 PDB_Param_Reveal: 6 PDB
 SG_ Set_Value : 0|32@1+ (1,0) [0|0] "" Vector__XXX
 SG_ ID : 32|16@1+ (1,0) [0|0] "" Vector__XXX

BO_ 774 PDB_Param_Change: 6 Vector__XXX
 SG_ Set_Value : 0|32@1+ (1,0) [0|0] "" PDB
 SG_ ID : 32|15@1+ (1,0) [0|0] "" PDB
 SG_ Write : 47|1@1+ (1,0) [0|0] "" PDB

BO_ 775 PDB_Param_Request: 2 Vector__XXX
 SG_ ID : 0|16@1+ (1,0) [0|0] "" PDB





*/