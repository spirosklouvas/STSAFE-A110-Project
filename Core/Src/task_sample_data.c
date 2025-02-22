#include "task_sample_data.h"

#include <string.h>

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include "core_mqtt.h"
#include "core_mqtt_agent.h"
#include "core_mqtt_config.h"

extern MQTTAgentContext_t xGlobalMqttAgentContext;

#define TELEMETRY_TOPIC "v1/devices/me/telemetry"

#define MESSAGE_BUFFER_SIZE 1000

static int CURRENT_MESSAGE_SIZE = 0;
static uint8_t MESSAGE_BUFFER[MESSAGE_BUFFER_SIZE];

static void ClearMessageBuffer();
static void UpdateTelemetryMessage();
static void PublishTelemetryMessage();

void RunTaskSampleData(GlobalState *globalState)
{
	// Wait for wifi and mqtt to connect
	while (!(globalState->WiFiConnected && globalState->MQTTConnected))
	{
		osDelay(1000);
	}

	// initialization
	ClearMessageBuffer();

	// main loop
	for (;;)
	{
		UpdateTelemetryMessage();
		PublishTelemetryMessage();
		osDelay(5000);
	}
}

static void UpdateTelemetryMessage()
{
	ClearMessageBuffer();
	CURRENT_MESSAGE_SIZE = snprintf((char*) MESSAGE_BUFFER, 1000, "{ticks:%lu}",
			xTaskGetTickCount());
}

static void PublishTelemetryMessage()
{
	LogInfo(
			("Sending publish message to agent with message '%s' on topic '%s'", MESSAGE_BUFFER, TELEMETRY_TOPIC));

	MQTTPublishInfo_t publishInfo =
	{ 0UL };
	memset((void*) &publishInfo, 0x00, sizeof(publishInfo));

	MQTTAgentCommandInfo_t commandInfo =
	{ 0UL };
	commandInfo.blockTimeMs = 500;

	publishInfo.qos = MQTTQoS1;
	publishInfo.pTopicName = TELEMETRY_TOPIC;
	publishInfo.topicNameLength = sizeof(TELEMETRY_TOPIC) - 1;
	publishInfo.pPayload = MESSAGE_BUFFER;
	publishInfo.payloadLength = CURRENT_MESSAGE_SIZE;

	MQTTAgent_Publish(&xGlobalMqttAgentContext, &publishInfo, &commandInfo);
}

static void ClearMessageBuffer()
{
	memset(MESSAGE_BUFFER, 0, MESSAGE_BUFFER_SIZE);
}
