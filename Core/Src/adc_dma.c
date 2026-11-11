/**********************************************************************************
 * adc_dma.c
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
#include "adc_dma.h"
#include "config.h"
#include "stm32h533xx.h"
#include "sys_params.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

#include "delay.h"

#define ADC_VREG_DELAY_US 20UL  /* Must delay after waking from deep power-down 20us */
#define ADC1_GPDMA_REQ    0U
#ifndef LV_CHARGE_PRIO
#define LV_CHARGE_PRIO    6U
#endif

/* Scan order: index = rank-1 = slot in adc_buf. Single source of truth for SQR + adc_read */
static const uint8_t adc_rank[ADC_NUM_CH] = {
    ADC_CH1_CUR, ADC_CH2_CUR, ADC_CH3_CUR, ADC_CH4_CUR,
    ADC_CH5_CUR, ADC_CH6_CUR, ADC_CH7_CUR, ADC_CH8_CUR,
    ADC_BUS_CUR, ADC_BUS_VOLT
};

/* DMA writes here, newest scan always present (circular) */
static volatile uint16_t adc_buf[ADC_NUM_CH];

/* GPDMA has no circular bit: one linked-list node that points at itself reloads
 * block size + dest address each time the block finishes. Node layout follows CLLR flags
 * (in register order): CBR1, CDAR, CLLR */
static uint32_t adc_ll_node[3] __attribute__((aligned(4)));

/**********************************************************************************
 * adc2_config
 *
 * Function:
 *      ADC2 = LV charge voltage only (PA6, ADC2_INP3), continuous, no DMA.
 *      AWD1 watches that one channel; window starts wide open (0-4095, never trips)
 *      until adc2_awd_set(). Called from adc_init() after power-up + calibration,
 *      while ADC2 is still disabled (CFGR, CFGR2, SQR, SMPR need ADEN = 0 / ADSTART = 0).
 **********************************************************************************/
static void adc2_config(void) {
    /* TODO Overmod could break alignment of channel order turn of or on?*/
    /* RES = 12-bit (0)
     * Right alignment (0)
     * Hardware trigger disabled (0) (maybe change later)
     * Discontinuous disabled (0)
     * Auto Delayed conversion mode disabled (0)
     * DMA disabled (0)
     * DMA CFG disabled
     * Analog watchdog channel disable
     * Continuous
     * Overmod
     * Analog Watch Dog on just PA6 (Charge Volt)
     * AWD on just PA6
     * Enable AWD1 */
    ADC2->CFGR = (ADC2->CFGR & ~(ADC_CFGR_RES_Msk | ADC_CFGR_ALIGN_Msk | ADC_CFGR_EXTEN_Msk
                               | ADC_CFGR_DISCEN_Msk | ADC_CFGR_AUTDLY_Msk | ADC_CFGR_DMAEN
                               | ADC_CFGR_DMACFG | ADC_CFGR_AWD1CH_Msk))
               | ADC_CFGR_CONT                                   /* continuous */
               | ADC_CFGR_OVRMOD                                 /* DR always holds the newest result */
               | ((uint32_t)ADC_CHARGE_VOLT << ADC_CFGR_AWD1CH_Pos)
               | ADC_CFGR_AWD1SGL                                /* AWD1 on that one channel */
               | ADC_CFGR_AWD1EN;                                /* regular group */

    /* 16x oversampling, shift 0: watchdog sees the 12-bit average in DR[15:4] */
    ADC2->CFGR2 = (ADC2->CFGR2 & ~(ADC_CFGR2_OVSR_Msk | ADC_CFGR2_OVSS_Msk
                                 | ADC_CFGR2_TROVS_Msk | ADC_CFGR2_ROVSM_Msk))
                | (ADC2_OVSR_VAL << ADC_CFGR2_OVSR_Pos)
                | ADC_CFGR2_ROVSE;
    
    /* Set the Sequence Register */
    ADC2->SQR1  = (0U << ADC_SQR1_L_Pos)                         /* 1 conversion */
                | ((uint32_t)ADC_CHARGE_VOLT << ADC_SQR1_SQ1_Pos);
    ADC2->SMPR1 = (ADC2->SMPR1 & ~ADC_SMPR1_SMP3_Msk) | (ADC2_SMP << ADC_SMPR1_SMP3_Pos);

    ADC2->TR1 = (0xFFFU << ADC_TR1_HT1_Pos)
              | (ADC2_AWD_FILT << ADC_TR1_AWDFILT_Pos)
              | (0U << ADC_TR1_LT1_Pos);
    NVIC_SetPriority(ADC2_IRQn, LV_CHARGE_PRIO);
    NVIC_EnableIRQ(ADC2_IRQn);
    ADC2->IER = 0;                                               /* nothing until adc2_awd_arm() */
    ADC2->ISR = ADC_ISR_ADRDY;
    ADC2->CR |= ADC_CR_ADEN;
    while (!(ADC2->ISR & ADC_ISR_ADRDY));
    ADC2->ISR = ADC_ISR_ADRDY;
}

