/**********************************************************************************
 * flash.c
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
#include <stdint.h>
#include <stddef.h>     /* offsetof */
#include <string.h>     /* memcpy, memcmp */
#include "FreeRTOS.h"
#include "task.h"
#include "stm32h5xx_hal.h"
#include "flash.h"
#include "flash_defaults.h"
#include "sys_params.h"
#include "FreeRTOS.h"
#include "task.h"

#define SETTINGS_ADDR    0x0807E000u   /* bank 2, sector 31 */
#define SETTINGS_SECTOR  31u           /* sector number inside bank 2 */

PDB_SETTINGS_t pdb_settings;           /* packed, matches flash. The copy that gets edited */
PDB_RUNTIME_t  pdb_rt;                 /* unpacked from pdb_settings, read only */

/* Could enable a CRC in stm32 */
static uint32_t crc32(const void *p, uint32_t len) {
    const uint8_t *b = p;
    uint32_t c = 0xFFFFFFFFu;
    while (len--) {
        c ^= *b++;
        for (int i = 0; i < 8; i++) c = (c >> 1) ^ (0xEDB88320u & -(c & 1u));
    }
    return ~c;
}

#define SETTINGS_CRC(s) crc32((s), offsetof(PDB_SETTINGS_t, crc))

static const PDB_SETTINGS_t default_settings = {
    .magic             = SETTINGS_MAGIC,
    .version           = SETTINGS_VERSION,
    .LV_CHARGE_THRESHS = LV_THRESHS(DEFAULT_LV_THRESH_HIGH, DEFAULT_LV_THRESH_LOW),
    .PWM_CONFIG        = PWM_CFG(DEFAULT_PUMP_PWM, DEFAULT_FAN_PWM),
    .channel_config.ch = {
        [PDB_CH1_CTRL]    = DEFAULT_CH1_CONFIG,
        [PDB_CH2_CTRL]    = DEFAULT_CH2_CONFIG,
        [PDB_CH3_CTRL]    = DEFAULT_CH3_CONFIG,
        [PDB_CH4_CTRL]    = DEFAULT_CH4_CONFIG,
        [PDB_CH5_CTRL]    = DEFAULT_CH5_CONFIG,
        [PDB_CH6_CTRL]    = DEFAULT_CH6_CONFIG,
        [PDB_CH7_CTRL]    = DEFAULT_CH7_CONFIG,
        [PDB_CH8_CTRL]    = DEFAULT_CH8_CONFIG,
        [PDB_CHARGE_CTRL] = DEFAULT_CHARGE_CONFIG,
    },
    .adc = {
        .sample_us        = DEFAULT_ADC_SAMPLE_US,
        .oversample_ratio = DEFAULT_ADC_OVERSAMPLE,
        .prescaler        = DEFAULT_ADC_PRESCALER,
    },
};

/**********************************************************************************
 * settings_reset_defaults
 *
 * Function:
 *      pdb_settings back to flash_defaults.h and pdb_rt rebuilt. RAM only,
 *      settings_save() saves them to flash.
 **********************************************************************************/
void settings_reset_defaults(void) {
    memcpy(&pdb_settings, &default_settings, sizeof pdb_settings);
    settings_decode();
}

/**********************************************************************************
 * settings_load
 *
 * Function:
 *      Copies flash into pdb_settings if magic, version and CRC all check out,
 *      otherwise defaults (blank, corrupt or old layout). Then builds pdb_rt.
 **********************************************************************************/
void settings_load(void) {
    const PDB_SETTINGS_t *f = (const PDB_SETTINGS_t *)SETTINGS_ADDR;
    if (f->magic == SETTINGS_MAGIC && f->version == SETTINGS_VERSION && f->crc == SETTINGS_CRC(f)) {
        memcpy(&pdb_settings, f, sizeof pdb_settings);
        settings_decode();
    } else {
        settings_reset_defaults();
    }
}

