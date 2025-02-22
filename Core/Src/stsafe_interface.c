#include "stsafe_interface.h"

#include "wolfssl/wolfcrypt/asn_public.h"
#include "wolfssl/wolfcrypt/ecc.h"

#define STSAFE_A_PROD_CA_01_CERTIFICATE_PEM \
"-----BEGIN CERTIFICATE-----\n" \
"MIIBoDCCAUagAwIBAgIBATAKBggqhkjOPQQDAjBPMQswCQYDVQQGEwJOTDEeMBwGA1UECgwVU1RNaWNyb2VsZ\n" \
"WN0cm9uaWNzIG52MSAwHgYDVQQDDBdTVE0gU1RTQUZFLUEgUFJPRCBDQSAwMTAeFw0xODA3MjcwMDAwMDBaFw\n" \
"00ODA3MjcwMDAwMDBaME8xCzAJBgNVBAYTAk5MMR4wHAYDVQQKDBVTVE1pY3JvZWxlY3Ryb25pY3MgbnYxIDA\n" \
"eBgNVBAMMF1NUTSBTVFNBRkUtQSBQUk9EIENBIDAxMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEghlPJsyj\n" \
"bg6CGVzmZljsZKRmki9YyeZLXeGinn85hj0EJpLkyKx5+W0v7VJ3TVKBlTnyHz7NGTj4PXCu4JzNjaMTMBEwD\n" \
"wYDVR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAgNIADBFAiBu5UMyR6xyNPydF1qlHoMnaQGt7B8AXjcfQHNN44\n" \
"zFLgIhALHZUWqtmj6G0iuOOzvQFG+rubki8EUmNP6Sf/XWNs2Q\n" \
"-----END CERTIFICATE-----\n"

#define PEM_CERT_MAX_SIZE 1024U

#define STSAFE_MAX_KEY_LEN ((uint32_t)48) /* for up to 384-bit keys */
#define STSAFE_MAX_PUBKEY_RAW_LEN ((uint32_t)STSAFE_MAX_KEY_LEN * 2) /* x/y */
#define STSAFE_MAX_SIG_LEN ((uint32_t)STSAFE_MAX_KEY_LEN * 2) /* r/s */

static uint8_t cert_read_buffer[STSAFEA_MAX_CERTIFICATE_SIZE];

static uint8_t cert_chain_buffer[PEM_CERT_MAX_SIZE + sizeof(STSAFE_A_PROD_CA_01_CERTIFICATE_PEM)];

bool stsafea_init(StSafeA_Handle_t *stsafea_handle,
		uint8_t *a_rx_tx_stsafea_data)
{
	StSafeA_ResponseCode_t init_status = STSAFEA_UNEXPECTED_ERROR;
	init_status = StSafeA_Init(stsafea_handle, a_rx_tx_stsafea_data);
	if (init_status == STSAFEA_OK)
	{
		printf("\r\nSTSAFEA-A110 initialized successfully\r\n");
	}
	else
	{
		printf("\r\nSTSAFEA-A110 NOT initialized. error code: %d\r\n",
				init_status);
		return false;
	}

	const char *test_data = "STSAFEA TEST ECHO DATA";
	uint16_t test_data_size = strlen(test_data);

	uint8_t output_data[50];
	memset(&output_data, 0, 50);
	StSafeA_LVBuffer_t outputBuffer =
	{ .Data = output_data, .Length = 50 };

	StSafeA_ResponseCode_t echo_status = STSAFEA_UNEXPECTED_ERROR;
	echo_status = StSafeA_Echo(stsafea_handle, (uint8_t*) test_data,
			test_data_size, &outputBuffer, STSAFEA_MAC_NONE);

	printf("Sending stsafea test echo data: '%s'\r\n", test_data);
	if (echo_status == STSAFEA_OK)
	{
		printf("Received data: '%s'\r\n", output_data);
	}
	else
	{
		printf("\r\nSTSAFEA-A110 Echo test failed. Error code: %d\r\n",
				echo_status);
		return false;
	}

	return true;
}

