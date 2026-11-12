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

#define ADC_OVSR_VAL 3U         /* 16x */
#define ADC_OVSS_VAL 4U         /* right shift 4 */
#define ADC_NUM_CH 10U          /* channels in ADC1's scan (charge voltage is on ADC2) */

/* ADC2: LV charge voltage only (PA6 = ADC2_INP3), continuous, no DMA.
 * 16x oversampling with NO shift: DR holds the 16-bit sum, and the analog watchdog compares
 * DR[15:4] = the 12-bit average, so watchdog thresholds are normal 0-4095 counts.
 * 640.5-cycle sample (suits a high-impedance divider): 653 x 16 cycles = 167 us per result */
#define ADC2_SMP      0b111     /* 640.5 cycles */
#define ADC2_OVSR_VAL 3U        /* 16x */
#define ADC2_AWD_FILT 7U        /* trip after 8 consecutive results outside the window (~1.3 ms) */

typedef enum{
    ADC_CH5_CUR     = 0,    /* PA0 */
    ADC_CH6_CUR     = 1,    /* PA1 */
    ADC_CHARGE_VOLT = 3,    /* PA6, also ADC2_INP3: converted by ADC2, not in ADC1's scan */
    ADC_CH1_CUR     = 10,   /* PC0 */
    ADC_CH2_CUR     = 11,   /* PC1 */
    ADC_CH3_CUR     = 12,   /* PC2 */
    ADC_CH4_CUR     = 13,   /* PC3 */
    ADC_CH7_CUR     = 14,   /* PA2 */
    ADC_CH8_CUR     = 15,   /* PA3 */
    ADC_BUS_CUR     = 18,   /* PA4 */
    ADC_BUS_VOLT    = 19    /* PA5 */
} adc_channel_t;

void adc_init();                /* powers up, calibrates and configures ADC1 + ADC2, doesn't start them */
void adc1_start(void);
void adc2_start(void);
uint16_t adc_read(adc_channel_t channel);

/* ADC2 (LV charge voltage) */
uint16_t adc2_read(void);
void     adc2_awd_set(uint16_t low, uint16_t high);
void     adc2_awd_arm(void);
void     adc2_awd_disarm(void);

#endif
