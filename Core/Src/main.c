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
#include <stdint.h>
#include "stm32h5xx.h"
#include "config.h"
#include "sys_params.h"
#include "main.h"
#include "delay.h"
#include "flash.h"
#include "FreeRTOS.h"

#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

PDB_STATE_t PDB_state;

void CAN_rxTask(void* params);
void Channel_Control_Task(void* params);
void LV_Task(void* params);
void Logging_Task(void* params);

void main(){
    sys_clk_init();     /* 250 MHz first. HAL_Init sizes the HAL tick from the clocks it sees */
    delay_init();
    HAL_Init();       /* NVIC priority grouping 4 (what FreeRTOS needs), HAL tick on TIM6 (stm32h5xx_hal_timebase_tim.c) */
    settings_load();  /* before anything reads pdb_rt */
    
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