#ifndef INC_WIFI_UTILS_H_
#define INC_WIFI_UTILS_H_

#include <stdbool.h>
#include <stdint.h>

bool SendTcpData(const uint8_t *ipaddr, uint16_t port, const uint8_t *pdata, uint16_t Reqlen);

bool wifi_connect(void);

#endif /* INC_WIFI_UTILS_H_ */
