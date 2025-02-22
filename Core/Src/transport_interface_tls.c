#include "transport_interface.h"
#include "core_mqtt_config.h"

#include "main.h"

#include "es_wifi.h"
#include "wifi.h"

#include "wifi_utils.h"

#include "TESTING_KEYS.h"

#include "stsafe_interface.h"

#define TLS_TRANSPORT_USE_STSAFEA 1

// Adapted from https://github.com/FreeRTOS/FreeRTOS/blob/main/FreeRTOS-Plus/Source/Application-Protocols/network_transport/transport_wolfSSL.c

/**
 * @brief Set up TLS on a TCP connection.
 *
 * @param[in] pNetworkContext Network context.
 * @param[in] pHostName Remote host name, used for server name indication.
 * @param[in] pNetworkCredentials TLS setup parameters.
 *
 * @return #TLS_TRANSPORT_SUCCESS, #TLS_TRANSPORT_INSUFFICIENT_MEMORY, #TLS_TRANSPORT_INVALID_CREDENTIALS,
 * #TLS_TRANSPORT_HANDSHAKE_FAILED, or #TLS_TRANSPORT_INTERNAL_ERROR.
 */
static TlsTransportStatus_t tlsSetup(NetworkContext_t *pNetworkContext,
		const char *pHostName, const NetworkCredentials_t *pNetworkCredentials);

/**
 * @brief  Initialize TLS component.
 *
 * @return #TLS_TRANSPORT_SUCCESS, #TLS_TRANSPORT_INSUFFICIENT_MEMORY, or #TLS_TRANSPORT_INTERNAL_ERROR.
 */
static TlsTransportStatus_t initTLS(void);

/*
 *  @brief  Receive date from the socket passed as the context
 *
 *  @param[in] ssl WOLFSSL object.
 *  @param[in] buf Buffer for received data
 *  @param[in] sz  Size to receive
 *  @param[in] context Socket to be received from
 *
 *  @return received size( > 0 ), #WOLFSSL_CBIO_ERR_CONN_CLOSE, #WOLFSSL_CBIO_ERR_WANT_READ.
 */
static int wolfSSL_IORecvGlue(WOLFSSL *ssl, char *buf, int sz, void *context);

/*
 *  @brief  Send date to the socket passed as the context
 *
 *  @param[in] ssl WOLFSSL object.
 *  @param[in] buf Buffer for data to be sent
 *  @param[in] sz  Size to send
 *  @param[in] context Socket to be sent to
 *
 *  @return received size( > 0 ), #WOLFSSL_CBIO_ERR_CONN_CLOSE, #WOLFSSL_CBIO_ERR_WANT_WRITE.
 */
static int wolfSSL_IOSendGlue(WOLFSSL *ssl, char *buf, int sz, void *context);

/*
 *  @brief  Load credentials from file/buffer
 *
 *  @param[in] pNetCtx  NetworkContext_t
 *  @param[in] pNetCred NetworkCredentials_t
 *
 *  @return #TLS_TRANSPORT_SUCCESS, #TLS_TRANSPORT_INVALID_CREDENTIALS.
 */
static TlsTransportStatus_t loadCredentials(NetworkContext_t *pNetCtx,
		const NetworkCredentials_t *pNetCred);

// IMPLEMENTATIONS

static int wolfSSL_IOSendGlue(WOLFSSL *ssl, char *buf, int sz, void *context)
{
	(void) ssl; /* to prevent unused warning*/

	uint32_t socket = *((uint32_t*) context);
	uint16_t SentDataSize = 0;

	WIFI_Status_t ret = WIFI_SendData(socket, (const uint8_t*) buf,
			(uint16_t) sz, &SentDataSize, WIFI_SEND_TIMEOUT);

	if (ret != WIFI_STATUS_OK)
	{
		return WOLFSSL_CBIO_ERR_GENERAL;
	}
	return (int) SentDataSize;
}

static int wolfSSL_IORecvGlue(WOLFSSL *ssl, char *buf, int sz, void *context)
{
	(void) ssl; /* to prevent unused warning*/

	uint32_t socket = *((uint32_t*) context);
	uint16_t ReceivedDataSize = 0;

	WIFI_Status_t ret = WIFI_ReceiveData(socket, (uint8_t*) buf, (uint16_t) sz,
			&ReceivedDataSize, WIFI_RECV_TIMEOUT);

	printf("wolfssl_IORecv: Req: %d bytes, Recved: %u bytes, ret = %d\r\n", sz,
			ReceivedDataSize, ret);

	if (ret != WIFI_STATUS_OK)
	{
		if (ret == WIFI_STATUS_TIMEOUT)
		{
			return WOLFSSL_CBIO_ERR_TIMEOUT;
		}
		else
		{
			return WOLFSSL_CBIO_ERR_GENERAL;
		}
	}

	if (ret == WIFI_STATUS_OK && sz > 0 && ReceivedDataSize == 0)
	{
		// no data was available
		return WOLFSSL_CBIO_ERR_WANT_READ;
	}

	return (int) ReceivedDataSize;
}

