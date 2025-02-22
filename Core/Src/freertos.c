/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : freertos.c
 * Description        : Code for freertos applications
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "task_default.h"

#include "task_mqtt_agent.h"

#include "task_sample_data.h"

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
/* USER CODE BEGIN Variables */

GlobalState GLOBAL_STATE =
{ .WiFiConnected = false, .MQTTConnected = false };

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 64 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for mqttTask */
osThreadId_t mqttTaskHandle;
const osThreadAttr_t mqttTask_attributes = {
  .name = "mqttTask",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for sampleDataTask */
osThreadId_t sampleDataTaskHandle;
const osThreadAttr_t sampleDataTask_attributes = {
  .name = "sampleDataTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartMQTTTask(void *argument);
void StartSampleDataTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
	/* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
	/* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
	/* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of mqttTask */
  mqttTaskHandle = osThreadNew(StartMQTTTask, NULL, &mqttTask_attributes);

  /* creation of sampleDataTask */
  sampleDataTaskHandle = osThreadNew(StartSampleDataTask, NULL, &sampleDataTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
	/* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
	/* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
	/* Infinite loop */
	GlobalState *state = &GLOBAL_STATE;
	for (;;)
	{
		RunDefaultTask(state);
	}
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartMQTTTask */
/**
 * @brief Function implementing the mqttTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartMQTTTask */
void StartMQTTTask(void *argument)
{
  /* USER CODE BEGIN StartMQTTTask */
	/* Infinite loop */
	GlobalState *state = &GLOBAL_STATE;
	ConnectAndStartMQTTAgentTask(state);
  /* USER CODE END StartMQTTTask */
}

/* USER CODE BEGIN Header_StartSampleDataTask */
/**
 * @brief Function implementing the sampleDataTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartSampleDataTask */
void StartSampleDataTask(void *argument)
{
  /* USER CODE BEGIN StartSampleDataTask */
	/* Infinite loop */
	GlobalState *state = &GLOBAL_STATE;
	RunTaskSampleData(state);
  /* USER CODE END StartSampleDataTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