bool stsafea_load_client_cert(StSafeA_Handle_t *stsafea_handle,
		NetworkCredentials_t *NetworkCredentials)
{
	printf("Reading leaf stsafe-a cert from zone 0....\r\n");

	StSafeA_LVBuffer_t sts_cert_size_read;
	uint8_t data_sts_cert_size_read[STSAFEA_NUMBER_OF_BYTES_TO_GET_CERTIFICATE_SIZE];
	sts_cert_size_read.Length = STSAFEA_NUMBER_OF_BYTES_TO_GET_CERTIFICATE_SIZE;
	sts_cert_size_read.Data = data_sts_cert_size_read;

	StSafeA_ResponseCode_t readCertSizeStatus = STSAFEA_UNEXPECTED_ERROR;
	readCertSizeStatus = StSafeA_Read(stsafea_handle, 0, 0, STSAFEA_AC_ALWAYS,
			0, 0, STSAFEA_NUMBER_OF_BYTES_TO_GET_CERTIFICATE_SIZE,
			STSAFEA_NUMBER_OF_BYTES_TO_GET_CERTIFICATE_SIZE,
			&sts_cert_size_read,
			STSAFEA_MAC_NONE);

	if (readCertSizeStatus != STSAFEA_OK)
	{
		printf("Read cert size failed. Error code: %d\r\n", readCertSizeStatus);
		return false;
	}

	uint16_t CertificateSize = 0;
	switch (sts_cert_size_read.Data[1])
	{
	case 0x81U:
		CertificateSize = (uint16_t) sts_cert_size_read.Data[2] + 3U;
		break;

	case 0x82U:
		CertificateSize = (((uint16_t) sts_cert_size_read.Data[2]) << 8)
				+ sts_cert_size_read.Data[3] + 4U;
		break;

	default:
		if (sts_cert_size_read.Data[1] < 0x81U)
		{
			CertificateSize = sts_cert_size_read.Data[1];
		}
		break;
	}

	if (CertificateSize == 0)
	{
		printf("Could not retrieve certificate size\r\n");
		return false;
	}

	printf("Got certificate size: %u\r\n", CertificateSize);

	StSafeA_LVBuffer_t sts_cert_read;
	sts_cert_read.Length = CertificateSize;
	sts_cert_read.Data = (uint8_t*) cert_read_buffer;

	StSafeA_ResponseCode_t readCertStatus = STSAFEA_UNEXPECTED_ERROR;
	readCertStatus = StSafeA_Read(stsafea_handle, 0, 0, STSAFEA_AC_ALWAYS, 0, 0,
			CertificateSize, CertificateSize, &sts_cert_read,
			STSAFEA_MAC_NONE);

	if (readCertStatus != STSAFEA_OK)
	{
		printf("Read cert failed. Error code: %d\r\n", readCertSizeStatus);
		return false;
	}

	uint8_t pemCert[PEM_CERT_MAX_SIZE];
	memset(pemCert, 0, PEM_CERT_MAX_SIZE);

	int convert_result_size = wc_DerToPem(cert_read_buffer, CertificateSize, pemCert,
	PEM_CERT_MAX_SIZE, CERT_TYPE);

	if (convert_result_size < 0)
	{
		printf(
				"Could not convert cert to PEM format. Error: %d\r\n",
				convert_result_size);
		return false;
	}

	printf("\r\nSTSAFE-A110 Leaf Certificate:\r\n");
	// Just add '\r' on new lines
	for (int i = 0; i < convert_result_size; ++i)
	{
		if (pemCert[i] == '\n')
		{
			putchar('\r');
		}
		putchar(pemCert[i]);
	}
	printf("\r\n");

	// concatenate the stsafe-a built-in certificate with the st root certificate
	memset(cert_chain_buffer, 0, sizeof(cert_chain_buffer));
	memcpy(cert_chain_buffer, pemCert, convert_result_size);
	memcpy(&cert_chain_buffer[convert_result_size], STSAFE_A_PROD_CA_01_CERTIFICATE_PEM, sizeof(STSAFE_A_PROD_CA_01_CERTIFICATE_PEM));
	size_t chainSize = convert_result_size + sizeof(STSAFE_A_PROD_CA_01_CERTIFICATE_PEM);

	NetworkCredentials->pClientCert = cert_chain_buffer;
	NetworkCredentials->clientCertSize = chainSize;

	return true;
}

