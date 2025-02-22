#ifndef INC_TASK_MQTT_AGENT_H_
#define INC_TASK_MQTT_AGENT_H_

#include "main.h"

#define TASK_MQTT_AGENT_USE_TLS 1

void ConnectAndStartMQTTAgentTask(GlobalState* globalState);

#endif /* INC_TASK_MQTT_AGENT_H_ */
