#include "wifi_utils.h"

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "stdio.h"

#include "es_wifi.h"
#include "wifi.h"

#define WIFI_WRITE_TIMEOUT 10000
#define WIFI_READ_TIMEOUT  10000

#define SEND_TCP_SOCKET 0

#define WIFI_SSID "WIFI_SSID"
#define WIFI_PASS "WIFI_PASSWORD"

static uint8_t IP_Addr[4];

bool wifi_connect(void)
{
	printf("Connecting to %s ...\r\n", WIFI_SSID);
	WIFI_Ecn_t security;
	security = WIFI_ECN_WPA2_PSK;

	if (WIFI_Connect(WIFI_SSID, WIFI_PASS, security) == WIFI_STATUS_OK)
	{
		if (WIFI_GetIP_Address(IP_Addr, sizeof(IP_Addr)) == WIFI_STATUS_OK)
		{
			printf("eS-WiFi module connected: got IP Address : %d.%d.%d.%d\r\n",
					IP_Addr[0], IP_Addr[1], IP_Addr[2], IP_Addr[3]);
		}
		else
		{
			LogLine(("ERROR : es-wifi module CANNOT get IP address"));
			return false;
		}
	}
	else
	{
		LogLine(("ERROR : es-wifi module NOT connected"));
		return false;
	}

	return true;
}

bool SendTcpData(const uint8_t *ipaddr, uint16_t port, const uint8_t *pdata,
		uint16_t Reqlen)
{

	if (WIFI_OpenClientConnection(SEND_TCP_SOCKET, WIFI_TCP_PROTOCOL,
			"TCP_CLIENT", ipaddr, port, 0) != WIFI_STATUS_OK)
	{
		WIFI_CloseClientConnection(SEND_TCP_SOCKET);
		return false;
	}

	uint16_t SentDatalen;

	if (WIFI_SendData(SEND_TCP_SOCKET, pdata, Reqlen, &SentDatalen,
	WIFI_WRITE_TIMEOUT) != WIFI_STATUS_OK)
	{
		WIFI_CloseClientConnection(SEND_TCP_SOCKET);
		return false;
	}

	WIFI_CloseClientConnection(SEND_TCP_SOCKET);
	return true;
}
