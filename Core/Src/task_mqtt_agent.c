// Adapted from https://github.com/FreeRTOS/coreMQTT-Agent-Demos/blob/main/source/mqtt-agent-task.c

#include "task_mqtt_agent.h"

#include <stdio.h>
#include <string.h>
#include "main.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "core_mqtt.h"
#include "core_mqtt_agent.h"
#include "core_mqtt_config.h"

#include "transport_interface.h"

#include "freertos_agent_message.h"
#include "freertos_command_pool.h"

#include "subscription_manager.h"

#include "wifi_utils.h"

#define MQTT_BROKER_ENDPOINT "0.0.0.0"

static uint8_t MQTT_BROKER_ENDPOINT_IP[] =
{ 0, 0, 0, 0 };

#define MQTT_BROKER_PORT 1883

#define MQTT_BROKER_TLS_PORT 8883
#define MQTT_BROKER_TLS_HOSTNAME "example.com"

#define CLIENT_IDENTIFIER "testClient"__TIME__

#define TEST_USER_NAME "TEST_USER_NAME"

#define MILLISECONDS_PER_SECOND           ( 1000U )
#define MILLISECONDS_PER_TICK             ( MILLISECONDS_PER_SECOND / configTICK_RATE_HZ )

/**
 * @brief Timeout for receiving CONNACK after sending an MQTT CONNECT packet.
 * Defined in milliseconds.
 */
#define CONNACK_RECV_TIMEOUT_MS           ( 2000U )
#define KEEP_ALIVE_INTERVAL_SECONDS       ( 60U )

static NetworkContext_t xNetworkContext;

static uint32_t ulGlobalEntryTimeMs;
MQTTAgentContext_t xGlobalMqttAgentContext;
static uint8_t xNetworkBuffer[MQTT_AGENT_NETWORK_BUFFER_SIZE];
static MQTTAgentMessageContext_t xCommandQueue;
SubscriptionElement_t xGlobalSubscriptionList[SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS];

static NetworkCredentials_t NetworkCredentials;

/**
 * @brief Initializes an MQTT context, including transport interface and
 * network buffer.
 *
 * @return `MQTTSuccess` if the initialization succeeds, else `MQTTBadParameter`.
 */
static MQTTStatus_t prvMQTTInit(void);

/**
 * @brief The timer query function provided to the MQTT context.
 *
 * @return Time in milliseconds.
 */
static uint32_t prvGetTimeMs(void);

/**
 * @brief Fan out the incoming publishes to the callbacks registered by different
 * tasks. If there are no callbacks registered for the incoming publish, it will be
 * passed to the unsolicited publish handler.
 *
 * @param[in] pMqttAgentContext Agent context.
 * @param[in] packetId Packet ID of publish.
 * @param[in] pxPublishInfo Info of incoming publish.
 */
static void prvIncomingPublishCallback(MQTTAgentContext_t *pMqttAgentContext,
		uint16_t packetId, MQTTPublishInfo_t *pxPublishInfo);

/**
 * @brief Sends an MQTT Connect packet over the already connected TCP socket.
 *
 * @param[in] pxMQTTContext MQTT context pointer.
 * @param[in] xCleanSession If a clean session should be established.
 *
 * @return `MQTTSuccess` if connection succeeds, else appropriate error code
 * from MQTT_Connect.
 */
static MQTTStatus_t prvMQTTConnect( bool xCleanSession);

/**
 * @brief Connect a TCP socket to the MQTT broker.
 *
 * @param[in] pxNetworkContext Network context.
 *
 * @return `pdPASS` if connection succeeds, else `pdFAIL`.
 */
static BaseType_t prvSocketConnect(NetworkContext_t *pxNetworkContext);

/**
 * @brief Disconnect a TCP connection.
 *
 * @param[in] pxNetworkContext Network context.
 *
 * @return `pdPASS` if disconnect succeeds, else `pdFAIL`.
 */
static BaseType_t prvSocketDisconnect(NetworkContext_t *pxNetworkContext);

/**
 * @brief Connects a TCP socket to the MQTT broker, then creates and MQTT
 * connection to the same.
 */
static bool ConnectToMQTTBroker(void);

/**
 * @brief Function to attempt to resubscribe to the topics already present in the
 * subscription list.
 *
 * This function will be invoked when this demo requests the broker to
 * reestablish the session and the broker cannot do so. This function will
 * enqueue commands to the MQTT Agent queue and will be processed once the
 * command loop starts.
 *
 * @return `MQTTSuccess` if adding subscribes to the command queue succeeds, else
 * appropriate error code from MQTTAgent_Subscribe.
 * */