/**********************************************************************************
 * adc_init
 *
 * Function:
 *      
 **********************************************************************************/
void adc_init() {
    /* Enable CLK's for GPIOA + C, GPDMA1, ADC1 */
    RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN | RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOCEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPDMA1EN;
    (void)RCC->AHB1ENR;
    /* Set all GPIO to analog */
    GPIOA->MODER |= GPIO_MODER_MODE0_Msk     /* PA0 CH5_CUR */
                  | GPIO_MODER_MODE1_Msk     /* PA1 CH6_CUR */
                  | GPIO_MODER_MODE2_Msk     /* PA2 CH7_CUR */
                  | GPIO_MODER_MODE3_Msk     /* PA3 CH8_CUR */
                  | GPIO_MODER_MODE4_Msk     /* PA4 BUS_CUR */
                  | GPIO_MODER_MODE5_Msk     /* PA5 BUS_VOLT */
                  | GPIO_MODER_MODE6_Msk;    /* PA6 CHARGE_VOLT (converted by ADC2) */
    GPIOC->MODER |= GPIO_MODER_MODE0_Msk     /* PC0 CH1_CUR */
                  | GPIO_MODER_MODE1_Msk     /* PC1 CH2_CUR */
                  | GPIO_MODER_MODE2_Msk     /* PC2 CH3_CUR */
                  | GPIO_MODER_MODE3_Msk;    /* PC3 CH4_CUR */

    /* NO-pull up/pull down*/
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD0_Msk | GPIO_PUPDR_PUPD1_Msk | GPIO_PUPDR_PUPD2_Msk
                    | GPIO_PUPDR_PUPD3_Msk | GPIO_PUPDR_PUPD4_Msk | GPIO_PUPDR_PUPD5_Msk
                    | GPIO_PUPDR_PUPD6_Msk);
    GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPD0_Msk | GPIO_PUPDR_PUPD1_Msk | GPIO_PUPDR_PUPD2_Msk
                    | GPIO_PUPDR_PUPD3_Msk);

    /* Turn on the CLK for ADC1 (Common with ADC2)
     * Take the SYS clk and divide by 4 = 250M/4 = 62.5
     * Fadc_hclk ≥ FADC / 4 if the resolution of all channels are 12-bit or 10-bit */
    ADC12_COMMON->CCR = (ADC12_COMMON->CCR & ~(ADC_CCR_CKMODE_Msk | ADC_CCR_PRESC_Msk))
                      | (3U << ADC_CCR_CKMODE_Pos) | (ADC_CCR_VREFEN_Msk); /* DC_CCR_VREFEN_Msk) maybe remove? */

    /* Both ADCs: wake from deep power-down, turn on the internal regulator,
     * wait 20us for it to settle */
    ADC1->CR &= ~ADC_CR_DEEPPWD;
    ADC2->CR &= ~ADC_CR_DEEPPWD;
    ADC1->CR |= ADC_CR_ADVREGEN_Msk;
    ADC2->CR |= ADC_CR_ADVREGEN_Msk;
    delay_us(ADC_VREG_DELAY_US);


    /* Calibrate the ADC for single ended inputs (0)
     * Must disable ADC
     * Reset by setting ADCAL to (1)
     * Wait until ADCAL turns to (0) ready!
     * Note: possible to store ADC_CALFACT */
    ADC1->DIFSEL = 0;
    ADC2->DIFSEL = 0;
    ADC1->CR &= ~ADC_CR_ADCALDIF;
    ADC2->CR &= ~ADC_CR_ADCALDIF;
    ADC1->CR |= ADC_CR_ADCAL_Msk;
    ADC2->CR |= ADC_CR_ADCAL_Msk;
    while ((ADC1->CR | ADC2->CR) & ADC_CR_ADCAL);   /* both calibrate in parallel, wait for both */

    /* Stupid thing, page 1307 of RM0481: on ADC1, OP0 connects PA0 to ADC1_INP0 (CH5_CUR).
     * ADC1 only: on ADC2 the same bit switches in the internal VddCore channel instead */
    ADC1->OR |= ADC_OR_OP0;

    /************************ ADC1 Config ************************************/
    
    /* TODO Overmod could break alignment of channel order turn of or on?*/
    /* RES = 12-bit (0)
     * Right alignment (0_)
     * Hardware trigger disabled (0) (maybe change later)
     * Discontinuous disabled (0)
     * Auto Delayed conversion mode disabled (0)
     * OVRMOD want fresh data when overflow (1)*/
    ADC1->CFGR = (ADC1->CFGR & ~(ADC_CFGR_RES_Msk | ADC_CFGR_ALIGN_Msk | ADC_CFGR_EXTEN_Msk
                            | ADC_CFGR_DISCEN_Msk | ADC_CFGR_AUTDLY_Msk))
                            | ADC_CFGR_CONT      /* continuous (1) */
                            | ADC_CFGR_DMAEN     /* DMA on (1) */
                            | ADC_CFGR_DMACFG    /* circular (1) */
                            | ADC_CFGR_OVRMOD;   /* overwrite (1) */
    /* Oversampling: regular group, 16x, >>4 -> 12-bit average per channel.
     * Must be written while ADEN=0 (ADC is enabled further down).
     * Cost: each channel result takes ADC_OVS ratio x conversion time */
    ADC1->CFGR2 = (ADC1->CFGR2 & ~(ADC_CFGR2_OVSR_Msk | ADC_CFGR2_OVSS_Msk
                                 | ADC_CFGR2_TROVS_Msk | ADC_CFGR2_ROVSM_Msk))
                | (ADC_OVSR_VAL << ADC_CFGR2_OVSR_Pos)
                | (ADC_OVSS_VAL << ADC_CFGR2_OVSS_Pos)
                | ADC_CFGR2_ROVSE;

    /* SQR1 holds L + ranks 1-4, SQR2 ranks 5-9, SQR3 ranks 10-14. 6 bits per rank */
    uint32_t sqr1 = (ADC_NUM_CH - 1U) << ADC_SQR1_L_Pos;
    uint32_t sqr2 = 0, sqr3 = 0;
    for (uint32_t i = 0; i < ADC_NUM_CH; i++){
        if (i < 4U)      sqr1 |= (uint32_t)adc_rank[i] << (6U * (i + 1U));
        else if (i < 9U) sqr2 |= (uint32_t)adc_rank[i] << (6U * (i - 4U));
        else             sqr3 |= (uint32_t)adc_rank[i] << (6U * (i - 9U));
    }

    
    ADC1->SQR1 = sqr1;
    ADC1->SQR2 = sqr2;
    ADC1->SQR3 = sqr3;
    // ADC clk = 62.5 MHz (HCLK/4) -> 16 ns/cycle, 12-bit adds 12.5 cycles, 10 channels
    // Per ch rate = 1 / scan time. Total rate = 10 * per ch rate.
    // Data rate = total rate * 2 bytes (12-bit result in a 16-bit DMA halfword)
    //
    //  SMP | sample | total |  per conv | 10-ch scan |  per ch Hz |  total S/s | DMA data
    //  000 |    2.5 |    15 |  0.240 us |   2.400 us | 416,667 Hz | 4,166,667  | 8.33 MB/s
    //  001 |    6.5 |    19 |  0.304 us |   3.040 us | 328,947 Hz | 3,289,474  | 6.58 MB/s
    //  010 |   12.5 |    25 |  0.400 us |   4.000 us | 250,000 Hz | 2,500,000  | 5.00 MB/s
    //  011 |   24.5 |    37 |  0.592 us |   5.920 us | 168,919 Hz | 1,689,189  | 3.38 MB/s
    //  100 |   47.5 |    60 |  0.960 us |   9.600 us | 104,167 Hz | 1,041,667  | 2.08 MB/s
    //  101 |   92.5 |   105 |  1.680 us |  16.800 us |  59,524 Hz |   595,238  | 1.19 MB/s
    //  110 |  247.5 |   260 |  4.160 us |  41.600 us |  24,038 Hz |   240,385  | 0.48 MB/s
    //  111 |  640.5 |   653 | 10.448 us | 104.480 us |   9,571 Hz |    95,712  | 0.19 MB/s

    /* ADC_SMP (adc_dma.h) picks the table row above: 0b100 = 47.5 cycles (60 total = 0.96 us).
     * With 16x oversampling: 15.4 us per channel, 154 us per 10-ch scan -> ~6.5 kHz per channel.
     * (Table rows above are the raw, non-oversampled numbers.)
     * Same sample time on every channel. 3 bits per channel: SMPR1 = ch 0-9, SMPR2 = ch 10-19 */
    uint32_t smp = 0;
    for (uint32_t i = 0; i < 10U; i++){
        smp |= ADC_SMP << (3U * i);
    }
    ADC1->SMPR1 = smp;
    ADC1->SMPR2 = smp;

    /* Enable the ADC1 */
    ADC1->ISR = ADC_ISR_ADRDY;              /* clear by writing 1 */
    ADC1->CR |= ADC_CR_ADEN;
    while (!(ADC1->ISR & ADC_ISR_ADRDY));   /* wait until ready */
    ADC1->ISR = ADC_ISR_ADRDY;

    /* GPDMA1 CH0: ADC1->DR (16-bit, fixed) -> adc_buf (16-bit, incrementing), paced by ADC1 request.
     * Block = one full scan (ADC_NUM_CH halfwords), self-linked node restarts it forever */
    DMA_Channel_TypeDef *ch = GPDMA1_Channel0;
    ch->CCR = DMA_CCR_RESET;                /* known state, channel must be off */
    while (ch->CCR & DMA_CCR_EN);

    const uint32_t blk_bytes = ADC_NUM_CH * sizeof(uint16_t);

    adc_ll_node[0] = blk_bytes;                                     /* CBR1 */
    adc_ll_node[1] = (uint32_t)adc_buf;                             /* CDAR */
    adc_ll_node[2] = DMA_CLLR_UB1 | DMA_CLLR_UDA | DMA_CLLR_ULL     /* CLLR: points to itself */
                   | ((uint32_t)adc_ll_node & DMA_CLLR_LA);

    ch->CLBAR = (uint32_t)adc_ll_node & DMA_CLBAR_LBA;              /* upper 16 bits of node address */
    ch->CTR1  = (1U << DMA_CTR1_SDW_LOG2_Pos)                       /* src halfword */
              | (1U << DMA_CTR1_DDW_LOG2_Pos)                       /* dst halfword */
              | DMA_CTR1_DINC;                                      /* dst increments, src fixed */
    ch->CTR2  = (ADC1_GPDMA_REQ << DMA_CTR2_REQSEL_Pos);       /* HW request from ADC1 */
    ch->CBR1  = blk_bytes;
    ch->CSAR  = (uint32_t)&ADC1->DR;
    ch->CDAR  = (uint32_t)adc_buf;
    ch->CLLR  = adc_ll_node[2];
    ch->CCR   = DMA_CCR_EN;                                         /* arm DMA before ADC makes requests */

    adc2_config();
    /* Neither ADC is converting yet: adc1_start() / adc2_start() */
}

