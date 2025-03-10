/**
  ******************************************************************************
  * @file    wifi.c
  * @author  MCD Application Team
  * @brief   WIFI interface file.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "wifi.h"
#include "es_wifi.h"

/* Private define ------------------------------------------------------------*/
#define MIN(a,b) ((a)<(b)?(a):(b))
/* Private variables ---------------------------------------------------------*/
static ES_WIFIObject_t EsWifiObj;

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  Initialize the WIFI core
  * @param  None
  * @retval Operation status
  */
WIFI_Status_t WIFI_Init(void)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if(ES_WIFI_RegisterBusIO(&EsWifiObj,
                           SPI_WIFI_Init,
                           SPI_WIFI_DeInit,
                           SPI_WIFI_Delay,
                           SPI_WIFI_SendData,
                           SPI_WIFI_ReceiveData) == ES_WIFI_STATUS_OK)
  {
    if(ES_WIFI_Init(&EsWifiObj) == ES_WIFI_STATUS_OK)
    {
      ret = WIFI_STATUS_OK;
    }
  }
  return ret;
}

/**
  * @brief  List a defined number of available access points
  * @param  APs : pointer to APs structure
  * @param  AP_MaxNbr : Max APs number to be listed
  * @retval Operation status
  */
WIFI_Status_t WIFI_ListAccessPoints(WIFI_APs_t *APs, uint8_t AP_MaxNbr)
{
  uint8_t APCount;
  WIFI_Status_t ret = WIFI_STATUS_ERROR;
  ES_WIFI_APs_t esWifiAPs;

  memset(&esWifiAPs, 0, sizeof(esWifiAPs));

  if (ES_WIFI_ListAccessPoints(&EsWifiObj, &esWifiAPs) == ES_WIFI_STATUS_OK)
  {
    if (esWifiAPs.nbr > 0)
    {
      APs->count = MIN(esWifiAPs.nbr, AP_MaxNbr);
      for(APCount = 0; APCount < APs->count; APCount++)
      {
        APs->ap[APCount].Ecn = (WIFI_Ecn_t)esWifiAPs.AP[APCount].Security;
        strncpy((char *)APs->ap[APCount].SSID, (char *)esWifiAPs.AP[APCount].SSID, sizeof(APs->ap[APCount].SSID) - 1);
        APs->ap[APCount].SSID[sizeof(APs->ap[APCount].SSID) - 1] = '\0';

        APs->ap[APCount].RSSI = esWifiAPs.AP[APCount].RSSI;
        memcpy(APs->ap[APCount].MAC, esWifiAPs.AP[APCount].MAC, 6);
        APs->ap[APCount].Channel = esWifiAPs.AP[APCount].Channel;
      }
    }
    ret = WIFI_STATUS_OK;
  }
  return ret;
}

/**
  * @brief  Join an Access Point
  * @param  SSID : SSID string
  * @param  Password : Password string
  * @param  ecn : Encryption type
  * @retval Operation status
  */
WIFI_Status_t WIFI_Connect(const char* SSID, const char* Password, WIFI_Ecn_t ecn)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if(ES_WIFI_Connect(&EsWifiObj, SSID, Password, (ES_WIFI_SecurityType_t) ecn) == ES_WIFI_STATUS_OK)
  {
    if(ES_WIFI_GetNetworkSettings(&EsWifiObj) == ES_WIFI_STATUS_OK)
    {
       ret = WIFI_STATUS_OK;
    }
  }
  return ret;
}

/**
  * @brief  This function retrieves the WiFi interface's MAC address.
  * @retval Operation Status.
  */
WIFI_Status_t WIFI_GetMAC_Address(uint8_t *mac, uint8_t MacLength)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if ((mac != NULL) && (0 < MacLength))
  {
    if(ES_WIFI_GetMACAddress(&EsWifiObj, mac, MacLength) == ES_WIFI_STATUS_OK)
    {
      ret = WIFI_STATUS_OK;
    }
  }
  return ret;
}

/**
  * @brief  This function retrieves the WiFi interface's IP address.
  * @retval Operation Status.
  */
WIFI_Status_t WIFI_GetIP_Address(uint8_t *ipaddr, uint8_t IpAddrLength)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if ((ipaddr != NULL) && (4 <= IpAddrLength))
  {
    if (ES_WIFI_IsConnected(&EsWifiObj) == 1)
    {
      memcpy(ipaddr, EsWifiObj.NetSettings.IP_Addr, 4);
      ret = WIFI_STATUS_OK;
    }
  }
  return ret;
}