int stsafe_VerifyPeerCertCb(WOLFSSL *ssl, const unsigned char *sig,
		unsigned int sigSz, const unsigned char *hash, unsigned int hashSz,
		const unsigned char *keyDer, unsigned int keySz, int *result, void *ctx)
{
	NetworkContext_t *netCtx = (NetworkContext_t*) ctx;
	StSafeA_Handle_t *stsafeHandle = &(netCtx->sslContext.stsafea_handle);

	int err = 0;

	uint8_t pubKeyX[STSAFE_MAX_PUBKEY_RAW_LEN / 2];
	uint8_t pubKeyY[STSAFE_MAX_PUBKEY_RAW_LEN / 2];
	uint8_t sigR[STSAFE_MAX_SIG_LEN / 2];
	uint8_t sigS[STSAFE_MAX_SIG_LEN / 2];

	memset(pubKeyX, 0, sizeof(pubKeyX));
	memset(pubKeyY, 0, sizeof(pubKeyY));
	memset(sigR, 0, sizeof(sigR));
	memset(sigS, 0, sizeof(sigS));

	unsigned int pubKeyXLen = sizeof(pubKeyX);
	unsigned int pubKeyYLen = sizeof(pubKeyY);
	unsigned int sigRLen = sizeof(sigR);
	unsigned int sigSLen = sizeof(sigS);

	ecc_key key;
	err = wc_ecc_init(&key);
	if (err != 0)
	{
		goto ret;
	}

	word32 inOutIdx = 0;
	err = wc_EccPublicKeyDecode(keyDer, &inOutIdx, &key, keySz);
	if (err != 0)
	{
		goto ret;
	}
	err = wc_ecc_export_public_raw(&key, pubKeyX, &pubKeyXLen, pubKeyY,
			&pubKeyYLen);
	if (err != 0)
	{
		goto ret;
	}

	err = wc_ecc_sig_to_rs(sig, sigSz, sigS, &sigSLen, sigR, &sigRLen);
	if (err != 0)
	{
		goto ret;
	}

	StSafeA_LVBuffer_t PubX =
	{ .Data = pubKeyX, .Length = (uint16_t) pubKeyXLen };
	StSafeA_LVBuffer_t PubY =
	{ .Data = pubKeyY, .Length = (uint16_t) pubKeyYLen };
	StSafeA_LVBuffer_t SignatureR =
	{ .Data = sigR, .Length = (uint16_t) sigRLen };
	StSafeA_LVBuffer_t SignatureS =
	{ .Data = sigS, .Length = (uint16_t) sigSLen };

	StSafeA_LVBuffer_t InDigest =
	{ .Data = (uint8_t*) hash, .Length = (uint16_t) hashSz };

	StSafeA_VerifySignatureBuffer_t verificationResultBuffer =
	{ 0 };

	StSafeA_ResponseCode_t verifySignatureResult = STSAFEA_UNEXPECTED_ERROR;
	verifySignatureResult = StSafeA_VerifyMessageSignature(stsafeHandle,
			STSAFEA_NIST_P_256, &PubX, &PubY, &SignatureR, &SignatureS,
			&InDigest, &verificationResultBuffer, STSAFEA_MAC_NONE);

	if (verifySignatureResult != STSAFEA_OK)
	{
		printf(
				"VerifyPeerCertCb: Got error from StSafeA_VerifyMessageSignature: %d\r\n",
				verifySignatureResult);
		err = -verifySignatureResult;
		*result = 0;
		goto ret;
	}

	err = 0;
	*result = 1;

	ret:

	wc_ecc_free(&key);
	return err;
}