static TlsTransportStatus_t loadCredentials(NetworkContext_t *pNetCtx,
		const NetworkCredentials_t *pNetCred)
{
	TlsTransportStatus_t returnStatus = TLS_TRANSPORT_SUCCESS;

	configASSERT(pNetCtx != NULL);
	configASSERT(pNetCred != NULL);

	if (wolfSSL_CTX_load_verify_buffer(pNetCtx->sslContext.ctx,
			(const byte*) (pNetCred->pRootCa), (long) (pNetCred->rootCaSize),
			SSL_FILETYPE_PEM) == SSL_SUCCESS)
	{
#if TLS_TRANSPORT_USE_STSAFEA
		if (wolfSSL_CTX_use_certificate_chain_buffer(pNetCtx->sslContext.ctx,
				(const byte*) (pNetCred->pClientCert),
				(long) (pNetCred->clientCertSize)) == SSL_SUCCESS)
#else
		if (wolfSSL_CTX_use_certificate_buffer(pNetCtx->sslContext.ctx,
				(const byte*) (pNetCred->pClientCert),
				(long) (pNetCred->clientCertSize),
				SSL_FILETYPE_PEM) == SSL_SUCCESS)
#endif
		{
#if TLS_TRANSPORT_USE_STSAFEA
			stsafe_SetupPkCallbacks(pNetCtx);
#else
			if (wolfSSL_CTX_use_PrivateKey_buffer(pNetCtx->sslContext.ctx,
					(const byte*) (pNetCred->pPrivateKey),
					(long) (pNetCred->privateKeySize),
					SSL_FILETYPE_PEM) == SSL_SUCCESS)
			{
				returnStatus = TLS_TRANSPORT_SUCCESS;
			}
			else
			{
				LogError(( "Failed to load client-private-key from buffer" ));
				returnStatus = TLS_TRANSPORT_INVALID_CREDENTIALS;
			}
#endif
		}
		else
		{
			LogError(( "Failed to load client-certificate from buffer" ));
			returnStatus = TLS_TRANSPORT_INVALID_CREDENTIALS;
		}
	}
	else
	{
		LogError(( "Failed to load ca-certificate from buffer" ));
		returnStatus = TLS_TRANSPORT_INVALID_CREDENTIALS;
	}

	return returnStatus;
}

