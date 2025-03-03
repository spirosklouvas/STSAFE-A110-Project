### Description

This project contains an application demonstrating the use of the
[STSAFE-A110](https://www.st.com/en/secure-mcus/stsafe-a110.html)
secure element by STMicroelectronics, combined with the [WolfSSL](https://www.wolfssl.com/) and
[CoreMQTT](https://www.freertos.org/Documentation/03-Libraries/03-FreeRTOS-core/02-coreMQTT/00-coreMQTT) libraries,
to securely authenticate a device to a remote MQTT broker and to encrypt the communication
channel using TLS. The [ThingsBoard](https://thingsboard.io/) IoT platform was used as the MQTT broker for
the demonstration, though any MQTT broker supporting Mutual Authentication using
TLS could work. The application was developed on the
[B-L4S5I-IOT01A](https://www.st.com/en/evaluation-tools/b-l4s5i-iot01a.html) board by
STMicroelectronics, and uses the included Wi-Fi module to connect to the
Internet. The code can be built and extended using
[STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html).

### Configuration

The following constants need to be set for the application to work:

| File Location | Constant | Description |
| --- | --- | --- |
| `Core/Src/wifi_utils.c`      | `WIFI_SSID` | The SSID (name) of the Wi-Fi network to be used |
| `Core/Src/wifi_utils.c`      | `WIFI_PASS` | The password of the Wi-Fi network to be used |
| `Core/Src/task_mqtt_agent.c` | `MQTT_BROKER_ENDPOINT` | The IP address of the MQTT broker (as a text string) |
| `Core/Src/task_mqtt_agent.c` | `MQTT_BROKER_ENDPOINT_IP` | The IP address of the MQTT broker (as an integer array) |
| `Core/Src/task_mqtt_agent.c` | `MQTT_BROKER_TLS_PORT` | The TCP port used for MQTT over TLS connection by the MQTT broker |
| `Core/Src/task_mqtt_agent.c` | `MQTT_BROKER_TLS_HOSTNAME` | The hostname of the MQTT broker as specified in its TLS certificate |

Additionally, a TLS certificate that will be used by the device to verify the
MQTT broker's TLS certificate must be provided. This can be done by creating the
`Core/Inc/TESTING_KEYS.h` file with the following format (and filling in the
contents of the certificate):

```C
#ifndef INC_TESTING_KEYS_H_
#define INC_TESTING_KEYS_H_

#define ROOT_CA_PEM \
"-----BEGIN CERTIFICATE-----\n" \
"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n" \
"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n" \
"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n" \
"xxxxxxxxxxx==\n" \
"-----END CERTIFICATE-----\n"

#endif /* INC_TESTING_KEYS_H_ */
```

### Additional Operating Modes

The application has 2 compilation options that can modify the methods of
communication and are useful for testing and debugging. By default both are set
to `1`.

The `TASK_MQTT_AGENT_USE_TLS` constant in `Core/Inc/task_mqtt_agent.h` controls
whether the communication with the MQTT broker is encrypted with TLS. If it is
set to `0`, the WolfSSL and STSAFE-A110 code is disabled, and the MQTT messages
are send without any encryption. In this case, the following 2 constants must be
set:

| File Location | Constant | Description |
| --- | --- | --- |
| `Core/Src/task_mqtt_agent.c` | `MQTT_BROKER_PORT` | The TCP port used for plain-text MQTT connections by the MQTT broker |
| `Core/Src/task_mqtt_agent.c` | `TEST_USER_NAME` | The token assigned to the device in ThingsBoard, used for [Access Token based authentication](https://thingsboard.io/docs/user-guide/access-token/) |

The `TLS_TRANSPORT_USE_STSAFEA` constant in `Core/Src/transport_interface_tls.c`
controls whether the STSAFE-A110 chip is used for TLS. If it is set to `0`, the
WolfSSL callbacks that utilize the chip are not registered, and the certificates
and keys contained in the chip are not used. Instead, they have to be provided
as constants in the `TESTING_KEYS.h` file, in the same manner as the
`ROOT_CA_PEM` constant mentioned above. Specifically, the device's private key
needs to be specified as the `CLIENT_PRIVATE_KEY_PEM` constant, and the device's
certificate as the `CLIENT_CERTIFICATE_PEM` constant.

### License

Except where mentioned otherwise, this project is available under the GPLv2 license.
