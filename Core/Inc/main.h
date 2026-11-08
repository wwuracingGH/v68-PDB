/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h5xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CHAR_CTRL_Pin GPIO_PIN_13
#define CHAR_CTRL_GPIO_Port GPIOC
#define ADC_CH1_CUR_Pin GPIO_PIN_0
#define ADC_CH1_CUR_GPIO_Port GPIOC
#define ADC_CH2_CUR_Pin GPIO_PIN_1
#define ADC_CH2_CUR_GPIO_Port GPIOC
#define ADC_CH3_CUR_Pin GPIO_PIN_2
#define ADC_CH3_CUR_GPIO_Port GPIOC
#define ADC_CH4_CUR_Pin GPIO_PIN_3
#define ADC_CH4_CUR_GPIO_Port GPIOC
#define ADC_CH5_CUR_Pin GPIO_PIN_0
#define ADC_CH5_CUR_GPIO_Port GPIOA
#define ADC_CH6_CUR_Pin GPIO_PIN_1
#define ADC_CH6_CUR_GPIO_Port GPIOA
#define ADC_CH7_CUR_Pin GPIO_PIN_2
#define ADC_CH7_CUR_GPIO_Port GPIOA
#define ADC_CH8_CUR_Pin GPIO_PIN_3
#define ADC_CH8_CUR_GPIO_Port GPIOA
#define ADC_BUS_CUR_Pin GPIO_PIN_4
#define ADC_BUS_CUR_GPIO_Port GPIOA
#define ADC_BUS_VOLT_Pin GPIO_PIN_5
#define ADC_BUS_VOLT_GPIO_Port GPIOA
#define ADC_CHARGE_VOLT_Pin GPIO_PIN_6
#define ADC_CHARGE_VOLT_GPIO_Port GPIOA
#define CH2_CTRL_Pin GPIO_PIN_7
#define CH2_CTRL_GPIO_Port GPIOA
#define CH3_CTRL_Pin GPIO_PIN_4
#define CH3_CTRL_GPIO_Port GPIOC
#define CH4_CTRL_Pin GPIO_PIN_5
#define CH4_CTRL_GPIO_Port GPIOC
#define CH5_CTRL_Pin GPIO_PIN_0
#define CH5_CTRL_GPIO_Port GPIOB
#define CH6_CTRL_Pin GPIO_PIN_1
#define CH6_CTRL_GPIO_Port GPIOB
#define CH7_CTRL_Pin GPIO_PIN_2
#define CH7_CTRL_GPIO_Port GPIOB
#define CH8_CTRL_Pin GPIO_PIN_10
#define CH8_CTRL_GPIO_Port GPIOB
#define L_ESTOP_VT_Pin GPIO_PIN_14
#define L_ESTOP_VT_GPIO_Port GPIOB
#define I_SWITCH_VT_Pin GPIO_PIN_15
#define I_SWITCH_VT_GPIO_Port GPIOB
#define CP_ESTOP_VT_Pin GPIO_PIN_6
#define CP_ESTOP_VT_GPIO_Port GPIOC
#define BSPD_VT_Pin GPIO_PIN_7
#define BSPD_VT_GPIO_Port GPIOC
#define BOTS_VT_Pin GPIO_PIN_8
#define BOTS_VT_GPIO_Port GPIOC
#define LAMP_CHECK_Pin GPIO_PIN_9
#define LAMP_CHECK_GPIO_Port GPIOC
#define BLUE_PWM_Pin GPIO_PIN_8
#define BLUE_PWM_GPIO_Port GPIOA
#define GREEN_PWM_Pin GPIO_PIN_9
#define GREEN_PWM_GPIO_Port GPIOA
#define RED_PWM_Pin GPIO_PIN_10
#define RED_PWM_GPIO_Port GPIOA
#define LVBB_VT_Pin GPIO_PIN_11
#define LVBB_VT_GPIO_Port GPIOC
#define FANS_PWM_Pin GPIO_PIN_3
#define FANS_PWM_GPIO_Port GPIOB
#define PUMP_PWM_Pin GPIO_PIN_4
#define PUMP_PWM_GPIO_Port GPIOB
#define CH1_CTRL_Pin GPIO_PIN_12
#define CH1_CTRL_GPIO_Port GPIOB
#define R_ESTOP_VT_Pin GPIO_PIN_8
#define R_ESTOP_VT_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
