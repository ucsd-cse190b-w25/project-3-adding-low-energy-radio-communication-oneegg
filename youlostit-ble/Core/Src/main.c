/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
//#include "ble_commands.h"
#include "timer.h"
#include "i2c.h"
#include "lsm6dsl.h"
#include "ble.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <stm32l475xx.h>

int dataAvailable = 0;

SPI_HandleTypeDef hspi3;

// Timer interrupt variable
volatile int timer_triggered = 0;

// LPTIM1 interrupt handler; checks & resets UIF
void LPTIM1_IRQHandler()
{
	if (LPTIM1->ISR & LPTIM_ISR_ARRM) {
		LPTIM1->ICR |= LPTIM_ICR_ARRMCF;
		timer_triggered = 1;
	}
}

// TIM2 interrupt handler; checks & resets UIF
void TIM2_IRQHandler()
{
	if (TIM2->SR & TIM_SR_UIF) {
		TIM2->SR &= ~TIM_SR_UIF;
		timer_triggered = 1;
	}
}

// Redefinition of libc _write() so we can use printf() for debugging
int _write(int file, char *ptr, int len)
{
    int i = 0;
    for (i = 0; i < len; i++) {
        ITM_SendChar(*ptr++);
    }
    return len;
}

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI3_Init(void);

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  // Set low-power run mode
  /*
   * Note: don't uncomment this unless we can manage to run sysclk at <= 2MHz,
   * 	   because one of the following two things will happen:
   * 	   1) it just does nothing
   * 	   2) it makes us enter Stop 1 instead of Stop 2, which is worse
   */
  // PWR->CR1 |= PWR_CR1_LPR;

  // Undervolt 1.2V -> 1.0V (Range 1 -> Range 2)
  PWR->CR1 ^= PWR_CR1_VOS;

  // Initialize our peripherals
  LPtimer_init(LPTIM1);
  i2c_init();
  lsm6dsl_init();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI3_Init();

  /* RESET BLE MODULE */
  HAL_GPIO_WritePin(BLE_RESET_GPIO_Port,BLE_RESET_Pin,GPIO_PIN_RESET);
  HAL_Delay(10);
  HAL_GPIO_WritePin(BLE_RESET_GPIO_Port,BLE_RESET_Pin,GPIO_PIN_SET);

  ble_init();
  HAL_Delay(10);


  // Make sure unused GPIOs are disabled
  __HAL_RCC_GPIOF_CLK_DISABLE();
  __HAL_RCC_GPIOG_CLK_DISABLE();
  __HAL_RCC_GPIOH_CLK_DISABLE();

  // These GPIOs were initialized in ble_init(), but don't seem to be used for anything, so...
  __HAL_RCC_GPIOA_CLK_DISABLE();
  __HAL_RCC_GPIOC_CLK_DISABLE();

  // Initialize device in nondiscoverable state
  uint8_t nonDiscoverable = 1;
  setDiscoverability(0);

  /*
  // Disable EVERY NVIC interrupt!!!!!!
  // This is just to make sure there are no other interrupts being turned on except for th eones that we made
  // lol
  for (int i = -14; i <= 81; i++) {
	  NVIC_DisableIRQ(i);
  }

  // Manually re-enable LPTIM1, EXTI9_5 (BLE) interrupts
  NVIC_SetPriority(LPTIM1_IRQn, 1);
  NVIC_EnableIRQ(LPTIM1_IRQn);
  NVIC_SetPriority(EXTI9_5_IRQn, 1);
  NVIC_EnableIRQ(EXTI9_5_IRQn);
  */

  // Important timer/accel data variables
  int32_t timer_cycles = 0;
  int16_t prev_x, prev_y, prev_z;

  while (1)
  {
	  if(!nonDiscoverable && HAL_GPIO_ReadPin(BLE_INT_GPIO_Port,BLE_INT_Pin)) {
	    catchBLE();
	  } else if (timer_triggered) {

		// Read data from accelerometer
	    int16_t x, y, z;
	    lsm6dsl_read_xyz(&x, &y, &z);

	    // Calculate raw difference in magnitude for each component of acceleration
		int16_t diff_x = abs(x - prev_x);
		int16_t diff_y = abs(y - prev_y);
		int16_t diff_z = abs(z - prev_z);

		prev_x = x;
		prev_y = y;
		prev_z = z;

		// Determine if device is moving based on individual acceleration components
		if (diff_x > 1500 || diff_y > 1500 || diff_z > 1500) {
			// Moving -> make nondiscoverable and disconnect if it's connected
			if (nonDiscoverable == 0) {
				disconnectBLE();
				setDiscoverability(0);
				nonDiscoverable = 1;

				// Put BLE radio in standby mode to save power
				standbyBle();
			}

			timer_cycles = 0;
		} else {
			// Device is not moving -> increment timer cycles
			timer_cycles++;
		}

		// Check if device has been not moving for more than one minute (0.2Hz -> 12 interrupts/minute)
		if (timer_cycles >= 12) {
			// Not moving for a significant amount of time -> enter "lost mode," beacon Bluetooth signal
			if (nonDiscoverable == 1) {
				setDiscoverability(1);
				nonDiscoverable = 0;
			}

			// Send update message every 10 seconds to the NORDIC UART service
			if (timer_cycles % 2 == 0) {
				// Load fancy format string into buffer (don't include a newline)
				uint8_t message[20];
				int time_since_lost = (timer_cycles - 12) * 5;
				int cx = snprintf(message, 20, "1egg lost for %ds", time_since_lost);

				// cx should be leq 20 no matter what, but just in case...
				if (cx <= 20) {
					// Actually send the message using BLE
					updateCharValue(NORDIC_UART_SERVICE_HANDLE, READ_CHAR_HANDLE, 0, cx, message);
				}
			}
		}

		timer_triggered = 0;
	  }

	  // Disable SysTick; anything using HAL relies on it so probably has to go after all the BLE stuff
	  SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;

	  // Disable in-use GPIOs since we don't need them in sleep
	  RCC->AHB2ENR &= ~(RCC_AHB2ENR_GPIOBEN);
	  RCC->AHB2ENR &= ~(RCC_AHB2ENR_GPIOCEN);
	  RCC->AHB2ENR &= ~(RCC_AHB2ENR_GPIOEEN);

	  // Enter Stop 2 mode and enter deep sleep until interrupted
	  PWR->CR1 &= ~PWR_CR1_LPMS;
	  PWR->CR1 |= PWR_CR1_LPMS_STOP2;
	  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
	  __WFI();

	  // Restore state prior to entering deep sleep
	  SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;

	  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
	  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
	  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOEEN;
  }
}