int stsafe_SharedSecretCb(WOLFSSL *ssl, ecc_key *otherKey,
		unsigned char *pubKeyDer, unsigned int *pubKeySz, unsigned char *out,
		unsigned int *outlen, int side, void *ctx)
{
	if (side != WOLFSSL_CLIENT_END)
	{
		printf("SharedSecretCb: only supports WOLFSSL_CLIENT_END\r\n");
		return -1;
	}

	NetworkContext_t *netCtx = (NetworkContext_t*) ctx;
	StSafeA_Handle_t *stsafeHandle = &(netCtx->sslContext.stsafea_handle);

	int err = 0;

	/* ----- Generate a new key pair in the EPHEMERAL key slot ----- */

	const uint8_t keyslot = STSAFEA_KEY_SLOT_EPHEMERAL;
	const StSafeA_CurveId_t keyCurveId = STSAFEA_NIST_P_256;
	const uint8_t keyAuthFlags =
			(STSAFEA_PRVKEY_MODOPER_AUTHFLAG_CMD_RESP_SIGNEN |
			STSAFEA_PRVKEY_MODOPER_AUTHFLAG_MSG_DGST_SIGNEN |
			STSAFEA_PRVKEY_MODOPER_AUTHFLAG_KEY_ESTABLISHEN);

	uint8_t pubKeyX_data[STSAFE_MAX_KEY_LEN];
	uint8_t pubKeyY_data[STSAFE_MAX_KEY_LEN];
	memset(pubKeyX_data, 0, sizeof(pubKeyX_data));
	memset(pubKeyY_data, 0, sizeof(pubKeyY_data));
	StSafeA_LVBuffer_t PubKeyX =
	{ .Data = pubKeyX_data, .Length = sizeof(pubKeyX_data) };
	StSafeA_LVBuffer_t PubKeyY =
	{ .Data = pubKeyY_data, .Length = sizeof(pubKeyY_data) };

	uint8_t PointRepresentationId = 0;

	StSafeA_ResponseCode_t generateKeyPairResponse = STSAFEA_UNEXPECTED_ERROR;
	generateKeyPairResponse = StSafeA_GenerateKeyPair(stsafeHandle, keyslot,
			0xFFFF, STSAFEA_FLAG_FALSE, keyAuthFlags, keyCurveId,
			STSAFE_MAX_KEY_LEN, &PointRepresentationId, &PubKeyX, &PubKeyY,
			STSAFEA_MAC_NONE);
	if (generateKeyPairResponse != STSAFEA_OK)
	{
		printf("SharedSecretCb: Got error from StSafeA_GenerateKeyPair: %d\r\n",
				generateKeyPairResponse);
		err = -generateKeyPairResponse;
		return err;
	}

	/* ----- Parse otherKey ----- */

	uint8_t otherKeyX[STSAFE_MAX_KEY_LEN];
	uint8_t otherKeyY[STSAFE_MAX_KEY_LEN];
	unsigned int otherKeyXLen = sizeof(otherKeyX);
	unsigned int otherKeyYLen = sizeof(otherKeyY);
	memset(otherKeyX, 0, sizeof(otherKeyX));
	memset(otherKeyY, 0, sizeof(otherKeyY));

	err = wc_ecc_export_public_raw(otherKey, otherKeyX, &otherKeyXLen,
			otherKeyY, &otherKeyYLen);
	if (err != 0)
	{
		return 0;
	}

	StSafeA_LVBuffer_t OtherKeyX =
	{ .Data = otherKeyX, .Length = otherKeyXLen };
	StSafeA_LVBuffer_t OtherKeyY =
	{ .Data = otherKeyY, .Length = otherKeyYLen };

	/* ----- Generate Shared Secret via STSAFE-A ----- */

	uint8_t sharedSecret_buf[STSAFE_MAX_PUBKEY_RAW_LEN];
	memset(sharedSecret_buf, 0, sizeof(sharedSecret_buf));
	StSafeA_LVBuffer_t sharedSecret =
	{ .Data = sharedSecret_buf, .Length = sizeof(sharedSecret_buf) };
	StSafeA_SharedSecretBuffer_t OutSharedSecret =
	{ .Length = 0, .SharedKey = sharedSecret };

	StSafeA_ResponseCode_t sharedSecretResult = STSAFEA_UNEXPECTED_ERROR;
	sharedSecretResult = StSafeA_EstablishKey(stsafeHandle, keyslot, &OtherKeyX,
			&OtherKeyY, STSAFEA_XYRS_ECDSA_SHA256_LENGTH, &OutSharedSecret,
			STSAFEA_MAC_NONE, STSAFEA_ENCRYPTION_NONE);
	if (sharedSecretResult != STSAFEA_OK)
	{
		printf("SharedSecretCb: Got error from StSafeA_EstablishKey: %d\r\n",
				sharedSecretResult);
		err = -sharedSecretResult;
		return err;
	}

	/* ----- Parse and return public key ----- */

	ecc_key tmpKey;
	bool tmpKeyFreed = false;
	err = wc_ecc_init(&tmpKey);
	if (err == 0)
	{
		err = wc_ecc_import_unsigned(&tmpKey, pubKeyX_data, pubKeyY_data,
		NULL, ECC_SECP256R1);
		if (err == 0)
		{
			err = wc_ecc_export_x963(&tmpKey, pubKeyDer, pubKeySz);
		}
		wc_ecc_free(&tmpKey);
		tmpKeyFreed = true;
	}

	if (!tmpKeyFreed)
	{
		wc_ecc_free(&tmpKey);
		tmpKeyFreed = true;
	}
	if (err != 0)
	{
		return err;
	}

	/* ----- Return shared secret ----- */
	memcpy(out, OutSharedSecret.SharedKey.Data,
			OutSharedSecret.SharedKey.Length);
	*outlen = (unsigned int) (OutSharedSecret.SharedKey.Length);

	return 0;
}

