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
#include "main.h"
#include "delay.h"
#include "FreeRTOS.h"

#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

void CAN_rxTask(void* params);
void Channel_Control_Task(void* params);
void LV_Task(void* params);
void Logging_Task(void* params);

void main(){
    sys_clk_init();     /* 250 MHz first. HAL_Init sizes the HAL tick from the clocks it sees */
    delay_init();
    
    delay_init();       /* DWT cycle counter for delay_us/delay_ms */
    HAL_Init();       /* NVIC priority grouping 4 (what FreeRTOS needs), HAL tick on TIM6 (stm32h5xx_hal_timebase_tim.c) */

    

}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    __asm volatile("cpsid i");
    for (;;);
}








/* HAL */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  /* TODO*/
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

// Turn on the instruction cache (ICACHE->CR |= ICACHE_CR_EN