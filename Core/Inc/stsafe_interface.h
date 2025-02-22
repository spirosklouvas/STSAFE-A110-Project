#ifndef INC_STSAFE_INTERFACE_H_
#define INC_STSAFE_INTERFACE_H_

#include "main.h"

#include "safea1_conf.h"
#include "stsafea_core.h"
#include "stsafea_service.h"

#include "transport_interface.h"

bool stsafea_init(StSafeA_Handle_t *stsafea_handle,
		uint8_t *a_rx_tx_stsafea_data);

bool stsafea_load_client_cert(StSafeA_Handle_t *stsafea_handle,
		NetworkCredentials_t *NetworkCredentials);

int stsafe_VerifyPeerCertCb(WOLFSSL *ssl, const unsigned char *sig,
		unsigned int sigSz, const unsigned char *hash, unsigned int hashSz,
		const unsigned char *keyDer, unsigned int keySz, int *result, void *ctx);

int stsafe_SharedSecretCb(WOLFSSL *ssl, ecc_key *otherKey,
		unsigned char *pubKeyDer, unsigned int *pubKeySz, unsigned char *out,
		unsigned int *outlen, int side, void *ctx);

int stsafe_SignCertificateCb(WOLFSSL *ssl, const unsigned char *in,
		unsigned int inSz, unsigned char *out, unsigned int *outSz,
		const unsigned char *key, unsigned int keySz, void *ctx);

void stsafe_SetupPkCallbacks(NetworkContext_t *NetworkContext);
void stsafe_SetupPkCallbacksContext(NetworkContext_t *NetworkContext);

#endif /* INC_STSAFE_INTERFACE_H_ */