int stsafe_SignCertificateCb(WOLFSSL *ssl, const unsigned char *in,
		unsigned int inSz, unsigned char *out, unsigned int *outSz,
		const unsigned char *key, unsigned int keySz, void *ctx)
{
	NetworkContext_t *netCtx = (NetworkContext_t*) ctx;
	StSafeA_Handle_t *stsafeHandle = &(netCtx->sslContext.stsafea_handle);

	uint8_t OutSignR_data[STSAFEA_XYRS_ECDSA_SHA256_LENGTH];
	memset(OutSignR_data, 0, sizeof(OutSignR_data));
	StSafeA_LVBuffer_t OutSignR =
	{ .Data = OutSignR_data, .Length = sizeof(OutSignR_data) };

	uint8_t OutSignS_data[STSAFEA_XYRS_ECDSA_SHA256_LENGTH];
	memset(OutSignS_data, 0, sizeof(OutSignS_data));
	StSafeA_LVBuffer_t OutSignS =
	{ .Data = OutSignS_data, .Length = sizeof(OutSignR_data) };

	StSafeA_ResponseCode_t generateSignatureResult = STSAFEA_UNEXPECTED_ERROR;
	generateSignatureResult = StSafeA_GenerateSignature(stsafeHandle,
	STSAFEA_KEY_SLOT_0, in, STSAFEA_SHA_256,
	STSAFEA_XYRS_ECDSA_SHA256_LENGTH, &OutSignR, &OutSignS,
	STSAFEA_MAC_NONE, STSAFEA_ENCRYPTION_NONE);

	if (generateSignatureResult != STSAFEA_OK)
	{
		printf(
				"SingCertificateCb: Got error from StSafeA_GenerateSignature: %d\r\n",
				generateSignatureResult);
		return -generateSignatureResult;
	}

	int err = wc_ecc_rs_raw_to_sig(OutSignR.Data, OutSignR.Length,
			OutSignS.Data, OutSignS.Length, out, outSz);
	if (err != 0)
	{
		printf(
				"SingCertificateCb: Error while converting RS to signature: %d\r\n",
				err);
	}

	return err;
}

void stsafe_SetupPkCallbacks(NetworkContext_t *NetworkContext)
{
	WOLFSSL_CTX *ctx = NetworkContext->sslContext.ctx;

	wolfSSL_CTX_SetEccVerifyCb(ctx, stsafe_VerifyPeerCertCb);
	wolfSSL_CTX_SetEccSharedSecretCb(ctx, stsafe_SharedSecretCb);
	wolfSSL_CTX_SetEccSignCb(ctx, stsafe_SignCertificateCb);
	wolfSSL_CTX_SetDevId(ctx, 0);
}

void stsafe_SetupPkCallbacksContext(NetworkContext_t *NetworkContext)
{
	WOLFSSL *ssl = NetworkContext->sslContext.ssl;

	wolfSSL_SetEccVerifyCtx(ssl, NetworkContext);
	wolfSSL_SetEccSharedSecretCtx(ssl, NetworkContext);
	wolfSSL_SetEccSignCtx(ssl, NetworkContext);
}