static TlsTransportStatus_t tlsSetup(NetworkContext_t *pNetCtx,
		const char *pHostName, const NetworkCredentials_t *pNetCred)
{
	TlsTransportStatus_t returnStatus = TLS_TRANSPORT_SUCCESS;

	uint32_t *xSocket = NULL;

	configASSERT(pNetCtx != NULL);
	//configASSERT(pNetCtx->isSSL == true)
	configASSERT(pHostName != NULL);
	configASSERT(pNetCred != NULL);
	configASSERT(pNetCred->pRootCa != NULL);

	if (pNetCtx->sslContext.ctx == NULL)
	{
		/* Attempt to create a context that uses the TLS 1.3 or 1.2 */

		WOLFSSL_METHOD *method = 0;
		method = wolfSSLv23_client_method_ex( NULL);

		pNetCtx->sslContext.ctx = wolfSSL_CTX_new(method);
	}

	if (pNetCtx->sslContext.ctx != NULL)
	{
		/* load credentials from file */
		if (loadCredentials(pNetCtx, pNetCred) == TLS_TRANSPORT_SUCCESS)
		{
			/* create a ssl object */
			pNetCtx->sslContext.ssl = wolfSSL_new(pNetCtx->sslContext.ctx);

			if (pNetCtx->sslContext.ssl != NULL)
			{
				xSocket = &(pNetCtx->socket);

				/* set Recv/Send glue functions to the WOLFSSL object */
				wolfSSL_SSLSetIORecv(pNetCtx->sslContext.ssl,
						wolfSSL_IORecvGlue);
				wolfSSL_SSLSetIOSend(pNetCtx->sslContext.ssl,
						wolfSSL_IOSendGlue);

				/* set socket as a context of read/send glue funcs */
				wolfSSL_SetIOReadCtx(pNetCtx->sslContext.ssl, xSocket);
				wolfSSL_SetIOWriteCtx(pNetCtx->sslContext.ssl, xSocket);

#if TLS_TRANSPORT_USE_STSAFEA
				stsafe_SetupPkCallbacksContext(pNetCtx);
#endif

				/* let wolfSSL perform tls handshake */
				if (wolfSSL_connect(pNetCtx->sslContext.ssl) == SSL_SUCCESS)
				{
					returnStatus = TLS_TRANSPORT_SUCCESS;
				}
				else
				{
					wolfSSL_shutdown(pNetCtx->sslContext.ssl);
					wolfSSL_free(pNetCtx->sslContext.ssl);
					pNetCtx->sslContext.ssl = NULL;
					wolfSSL_CTX_free(pNetCtx->sslContext.ctx);
					pNetCtx->sslContext.ctx = NULL;

					LogError(( "Failed to establish a TLS connection" ));
					returnStatus = TLS_TRANSPORT_HANDSHAKE_FAILED;
				}
			}
			else
			{
				wolfSSL_CTX_free(pNetCtx->sslContext.ctx);
				pNetCtx->sslContext.ctx = NULL;

				LogError(( "Failed to create wolfSSL object" ));
				returnStatus = TLS_TRANSPORT_INTERNAL_ERROR;
			}
		}
		else
		{
			wolfSSL_CTX_free(pNetCtx->sslContext.ctx);
			pNetCtx->sslContext.ctx = NULL;

			LogError(( "Failed to load credentials" ));
			returnStatus = TLS_TRANSPORT_INVALID_CREDENTIALS;
		}
	}
	else
	{
		LogError(( "Failed to create a wolfSSL_CTX" ));
		returnStatus = TLS_TRANSPORT_CONNECT_FAILURE;
	}

	return returnStatus;
}

static TlsTransportStatus_t initTLS(void)
{
	/* initialize wolfSSL */
	wolfSSL_Init();

#ifdef DEBUG_WOLFSSL
	wolfSSL_Debugging_ON();
#endif

	return TLS_TRANSPORT_SUCCESS;
}

bool InitSSLContext(NetworkContext_t *NetworkContext)
{
#if TLS_TRANSPORT_USE_STSAFEA
	return stsafea_init(&(NetworkContext->sslContext.stsafea_handle),
			NetworkContext->sslContext.a_rx_tx_stsafea_data);
#endif
	return true;
}

void InitTLSTransport(NetworkContext_t *NetworkContext,
		TransportInterface_t *Transport)
{
	Transport->send = TLSSend;
	Transport->recv = TLSRecv;
	Transport->pNetworkContext = NetworkContext;
	Transport->writev = NULL;
}

TlsTransportStatus_t TLSWiFiConnect(NetworkContext_t *NetworkContext,
		const char *HostName, const uint8_t *ipaddr, uint16_t port,
		const NetworkCredentials_t *NetworkCredentials)
{
	TlsTransportStatus_t returnStatus = TLS_TRANSPORT_SUCCESS;

	BaseType_t isSocketConnected = pdFALSE;

	// Establish TCP Connection
	bool tcpConnected = PlaintextWiFiConnect(NetworkContext, ipaddr, port);
	if (!tcpConnected)
	{
		LogError(("Failed to open TCP connection to server"));
		return TLS_TRANSPORT_CONNECT_FAILURE;
	}

	/* Initialize TLS. */
	if (returnStatus == TLS_TRANSPORT_SUCCESS)
	{
		isSocketConnected = pdTRUE;

		returnStatus = initTLS();
	}

	/* Perform TLS handshake. */
	if (returnStatus == TLS_TRANSPORT_SUCCESS)
	{
		returnStatus = tlsSetup(NetworkContext, HostName, NetworkCredentials);
	}

	/* Clean up on failure. */
	if (returnStatus != TLS_TRANSPORT_SUCCESS)
	{
		if (isSocketConnected == pdTRUE)
		{
			PlaintextWifiDisconnect(NetworkContext);
		}
	}
	else
	{
		LogInfo(
				( "(Network connection %p) Connection to %s established.", NetworkContext, HostName ));
	}

	return returnStatus;

}