static MQTTStatus_t prvHandleResubscribe(void);

/**
 * @brief Passed into MQTTAgent_Subscribe() as the callback to execute when the
 * broker ACKs the SUBSCRIBE message. This callback implementation is used for
 * handling the completion of resubscribes. Any topic filter failed to resubscribe
 * will be removed from the subscription list.
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html#example_mqtt_api_call
 *
 * @param[in] pxCommandContext Context of the initial command.
 * @param[in] pxReturnInfo The result of the command.
 */
static void prvSubscriptionCommandCallback(
		MQTTAgentCommandContext_t *pxCommandContext,
		MQTTAgentReturnInfo_t *pxReturnInfo);

/**
 * @brief Task used to run the MQTT agent.  In this example the first task that
 * is created is responsible for creating all the other demo tasks.  Then,
 * rather than create prvMQTTAgentTask() as a separate task, it simply calls
 * prvMQTTAgentTask() to become the agent task itself.
 *
 * This task calls MQTTAgent_CommandLoop() in a loop, until MQTTAgent_Terminate()
 * is called. If an error occurs in the command loop, then it will reconnect the
 * TCP and MQTT connections.
 *
 * @param[in] pvParameters Parameters as passed at the time of task creation. Not
 * used in this example.
 */
static void prvMQTTAgentTask(void *pvParameters);

/*-----------------------------------------------------------*/

void ConnectAndStartMQTTAgentTask(GlobalState *globalState)
{
	/* Miscellaneous initialization. */
	ulGlobalEntryTimeMs = prvGetTimeMs();

	do
	{
		LogInfo(("Attempting to connect to WiFi..."));
		bool connResult = wifi_connect();
		globalState->WiFiConnected = connResult;
	} while (!(globalState->WiFiConnected));

	do
	{
		LogInfo(("Attempting to connect to MQTT broker..."));
		bool connResult = ConnectToMQTTBroker();

#ifdef DEBUG_WOLFSSL
		if (!connResult)
		{
			return;
		}
#endif

		globalState->MQTTConnected = connResult;
	} while (!(globalState->MQTTConnected));

	/* This task has nothing left to do, so rather than create the MQTT
	 * agent as a separate thread, it simply calls the function that implements
	 * the agent - in effect turning itself into the agent. */
	prvMQTTAgentTask(NULL);

	/* Should not get here.  Force an assert if the task returns from
	 * prvMQTTAgentTask(). */
	configASSERT(0);
}

static void prvMQTTAgentTask(void *pvParameters)
{
	BaseType_t xNetworkResult = pdFAIL;
	MQTTStatus_t xMQTTStatus = MQTTSuccess, xConnectStatus = MQTTSuccess;
	MQTTContext_t *pMqttContext = &(xGlobalMqttAgentContext.mqttContext);

	(void) pvParameters;

	do
	{
		/* MQTTAgent_CommandLoop() is effectively the agent implementation.  It
		 * will manage the MQTT protocol until such time that an error occurs,
		 * which could be a disconnect.  If an error occurs the MQTT context on
		 * which the error happened is returned so there can be an attempt to
		 * clean up and reconnect however the application writer prefers. */
		xMQTTStatus = MQTTAgent_CommandLoop(&xGlobalMqttAgentContext);

		/* Success is returned for disconnect or termination. The socket should
		 * be disconnected. */
		if (xMQTTStatus == MQTTSuccess)
		{
			/* MQTT Disconnect. Disconnect the socket. */
			xNetworkResult = prvSocketDisconnect(&xNetworkContext);
		}
		/* Error. */
		else
		{
			/* Reconnect TCP. */
			xNetworkResult = prvSocketDisconnect(&xNetworkContext);
			configASSERT(xNetworkResult == pdPASS);
			xNetworkResult = prvSocketConnect(&xNetworkContext);
			configASSERT(xNetworkResult == pdPASS);
			pMqttContext->connectStatus = MQTTNotConnected;
			/* MQTT Connect with a persistent session. */
			xConnectStatus = prvMQTTConnect( false);
			configASSERT(xConnectStatus == MQTTSuccess);
		}
	} while (xMQTTStatus != MQTTSuccess);
}

