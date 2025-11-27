/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "spif.h"
#include "backend.h"
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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
// Объявление C-интерфейса для сигнала VSYNC из TouchGFX
void TGFX_SignalVSync(void);

void AppTimer1ms();

void AppInit();
void AppProcess();

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BRP_485_EN_Pin GPIO_PIN_13
#define BRP_485_EN_GPIO_Port GPIOC
#define KEY_2_Pin GPIO_PIN_0
#define KEY_2_GPIO_Port GPIOA
#define KEY_1_Pin GPIO_PIN_1
#define KEY_1_GPIO_Port GPIOA
#define FLASH_CS_Pin GPIO_PIN_4
#define FLASH_CS_GPIO_Port GPIOA
#define SOUND_Pin GPIO_PIN_5
#define SOUND_GPIO_Port GPIOC
#define ST2_MK_Pin GPIO_PIN_14
#define ST2_MK_GPIO_Port GPIOB
#define ST1_MK_Pin GPIO_PIN_15
#define ST1_MK_GPIO_Port GPIOB
#define LED_Pin GPIO_PIN_8
#define LED_GPIO_Port GPIOA
#define ESP32_EN_Pin GPIO_PIN_15
#define ESP32_EN_GPIO_Port GPIOA
#define ESP32_BOOT_Pin GPIO_PIN_12
#define ESP32_BOOT_GPIO_Port GPIOC
#define DISP_RES_Pin GPIO_PIN_2
#define DISP_RES_GPIO_Port GPIOD
#define DISP_CS_Pin GPIO_PIN_4
#define DISP_CS_GPIO_Port GPIOB
#define DISP_D_C_Pin GPIO_PIN_8
#define DISP_D_C_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define GFX_RATIO_MS 5
#define LED_TOGGLE HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);

extern uint8_t setup_change;

#define NUM_ADC_CHANNEL 5
#define FILTERSIZE 128

extern uint16_t ADC_VAL[NUM_ADC_CHANNEL];
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
