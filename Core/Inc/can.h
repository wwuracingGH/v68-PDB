#ifndef CAN_H
#define CAN_H

#include "stm32h5xxCANSRAM.h"
#include <stdbool.h>
#include "FreeRTOS.h"
#include "config.h"

#define CAN1_BAUD 250000UL
#define CAN2_BAUD 1000000UL

/* FDCAN kernel clock = 16 MHz HSE (config.c). Nominal bit = 16 time quanta at both rates:
 * sync 1 + TSEG1 13 + TSEG2 2 -> sample point 87.5% (CiA recommendation), resync jump 2 tq */
#define CAN_KER_CLK_HZ  HSE_CLK_HZ
#define CAN_TQ_PER_BIT  16UL
#define CAN_TSEG1       13UL
#define CAN_TSEG2       2UL
#define CAN_SJW         2UL

#define CAN1_BAUD_PRES ((CAN_KER_CLK_HZ / (CAN_TQ_PER_BIT * CAN1_BAUD)) - 1UL)   /* 3 -> 250 ns tq */
#define CAN2_BAUD_PRES ((CAN_KER_CLK_HZ / (CAN_TQ_PER_BIT * CAN2_BAUD)) - 1UL)   /* 0 -> 62.5 ns tq */

_Static_assert(1UL + CAN_TSEG1 + CAN_TSEG2 == CAN_TQ_PER_BIT, "CAN bit segments must add up to CAN_TQ_PER_BIT");
_Static_assert(CAN_KER_CLK_HZ % (CAN_TQ_PER_BIT * CAN1_BAUD) == 0, "CAN1_BAUD does not divide evenly from the kernel clock");
_Static_assert(CAN_KER_CLK_HZ % (CAN_TQ_PER_BIT * CAN2_BAUD) == 0, "CAN2_BAUD does not divide evenly from the kernel clock");

/* FreeRTOS */
// Sys PRIO is 4 ?
#define CAN1_IRQ_PRIORITY 5 /*CAN1 contains CTRL*/
#define CAN2_IRQ_PRIORITY 6
#define CAN_RX_QUEUE_LEN 64
#define CAN_TX_LOCK_WAIT pdMS_TO_TICKS(5)
//#define CAN_NUM_BUSES 2
//extern SemaphoreHandle_t can_mutex;


typedef struct {
    uint32_t id;
    uint16_t dlc;
    uint16_t bus_id;
    uint32_t data[2];
} CAN_Message_t;

typedef enum { 
    CAN1_BUS = 0,
    CAN2_BUS = 1,
    CAN_NUM_BUSES = 2
} CAN_Bus_t;

void CAN_init();
void CAN_reset();
void CAN_sendmessage(FDCAN_GlobalTypeDef* FDCAN, uint16_t id, uint8_t length, uint8_t* data);
int CAN_bytesFromDLC(int dlc);
int CAN_DLCFromBytes(int bytes);
void CAN_rxTask(void* params);
void Shared_processCAN(CAN_Message_t* msg);

/* New */
uint32_t CAN_rxDropped(uint16_t bus);
bool CAN_getMessage(uint16_t bus, CAN_Message_t* msg, TickType_t wait);

#endif