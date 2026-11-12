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
 * TODO:
 *      Add startup
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
 *  CHARGE  -> EN_UVLO of the LM74801 ideal diode on the LV charge path (Ideal_Diode sheet) */

/* Current limit -> DAC code, x in mA:
 * C = 1024/3.3 * (1.22 + R_DAC * (1.22/R_ILIM - 0.001x * R_SENSE / (9.83 * R_SET))) */
#define DAC_VREF_V      3.3f
#define ILIM_VREF_V     1.22f
#define CS_GAIN         9.83f
#define R_SENSE_OHM     0.005f
#define R_ILIM_OHM      68000.0f
#define R_DAC_OHM       120000.0f
#define R_SET_OHM       51.0

static const ch_pin_t ctrl_pins[PDB_NUM_CTRL] = {
    [PDB_CH1_CTRL]    = { GPIOB,  12}, 
    [PDB_CH2_CTRL]    = { GPIOA,  7 },
    [PDB_CH3_CTRL]    = { GPIOC,  4 },   /* ANDed with MC_RST (U1) */
    [PDB_CH4_CTRL]    = { GPIOC,  5 },
    [PDB_CH5_CTRL]    = { GPIOB,  0 },
    [PDB_CH6_CTRL]    = { GPIOB,  1 },   /* CH6 output is the SHUTDOWN pin on the output connector */
    [PDB_CH7_CTRL]    = { GPIOB,  2 },
    [PDB_CH8_CTRL]    = { GPIOB, 10 },
    [PDB_CHARGE_CTRL] = { GPIOC, 13 }  /* PC13 might have an issue */
};

/* DAC53608, 8 ch 10-bit I2C DAC (datasheet SLASEQ4A)
 * Frame: [addr+W][cmd][MSB][LSB], every register is 16 bits sent MSB first.
 * All channels power up at 10k to AGND and stay there until their PDN bit is cleared.
 * Wait 5 ms after VDD before talking to it. No EEPROM, codes are 0 after every reset */
/* Command bytes (register addresses) */
#define DAC_CMD_DEVICE_CONFIG   0x01U   /* power down bits, resets to 0x00FF (all channels off) */
#define DAC_CMD_STATUS_TRIGGER  0x02U   /* device ID (read) + soft reset (write), resets to 0x0300 */
#define DAC_CMD_BRDCAST         0x03U   /* write one code to every channel */
#define DAC_CMD_DACA_12VOUT     0x08U
#define DAC_CMD_DACB_5VOUT      0x09U
#define DAC_CMD_DACC_CH1        0x0AU
#define DAC_CMD_DACD_CH2        0x0BU
#define DAC_CMD_DACE_CH3        0x0CU
#define DAC_CMD_DACF_CH4        0x0DU
#define DAC_CMD_DACG_CH5        0x0EU
#define DAC_CMD_DACH_CH6        0x0FU
#define DAC_CMD_DAC_DATA(ch)    (DAC_CMD_DACA_12VOUT + (ch))  /* ch 0-7 = VOUTA-VOUTH */

/* DEVICE_CONFIG bits */
#define DAC_CFG_PDN(ch)         (1U << (ch))    /* 1 = channel ch (0-7) powered down, 10k to AGND */
#define DAC_CFG_PDN_ALL         (1U << 8)       /* 1 Bit 8 powers all channels down */
#define DAC_CFG_ALL_ON          0x0000U         /* All 8 Channel enable */

/* STATUS/TRIGGER bits */
#define DAC_DEVICE_ID_Pos       6U
#define DAC_DEVICE_ID_Msk       (0x3FU << DAC_DEVICE_ID_Pos)
#define DAC_DEVICE_ID_53608     0x0CU           /* 001100. DAC43608 reads 010100 */
#define DAC_SW_RST              0x000AU         /* 1010 in bits 3:0. Datasheet 8.3.4 says "0x1010", the register table says 1010 binary */

/* DACn_DATA / BRDCAST: 10-bit code left aligned in bits 11:2. VOUT = code / 1024 * VREFIN */
#define DAC_DATA_Pos            2U
#define DAC_DATA_MAX            0x3FFU
#define DAC_DATA(code)          ((uint16_t)(((code) & DAC_DATA_MAX) << DAC_DATA_Pos))

/* 7-bit addresses, 1001 + A0 strap. HAL takes them << 1 like the pots */
#define DAC_ADDR_A0_AGND    0x48U
#define DAC_ADDR_A0_VDD     0x49U
#define DAC_ADDR_A0_SDA     0x4AU
#define DAC_ADDR_A0_SCL     0x4BU
#define DAC_ADDR_BRDCAST    0x47U       /* hits every DACx3608 on the bus, write only */

/* One DAC on I2C1 */
static const uint8_t dac_addr = DAC_ADDR_A0_AGND;   /* TODO: confirm A0 strap on the schematic */