void TLSWiFiDisconnect(NetworkContext_t *NetworkContext)
{
	WOLFSSL *pSsl = NetworkContext->sslContext.ssl;
	WOLFSSL_CTX *pCtx = NULL;

	/* shutdown an active TLS connection */
	wolfSSL_shutdown(pSsl);

	/* cleanup WOLFSSL object */
	wolfSSL_free(pSsl);
	NetworkContext->sslContext.ssl = NULL;

	/* Call socket shutdown function to close connection. */
	PlaintextWifiDisconnect(NetworkContext);

	/* free WOLFSSL_CTX object*/
	pCtx = NetworkContext->sslContext.ctx;

	wolfSSL_CTX_free(pCtx);
	NetworkContext->sslContext.ctx = NULL;

	wolfSSL_Cleanup();

}

int32_t TLSSend(NetworkContext_t *NetworkContext, const void *Buffer,
		size_t bytesToSend)
{
	int32_t tlsStatus = 0;
	int iResult = 0;
	WOLFSSL *pSsl = NULL;

	if ((NetworkContext == NULL) || (NetworkContext->sslContext.ssl == NULL))
	{
		LogError(
				( "TLSSend: invalid input, NetworkContext=%p", NetworkContext ));
		tlsStatus = -1;
	}
	else if (Buffer == NULL)
	{
		LogError(( "TLSSend: invalid input, Buffer == NULL" ));
		tlsStatus = -1;
	}
	else if (bytesToSend == 0)
	{
		LogError(( "TLSSend: invalid input, bytesToSend == 0" ));
		tlsStatus = -1;
	}
	else
	{
		pSsl = NetworkContext->sslContext.ssl;

		iResult = wolfSSL_write(pSsl, Buffer, bytesToSend);

		if (iResult > 0)
		{
			tlsStatus = iResult;
		}
		else if (wolfSSL_want_write(pSsl) == 1)
		{
			tlsStatus = 0;
		}
		else
		{
			tlsStatus = wolfSSL_state(pSsl);
			LogError(
					( "TLSSend: Error from wolfSL_write %d : %s ", iResult, wolfSSL_ERR_reason_error_string( tlsStatus ) ));
		}
	}

	return tlsStatus;
}

int32_t TLSRecv(NetworkContext_t *NetworkContext, void *Buffer,
		size_t bytesToRecv)
{
	int32_t tlsStatus = 0;
	int iResult = 0;
	WOLFSSL *pSsl = NULL;

	if ((NetworkContext == NULL) || (NetworkContext->sslContext.ssl == NULL))
	{
		LogError(
				( "TLSRecv: invalid input, NetworkContext=%p", NetworkContext ));
		tlsStatus = -1;
	}
	else if (Buffer == NULL)
	{
		LogError(( "TLSRecv: invalid input, Buffer == NULL" ));
		tlsStatus = -1;
	}
	else if (bytesToRecv == 0)
	{
		LogError(( "TLSRecv: invalid input, bytesToRecv == 0" ));
		tlsStatus = -1;
	}
	else
	{
		pSsl = NetworkContext->sslContext.ssl;

		iResult = wolfSSL_read(pSsl, Buffer, bytesToRecv);

		if (iResult > 0)
		{
			tlsStatus = iResult;
		}
		else if (wolfSSL_want_read(pSsl) == 1)
		{
			tlsStatus = 0;
		}
		else
		{
			tlsStatus = wolfSSL_state(pSsl);
			LogError(
					( "TLSRecv: Error from wolfSSL_read %d : %s ", iResult, wolfSSL_ERR_reason_error_string( tlsStatus ) ));
		}
	}

	return tlsStatus;
}

bool LoadTLSCredentials(NetworkCredentials_t *NetworkCredentials,
		NetworkContext_t *NetworkContext)
{
	NetworkCredentials->pRootCa = (const unsigned char*) ROOT_CA_PEM;
	NetworkCredentials->rootCaSize = strlen( ROOT_CA_PEM);

#if TLS_TRANSPORT_USE_STSAFEA
	return stsafea_load_client_cert(&NetworkContext->sslContext.stsafea_handle,
			NetworkCredentials);
#else
	NetworkCredentials->pClientCert =
			(const unsigned char*) CLIENT_CERTIFICATE_PEM;
	NetworkCredentials->pPrivateKey =
			(const unsigned char*) CLIENT_PRIVATE_KEY_PEM;

	NetworkCredentials->clientCertSize = strlen(CLIENT_CERTIFICATE_PEM);
	NetworkCredentials->privateKeySize = strlen( CLIENT_PRIVATE_KEY_PEM);
	return true;
#endif
}

