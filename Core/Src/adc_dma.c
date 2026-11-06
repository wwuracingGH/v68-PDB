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
#include "FreeRTOS.h"

#define ADC_VREG_DELAY (SYS_CLK_HZ / 1000000UL * 20UL)  /* Must delay after waking from deep power-down 20us */

/* Simple Delay Function (Notice volatile) */
static void delay_cycles(uint32_t n){
    for (volatile uint32_t i = 0; i < n; i++);
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
    GPIOA->MODER |= GPIO_MODER_MODE0_Msk     /* PA0 CH5_CUR     */
                  | GPIO_MODER_MODE1_Msk     /* PA1 CH6_CUR     */
                  | GPIO_MODER_MODE2_Msk     /* PA2 CH7_CUR     */
                  | GPIO_MODER_MODE3_Msk     /* PA3 CH8_CUR     */
                  | GPIO_MODER_MODE4_Msk     /* PA4 BUS_CUR     */
                  | GPIO_MODER_MODE5_Msk     /* PA5 BUS_VOLT    */
                  | GPIO_MODER_MODE6_Msk;    /* PA6 CHARGE_VOLT */
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

    /* Wake from deep power-down
     * Turn on Reference voltage
     * Wait 20us to wake */
    ADC1->CR &= ~ADC_CR_DEEPPWD;
    ADC1->CR |= ADC_CR_ADVREGEN_Msk;
    delay_cycles(ADC_VREG_DELAY);

    /* Calibrate the ADC for single ended inputs (0)
     * Must disable ADC
     * Reset by setting ADCAL to (1)
     * Wait until ADCAL turns to (0) ready!
     * Note: possible to store ADC_CALFACT */
    ADC1->DIFSEL = 0;
    ADC1->CR &= ~ADC_CR_ADCALDIF;
    ADC1->CR |= ADC_CR_ADCAL_Msk;
    while (ADC1->CR & ADC_CR_ADCAL);

    /* Stupid thing, page 1307 of RM0481 */
    ADC1->OR |= ADC_OR_OP0;


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


    /* There are 4 SQR Sequence regular registers each has 4 channels it can control */
    ADC1->SQR1 = (((ADC_NUM_CH - 1)) << ADC_SQR1_L_Pos)                /* 11 Channels so 11U-1 value counts from 0*/
               | (10U << ADC_SQR1_SQ1_Pos)              /* rank 1:  PC0 CH1_CUR */
               | (11U << ADC_SQR1_SQ2_Pos)              /* rank 2:  PC1 CH2_CUR */
               | (12U << ADC_SQR1_SQ3_Pos)              /* rank 3:  PC2 CH3_CUR */
               | (13U << ADC_SQR1_SQ4_Pos);             /* rank 4:  PC3 CH4_CUR */
    ADC1->SQR2 = ( 0U << ADC_SQR2_SQ5_Pos)              /* rank 5:  PA0 CH5_CUR */
               | ( 1U << ADC_SQR2_SQ6_Pos)              /* rank 6:  PA1 CH6_CUR */
               | (14U << ADC_SQR2_SQ7_Pos)              /* rank 7:  PA2 CH7_CUR */
               | (15U << ADC_SQR2_SQ8_Pos)              /* rank 8:  PA3 CH8_CUR */
               | (18U << ADC_SQR2_SQ9_Pos);             /* rank 9:  PA4 BUS_CUR */
    ADC1->SQR3 = (19U << ADC_SQR3_SQ10_Pos)             /* rank 10: PA5 BUS_VOLT    */
               | ( 3U << ADC_SQR3_SQ11_Pos);            /* rank 11: PA6 CHARGE_VOLT */
    // ADC clk = 62.5 MHz (HCLK/4) -> 16 ns/cycle, 12-bit adds 12.5 cycles, 11 channels
    // Per ch rate = 1 / scan time. Total rate = 11 * per ch rate.
    // Data rate = total rate * 2 bytes (12-bit result in a 16-bit DMA halfword)
    //
    //  SMP | sample | total |  per conv | 11-ch scan |  per ch Hz |  total S/s | DMA data
    //  000 |    2.5 |    15 |  0.240 us |   2.640 us | 378,788 Hz | 4,166,667  | 8.33 MB/s
    //  001 |    6.5 |    19 |  0.304 us |   3.344 us | 299,043 Hz | 3,289,474  | 6.58 MB/s
    //  010 |   12.5 |    25 |  0.400 us |   4.400 us | 227,273 Hz | 2,500,000  | 5.00 MB/s
    //  011 |   24.5 |    37 |  0.592 us |   6.512 us | 153,563 Hz | 1,689,189  | 3.38 MB/s
    //  100 |   47.5 |    60 |  0.960 us |  10.560 us |  94,697 Hz | 1,041,667  | 2.08 MB/s
    //  101 |   92.5 |   105 |  1.680 us |  18.480 us |  54,113 Hz |   595,238  | 1.19 MB/s
    //  110 |  247.5 |   260 |  4.160 us |  45.760 us |  21,853 Hz |   240,385  | 0.48 MB/s
    //  111 |  640.5 |   653 | 10.448 us | 114.928 us |   8,701 Hz |    95,712  | 0.19 MB/s

    /* 62.5M = 16ns*/
    /* 160ns*(247.5 + 12.5) = 41.6us = 24,038 Hz*/
    /* 247.5 cycles on every channel. 3 bits per channel: SMPR1 = ch 0-9, SMPR2 = ch 10-19 */
    uint32_t smp = 0;
    for (uint32_t i = 0; i < 10U; i++){
        smp |= ADC_SMP << (3U * i);
    }
    ADC1->SMPR1 = smp;
    ADC1->SMPR2 = smp;

    /* Enable the ADC1 */
    ADC1->ISR = ADC_ISR_ADRDY;              /* clear by writing 1 */
    ADC1->CR |= (ADC_CR_ADEN << ADC_CR_ADEN_Msk);
    while (!(ADC1->ISR & ADC_ISR_ADRDY));   /* wait until ready */
    ADC1->ISR = ADC_ISR_ADRDY;

    DMA_Channel_TypeDef *ch = GPDMA1_Channel0;
    /* TODO*/



}

