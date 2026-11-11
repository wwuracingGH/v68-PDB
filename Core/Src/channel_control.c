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
#include "main.h"

#include "FreeRTOS.h"
#include "channel_control.h"
#include "flash.h"

#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"
/* Control outputs, all active high. Every control net has a 10k pull-down on the board,
 * so everything is off until the MCU drives it high.
 *   CH1-CH8 -> EN + SLEEP_N of that channel's LM74910H eFuseDone with init nexdt shit  

 *  Need to add startup but writing to TLP should be done 
 *  CHARGE  -> EN_UVLO of the LM74801 ideal diode on the LV charge path (Ideal_Diode sheet) */
static const ch_pin_t ctrl_pins[PDB_NUM_CTRL] = {
    [PDB_CH1_CTRL]    = { GPIOB,  12 }, 
    [PDB_CH2_CTRL]    = { GPIOA,  7 },
    [PDB_CH3_CTRL]    = { GPIOC,  4 },   /* ANDed with MC_RST (U1) */
    [PDB_CH4_CTRL]    = { GPIOC,  5 },
    [PDB_CH5_CTRL]    = { GPIOB,  0 },
    [PDB_CH6_CTRL]    = { GPIOB,  1 },   /* CH6 output is the SHUTDOWN pin on the output connector */
    [PDB_CH7_CTRL]    = { GPIOB,  2 },
    [PDB_CH8_CTRL]    = { GPIOB, 10 },
    [PDB_CHARGE_CTRL] = { GPIOC, 13 }  /* PC13 might have an issue */
};

/* Each TPL0102 addresses */
static const uint8_t pot_addr[PDB_NUM_POTS] = {
    0x50,   /* CH1 A2 A1 A0 = 000 */
    0x51,   /* CH2            001 */
    0x52,   /* CH3            010 */
    0x53,   /* CH4            011 */
    0x54,   /* CH5            100 */
    0x55,   /* CH6            101 */
};

/* TPL0102 register addresses (datasheet 7.6) */
/* Thanks to Joji's infinite wisdom we only use POT A on each IC */
/* ACR is 8- bits, bit 7 selects vol or non-vol mem, bit 6 it shutdown */
#define TPL_REG_WRA     0x00U       /* Wiper resistance*/
#define TPL_REG_ACR     0x10U       /* access control register, resets to 0x40 */
#define TPL_ACR_VOL     (1U << 7)   /* 1 = writes only hit the wiper reg. 0 (reset) = every write also burns EEPROM (20 ms, 100k cycles) */
#define TPL_ACR_SHDN    (1U << 6)   /* 1 = normal. 0 = shutdown, H goes high-Z and ILIM floats. Keep it 1 */

#define I2C_TIMEOUT_MS  10U         /* HAL timeout per transfer (HAL tick on TIM6). A 3 byte write at 100 kHz is ~0.3 ms */

I2C_HandleTypeDef hi2c1;

/**********************************************************************************
 * ch_gpio_init
 *
 * Function:
 *      CH1-CH8 and charge enable pins as push-pull outputs, driven low (off).
 *      Low speed, which also keeps PC13 under its 2 MHz limit
 **********************************************************************************/
static void ch_gpio_init(void) {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOCEN;
    (void)RCC->AHB2ENR;
    for (uint32_t ch = 0; ch < PDB_NUM_CTRL; ch++) {
        GPIO_TypeDef *port = ctrl_pins[ch].port;
        uint32_t      pin  = ctrl_pins[ch].pin;

        port->BSRR     = 1U << (pin + 16U);                /* ODR low first so the pin never glitches high */
        port->OTYPER  &= ~(1U << pin);                     /* push-pull */
        port->PUPDR   &= ~(3U << (2U * pin));              /* no pull, board has the pull-down */
        port->OSPEEDR &= ~(3U << (2U * pin));              /* low speed */
        port->MODER    = (port->MODER & ~(3U << (2U * pin)))
                       | (1U << (2U * pin));               /* 01 = output */
    }
}

/**********************************************************************************
 * HAL_I2C_MspInit
 *
 * Function:
 *      HAL I2C INIT
 **********************************************************************************/