/* Data register for each DAC output, indexed by PDB_DAC_t */
static const uint8_t dac_ch_reg[PDB_NUM_DAC] = {
    [PDB_DAC_CH1]    = DAC_CMD_DACC_CH1,
    [PDB_DAC_CH2]    = DAC_CMD_DACD_CH2,
    [PDB_DAC_CH3]    = DAC_CMD_DACE_CH3,
    [PDB_DAC_CH4]    = DAC_CMD_DACF_CH4,
    [PDB_DAC_CH5]    = DAC_CMD_DACG_CH5,
    [PDB_DAC_CH6]    = DAC_CMD_DACH_CH6,
    [PDB_DAC_12VOUT] = DAC_CMD_DACA_12VOUT,
    [PDB_DAC_5AVOUT] = DAC_CMD_DACB_5VOUT,
};

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
    MX_I2C1_Init();
    ch_gpio_init();
    ch_cur_ctrl();


    return status;
}
/**********************************************************************************
 * cur_lim_set
 *
 * Function:
 *      Given a selected channel set its volatile or non-volatile cur limits
 **********************************************************************************/
uint8_t cur_lim_set(PDB_Channel_t Channel,  uint8_t val, uint8_t volatility) {

}

/**********************************************************************************
 * PDB_STARTUP
 *
 * Function:
 *      Turns CH1-CH8 on, each pdb_rt.delay_ms[ch] after this starts, so the order
 *      comes from the delays. CH_CFG_DELAY_NONE channels are left off, the charge
 *      path is lv_charge.c's. Blocks with vTaskDelay so it has to run in a task,
 *      needs high priority
 **********************************************************************************/
void PDB_STARTUP(void) {
    uint8_t    order[PDB_NUM_OUTPUTS];
    uint32_t   n      = 0;
    TickType_t waited = 0;

    /* Channels that start, sorted by delay (insertion sort, 8 entries) */
    for (uint32_t ch = PDB_CH1_CTRL; ch <= PDB_CH8_CTRL; ch++) {
        if (pdb_rt.delay_ms[ch] == CH_CFG_DELAY_NONE) {
            continue;
        }
        uint32_t j = n++;
        while (j > 0U && pdb_rt.delay_ms[order[j - 1U]] > pdb_rt.delay_ms[ch]) {
            order[j] = order[j - 1U];
            j--;
        }
        order[j] = (uint8_t)ch;
    }

    for (uint32_t i = 0; i < n; i++) {
        TickType_t due = pdMS_TO_TICKS(pdb_rt.delay_ms[order[i]]);
        if (due > waited) {
            vTaskDelay(due - waited);
            waited = due;
        }
        /* TODO: return here if a shutdown/fault happened during the last delay */
        PDB_CHANNEL_CTRL_REQ((PDB_CTRL_t)order[i], 1);
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

/**********************************************************************************
 * efuse_cur_ctrl
 *
 * Function:
 *      Load every current limit, then power all 8 DAC outputs up in one write.
 *
 * Returns:
 *      Bitmask of outputs that failed bit 0 = PDB_DAC_CH1.
 *      0xFF = power-up write failed. 0 = SUCCESS
 **********************************************************************************/
uint8_t efuse_cur_ctrl() {
    uint8_t status = 0;

    for (uint32_t i = 0; i < PDB_NUM_DAC; i++) {
        //uint16_t count = cur_to_count();
        if (count == 0xFFFFU || dac_write(dac_ch_reg[i], DAC_DATA(code)) != HAL_OK) {
            status |= (uint8_t)(1U << i);
        }
    }
    /* If any failures exit */
    if (status != 0U) {
        return status;
    }
    /* Turn them all on */
    if (dac_write(DAC_CMD_DEVICE_CONFIG, DAC_CFG_ALL_ON) != HAL_OK) {
        return 0xFFU;
    }
    return 0;
}

/**********************************************************************************
 * dac_write
 *
 * Function:
 *      Data structure:
 *      | B22-B16 CMD byte | B15-B12 NULL | B11-B2 Data| B1-B0 NULL|
 *      Data is MSB left aligned
 **********************************************************************************/
static HAL_StatusTypeDef dac_write(uint8_t reg, uint16_t val) {
    uint8_t buf[2] = { (uint8_t)(val >> 8), (uint8_t)val };          /* MSB first */
    return HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(dac_addr << 1), reg, I2C_MEMADD_SIZE_8BIT,
                             buf, 2, I2C_TIMEOUT_MS);
}

/**********************************************************************************
 * HAL_StatusTypeDef dac_read
 *
 * Function:
 *      Data structure:
 *      | B22-B16 CMD byte | B15-B12 NULL | B11-B2 Data| B1-B0 NULL|
 *      Data is MSB left aligned
 **********************************************************************************/
static HAL_StatusTypeDef dac_read(uint8_t reg, uint16_t *val) {
    uint8_t buf[2] = { 0 };
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(dac_addr << 1), reg, I2C_MEMADD_SIZE_8BIT,
                                            buf, 2, I2C_TIMEOUT_MS);
    *val = (uint16_t)((buf[0] << 8) | buf[1]);
    return st;
}

/**********************************************************************************
 * cur_ma_to_dac_code
 *
 * Function:
 **********************************************************************************/
static uint16_t cur_to_count(float mA) {
    float c = CUR_MA_TO_DAC_COUNTS(mA) + 0.5f;     /* round */
    if (c < 0.0f || c > (float)DAC_DATA_MAX) {
        return 0xFFFFU;
    }
    return (uint16_t)c;
}