static MQTTStatus_t prvMQTTInit(void)
{
	TransportInterface_t xTransport;
	MQTTStatus_t xReturn;
	MQTTFixedBuffer_t xFixedBuffer =
	{ .pBuffer = xNetworkBuffer, .size = MQTT_AGENT_NETWORK_BUFFER_SIZE };
	static uint8_t staticQueueStorageArea[MQTT_AGENT_COMMAND_QUEUE_LENGTH
			* sizeof(MQTTAgentCommand_t*)];
	static StaticQueue_t staticQueueStructure;
	MQTTAgentMessageInterface_t messageInterface =
	{ .pMsgCtx = NULL, .send = Agent_MessageSend, .recv = Agent_MessageReceive,
			.getCommand = Agent_GetCommand, .releaseCommand =
					Agent_ReleaseCommand };

	LogDebug(( "Creating command queue." ));
	xCommandQueue.queue = xQueueCreateStatic(MQTT_AGENT_COMMAND_QUEUE_LENGTH,
			sizeof(MQTTAgentCommand_t*), staticQueueStorageArea,
			&staticQueueStructure);
	configASSERT(xCommandQueue.queue);
	messageInterface.pMsgCtx = &xCommandQueue;

	/* Initialize the task pool. */
	Agent_InitializePool();

#if TASK_MQTT_AGENT_USE_TLS
	InitTLSTransport(&xNetworkContext, &xTransport);
#else
	InitPlainTextTransport(&xNetworkContext, &xTransport);
#endif

	/* Initialize MQTT library. */
	xReturn = MQTTAgent_Init(&xGlobalMqttAgentContext, &messageInterface,
			&xFixedBuffer, &xTransport, prvGetTimeMs,
			prvIncomingPublishCallback,
			/* Context to pass into the callback. Passing the pointer to subscription array. */
			xGlobalSubscriptionList);

	return xReturn;
}

static MQTTStatus_t prvMQTTConnect( bool xCleanSession)
{
	MQTTStatus_t xResult;
	MQTTConnectInfo_t xConnectInfo;
	bool xSessionPresent = false;

	/* Many fields are not used in this demo so start with everything at 0. */
	memset(&xConnectInfo, 0x00, sizeof(xConnectInfo));

	/* Start with a clean session i.e. direct the MQTT broker to discard any
	 * previous session data. Also, establishing a connection with clean session
	 * will ensure that the broker does not store any data when this client
	 * gets disconnected. */
	xConnectInfo.cleanSession = xCleanSession;

	/* The client identifier is used to uniquely identify this MQTT client to
	 * the MQTT broker. In a production device the identifier can be something
	 * unique, such as a device serial number. */
	xConnectInfo.pClientIdentifier = CLIENT_IDENTIFIER;
	xConnectInfo.clientIdentifierLength = (uint16_t) strlen( CLIENT_IDENTIFIER);

	/* Set MQTT keep-alive period. It is the responsibility of the application
	 * to ensure that the interval between Control Packets being sent does not
	 * exceed the Keep Alive value. In the absence of sending any other Control
	 * Packets, the Client MUST send a PINGREQ Packet.  This responsibility will
	 * be moved inside the agent. */
	xConnectInfo.keepAliveSeconds = KEEP_ALIVE_INTERVAL_SECONDS;

#if !TASK_MQTT_AGENT_USE_TLS
	xConnectInfo.pUserName = TEST_USER_NAME;
	xConnectInfo.userNameLength = sizeof(TEST_USER_NAME) - 1;
#endif

	/* Send MQTT CONNECT packet to broker. MQTT's Last Will and Testament feature
	 * is not used in this demo, so it is passed as NULL. */
	xResult = MQTT_Connect(&(xGlobalMqttAgentContext.mqttContext),
			&xConnectInfo,
			NULL, CONNACK_RECV_TIMEOUT_MS, &xSessionPresent);

	LogInfo(( "Session present: %d\n", xSessionPresent ));

	/* Resume a session if desired. */
	if ((xResult == MQTTSuccess) && (xCleanSession == false))
	{
		xResult = MQTTAgent_ResumeSession(&xGlobalMqttAgentContext,
				xSessionPresent);

		/* Resubscribe to all the subscribed topics. */
		if ((xResult == MQTTSuccess) && (xSessionPresent == false))
		{
			xResult = prvHandleResubscribe();
		}
	}

	return xResult;
}