/**********************************************************************************
 * settings_decode
 *
 * Function:
 *      Unpacks pdb_settings into the pdb_rt tables. Call after anything changes
 *      pdb_settings (CAN param write, reset to defaults)
 **********************************************************************************/
void settings_decode(void) {
    for (uint32_t i = 0; i < PDB_NUM_CTRL; i++) {
        uint32_t c = pdb_settings.channel_config.ch[i];
        pdb_rt.limit[i]        = (uint16_t)((c & CH_CFG_LIMIT_Msk) >> CH_CFG_LIMIT_Pos);
        pdb_rt.delay_ms[i]     = (uint16_t)((c & CH_CFG_DELAY_Msk) >> CH_CFG_DELAY_Pos);
        pdb_rt.controllable[i] = (uint8_t)((c & CH_CFG_CTRL)   ? 1U : 0U);
        pdb_rt.logged[i]       = (uint8_t)((c & CH_CFG_LOGGED) ? 1U : 0U);
    }
    pdb_rt.lv_thresh_high = (uint16_t)((pdb_settings.LV_CHARGE_THRESHS & LV_THRESH_HIGH_Msk) >> LV_THRESH_HIGH_Pos);
    pdb_rt.lv_thresh_low  = (uint16_t)((pdb_settings.LV_CHARGE_THRESHS & LV_THRESH_LOW_Msk)  >> LV_THRESH_LOW_Pos);
    pdb_rt.pump_pwm       = (uint16_t)((pdb_settings.PWM_CONFIG & PWM_PUMP_Msk) >> PWM_PUMP_Pos);
    pdb_rt.fan_pwm        = (uint16_t)((pdb_settings.PWM_CONFIG & PWM_FAN_Msk)  >> PWM_FAN_Pos);
}

/**********************************************************************************
 * settings_save
 *
 * Function:
 *      Snapshots pdb_settings, stamps magic, version and CRC on the snapshot, erases
 *      bank 2 sector 31, programs it 16 bytes (one quad-word) at a time and reads it
 *      back. Everything after the copy uses the snapshot, so a CAN param write that
 *      lands mid-save can't leave flash with a CRC for different data. That write
 *      just isn't in this save, it goes in the next one.
 *      Code runs from bank 1 so the CPU doesn't stall, but the erase takes ms:
 *      call from a task, never an ISR or the CAN callback.
 *
 * Returns:
 *      0 = ok, -1 = erase failed, -2 = program failed, -3 = read-back mismatch
 **********************************************************************************/
int settings_save(void) {
    static PDB_SETTINGS_t temp;
    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_SECTORS,
        .Banks     = FLASH_BANK_2,
        .Sector    = SETTINGS_SECTOR,
        .NbSectors = 1,
    };
    uint32_t bad_sector = 0;
    int rc = 0;

    /* prevents anything from interrupting */
    taskENTER_CRITICAL();
    memcpy(&temp, &pdb_settings, sizeof temp);
    taskEXIT_CRITICAL();

    temp.magic   = SETTINGS_MAGIC;
    temp.version = SETTINGS_VERSION;
    temp.crc     = SETTINGS_CRC(&temp);

    HAL_FLASH_Unlock();
    if (HAL_FLASHEx_Erase(&erase, &bad_sector) != HAL_OK) {
        rc = -1;
    } else {
        for (uint32_t off = 0; off < sizeof(PDB_SETTINGS_t); off += 16U) {
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, SETTINGS_ADDR + off,
                                  (uint32_t)((const uint8_t *)&temp + off)) != HAL_OK) {
                rc = -2;
                break;
            }
        }
    }
    HAL_FLASH_Lock();

    if (rc == 0 && memcmp((const void *)SETTINGS_ADDR, &temp, sizeof temp) != 0) {
        rc = -3;
    }
    return rc;
}

void settings_set(uint16_t id, uint8, uint32_t value) {
    if (id )
}


int settings_get(uint16_t id, uint32_t *value) {

}