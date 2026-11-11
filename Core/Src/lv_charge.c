/**********************************************************************************
 * lv_charge.c
 *
 * Author:  WWU FSAE
 *          Jackson Newman
 *
 * Last Uptdated: 11/6/26
 *
 * Version: 0.1 v68
 *
 * Function:
 *      Allows the LV battery to charge if 14-14.6V is present on its terminals
 **********************************************************************************/
#include "lv_charge.h"
#include "main.h"
#include "sys_params.h"
#include "flash.h"
#include "adc_dma.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

/* steps = 3.3/2^12 = 0.81 mv
 * voltage divider 1.5/7.5 = 0.2
 * desired voltage 3v * 0.2 = 0.6
 * 0.6v / steps =  740.7407
 *  */
TaskHandle_t lv_charge_task_handle;

typedef enum {
    idle,
    init,
    charging
} Charge_State_t;
Charge_State_t lv_charge_state;

static void switch_lv_charge_state(Charge_State_t next_state) {
    // needs a semaphore and mutex so no races
    lv_charge_state = next_state;
    switch (next_state) {
        case idle:
            adc2_awd_set(0,pdb_settings.lv_charge_thresh.thresh_low);
            break;
        case charging:
            adc2_awd_set(pdb_settings.lv_charge_thresh.thresh_low, 
                        pdb_settings.lv_charge_thresh.thresh_high);
            PDB_state.lv_charge = 1;
            break;
    }
}

/**********************************************************************************
 * LV_Charge_Task
 *
 * Function:
 *      Start at idle AWD set to [0,14]
 *      wait forever, wakes when Charge_Volt > 14V
 *      start counting, if successfully within threshold 14-14.6V begin charging
 *      Set AWD to trigger if it breaks charging threshold, disable charging and
 *      start again.
 * 
 *      Note: should probalby read the ADC again here for fresh data
 **********************************************************************************/
void LV_Charge_Task(void *params) {
    (void)params;
    uint32_t   v;
    uint8_t    counting = 0;
    TickType_t start    = 0;

    switch_lv_charge_state(idle);
    adc2_awd_arm();

    for (;;) {
        TickType_t timeout = counting ? pdMS_TO_TICKS(100) : portMAX_DELAY;
        if (xTaskNotifyWait(0, 0, &v, timeout) == pdFALSE) {
            counting = 0;
            continue;
        }

        if (lv_charge_state == idle) {
            if (!counting) {
                counting = 1;
                start = xTaskGetTickCount();
            }
            if (xTaskGetTickCount() - start >= pdMS_TO_TICKS(1000)) {
                if (v <= pdb_settings.lv_charge_thresh.thresh_high) {
                    counting = 0;
                    switch_lv_charge_state(charging);
                    adc2_awd_arm();
                    continue;
                } else {
                    counting = 0;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            adc2_awd_arm();
        } else {
            switch_lv_charge_state(idle);
            adc2_awd_arm();
        }
    }
}
