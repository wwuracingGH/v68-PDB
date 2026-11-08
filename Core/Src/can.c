/**********************************************************************************
 * can.c
 *
 * Author:  WWU FSAE 
 *          Jackson Newman
 * 
 * Last Uptdated: 11/6/26
 *
 * Version: 0.1 v68
 *
 * Function:
 *      Uses Functions from v68-VCU
 **********************************************************************************/
#include <stdint.h>
#include <stddef.h>     /* offsetof */
#include <string.h>     /* memcpy, memcmp */
#include "stm32h533xx.h"
#include "can.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"
/* FreeRTOS */
static QueueHandle_t rx_queue[CAN_NUM_BUSES];   /* Two Queue Handles */
static SemaphoreHandle_t tx_mutex[CAN_NUM_BUSES];
static volatile uint32_t rx_dropped[CAN_NUM_BUSES];
static bool startup = true;

/* write the shared process can later*/
__attribute__((weak)) void Shared_processCAN(CAN_Message_t* msg) { (void)msg; }

/*******************************************************************************
 * CAN_busInit()
 *
 * Configures one FDCAN peripheral and puts it on the bus. Shared by both buses;
 * only the prescaler differs.
 *
 * Bit time = 5 tq (sync 1 + TSEG1 3 + TSEG2 1) -> sample point 80%.
 * Kernel clock 125 MHz, so nbrp = 125 MHz / (5 * baud) - 1 (see CANx_BAUD_PRES).
 *******************************************************************************/
static void CAN_busInit(FDCAN_GlobalTypeDef* FDCAN, uint32_t nbrp) {
    /* Wake from sleep */
    FDCAN->CCCR &= ~FDCAN_CCCR_CSR;
    while (FDCAN->CCCR & FDCAN_CCCR_CSA);

    /* Enter init mode, then unlock the protected config registers */
    FDCAN->CCCR |= FDCAN_CCCR_INIT;
    while ((FDCAN->CCCR & FDCAN_CCCR_INIT) == 0);
    FDCAN->CCCR |= FDCAN_CCCR_CCE;
    while ((FDCAN->CCCR & FDCAN_CCCR_CCE) == 0);

    /* TODO: get rid of this */
    FDCAN->CCCR |= FDCAN_CCCR_DAR;   /* disable automatic retransmission */
    FDCAN->CCCR |= FDCAN_CCCR_FDOE;  /* FD frame format enabled */

    FDCAN->NBTP = (nbrp << FDCAN_NBTP_NBRP_Pos) |
                  (2 << FDCAN_NBTP_NTSEG1_Pos) |
                  (0 << FDCAN_NBTP_NTSEG2_Pos) |
                  (0 << FDCAN_NBTP_NSJW_Pos);

    /* No filters: accept all std + ext frames into FIFO0, overwrite when full */
    FDCAN->RXGFC |= (0 << FDCAN_RXGFC_ANFS_Pos) |
                    (0 << FDCAN_RXGFC_ANFE_Pos) |
                    FDCAN_RXGFC_F0OM | FDCAN_RXGFC_F1OM;

    FDCAN->TXBC &= ~FDCAN_TXBC_TFQM;  /* Tx FIFO mode, not queue */

    /* RX FIFO0 new msg -> IRQ line 0, RX FIFO1 new msg -> IRQ line 1 */
    FDCAN->IE  |= FDCAN_IE_RF0NE | FDCAN_IE_RF1NE;
    FDCAN->ILS |= FDCAN_ILS_RXFIFO1;
    FDCAN->ILE |= FDCAN_ILE_EINT0 | FDCAN_ILE_EINT1;

    /* Leave init mode (also clears CCE): peripheral joins the bus */
    FDCAN->CCCR &= ~FDCAN_CCCR_INIT;
    while (FDCAN->CCCR & FDCAN_CCCR_INIT);
}

