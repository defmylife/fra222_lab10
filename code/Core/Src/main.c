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
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <math.h>
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

SPI_HandleTypeDef hspi3;

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim11;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

//Read ADC
uint16_t ADCin = 0;

//Time in microsecond
uint64_t _micro = 0;

//SPI address
uint8_t DACConfig = 0b0011;
//SPI data (12 bits)
uint16_t dataOut = 0;

//Control variables
typedef struct
{
	double 	count;
	float 	frequency;
	float	max_voltage;
	float	min_voltage;
	uint8_t	duty_cycle;
	uint8_t wave_mode;
	uint8_t slope_down;
	uint64_t periodstamp;

} Generator;
Generator ControlVar = {0};

char RxDataBuffer[32] = {0};

typedef enum
{
	Main,
	Change_mode,
	Change_frequency,
	Change_max_voltage,
	Change_min_voltage,
	Change_duty_cycle

} State;
State UIState = Main;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI3_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM11_Init(void);
/* USER CODE BEGIN PFP */

void updateUI(int16_t dataIn);
void updateStatus();
int16_t UARTRecieveIT();
void generatorInit(Generator *var);
void generator();
void MCP4922SetOutput(uint8_t Config, uint16_t DACOutput);
uint64_t micros();

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_SPI3_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_TIM11_Init();
  /* USER CODE BEGIN 2 */

	HAL_TIM_Base_Start(&htim3);
	HAL_TIM_Base_Start_IT(&htim11);
	HAL_ADC_Start_DMA(&hadc1, (uint32_t*) &ADCin, 1);

	HAL_GPIO_WritePin(LOAD_GPIO_Port, LOAD_Pin, GPIO_PIN_RESET);

	generatorInit(&ControlVar);

	updateStatus();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1)
	{
		//Read UART
		HAL_UART_Receive_IT(&huart2,  (uint8_t*)RxDataBuffer, 32);

		int16_t inputchar = UARTRecieveIT();
		if (inputchar!=-1)
		{
			updateUI(inputchar);
		}

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

		static uint64_t timestamp = 0;
		if (micros() - timestamp > 100) // f 10kHz or T 100uS
		{
			timestamp = micros();
			generator();
		}
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */
  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

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
  hspi3.Init.DataSize = SPI_DATASIZE_16BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi3.Init.NSS = SPI_NSS_SOFT;
  hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 100;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 100;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM11 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM11_Init(void)
{

  /* USER CODE BEGIN TIM11_Init 0 */

  /* USER CODE END TIM11_Init 0 */

  /* USER CODE BEGIN TIM11_Init 1 */

  /* USER CODE END TIM11_Init 1 */
  htim11.Instance = TIM11;
  htim11.Init.Prescaler = 100;
  htim11.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim11.Init.Period = 65535;
  htim11.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim11.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim11) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM11_Init 2 */

  /* USER CODE END TIM11_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SPI_SS_GPIO_Port, SPI_SS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SHDN_GPIO_Port, SHDN_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LOAD_GPIO_Port, LOAD_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD2_Pin LOAD_Pin */
  GPIO_InitStruct.Pin = LD2_Pin|LOAD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : SPI_SS_Pin */
  GPIO_InitStruct.Pin = SPI_SS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SPI_SS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SHDN_Pin */
  GPIO_InitStruct.Pin = SHDN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SHDN_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

void updateUI(int16_t dataIn)
{
	char temp[256];

	switch (UIState)
	{
		case Main:
			if (dataIn == 'm')
			{
				if (ControlVar.wave_mode == 0) {		//sawtooth
					sprintf(temp,
							"\r\n"
							"========================\r\n"
							"Change Mode to...\r\n"
							"========================\r\n"
					  		"[n] Sine-wave\r\n"
							"[q] Square-wave\r\n"
							"[x] back\r\n");
				}
				else if (ControlVar.wave_mode == 1) {	//sine-wave
					sprintf(temp,
							"\r\n"
							"========================\r\n"
							"Change Mode to...\r\n"
							"========================\r\n"
					  		"[w] Sawtooth\r\n"
							"[q] Square-wave\r\n"
							"[x] back\r\n");
				}
				else if (ControlVar.wave_mode == 2) {	//square-wave
					sprintf(temp,
							"\r\n"
							"========================\r\n"
							"Change Mode to...\r\n"
							"========================\r\n"
					  		"[w] Sawtooth\r\n"
							"[n] Sine-wave\r\n"
							"[x] back\r\n");
				}
				HAL_UART_Transmit(&huart2, (uint8_t*)temp, strlen(temp), 1000);

				UIState = Change_mode;
			}
			else if (dataIn == 'f')
			{
				sprintf(temp,
						"\r\n"
						"========================\r\n"
						"Change Frequency : %.1f Hz\r\n"
						"========================\r\n"
				  		"[1] Increase +1Hz\r\n"
						"[2] Decrease -1Hz\r\n"
				  		"[3] Increase +0.1Hz\r\n"
						"[4] Decrease -0.1Hz\r\n"
						"[x] back\r\n",
						ControlVar.frequency);
				HAL_UART_Transmit(&huart2, (uint8_t*)temp, strlen(temp), 1000);

				UIState = Change_frequency;
			}
			else if (dataIn == 'a')
			{
				sprintf(temp,
						"\r\n"
						"========================\r\n"
						"Change Max-voltage : %.1f V\r\n"
						"========================\r\n"
				  		"[1] Increase +1V\r\n"
						"[2] Decrease -1V\r\n"
				  		"[3] Increase +0.1V\r\n"
						"[4] Decrease -0.1V\r\n"
						"[x] back\r\n",
						ControlVar.max_voltage);
				HAL_UART_Transmit(&huart2, (uint8_t*)temp, strlen(temp), 1000);

				UIState = Change_max_voltage;
			}
			else if (dataIn == 'i')
			{
				sprintf(temp,
						"\r\n"
						"========================\r\n"
						"Change Min-voltage : %.1f V\r\n"
						"========================\r\n"
				  		"[1] Increase +1V\r\n"
						"[2] Decrease -1V\r\n"
				  		"[3] Increase +0.1V\r\n"
						"[4] Decrease -0.1V\r\n"
						"[x] back\r\n",
						ControlVar.min_voltage);
				HAL_UART_Transmit(&huart2, (uint8_t*)temp, strlen(temp), 1000);

				UIState = Change_min_voltage;
			}
			else if (dataIn == 's' && ControlVar.wave_mode == 0)
			{
				ControlVar.slope_down = !ControlVar.slope_down;

				updateStatus();
			}
			else if (dataIn == 'd' && ControlVar.wave_mode == 2)
			{
				sprintf(temp,
						"\r\n"
						"========================\r\n"
						"Change Duty-cycle : %d %\r\n"
						"========================\r\n"
				  		"[1] Increase +10%\r\n"
						"[2] Decrease -10%\r\n"
				  		"[3] Increase  +1%\r\n"
						"[4] Decrease  -1%\r\n"
						"[x] back\r\n",
						ControlVar.duty_cycle);
				HAL_UART_Transmit(&huart2, (uint8_t*)temp, strlen(temp), 1000);

				UIState = Change_duty_cycle;
			}
			break;

		case Change_mode:
			if (dataIn == 'w') {
				ControlVar.wave_mode = 0;
			}
			else if (dataIn == 'n') {
				ControlVar.wave_mode = 1;
			}
			else if (dataIn == 'q') {
				ControlVar.wave_mode = 2;
			}
			else if (dataIn == 'x') {
			}
			else {break;}

			updateStatus();

			UIState = Main;
			break;

		case Change_frequency:
			if (dataIn == '1' && ControlVar.frequency<=9.0001) {
				ControlVar.frequency += 1;
			}
			else if (dataIn == '2' && ControlVar.frequency>=0.9999) {
				ControlVar.frequency -= 1;
			}
			else if (dataIn == '3' && ControlVar.frequency<=9.9001) {
				ControlVar.frequency += 0.1;
			}
			else if (dataIn == '4' && ControlVar.frequency>=0.0999) {
				ControlVar.frequency -= 0.1;
			}
			else if (dataIn == 'x') {
				updateStatus();

				UIState = Main;
				break;
			}
			else {break;}

			sprintf(temp,
					"\r\n"
					"========================\r\n"
					"Change Frequency : %.1f Hz\r\n"
					"========================\r\n"
			  		"[1] Increase +1Hz\r\n"
					"[2] Decrease -1Hz\r\n"
			  		"[3] Increase +0.1Hz\r\n"
					"[4] Decrease -0.1Hz\r\n"
					"[x] back\r\n",
					ControlVar.frequency);
			HAL_UART_Transmit(&huart2, (uint8_t*)temp, strlen(temp), 1000);
			break;

		case Change_max_voltage:
			if (dataIn == '1' && ControlVar.max_voltage<=2.3001) {
				ControlVar.max_voltage += 1;
			}
			else if (dataIn == '2' && ControlVar.max_voltage>=0.9999 &&
					ControlVar.max_voltage-1 >= ControlVar.min_voltage+0.0999) {
				ControlVar.max_voltage -= 1;
			}
			else if (dataIn == '3' && ControlVar.max_voltage<=3.2001) {
				ControlVar.max_voltage += 0.1;
			}
			else if (dataIn == '4' && ControlVar.max_voltage>=0.0999 &&
					ControlVar.max_voltage-0.1 >= ControlVar.min_voltage+0.0999) {
				ControlVar.max_voltage -= 0.1;
			}
			else if (dataIn == 'x') {
				updateStatus();

				UIState = Main;
				break;
			}
			else {break;}

			sprintf(temp,
					"\r\n"
					"========================\r\n"
					"Change Max-voltage : %.1f V\r\n"
					"========================\r\n"
			  		"[1] Increase +1V\r\n"
					"[2] Decrease -1V\r\n"
			  		"[3] Increase +0.1V\r\n"
					"[4] Decrease -0.1V\r\n"
					"[x] back\r\n",
					ControlVar.max_voltage);
			HAL_UART_Transmit(&huart2, (uint8_t*)temp, strlen(temp), 1000);
			break;

		case Change_min_voltage:
			if (dataIn == '1' && ControlVar.min_voltage<=2.3001 &&
					ControlVar.min_voltage+1 <= ControlVar.max_voltage-0.0999) {
				ControlVar.min_voltage += 1;
			}
			else if (dataIn == '2' && ControlVar.min_voltage>=0.9999) {
				ControlVar.min_voltage -= 1;
			}
			else if (dataIn == '3' && ControlVar.min_voltage<=3.2001 &&
					ControlVar.min_voltage+0.1 <= ControlVar.max_voltage-0.0999) {
				ControlVar.min_voltage += 0.1;
			}
			else if (dataIn == '4' && ControlVar.min_voltage>=0.0999) {
				ControlVar.min_voltage -= 0.1;
			}
			else if (dataIn == 'x') {
				updateStatus();

				UIState = Main;
				break;
			}
			else {break;}

			sprintf(temp,
					"\r\n"
					"========================\r\n"
					"Change Min-voltage : %.1f V\r\n"
					"========================\r\n"
			  		"[1] Increase +1V\r\n"
					"[2] Decrease -1V\r\n"
			  		"[3] Increase +0.1V\r\n"
					"[4] Decrease -0.1V\r\n"
					"[x] back\r\n",
					ControlVar.min_voltage);
			HAL_UART_Transmit(&huart2, (uint8_t*)temp, strlen(temp), 1000);
			break;

		case Change_duty_cycle:
			if (dataIn == '1' && ControlVar.duty_cycle<=90) {
				ControlVar.duty_cycle += 10;
			}
			else if (dataIn == '2' && ControlVar.duty_cycle>=10) {
				ControlVar.duty_cycle -= 10;
			}
			else if (dataIn == '3' && ControlVar.duty_cycle<=99) {
				ControlVar.duty_cycle += 1;
			}
			else if (dataIn == '4' && ControlVar.duty_cycle>=1) {
				ControlVar.duty_cycle -= 1;
			}
			else if (dataIn == 'x') {
				updateStatus();

				UIState = Main;
				break;
			}
			else {break;}

			sprintf(temp,
					"\r\n"
					"========================\r\n"
					"Change Duty-cycle : %d %\r\n"
					"========================\r\n"
			  		"[1] Increase +10%\r\n"
					"[2] Decrease -10%\r\n"
			  		"[3] Increase  +1%\r\n"
					"[4] Decrease  -1%\r\n"
					"[x] back\r\n",
					ControlVar.duty_cycle);
			HAL_UART_Transmit(&huart2, (uint8_t*)temp, strlen(temp), 1000);
			break;

		default:
			break;
	}
}

void updateStatus()
{
	char temp[] = "\r\n"
			"========================\r\n"
			"MAIN MENU\r\n"
			"========================\r\n"
	  		"[m] Change Mode		: ";
	HAL_UART_Transmit(&huart2, (uint8_t*)temp, strlen(temp), 1000);

	switch (ControlVar.wave_mode) {
		case 0:
			sprintf(temp,
					"Sawtooth\r\n");
			break;
		case 1:
			sprintf(temp,
					"Sine-wave\r\n");
			break;
		case 2:
			sprintf(temp,
					"Square-wave\r\n");
			break;
		default:
			break;
	}
	HAL_UART_Transmit(&huart2, (uint8_t*)temp, strlen(temp), 1000);

	char temp2[128];
	sprintf(temp2,
	  		"[f] Change Frequency	: %.1f Hz\r\n"
			"[a] Change Max-voltage	: %.1f V\r\n"
			"[i] Change Min-voltage	: %.1f V\r\n",
			ControlVar.frequency,
			ControlVar.max_voltage,
			ControlVar.min_voltage);
	HAL_UART_Transmit(&huart2, (uint8_t*)temp2, strlen(temp2), 1000);

	if (ControlVar.wave_mode==0 && ControlVar.slope_down) {
		sprintf(temp2,
				"[s] Change Slope	: Down\r\n");
		HAL_UART_Transmit(&huart2, (uint8_t*)temp2, strlen(temp2), 1000);
	}
	else if (ControlVar.wave_mode==0 && !ControlVar.slope_down) {
		sprintf(temp2,
				"[s] Change Slope	: Up\r\n");
		HAL_UART_Transmit(&huart2, (uint8_t*)temp2, strlen(temp2), 1000);
	}
	else if (ControlVar.wave_mode==2) {
		sprintf(temp2,
				"[d] Change Duty-cycle	: %d %\r\n",
				ControlVar.duty_cycle);
		HAL_UART_Transmit(&huart2, (uint8_t*)temp2, strlen(temp2), 1000);
	}
}

int16_t UARTRecieveIT()
{
	//store last data position
	static uint32_t dataPos = 0;
	//create dummy data
	int16_t data=-1;
	//check
	if(huart2.RxXferSize - huart2.RxXferCount!=dataPos)
	{
		data=RxDataBuffer[dataPos];
		dataPos= (dataPos+1)%huart2.RxXferSize;
	}
	return data;
}

void generatorInit(Generator *var)
{
	var->wave_mode		= 0;
						//0:sawtooth
						//1:sinewave
						//2:squarewave
	var->count			= 0; 		//0-99 %
	var->frequency 		= 1;		//0-10 Hz
	var->max_voltage	= 3.3;		//0-3.3 V
	var->min_voltage	= 0;		//0-3.3 V
	var->duty_cycle		= 50;		//0-100 %
	var->slope_down		= 0;		//0:slope up 1:slope down (sawtooth mode)
	var->periodstamp	= micros();	//uS
}

void generator()
{
	//Count by normalized to 0-100
	ControlVar.count += 100 * ControlVar.frequency /10000.0;
	//Overflow
	if (ControlVar.frequency > 0.0001 &&
			micros()-ControlVar.periodstamp > 1/ControlVar.frequency*1000000) {
		ControlVar.count = 0; ControlVar.periodstamp = micros();}

	//Amplify to 12 bits (0-4096)
	switch (ControlVar.wave_mode)
	{
		case 0: //sawtooth
			if (ControlVar.slope_down) {							//slope down
				dataOut = (100-ControlVar.count) * (
						4096/3.3*ControlVar.max_voltage - 4096/3.3*ControlVar.min_voltage) /100
								+ 4096/3.3*ControlVar.min_voltage;
			} else {												//slope up
				dataOut = ControlVar.count * (
						4096/3.3*ControlVar.max_voltage - 4096/3.3*ControlVar.min_voltage) /100
								+ 4096/3.3*ControlVar.min_voltage;
			}
			break;

		case 1: //sinewave
			dataOut = (2048/3.3*ControlVar.max_voltage - 2048/3.3*ControlVar.min_voltage) * (
					sin(ControlVar.count*2.0/100.0*M_PI) + 1) + 4096/3.3*ControlVar.min_voltage;
			break;

		case 2: //squarewave
			if (ControlVar.count < ControlVar.duty_cycle) {
				dataOut = 4096/3.3*ControlVar.max_voltage;
			}
			else {
				dataOut = 4096/3.3*ControlVar.min_voltage;
			}
			break;

		default:
			break;
	}

	//Send the SPI data
	if (hspi3.State == HAL_SPI_STATE_READY
			&& HAL_GPIO_ReadPin(SPI_SS_GPIO_Port, SPI_SS_Pin) == GPIO_PIN_SET)
	{
		MCP4922SetOutput(DACConfig, dataOut);
	}
}

void MCP4922SetOutput(uint8_t Config, uint16_t DACOutput)
{
	uint32_t OutputPacket = (DACOutput & 0x0fff) | ((Config & 0xf) << 12);
	HAL_GPIO_WritePin(SPI_SS_GPIO_Port, SPI_SS_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit_IT(&hspi3, &OutputPacket, 1);
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	if (hspi == &hspi3)
	{
		HAL_GPIO_WritePin(SPI_SS_GPIO_Port, SPI_SS_Pin, GPIO_PIN_SET);
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim == &htim11)
	{
		_micro += 65535;
	}
}

inline uint64_t micros()
{
	return htim11.Instance->CNT + _micro;
}
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
