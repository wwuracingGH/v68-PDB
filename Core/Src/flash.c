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
#include "stm32h5xx.h"
#include "flash.h"
#include "sys_params.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

#define SETTINGS_ADDR    0x0807E000u   /* bank 2, sector 31 */
#define SETTINGS_SECTOR  31u

PDB_SETTINGS_t pdb_settings;             /* the header only has extern; it has to be defined somewhere */

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

/* TODO Default values */
static const PDB_SETTINGS_t default_settings = {
    .magic   = SETTINGS_MAGIC,
    .version = SETTINGS_VERSION,
    .size    = sizeof(PDB_SETTINGS_t),

    .startup = {
        .channel  = { PDB_CH1, PDB_CH2, PDB_CH3, PDB_CH4, PDB_CH5 },
        .delay_ms = { 100, 100, 250, 250 },
    },
    .cur_limit = {
        .current_limit_mA = { 10000, 10000, 5000, 5000, 20000, 20000 },
        .transients = {1,1,1,1,1,1,1,1,1,1},     /* one per PDB_Channel_t */
    },
    .adc = {
        .sample_us        = 100,
        .oversample_ratio = 16,
        .prescaler        = 4,
    },
    .cooling = {
        .fan_speed  = 50,
        .pump_speed = 100,
    },
    .lv_charge_thresh = {
        .thresh_high = 1460,
        .thresh_low = 1400,
        .thresh_idle = 3300,

    }
};

void settings_reset_defaults(void) {
    memcpy(&pdb_settings, &default_settings, sizeof pdb_settings);
}

void settings_load(void) {
    const PDB_SETTINGS_t *f = (const PDB_SETTINGS_t *)SETTINGS_ADDR;
    if (f->magic == SETTINGS_MAGIC && f->version == SETTINGS_VERSION &&
        f->size == sizeof(PDB_SETTINGS_t) && f->crc == SETTINGS_CRC(f))
        memcpy(&pdb_settings, f, sizeof pdb_settings);
    else
        settings_reset_defaults();    /* blank, corrupt, or old layout */
}