/**
  * @brief  Disconnect from a network
  * @param  None
  * @retval Operation status
  */
WIFI_Status_t WIFI_Disconnect(void)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;
  if (ES_WIFI_Disconnect(&EsWifiObj) == ES_WIFI_STATUS_OK)
  {
    ret = WIFI_STATUS_OK;
  }

  return ret;
}

/**
  * @brief  Configure an Access Point
  * @param  ssid : SSID string
  * @param  pass : Password string
  * @param  ecn : Encryption type
  * @param  channel : channel number
  * @param  max_conn : Max allowed connections
  * @retval Operation status
  */
WIFI_Status_t WIFI_ConfigureAP(const uint8_t *ssid, const uint8_t *pass, WIFI_Ecn_t ecn, uint8_t channel,
                               uint8_t max_conn)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;
  ES_WIFI_Status_t ret_es_wifi = ES_WIFI_STATUS_ERROR;
  ES_WIFI_APConfig_t ApConfig;
  uint32_t saved_timeout;

  strncpy((char *)ApConfig.SSID, (char *)ssid, sizeof(ApConfig.SSID) - 1);
  ApConfig.SSID[sizeof(ApConfig.SSID) - 1] = '\0';

  strncpy((char *)ApConfig.Pass, (char *)pass, sizeof(ApConfig.Pass) - 1);
  ApConfig.Pass[sizeof(ApConfig.Pass) - 1] = '\0';

  ApConfig.Channel = channel;
  ApConfig.MaxConnections = WIFI_MAX_CONNECTED_STATIONS;
  ApConfig.Security = (ES_WIFI_SecurityType_t)ecn;

  saved_timeout = EsWifiObj.Timeout;

  EsWifiObj.Timeout = 0xC0000;

  ret_es_wifi = ES_WIFI_ActivateAP(&EsWifiObj, &ApConfig);

  EsWifiObj.Timeout = saved_timeout;

  if (ret_es_wifi == ES_WIFI_STATUS_OK)
  {
    if (ES_WIFI_GetNetworkSettings(&EsWifiObj) == ES_WIFI_STATUS_OK)
    {
      ret = WIFI_STATUS_OK;
    }
  }
  return ret;
}


/**
  * @brief  Handle the background events of the WiFi module
  * @param  setting :
  * @retval None
*/
WIFI_Status_t WIFI_HandleAPEvents(WIFI_APSettings_t *setting)
{
  WIFI_Status_t ret = WIFI_STATUS_OK;
  ES_WIFI_APState_t State;

  State= ES_WIFI_WaitAPStateChange(&EsWifiObj);

  switch (State)
  {
  case ES_WIFI_AP_ASSIGNED:
    memcpy(setting->IP_Addr, EsWifiObj.APSettings.IP_Addr, sizeof(setting->IP_Addr));
    memcpy(setting->MAC_Addr, EsWifiObj.APSettings.MAC_Addr, sizeof(setting->MAC_Addr));
    ret = WIFI_STATUS_ASSIGNED;
    break;

  case ES_WIFI_AP_JOINED:
    strncpy((char *)setting->SSID, (char *)EsWifiObj.APSettings.SSID, sizeof(setting->SSID) - 1);
    setting->SSID[sizeof(setting->SSID) - 1] = '\0';

    memcpy(setting->IP_Addr, EsWifiObj.APSettings.IP_Addr, sizeof(setting->IP_Addr));
    ret = WIFI_STATUS_JOINED;
    break;

  case ES_WIFI_AP_ERROR:
    ret = WIFI_STATUS_ERROR;
    break;

    case ES_WIFI_AP_NONE:
  default:
    break;
  }

  return ret;
}

/**
  * @brief  Ping an IP address in the network
  * @param  ipaddr : array of the IP address
  * @retval Operation status
  */
WIFI_Status_t WIFI_Ping(const uint8_t *ipaddr, uint16_t count, uint16_t interval_ms, int32_t result[])
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if(ES_WIFI_Ping(&EsWifiObj, ipaddr, count, interval_ms, result) == ES_WIFI_STATUS_OK)
  {
    ret = WIFI_STATUS_OK;
  }
  return ret;
}

