/**********************************************************************************
 * Viking v68 PDB main.c
 * 
 * Jackson Newman
 * 10/14/26
 * 
 * Version: 0.1
 * 
 * Function:
 *      ...
 **********************************************************************************/

#include <stdint.h>
#include "stm32h5xx.h"

#include "FreeRTOS.h"

#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

void main(){
    
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    __asm volatile("cpsid i");
    for (;;);
}


// Turn on the instruction cache (ICACHE->CR |= ICACHE_CR_EN