/**
  * @brief System Clock Configuration
  * @attention This changes the System clock frequency, make sure you reflect that change in your timer
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  // This lines changes system clock frequency
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_7;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI3_Init(void)
{

  /* USER CODE BEGIN SPI3_Init 0 */

  /* USER CODE END SPI3_Init 0 */

  /* USER CODE BEGIN SPI3_Init 1 */

  /* USER CODE END SPI3_Init 1 */
  /* SPI3 parameter configuration*/
  hspi3.Instance = SPI3;
  hspi3.Init.Mode = SPI_MODE_MASTER;
  hspi3.Init.Direction = SPI_DIRECTION_2LINES;
  hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi3.Init.NSS = SPI_NSS_SOFT;
  hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 7;
  hspi3.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi3.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIO_LED1_GPIO_Port, GPIO_LED1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BLE_CS_GPIO_Port, BLE_CS_Pin, GPIO_PIN_SET);


  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BLE_RESET_GPIO_Port, BLE_RESET_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : BLE_INT_Pin */
  GPIO_InitStruct.Pin = BLE_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BLE_INT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : GPIO_LED1_Pin BLE_RESET_Pin */
  GPIO_InitStruct.Pin = GPIO_LED1_Pin|BLE_RESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : BLE_CS_Pin */
  GPIO_InitStruct.Pin = BLE_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(BLE_CS_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