/**
  * @brief  Get IP address from URL using DNS
  * @param  location : Host URL
  * @param  ipaddr : array of the IP address
  * @param  IpAddrLength : The length of the IP address
  * @retval Operation status
  */
WIFI_Status_t WIFI_GetHostAddress(const char *location, uint8_t *ipaddr, uint8_t IpAddrLength)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if ((ipaddr != NULL) && (4 <= IpAddrLength))
  {
    if (ES_WIFI_DNS_LookUp(&EsWifiObj, location, ipaddr, IpAddrLength) == ES_WIFI_STATUS_OK)
    {
      return WIFI_STATUS_OK;
    }
  }
  return ret;
}

/**
  * @brief  Configure and start a client connection
  * @param  socket : socket
  * @param  type : Connection type TCP/UDP
  * @param  name : name of the connection
  * @param  ipaddr : IP address of the remote host
  * @param  port : Remote port
  * @param  local_port : Local port
  * @retval Operation status
  */
WIFI_Status_t WIFI_OpenClientConnection(uint32_t socket, WIFI_Protocol_t type, const char *name,
                                        const uint8_t *ipaddr, uint16_t port, uint16_t local_port)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;
  ES_WIFI_Conn_t conn;

  conn.Number = (uint8_t)socket;
  conn.RemotePort = port;
  conn.LocalPort = local_port;
  conn.Type = (type == WIFI_TCP_PROTOCOL)? ES_WIFI_TCP_CONNECTION : ES_WIFI_UDP_CONNECTION;
  conn.RemoteIP[0] = ipaddr[0];
  conn.RemoteIP[1] = ipaddr[1];
  conn.RemoteIP[2] = ipaddr[2];
  conn.RemoteIP[3] = ipaddr[3];

  if(ES_WIFI_StartClientConnection(&EsWifiObj, &conn)== ES_WIFI_STATUS_OK)
  {
    ret = WIFI_STATUS_OK;
  }
  return ret;
}

/**
  * @brief  Close client connection
  * @param  socket : socket
  * @retval Operation status
  */
WIFI_Status_t WIFI_CloseClientConnection(uint32_t socket)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;
  ES_WIFI_Conn_t conn;
  conn.Number = (uint8_t)socket;

  if(ES_WIFI_StopClientConnection(&EsWifiObj, &conn)== ES_WIFI_STATUS_OK)
  {
    ret = WIFI_STATUS_OK;
  }
  return ret;
}

/**
  * @brief  Configure and start a Server
  * @param  socket : socket
  * @param  protocol : Connection type TCP/UDP
  * @param  backlog : Set the number of listen backlogs (TCP connection requests) that can be queued.
  * @param  name : name of the connection
  * @param  port : Remote port
  * @retval Operation status
  */
WIFI_Status_t WIFI_StartServer(uint32_t socket, WIFI_Protocol_t protocol, uint16_t backlog ,const char *name,
                               uint16_t port)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;
  ES_WIFI_Conn_t conn;

  conn.Number = (uint8_t)socket;
  conn.LocalPort = port;
  conn.Type = (protocol == WIFI_TCP_PROTOCOL)? ES_WIFI_TCP_CONNECTION : ES_WIFI_UDP_CONNECTION;
  conn.Backlog = backlog;

  if(ES_WIFI_StartServerSingleConn(&EsWifiObj, &conn)== ES_WIFI_STATUS_OK)
  {
    ret = WIFI_STATUS_OK;
  }
  return ret;
}

/**
  * @brief  Wait for a client connection to the server
  * @param  socket : socket
  * @retval Operation status
  */
WIFI_Status_t WIFI_WaitServerConnection(uint32_t socket,uint32_t Timeout,
                                        uint8_t *RemoteIp, uint8_t RemoteIpAddrLength, uint16_t *RemotePort)
{
  ES_WIFI_Conn_t conn;
  ES_WIFI_Status_t ret;

  conn.Number = (uint8_t)socket;

  ret = ES_WIFI_WaitServerConnection(&EsWifiObj,Timeout,&conn);

  if (ES_WIFI_STATUS_OK == ret)
  {
    if (RemotePort)
    {
      *RemotePort = conn.RemotePort;
    }
    if ((RemoteIp != NULL) && (4 <= RemoteIpAddrLength))
    {
      memcpy(RemoteIp, conn.RemoteIP, 4);
    }
    return  WIFI_STATUS_OK;
  }

  if (ES_WIFI_STATUS_TIMEOUT == ret)
  {
    if (RemotePort)
    {
      *RemotePort = 0;
    }
    if ((RemoteIp != NULL) && (4 <= RemoteIpAddrLength))
    {
      memset(RemoteIp, 0, 4);
    }
    return  WIFI_STATUS_TIMEOUT;
  }

  return WIFI_STATUS_ERROR;
}

