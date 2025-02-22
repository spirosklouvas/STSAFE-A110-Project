#include "task_default.h"

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

void RunDefaultTask(GlobalState *globalState)
{
	HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
	osDelay(1);
	HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);

	osDelay(1000);
}