void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(hi2c->Instance==I2C1)
  {
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
    PeriphClkInitStruct.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**I2C1 GPIO Configuration
    PB6     ------> I2C1_SCL
    PB7     ------> I2C1_SDA
    */
    GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Peripheral clock enable */
    __HAL_RCC_I2C1_CLK_ENABLE();
  }

}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef* hi2c) {
  if(hi2c->Instance==I2C1)
  {
    /* Peripheral clock disable */
    __HAL_RCC_I2C1_CLK_DISABLE();

    /**I2C1 GPIO Configuration
    PB6     ------> I2C1_SCL
    PB7     ------> I2C1_SDA
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6);
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_7);
  }
}

static void MX_I2C1_Init(void) {
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x10707DBC;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**********************************************************************************
 * channel_ctrl_Init
 *
 * Function:
 *      All channel and charge enables low, I2C1 up, every TPL0102 switched to volatile-only
 *      wiper writes. Wipers are left at their power-up value (EEPROM, factory 0x80)
 *      until current_limit_set() writes them. Needs HAL_Init() first (I2C1 is HAL).
 *
 * Returns:
 *      Bitmask of pots that did not ACK, bit 0 = CH1 ... bit 5 = CH6. 0 = all good
 **********************************************************************************/
uint8_t channel_ctrl_Init(void) {
    uint8_t status = 0;
    ch_gpio_init();
    MX_I2C1_Init();

    /* Initialize the TPL0102 Variable resistor */
    /* data sheet says pad the MSB with a 0 to get a 8-bit address*/
    /* HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress,
                                    uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout) */
    for (uint32_t i = 0; i < PDB_NUM_POTS; i++) {
        uint8_t acr = TPL_ACR_VOL | TPL_ACR_SHDN;
        /* HAL takes the 7-bit address shifted left by 1 */
        if (HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(pot_addr[i] << 1), TPL_REG_ACR, I2C_MEMADD_SIZE_8BIT,
                              &acr, 1, I2C_TIMEOUT_MS) != HAL_OK) {
            status |= (uint8_t)(1U << i);
        }
    }
    return status;
}
/**********************************************************************************
 * cur_lim_set
 *
 * Function:
 *      Given a selected channel set its volatile or non-volatile cur limits
 **********************************************************************************/
uint8_t cur_lim_set(PDB_Channel_t Channel,  uint8_t val, uint8_t volatility) {
    /**/
    uint8_t status =    0;
    uint8_t acr =       0;

    if (volatility == VOL_MEM){
        acr = TPL_ACR_VOL | TPL_ACR_SHDN;
    } else {
        acr = TPL_ACR_SHDN;
    }
    if (HAL_I2C_Mem_Write(&hi2c1, (uint16_t)Channel, TPL_REG_ACR, I2C_MEMADD_SIZE_8BIT,
                                &acr, 1, I2C_TIMEOUT_MS) != HAL_OK) {
        status = 1;
    }
    return status;
}

/**********************************************************************************
 * PDB_STARTUP
 *
 * Function:
 *      This is going to have to be a task because it waits, needs high priority
 *          also need the delay
 **********************************************************************************/
void PDB_STARTUP(void) {
    for (uint32_t i = 0; i < PDB_STARTUP_CH_NUM; i++) {
        /* TODO: return here if a shutdown/fault happened during the last delay */
        // if fault! return
        PDB_CHANNEL_CTRL_REQ((PDB_CTRL_t)pdb_settings.startup.channel[i], 1);
        if (i < PDB_STARTUP_CH_NUM - 1U) {
            vTaskDelay(pdMS_TO_TICKS(pdb_settings.startup.delay_ms[i]));
        }
    }
}

/**********************************************************************************
 * PDB_CHANNEL_CTRL_REQ
 *
 * Function:
 *      Turn one control output on (val != 0) or off: CH1-CH8 or the charge path.
 *      One atomic BSRR write, safe from tasks and ISRs. Out of range values are ignored.
 **********************************************************************************/
void PDB_CHANNEL_CTRL_REQ(PDB_CTRL_t ctrl, uint8_t val) {
    if ((uint32_t)ctrl >= PDB_NUM_CTRL) {
        return;
    }
    GPIO_TypeDef *port = ctrl_pins[ctrl].port;
    uint32_t      pin  = ctrl_pins[ctrl].pin;
    port->BSRR = val ? (1U << pin)              /* on */
                     : (1U << (pin + 16U));     /* off */
}

/**********************************************************************************
 * PDB_CHANNEL_SHUTDOWN
 *
 * Function:
 *      Turns every channel (CH1-CH8) off by pulling its eFuse enable low.
 *      Only BSRR writes (one atomic store per channel), no HAL, I2C or FreeRTOS
 *      calls, so it is safe from a task, an ISR or a fault handler.
 *      Leaves the charge path (PDB_CHARGE_CTRL) as it is.
 **********************************************************************************/
void PDB_CHANNEL_SHUTDOWN(void) {
    for (uint32_t ch = PDB_CH1_CTRL; ch <= PDB_CH8_CTRL; ch++) {
        PDB_CHANNEL_CTRL_REQ((PDB_CTRL_t)ch, 0);
    }
}