/**
  * @brief  Close current connection from a client  to the server
  * @param  socket : socket
  * @retval Operation status
  */
WIFI_Status_t WIFI_CloseServerConnection(uint32_t socket)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if (ES_WIFI_STATUS_OK == ES_WIFI_CloseServerConnection(&EsWifiObj, (uint8_t)socket))
  {
    ret = WIFI_STATUS_OK;
  }
  return ret;
}

/**
  * @brief  Stop a server
  * @param  socket : socket
  * @retval Operation status
  */
WIFI_Status_t WIFI_StopServer(uint32_t socket)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if(ES_WIFI_StopServerSingleConn(&EsWifiObj, (uint8_t)socket)== ES_WIFI_STATUS_OK)
  {
    ret = WIFI_STATUS_OK;
  }
  return ret;
}

/**
  * @brief  Send Data on a socket
  * @param  socket : socket
  * @param  pdata : pointer to data to be sent
  * @param  Reqlen : length of data to be sent
  * @param  SentDatalen : (OUT) length actually sent
  * @param  Timeout : Socket write timeout (ms)
  * @retval Operation status
  */
WIFI_Status_t WIFI_SendData(uint32_t socket, const uint8_t *pdata, uint16_t Reqlen, uint16_t *SentDatalen,
                            uint32_t Timeout)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

    if (ES_WIFI_SendData(&EsWifiObj, (uint8_t)socket, pdata, Reqlen, SentDatalen, Timeout) == ES_WIFI_STATUS_OK)
    {
      ret = WIFI_STATUS_OK;
    }

  return ret;
}

/**
  * @brief  Send Data on a socket
  * @param  socket : socket
  * @param  pdata : pointer to data to be sent
  * @param  Reqlen : length of data to be sent
  * @param  SentDatalen : (OUT) length actually sent
  * @param  Timeout : Socket write timeout (ms)
  * @param  ipaddr : (IN) 4-byte array containing the IP address of the remote host
  * @param  port : (IN) port number of the remote host
  * @retval Operation status
  */
WIFI_Status_t WIFI_SendDataTo(uint32_t socket, const uint8_t *pdata, uint16_t Reqlen, uint16_t *SentDatalen,
                              uint32_t Timeout,
                              const uint8_t *ipaddr, uint16_t port)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if (ES_WIFI_SendDataTo(&EsWifiObj, socket, pdata, Reqlen, SentDatalen, Timeout,
                         ipaddr, port) == ES_WIFI_STATUS_OK)
  {
    ret = WIFI_STATUS_OK;
  }

  return ret;
}

/**
  * @brief  Receive Data from a socket
  * @param  socket : socket
  * @param  pdata : pointer to Rx buffer
  * @param  Reqlen : maximum length of the data to be received
  * @param  RcvDatalen : (OUT) length of the data actually received
  * @param  Timeout : Socket read timeout (ms)
  * @retval Operation status
  */
WIFI_Status_t WIFI_ReceiveData(uint32_t socket, uint8_t *pdata, uint16_t Reqlen, uint16_t *RcvDatalen,
                               uint32_t Timeout)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if(ES_WIFI_ReceiveData(&EsWifiObj, socket, pdata, Reqlen, RcvDatalen, Timeout) == ES_WIFI_STATUS_OK)
  {
    ret = WIFI_STATUS_OK;
  }
  return ret;
}

/**
  * @brief  Receive Data from a socket
  * @param  socket : socket
  * @param  pdata : pointer to Rx buffer
  * @param  Reqlen : maximum length of the data to be received
  * @param  RcvDatalen : (OUT) length of the data actually received
  * @param  Timeout : Socket read timeout (ms)
  * @param  ipaddr : (OUT) 4-byte array containing the IP address of the remote host
  * @param  IpAddrLength : The length of the IPv4 address given as parameter
  * @param  port : (OUT) port number of the remote host
  * @retval Operation status
  */
