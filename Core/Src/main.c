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
 *    Current goals:
 *          Config
 *          lv_charge
 *          pwm
 *          channel control
 *          ADC logging
 *    Current tasks:
 *          lv_charge( charging stages )
 *          PDB_STARTUP(goes through each channel has a delay)
 *          shutdown log? or do we just send a message through can).
 *          CANRX
 *          Logging / super logging
 *          control/ change parameters, change flash (CRITICAL),
 *              Change what channel is super logging, change states, etc.
 *          
 *          
 *          
 **********************************************************************************/
#include <stdint.h>
#include "stm32h5xx.h"
#include "config.h"
#include "sys_params.h"
#include "main.h"
#include "delay.h"
#include "flash.h"
#include "lv_charge.h"
#include "pwm.h"
#include "shutdown_log.h"
#include "control.h"
#include "can.h"
#include "adc_dma.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

PDB_STATE_t PDB_state;

void CAN_rxTask(void* params);
void Channel_Control_Task(void* params);
void Logging_Task(void* params);
void Shutdown_Log_Task(void* params);
void Current_Log_Task(void* params);
void PDB_CTRL_Task(void* params);
TimerHandle_t lamp_check_timer;
static void lamp_check_off_cb(TimerHandle_t t) {(void)t; lamp_check_off();}

void main(){
    sys_clk_init();     /* 250 MHz first. HAL_Init sizes the HAL tick from the clocks it sees */
    delay_init();
    HAL_Init();
    settings_load();  /* before anything reads pdb_rt */
    adc_init();
    channel_ctrl_Init();
    logging_gpio_init();

    xTaskCreate(LV_Charge_Task,
                "lv_charge",
                LV_CHARGE_TASK_STACK,
                NULL,
                LV_CHARGE_TASK_PRIO,
                &lv_charge_task_handle);

    xTaskCreate(CAN_rxTask, "can1_rx",
                CAN_RX_TASK_STACK,
                (void*)CAN1_BUS,
                CAN1_RX_TASK_PRIORITY,
                NULL);
    xTaskCreate(CAN_rxTask, "can2_rx",
                CAN_RX_TASK_STACK,
                (void*)CAN2_BUS,
                CAN2_RX_TASK_PRIORITY,
                NULL);

    xTaskCreate(Shutdown_Log_Task, "shutdown_log_task",
               SHUTDOWN_LOG_TASK_STACK,
               NULL,
               SHUTDOWN_LOG_TASK_PRIO,
               NULL);
    xTaskCreate(Current_Log_Task, "current_log_task",
                CURRENT_LOG_TASK_STACK,
                NULL,
                CURRENT_LOG_TASK_PRIO,
                NULL
                );

    xTaskCreate(PDB_CTRL_Task, "pdb_ctrl_task",
                PDB_CTRL_TASK_STACK,
                NULL,
                PDB_CTRL_TASK_PRIO,
                NULL);
    // xTaskCreate(CAN_txTask,"can1_tx");
    // xTaskCreate(CAN_txTask,"can2_tx");
    lamp_check_timer = xTimerCreate("lamp_check",
                                pdMS_TO_TICKS(LAMP_CHECK_DURATION_MS),
                                pdFALSE,
                                NULL,
                                lamp_check_off_cb);
    vTaskStartScheduler();      /* never returns */
    for (;;) {
      // GO!!!!
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    __asm volatile("cpsid i");
    for (;;);
}

/* HAL */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
}
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

// Turn on the instruction cache (ICACHE->CR |= ICACHE_CR_EN