void adc1_start(void) {
    ADC1->CR |= ADC_CR_ADSTART;             /* continuous scan */
}

void adc2_start(void) {
    ADC2->CR |= ADC_CR_ADSTART;             /* continuous, one channel */
}

/**********************************************************************************
 * adc2_read
 *
 * Function:
 *      Latest LV charge voltage result as a 12-bit average (0-4095).
 *      DR holds the 16x oversampled sum (no hardware shift), so >> 4 here.
 **********************************************************************************/
uint16_t adc2_read(void) {
    return (uint16_t)(ADC2->DR >> 4);
}

/**********************************************************************************
 * adc2_awd_set
 *
 * Function:
 *      Watchdog window in 12-bit counts. AWD1 flags when a result is < low or > high
 *      for ADC2_AWD_FILT + 1 results in a row. Thresholds can change while converting.
 **********************************************************************************/
void adc2_awd_set(uint16_t low, uint16_t high) {
    ADC2->TR1 = (ADC2->TR1 & ~(ADC_TR1_HT1_Msk | ADC_TR1_LT1_Msk))
              | (((uint32_t)high & 0xFFFU) << ADC_TR1_HT1_Pos)
              | (((uint32_t)low  & 0xFFFU) << ADC_TR1_LT1_Pos);
}

/* AWD1 is the only interrupt enabled on ADC2 */
void adc2_awd_arm(void) {
    ADC2->ISR  = ADC_ISR_AWD1;
    ADC2->IER |= ADC_IER_AWD1IE;
}

void adc2_awd_disarm(void) {
    ADC2->IER &= ~ADC_IER_AWD1IE;
    ADC2->ISR  = ADC_ISR_AWD1;
}

void ADC2_IRQHandler(void) {
    if (PDB_state.lv_charge) {
        //turn off channel with Charge
        PDB_state.lv_charge = 0;
        return;
    }
    BaseType_t woken = pdFALSE;
    adc2_awd_disarm();
    xTaskNotifyFromISR(lv_charge_task_handle, adc2_read(), eSetValueWithOverwrite, &woken);
    portYIELD_FROM_ISR(woken);
}

/**********************************************************************************
 * adc_read
 *
 * Function:
 *      Latest 12-bit sample for a channel (0-4095), or 0 if channel isn't in the scan.
 **********************************************************************************/
uint16_t adc_read(adc_channel_t channel) {
    for (uint32_t i = 0; i < ADC_NUM_CH; i++){
        if (adc_rank[i] == (uint8_t)channel) return adc_buf[i];
    }
    return 0;
}