static void prvIncomingPublishCallback(MQTTAgentContext_t *pMqttAgentContext,
		uint16_t packetId, MQTTPublishInfo_t *pxPublishInfo)
{
	bool xPublishHandled = false;
	char cOriginalChar, *pcLocation;

	(void) packetId;

	/* Fan out the incoming publishes to the callbacks registered using
	 * subscription manager. */
	xPublishHandled =
			handleIncomingPublishes(
					(SubscriptionElement_t*) pMqttAgentContext->pIncomingCallbackContext,
					pxPublishInfo);

	/* If there are no callbacks to handle the incoming publishes,
	 * handle it as an unsolicited publish. */
	if (xPublishHandled != true)
	{
		/* Ensure the topic string is terminated for printing.  This will over-
		 * write the message ID, which is restored afterwards. */
		pcLocation =
				(char*) &(pxPublishInfo->pTopicName[pxPublishInfo->topicNameLength]);
		cOriginalChar = *pcLocation;
		*pcLocation = 0x00;
		LogWarn(
				( "WARN:  Received an unsolicited publish from topic %s", pxPublishInfo->pTopicName ));
		*pcLocation = cOriginalChar;
	}
}

static MQTTStatus_t prvHandleResubscribe(void)
{
	MQTTStatus_t xResult = MQTTBadParameter;
	uint32_t ulIndex = 0U;
	uint16_t usNumSubscriptions = 0U;

	/* These variables need to stay in scope until command completes. */
	static MQTTAgentSubscribeArgs_t xSubArgs =
	{ 0 };
	static MQTTSubscribeInfo_t xSubInfo[SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS] =
	{ 0 };
	static MQTTAgentCommandInfo_t xCommandParams =
	{ 0 };

	/* Loop through each subscription in the subscription list and add a subscribe
	 * command to the command queue. */
	for (ulIndex = 0U; ulIndex < SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS;
			ulIndex++)
	{
		/* Check if there is a subscription in the subscription list. This demo
		 * doesn't check for duplicate subscriptions. */
		if (xGlobalSubscriptionList[ulIndex].usFilterStringLength != 0)
		{
			xSubInfo[usNumSubscriptions].pTopicFilter =
					xGlobalSubscriptionList[ulIndex].pcSubscriptionFilterString;
			xSubInfo[usNumSubscriptions].topicFilterLength =
					xGlobalSubscriptionList[ulIndex].usFilterStringLength;

			/* QoS1 is used for all the subscriptions in this demo. */
			xSubInfo[usNumSubscriptions].qos = MQTTQoS1;

			LogInfo(
					( "Resubscribe to the topic %.*s will be attempted.", xSubInfo[ usNumSubscriptions ].topicFilterLength, xSubInfo[ usNumSubscriptions ].pTopicFilter ));

			usNumSubscriptions++;
		}
	}

	if (usNumSubscriptions > 0U)
	{
		xSubArgs.pSubscribeInfo = xSubInfo;
		xSubArgs.numSubscriptions = usNumSubscriptions;

		/* The block time can be 0 as the command loop is not running at this point. */
		xCommandParams.blockTimeMs = 0U;
		xCommandParams.cmdCompleteCallback = prvSubscriptionCommandCallback;
		xCommandParams.pCmdCompleteCallbackContext = (void*) &xSubArgs;

		/* Enqueue subscribe to the command queue. These commands will be processed only
		 * when command loop starts. */
		xResult = MQTTAgent_Subscribe(&xGlobalMqttAgentContext, &xSubArgs,
				&xCommandParams);
	}
	else
	{
		/* Mark the resubscribe as success if there is nothing to be subscribed. */
		xResult = MQTTSuccess;
	}

	if (xResult != MQTTSuccess)
	{
		LogError(
				( "Failed to enqueue the MQTT subscribe command. xResult=%s.", MQTT_Status_strerror( xResult ) ));
	}

	return xResult;
}

