/**********************************************************************************
 * adc_dma.h
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

#ifndef ADC_DMA_H
#define ADC_DMA_H

#define ADC_SMP 0b100           /* 47.5 cycles, see timing table in adc_dma.c */

/* Hardware oversampling: ADC does ADC_OVS_RATIO back-to-back conversions per channel and
 * hands back one result. OVSR encoding: 0=2x 1=4x 2=8x 3=16x ... Shift keeps result at 12-bit:
 * 16x sum is 16 bits, >>4 = average, so adc_read() is still 0-4095 */
#define ADC_OVSR_VAL 3U         /* 16x */
#define ADC_OVSS_VAL 4U         /* right shift 4 */
#define ADC_NUM_CH 11U

/* ADC1 input channel numbers (ADC1_INPx), from the PDB pin concept .ioc */
typedef enum{
    ADC_CH5_CUR     = 0,    /* PA0 */
    ADC_CH6_CUR     = 1,    /* PA1 */
    ADC_CHARGE_VOLT = 3,    /* PA6 */
    ADC_CH1_CUR     = 10,   /* PC0 */
    ADC_CH2_CUR     = 11,   /* PC1 */
    ADC_CH3_CUR     = 12,   /* PC2 */
    ADC_CH4_CUR     = 13,   /* PC3 */
    ADC_CH7_CUR     = 14,   /* PA2 */
    ADC_CH8_CUR     = 15,   /* PA3 */
    ADC_BUS_CUR     = 18,   /* PA4 */
    ADC_BUS_VOLT    = 19    /* PA5 */
} adc_channel_t;

void adc_init();
uint16_t adc_read(adc_channel_t channel);

#endif
