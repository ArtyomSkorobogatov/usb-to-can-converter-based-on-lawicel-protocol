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
#include "stm32f0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <string.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef struct Statistics_ {
    uint32_t canBusOutgoingQueueOverflow;
    uint32_t canBusIncomingQueueOverflow;
    uint32_t usbCdcOutgoingQueueOverflow;
    uint32_t usbCdcIncomingQueueOverflow;
} Statistics_t;

#define INCREASE_STATISTIC_CNT(counter)                                                                                \
    do {                                                                                                               \
        if ((counter) < UINT32_MAX) {                                                                                  \
            (counter)++;                                                                                               \
        }                                                                                                              \
    } while (0)
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

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_RED_Pin        GPIO_PIN_0
#define LED_RED_GPIO_Port  GPIOA
#define LED_BLUE_Pin       GPIO_PIN_1
#define LED_BLUE_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */
#define CAN_BUS_RX_Pin         GPIO_PIN_8
#define CAN_BUS_RX_GPIO_Port   GPIOB
#define CAN_BUS_TX_Pin         GPIO_PIN_9
#define CAN_BUS_TX_GPIO_Port   GPIOB
#define CAN_BUS_CLK_ENA()      __HAL_RCC_CAN1_CLK_ENABLE()
#define CAN_BUS_PORT_CLK_ENA() __HAL_RCC_GPIOB_CLK_ENABLE()
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