static void prvSubscriptionCommandCallback(
		MQTTAgentCommandContext_t *pxCommandContext,
		MQTTAgentReturnInfo_t *pxReturnInfo)
{
	size_t lIndex = 0;
	MQTTAgentSubscribeArgs_t *pxSubscribeArgs =
			(MQTTAgentSubscribeArgs_t*) pxCommandContext;

	/* If the return code is success, no further action is required as all the topic filters
	 * are already part of the subscription list. */
	if (pxReturnInfo->returnCode != MQTTSuccess)
	{
		/* Check through each of the suback codes and determine if there are any failures. */
		for (lIndex = 0; lIndex < pxSubscribeArgs->numSubscriptions; lIndex++)
		{
			/* This demo doesn't attempt to resubscribe in the event that a SUBACK failed. */
			if (pxReturnInfo->pSubackCodes[lIndex] == MQTTSubAckFailure)
			{
				LogError(
						( "Failed to resubscribe to topic %.*s.", pxSubscribeArgs->pSubscribeInfo[ lIndex ].topicFilterLength, pxSubscribeArgs->pSubscribeInfo[ lIndex ].pTopicFilter ));
				/* Remove subscription callback for unsubscribe. */
				removeSubscription(xGlobalSubscriptionList,
						pxSubscribeArgs->pSubscribeInfo[lIndex].pTopicFilter,
						pxSubscribeArgs->pSubscribeInfo[lIndex].topicFilterLength);
			}
		}

		/* Hit an assert as some of the tasks won't be able to proceed correctly without
		 * the subscriptions. This logic will be updated with exponential backoff and retry.  */
		configASSERT(pdTRUE);
	}
}

static BaseType_t prvSocketConnect(NetworkContext_t *pxNetworkContext)
{
#if TASK_MQTT_AGENT_USE_TLS

	bool certsLoaded = LoadTLSCredentials(&NetworkCredentials, pxNetworkContext);
	if (!certsLoaded)
	{
		LogError(("Could not load TLS certificates"));
		return pdFAIL;
	}

	LogInfo(
			( "Creating a TLS connection to %s:%d (%s:%d).", MQTT_BROKER_TLS_HOSTNAME, MQTT_BROKER_TLS_PORT, MQTT_BROKER_ENDPOINT, MQTT_BROKER_TLS_PORT ));

	bool xNetworkStatus = (TLSWiFiConnect(pxNetworkContext,
	MQTT_BROKER_TLS_HOSTNAME, MQTT_BROKER_ENDPOINT_IP, MQTT_BROKER_TLS_PORT,
			&NetworkCredentials) == TLS_TRANSPORT_SUCCESS);
#else
	LogInfo(
			( "Creating a TCP connection to %s:%d.", MQTT_BROKER_ENDPOINT, MQTT_BROKER_PORT ));

	bool xNetworkStatus = PlaintextWiFiConnect(pxNetworkContext,
			MQTT_BROKER_ENDPOINT_IP, MQTT_BROKER_PORT);
#endif

	if (!xNetworkStatus)
	{
		LogError(( "Connection to the MQTT broker failed" ));
		return pdFAIL;
	}

	return pdPASS;
}

/*-----------------------------------------------------------*/

static BaseType_t prvSocketDisconnect(NetworkContext_t *pxNetworkContext)
{
#if TASK_MQTT_AGENT_USE_TLS
	TLSWiFiDisconnect(pxNetworkContext);
#else
	PlaintextWifiDisconnect(pxNetworkContext);
#endif

	return pdPASS;
}

static bool ConnectToMQTTBroker(void)
{
	BaseType_t xNetworkStatus = pdFAIL;
	MQTTStatus_t xMQTTStatus;

#if TASK_MQTT_AGENT_USE_TLS
	bool net_context_use_ssl = true;
#else
	bool net_context_use_ssl = false;
#endif
	if(!InitNetworkContext(&xNetworkContext, net_context_use_ssl)) {
		return false;
	}

	/* Connect a TCP socket to the broker. */
	xNetworkStatus = prvSocketConnect(&xNetworkContext);
	if (xNetworkStatus != pdPASS)
	{
		return false;
	}

	/* Initialize the MQTT context with the buffer and transport interface. */
	xMQTTStatus = prvMQTTInit();
	if (xMQTTStatus != MQTTSuccess)
	{
		return false;
	}

	/* Form an MQTT connection without a persistent session. */
	xMQTTStatus = prvMQTTConnect( true);
	if (xMQTTStatus != MQTTSuccess)
	{
		return false;
	}

	return true;
}

static uint32_t prvGetTimeMs(void)
{
	TickType_t xTickCount = 0;
	uint32_t ulTimeMs = 0UL;

	/* Get the current tick count. */
	xTickCount = xTaskGetTickCount();

	/* Convert the ticks to milliseconds. */
	ulTimeMs = (uint32_t) xTickCount * MILLISECONDS_PER_TICK;

	/* Reduce ulGlobalEntryTimeMs from obtained time so as to always return the
	 * elapsed time in the application. */
	ulTimeMs = (uint32_t) (ulTimeMs - ulGlobalEntryTimeMs);

	return ulTimeMs;
}
