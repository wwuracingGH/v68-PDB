#ifndef CAN_H
#define CAN_H

#include "stm32h5xxCANSRAM.h"
#include <stdbool.h>
#include "FreeRTOS.h"



#define CAN1_BAUD 250000UL
#define CAN2_BAUD 1000000UL

#define CAN1_BAUD_PRES ((125000000UL / (5UL * CAN1_BAUD)) - 1UL)
#define CAN2_BAUD_PRES ((125000000UL / (5UL * CAN2_BAUD)) - 1UL)

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