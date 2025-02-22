#include "transport_interface.h"

#include "main.h"

#include "es_wifi.h"
#include "wifi.h"

#include "wifi_utils.h"

// implemented in transport_interface_tls.c
extern bool InitSSLContext(NetworkContext_t *NetworkContext);

bool InitNetworkContext(NetworkContext_t *NetworkContext, bool useSSL)
{
	NetworkContext->socket = 0;
	NetworkContext->isSSL = useSSL;

	if (useSSL)
	{
		return InitSSLContext(NetworkContext);
	}

	return true;
}

bool PlaintextWiFiConnect(NetworkContext_t *NetworkContext,
		const uint8_t *ipaddr, uint16_t port)
{
	WIFI_Status_t ret = WIFI_OpenClientConnection(NetworkContext->socket,
			WIFI_TCP_PROTOCOL, "MQTT_CLIENT", ipaddr, port, 0);

	if (ret != WIFI_STATUS_OK)
	{
		WIFI_CloseClientConnection(NetworkContext->socket);
		return false;
	}

	return true;
}

void PlaintextWifiDisconnect(NetworkContext_t *NetworkContext)
{
	WIFI_CloseClientConnection(NetworkContext->socket);
}

int32_t PlaintextSend(NetworkContext_t *NetworkContext, const void *Buffer,
		size_t bytesToSend)
{
	uint16_t SentDataSize = 0;

	WIFI_Status_t ret = WIFI_SendData(NetworkContext->socket, Buffer,
			bytesToSend, &SentDataSize, WIFI_SEND_TIMEOUT);

	if (ret != WIFI_STATUS_OK)
	{
		return -1;
	}

	return (int32_t) SentDataSize;
}

int32_t PlaintextRecv(NetworkContext_t *NetworkContext, void *Buffer,
		size_t bytesToRecv)
{
	uint16_t ReceivedDataSize = 0;

	uint32_t Timeout = WIFI_RECV_TIMEOUT;
	if (bytesToRecv == 1)
	{
		Timeout = 0;
	}

	WIFI_Status_t ret = WIFI_ReceiveData(NetworkContext->socket,
			(uint8_t*) Buffer, bytesToRecv, &ReceivedDataSize, Timeout);

	if (ret != WIFI_STATUS_OK)
	{
		if (bytesToRecv == 1 && ret == WIFI_STATUS_TIMEOUT)
		{
			return (int32_t) ReceivedDataSize;
		}

		return -1;
	}

	return (int32_t) ReceivedDataSize;
}

void InitPlainTextTransport(NetworkContext_t *NetworkContext,
		TransportInterface_t *Transport)
{
	Transport->send = PlaintextSend;
	Transport->recv = PlaintextRecv;
	Transport->pNetworkContext = NetworkContext;
	Transport->writev = NULL;
}

bool NetworkIsUp()
{
	uint8_t IP_Addr[4];

	return WIFI_GetIP_Address(IP_Addr, sizeof(IP_Addr)) == WIFI_STATUS_OK;
}