void CAN_init() {
    /* First call only: create RTOS objects (CAN_reset() re-enters here) */
    if (startup) {
        for (int i = 0; i < CAN_NUM_BUSES; i++) {
            rx_queue[i] = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(CAN_Message_t));
            tx_mutex[i] = xSemaphoreCreateMutex();
            configASSERT(rx_queue[i] != NULL && tx_mutex[i] != NULL);
        }
        startup = false;
    }

    /* One clock enable covers both FDCAN peripherals */
    RCC->APB1HENR |= RCC_APB1HENR_FDCANEN;
    __DSB(); /* make sure the clock is on before touching FDCAN registers */

    CAN_busInit(FDCAN1, CAN1_BAUD_PRES);
    CAN_busInit(FDCAN2, CAN2_BAUD_PRES);

    /* Priorities: CAN1 (CTRL) preempts CAN2 */
    NVIC_SetPriority(FDCAN1_IT0_IRQn, CAN1_IRQ_PRIORITY);
    NVIC_SetPriority(FDCAN1_IT1_IRQn, CAN1_IRQ_PRIORITY);
    NVIC_SetPriority(FDCAN2_IT0_IRQn, CAN2_IRQ_PRIORITY);
    NVIC_SetPriority(FDCAN2_IT1_IRQn, CAN2_IRQ_PRIORITY);

    NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
    NVIC_EnableIRQ(FDCAN1_IT1_IRQn);
    NVIC_EnableIRQ(FDCAN2_IT0_IRQn);
    NVIC_EnableIRQ(FDCAN2_IT1_IRQn);
}

void CAN_reset() {
    /* Disable interrupts */
    NVIC_DisableIRQ(FDCAN1_IT0_IRQn);
    NVIC_DisableIRQ(FDCAN1_IT1_IRQn);
    NVIC_DisableIRQ(FDCAN2_IT0_IRQn);
    NVIC_DisableIRQ(FDCAN2_IT1_IRQn);

    RCC->APB1HENR &= ~RCC_APB1HENR_FDCANEN;
    __DSB();
    RCC->APB1HRSTR |= RCC_APB1HRSTR_FDCANRST;
    __DSB();
    RCC->APB1HRSTR &= ~RCC_APB1HRSTR_FDCANRST;
    __DSB();

    CAN_init();
}

uint32_t _get_id(FDCAN_Rx_FIFO_Element_Typedef* msg) {
    return (msg->H0 & FDCAN_RXBH0_XTD_Msk) ? (msg->H0 & FDCAN_RXBH0_EXTID_Msk) :
            (msg->H0 & FDCAN_RXBH0_STDID_Msk) >> FDCAN_RXBH0_STDID_Pos;
}

uint8_t _get_dlc(FDCAN_Rx_FIFO_Element_Typedef* msg) {
    return (msg->H1 & FDCAN_RXBH1_DLC_Msk) >> FDCAN_RXBH1_DLC_Pos;
}

uint32_t* _get_data(FDCAN_Rx_FIFO_Element_Typedef* msg) {
    return msg->data;
}

int CAN_bytesFromDLC(int dlc) {
    const int bytes[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64 };
    return bytes[dlc]; 
}
int CAN_DLCFromBytes(int bytes) {
    if (bytes <= 8) return bytes;
    if (bytes <= 24) return (bytes + 27) >> 2;
    return (bytes + 191) >> 4;
}