WIFI_Status_t WIFI_ReceiveDataFrom(uint32_t socket, uint8_t *pdata, uint16_t Reqlen, uint16_t *RcvDatalen,
                                   uint32_t Timeout, uint8_t *ipaddr, uint8_t IpAddrLength, uint16_t *port)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if ((ipaddr != NULL) && (4 <= IpAddrLength))
  {
    if(ES_WIFI_ReceiveDataFrom(&EsWifiObj, socket, pdata, Reqlen, RcvDatalen, Timeout,
                               ipaddr, IpAddrLength, port) == ES_WIFI_STATUS_OK)
    {
      ret = WIFI_STATUS_OK;
    }
  }
  return ret;
}

/**
  * @brief  Customize module data
  * @param  name : MFC name
  * @param  Mac :  Mac Address
  * @retval Operation status
  */
WIFI_Status_t WIFI_SetOEMProperties(const char *name, const uint8_t *Mac)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if(ES_WIFI_SetProductName(&EsWifiObj, name) == ES_WIFI_STATUS_OK)
  {
    if(ES_WIFI_SetMACAddress(&EsWifiObj, Mac) == ES_WIFI_STATUS_OK)
    {
      ret = WIFI_STATUS_OK;
    }
  }
  return ret;
}

/**
  * @brief  Reset the WIFI module
  * @retval Operation status
  */
WIFI_Status_t WIFI_ResetModule(void)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if(ES_WIFI_ResetModule(&EsWifiObj) == ES_WIFI_STATUS_OK)
  {
      ret = WIFI_STATUS_OK;
  }
  return ret;
}

/**
  * @brief  Restore module default configuration
  * @retval Operation status
  */
WIFI_Status_t WIFI_SetModuleDefault(void)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if(ES_WIFI_ResetToFactoryDefault(&EsWifiObj) == ES_WIFI_STATUS_OK)
  {
      ret = WIFI_STATUS_OK;
  }
  return ret;
}


/**
  * @brief  Update module firmware
  * @param  location : Binary Location IP address
  * @retval Operation status
  */
WIFI_Status_t WIFI_ModuleFirmwareUpdate(const char *location)
{
  return WIFI_STATUS_NOT_SUPPORTED;
}

/**
  * @brief  Return the module firmware revision
  * @param  rev : The revision string to fill
  * @param  RevLen : The length of the revision given as parameter
  * @retval Operation status
  */
WIFI_Status_t WIFI_GetModuleFwRevision(char *rev, uint8_t RevLength)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if ((rev != NULL) && (0 < RevLength) && (EsWifiObj.FW_Rev[0] != 0))
  {
    strncpy(rev, (char *)EsWifiObj.FW_Rev, RevLength - 1);
    *(rev + RevLength - 1) = '\0';
    ret = WIFI_STATUS_OK;
  }
  return ret;
}

/**
  * @brief  Return the module identifier as a string
  * @param  Id : The module identifier string to fill
  * @param  IdLen : The length of the module identifier given as parameter
  * @retval Operation status
  */
WIFI_Status_t WIFI_GetModuleID(char *Id, uint8_t IdLen)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if ((Id != NULL) && (IdLen > 1) && (EsWifiObj.Product_ID[0] != 0))
  {
    strncpy(Id, (char *)EsWifiObj.Product_ID, IdLen - 1);
    *(Id + IdLen - 1) = '\0';
    ret = WIFI_STATUS_OK;
  }

  return ret;
}

/**
  * @brief  Return the module name as a string
  * @param  ModuleName : The module name string to fill
  * @param  ModuleNameLen : The length of the module name given as parameter
  * @retval Operation status
  */
WIFI_Status_t WIFI_GetModuleName(char *ModuleName, uint8_t ModuleNameLen)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if ((EsWifiObj.Product_Name[0] != 0) && (ModuleName != NULL) && (ModuleNameLen > 0))
  {
    strncpy(ModuleName, (char *)EsWifiObj.Product_Name, ModuleNameLen - 1);
    *(EsWifiObj.Product_Name + ModuleNameLen - 1) = '\0';
    ret = WIFI_STATUS_OK;
  }

  return ret;
}
