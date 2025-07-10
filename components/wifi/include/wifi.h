/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: wifi.h
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V1.0
描述	   	: wifi驱动头文件
其他	   	: 无
日志	   	: 初版V1.0 2024/12/02 刘有为创建
***********************************************************************/
#ifndef __WIFI_H__
#define __WIFI_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

enum {
	AP_WIFI_CLIENT_NOT_CONNECT = 0,
	AP_WIFI_CLIENT_CONNECTED = 1,
};

#define WIFI_CONNECT_MAX_RETRY_TIME 			5
#define DEFAULT_CLIENT_IP_ADDR 					"192.168.1.254"
#define DEFAULT_NETMASK_ADDR 					"255.255.255.0"
#define DEFAULT_GATEWAY_ADDR 					"192.168.1.1"
#define DEFAULT_MAIN_DNS_SERVER 				"192.168.1.1"
#define DEFAULT_BACKUP_DNS_SERVER 				"0.0.0.0"
#ifdef CONFIG_EXAMPLE_STATIC_DNS_RESOLVE_TEST
#define EXAMPLE_RESOLVE_DOMAIN        CONFIG_EXAMPLE_STATIC_RESOLVE_DOMAIN
#endif

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries
 */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define WIFI_SSID_STR_LEN 	32
#define WIFI_PW_STR_LEN 	64

typedef struct _WIFI_NETWORK_PARAM
{
	uint32_t ip;
	uint32_t mask;
	uint32_t gw;
} WIFI_NETWORK_PARAM;

/*************************************************
 * @description			:	wifi站点模式初始化
 * @param 				:	无
 * @return 				:	无
**************************************************/
void wifi_init_sta(void);

/*************************************************
 * @description			:	AP模式初始化
 * @param 				:	无
 * @return 				:	无
**************************************************/
void wifi_init_ap(void);

/*************************************************
 * @description			:	记录ssid
 * @param - ssid		:	从flash读取的ssid
 * @return 				:	无
**************************************************/
void wifi_record_ssid(uint8_t *ssid);

/*************************************************
 * @description			:	记录ssid
 * @param - pw			:	从flash读取的password
 * @return 				:	无
**************************************************/
void wifi_record_password(uint8_t *pw);

/*************************************************
 * @description			:	获取本机网络参数
 * @param - params		:	无
 * @return 				:	参数地址
**************************************************/
WIFI_NETWORK_PARAM *wifi_get_network_params(void);

/*************************************************
 * @description			:	设置用户连接状态
 * @param - state		:	连接状态
 * @return 				:	无
**************************************************/
void wifi_set_client_connect_status(uint8_t state);

/*************************************************
 * @description			:	获取用户连接状态
 * @param - 			:	无
 * @return 				:	用户连接状态
**************************************************/
uint8_t wifi_get_client_connect_status(void);

#ifdef __cplusplus
}
#endif

#endif /* __WIFI_H__ */