/* FreeRTOS */
void CAN_sendmessage(FDCAN_GlobalTypeDef* FDCAN, uint16_t id, uint8_t length, uint8_t* data) {
    SemaphoreHandle_t lock; /* Mutex Selector */

    /* Select which mutex */
    if (FDCAN == FDCAN1) {
        lock = tx_mutex[CAN1_BUS];
    } else if (FDCAN ==FDCAN2) {
        lock = tx_mutex[CAN2_BUS];
    } else {
        return;
    }

    /* 
     * Take one of the mutexs if failed to acquire bail 
     * xSemaphoreTake outputs a BaseType_t of pdTRUE or pdFALSE
     * which equates to mutex acquired or failed to acquire mutex
     */
    if (xSemaphoreTake(lock, CAN_TX_LOCK_WAIT) != pdTRUE) return;
    
    /*
     * FDCAN->TXFQS & FDCAN_TXFQS_TFQF_Msk:
     * Tx FIFO/QUEUE Status Reg the mask looks at only the FIFO/QUEUE FULL
     * bit. Waits for the queue to not be full then exits.
     * If time out, release mutex and return.
     */
    int timeout = 100000;
    while ((FDCAN->TXFQS & FDCAN_TXFQS_TFQF_Msk) && timeout) timeout--;
    if (FDCAN->TXFQS & FDCAN_TXFQS_TFQF_Msk) {
        xSemaphoreGive(lock);
        return;
    }
    /* first open buffer slot */
    int j = (FDCAN->TXFQS & FDCAN_TXFQS_TFQPI_Msk) >> FDCAN_TXFQS_TFQPI_Pos;

    /* Find the indexed buffer */
    FDCAN_SRAM_Typedef * fd_ram = (FDCAN == FDCAN1) ? FDCAN1_RAM : FDCAN2_RAM;
    FDCAN_Tx_FIFO_Element_Typedef *msg = &fd_ram->tx_buffers[j];

    msg->H0 = 0; /* 11 bit ID goes to T0 */
    msg->H0 |= ((uint32_t)id << FDCAN_TXBH0_STDID_Pos); /* STDID */

    uint8_t buf[8] = {0};
    if (length > 8) length = 8;
    memcpy(buf, data, length);
    msg->H1 = ((uint32_t)length << FDCAN_TXBH1_DLC_Pos);
    memcpy((void*)msg->data, buf, 8);

    FDCAN->TXBAR |= (1UL << j); /* Trigger message by writing to TX buffer */
    xSemaphoreGive(lock);   /* Release mutex */
}

static void CAN_receiveMessage(uint32_t id, uint16_t bus,uint16_t len, uint32_t* data, BaseType_t* xHigherPriorityTaskWoken) {
    /* Create a CAN message */
    CAN_Message_t msg = {id, len, bus, {data[0], data[1]}};

    /* 
     * Sends the message to its respective queue, sets xHigher... to true if
     * the taks it interrupted had lower priority and returns if it succeeded or 
     *
     * 
     */
    if (xQueueSendFromISR(rx_queue[bus], &msg, xHigherPriorityTaskWoken) != pdTRUE) rx_dropped[bus]++;
}

/*******************************************************************************
 * fdcanX_itY_handler()
 * 
 * Activates when the FIFO interrupt triggers. Sends it to CAN_FIFO_Process...
 * 
 *******************************************************************************/
static void CAN_FIFO_Process(FDCAN_GlobalTypeDef* FDCAN, CAN_Bus_t bus, int fifo, BaseType_t* xHigherPriorityTaskWoken) {
    /*
     * Find where the bus's recieved frames are stored. Each CAN peripheral has
     * its own block of message in RAM, (ID, Len, data), Bus 0 = CAN1, Bus
     * 1 = CAN2, Additionally there are two FIFO blocks (0 and 1) that represent 
     * the two mail boxes for each CAN bus. We must know the bus and which FIFO block.
     */
    FDCAN_SRAM_Typedef* fd_ram = (bus == CAN1_BUS) ? FDCAN1_RAM : FDCAN2_RAM;

    /*
     * Select the status register of FDCAN RX FIFO x.
     * Bits [3:0] fill level or number of elements stored
     * Bits [9:8] index pointer of oldest frame to read
     * Bits [16-17] index to next frame to fill/ write
     */
    volatile uint32_t* status = fifo ? &FDCAN->RXF1S : &FDCAN->RXF0S;
    volatile uint32_t* ack    = fifo ? &FDCAN->RXF1A : &FDCAN->RXF0A;
    FDCAN_Rx_FIFO_Element_Typedef* elements = fifo ? fd_ram->rx_fifo1 : fd_ram->rx_fifo0;

    /* RXF0S and RXF1S share a layout, so the FIFO0 masks work for both */
    while (*status & FDCAN_RXF0S_F0FL_Msk) {
        uint32_t idx = (*status & FDCAN_RXF0S_F0GI_Msk) >> FDCAN_RXF0S_F0GI_Pos;
        FDCAN_Rx_FIFO_Element_Typedef* msg = elements + idx;
        CAN_receiveMessage(_get_id(msg), bus, CAN_bytesFromDLC(_get_dlc(msg)), _get_data(msg), xHigherPriorityTaskWoken);
        *ack = idx;
    }

}

/*******************************************************************************
 * fdcanX_itX_handler()
 * 
 * Activates when the FIFO interrupt triggers. Sends it to CAN_FIFO_Process...
 * 
 *******************************************************************************/
void FDCAN1_IT0_IRQHandler() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    FDCAN1->IR = FDCAN_IR_RF0N;    /* Clear new message */
    CAN_FIFO_Process(FDCAN1, CAN1_BUS, 0, &xHigherPriorityTaskWoken);
    /* If the CAN_rxTask has higher PRIO than what it interrupted, switch*/
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void FDCAN1_IT1_IRQHandler() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    FDCAN1->IR = FDCAN_IR_RF1N;    /* Clear new message */
    CAN_FIFO_Process(FDCAN1, CAN1_BUS, 1, &xHigherPriorityTaskWoken);
    /* xHigherPriorityTaskWoken = pdFALSE then no context switch
     * xHigherPriorityTaskWoken = pdTRUE switch context
     * Example:
     *      Logger task running (low PRIO), then the CAN task fires
     *
     */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void FDCAN2_IT0_IRQHandler() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    FDCAN2->IR = FDCAN_IR_RF0N;    /* Clear new message */
    CAN_FIFO_Process(FDCAN2, CAN2_BUS, 0, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void FDCAN2_IT1_IRQHandler() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    FDCAN2->IR = FDCAN_IR_RF1N;    /* Clear new message */
    CAN_FIFO_Process(FDCAN2, CAN2_BUS,1, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

bool CAN_getMessage(uint16_t bus, CAN_Message_t* msg, TickType_t wait) {
    return xQueueReceive(rx_queue[bus], msg, wait) == pdTRUE;
}

uint32_t CAN_rxDropped(uint16_t bus) {
    return rx_dropped[bus];
}
/*******************************************************************************
 * void CAN_rxTask(void* params)
 * 
 * params given at initialization (see main.c). Basically two tasks, one for each
 * CAN peripheral (two), CAN1 (can1_rx) is higher prio so if CAN2 is writing or
 * logging and CAN2's Queue has messages CAN1 will empty its messages first.
 * Loops stops when CAN_getMessage()'s xQueueReceive(rx_queue[bus], msg, wait)
 * notices queue is empty and sleeps letting other tasks run.
 * unblocks when additional messages come into the queue. When a message arrives
 * in the FIFO an interrupt fires and takes over the CPU (interrupted tasks TCB
 * gets sent to the stack). ISR functions requests xQueueSendFromISR() to write
 * the message to the queue, at the same time it compares the blocked task's
 * priority to the interrupted, regardless it places the blocked task CAN_rxTask
 * onto the READY list according to its priority. xQueueSendFromISR() changes
 * xHigherPriorityTaskWoken if the blocked task (CAN_rxTask) has higher priority
 * than the interrupted task. portYIELD_FROM_ISR(xHigherPriorityTaskWoken) will
 * activate the task switch/preemption if true. At that point instead of returning
 * to the lower priority task it just runs CAN_rxTask instead.
 *******************************************************************************/
void CAN_rxTask(void* params) {
    CAN_Bus_t bus = (CAN_Bus_t)(uint32_t)params;
    CAN_Message_t msg;

    for (;;) {
        if (CAN_getMessage(bus, &msg, portMAX_DELAY))
            Shared_processCAN(&msg);
    }
}