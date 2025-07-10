/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: wifi.c
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V1.0
描述	   	: wifi驱动头文件
其他	   	: 无
日志	   	: 初版V1.0 2024/12/02 刘有为创建
***********************************************************************/
#include <string.h>
#include <netdb.h>
#include "wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip_addr.h"
#include "web_server.h"
#include "esp_mac.h"
#include "esp_netif_types.h"
#include "uart.h"
#include "utils.h"

#define WIFI_TAG "WIFI"
#define WIFI_INFO(fmt, ...) ESP_LOGI(WIFI_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define WIFI_DBG(fmt, ...) ESP_LOGW(WIFI_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define WIFI_ERR(fmt, ...) ESP_LOGE(WIFI_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)

#define ESP_WIFI_AP_LEN				0
#define ESP_WIFI_AP_PASSWORD		"12345678"				//配网ssid密码
#define ESP_WIFI_AP_AUTHMODE		WIFI_AUTH_WPA2_PSK		//ap连接方式
#define ESP_WIFI_AP_MAX_CONNECT		4						//最多STA连接数

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t g_wifi_event_group;
static int g_retry_num = 0;
WIFI_NETWORK_PARAM g_nw_params;

uint8_t g_client_conn_state = AP_WIFI_CLIENT_NOT_CONNECT;

/*************************************************
 * @description			:	设置用户连接状态
 * @param - state		:	连接状态
 * @return 				:	无
**************************************************/
void wifi_set_client_connect_status(uint8_t state)
{
	g_client_conn_state = state;
}

/*************************************************
 * @description			:	获取用户连接状态
 * @param - 			:	无
 * @return 				:	用户连接状态
**************************************************/
uint8_t wifi_get_client_connect_status(void)
{
	return g_client_conn_state;
}

/* 配网信息 */
wifi_config_t g_wifi_config = {
	.sta = {
		.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,
		.sae_pwe_h2e = WIFI_AUTH_WPA_WPA2_PSK,
	},
};

/*************************************************
 * @description			:	保存本机网络参数
 * @param - params		:	要保存的网络参数
 * @return 				:	无
**************************************************/
void wifi_record_network_params(ip_event_got_ip_t *params)
{
	if (NULL != params)
	{
		g_nw_params.ip = params->ip_info.ip.addr;
		g_nw_params.mask = params->ip_info.netmask.addr;
		g_nw_params.mask = params->ip_info.gw.addr;
	}
}

/*************************************************
 * @description			:	获取本机网络参数
 * @param - params		:	无
 * @return 				:	参数地址
**************************************************/
WIFI_NETWORK_PARAM *wifi_get_network_params(void)
{
	return &g_nw_params;
}

/*************************************************
 * @description			:	设置dns服务器
 * @param - netif		:	wifi事件句柄
 * @param - addr		:	dns服务器ipv4地址
 * @param - type		:	dns服务器类型
 * @return 				:	esp_err_t
**************************************************/
static esp_err_t set_dns_server(esp_netif_t *netif, uint32_t addr, esp_netif_dns_type_t type)
{
	esp_netif_dns_info_t dns;

	if (addr && (addr != IPADDR_NONE)) {
		dns.ip.u_addr.ip4.addr = addr;
		dns.ip.type = IPADDR_TYPE_V4;
		ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, type, &dns));
	}

	return ESP_OK;
}

/*************************************************
 * @description			:	设置网络参数
 * @param - netif		:	wifi事件句柄
 * @return 				:	无
**************************************************/
static void set_network_param(esp_netif_t *netif)
{
	esp_netif_ip_info_t ip;
	NET_CONFIG net_cfg = {0};
	NET_CONFIG net_cfg_cmp = {0};


	memset(&ip, 0 , sizeof(esp_netif_ip_info_t));

	/* 读取flash中的IP设置 */
	if (ESP_OK != nvs_read_net_cfg_from_flash(&net_cfg, "OK"))
	{
		/* 读取失败，使用默认地址 */
		WIFI_ERR("read from flash failed");
		ip.ip.addr = ipaddr_addr(DEFAULT_CLIENT_IP_ADDR);
		ip.netmask.addr = ipaddr_addr(DEFAULT_NETMASK_ADDR);
		ip.gw.addr = ipaddr_addr(DEFAULT_GATEWAY_ADDR);
		WIFI_INFO("set network config: %s, netmask: %s, gw: %s", DEFAULT_CLIENT_IP_ADDR, 
			DEFAULT_NETMASK_ADDR, DEFAULT_GATEWAY_ADDR);
	}
	else
	{
		/* 查看配网方式 */
		if (0 == memcmp(net_cfg_cmp.connect_type, net_cfg.connect_type, CONNECT_TYPE_STR_LEN))
		{
			/* 未设置配网方式， */
			WIFI_DBG("set network params to default");
		}
		else
		{
			if (0 == memcmp(net_cfg.connect_type, "auto", strlen("auto")))
			{
				/* 自动配网 */
				WIFI_DBG("get network params from AP");
				return;
			}
		}

		if (0 == memcmp(net_cfg_cmp.client_ip, net_cfg.client_ip, IP_STRING_LEN))
		{
			ip.ip.addr = ipaddr_addr(DEFAULT_CLIENT_IP_ADDR);
			WIFI_DBG("set ip to default: %s", DEFAULT_CLIENT_IP_ADDR);
		}
		else
		{
			ip.ip.addr = ipaddr_addr(net_cfg.client_ip);
			WIFI_INFO("set ip to: %s", net_cfg.client_ip);
		}

		if (0 == memcmp(net_cfg_cmp.netmask, net_cfg.netmask, IP_STRING_LEN))
		{
			ip.netmask.addr = ipaddr_addr(DEFAULT_NETMASK_ADDR);
			WIFI_DBG("set netmask to default: %s", DEFAULT_NETMASK_ADDR);
		}
		else
		{
			ip.netmask.addr = ipaddr_addr(net_cfg.netmask);
			WIFI_INFO("set netmask to: %s", net_cfg.netmask);
		}
		
		if (0 == memcmp(net_cfg_cmp.gateway, net_cfg.gateway, IP_STRING_LEN))
		{
			ip.gw.addr = ipaddr_addr(DEFAULT_GATEWAY_ADDR);
			WIFI_DBG("set gateway to default: %s", DEFAULT_GATEWAY_ADDR);
		}
		else
		{
			ip.gw.addr = ipaddr_addr(net_cfg.gateway);
			WIFI_INFO("set gateway to: %s", net_cfg.gateway);
		}
	}

	if (esp_netif_dhcpc_stop(netif) != ESP_OK) {
		WIFI_ERR("Failed to stop dhcp client");
		return;
	}

	if (esp_netif_set_ip_info(netif, &ip) != ESP_OK) {
		WIFI_ERR("Failed to set ip info");
		return;
	}

	ESP_ERROR_CHECK(set_dns_server(netif, ipaddr_addr(DEFAULT_MAIN_DNS_SERVER), ESP_NETIF_DNS_MAIN));
	ESP_ERROR_CHECK(set_dns_server(netif, ipaddr_addr(DEFAULT_BACKUP_DNS_SERVER), ESP_NETIF_DNS_BACKUP));
}

/*************************************************
 * @description			:	STA模式处理函数
 * @param 				:	无
 * @return 				:	无
**************************************************/
static void wifi_sta_event_handler(void* arg, esp_event_base_t event_base,
								int32_t event_id, void* event_data)
{
	uint8_t notify[ESP_STM_MSG_LEN] = {0};

	notify[ES_MSG_OFT_HEAD] = (UART_MSG_HEAD&0xFF00)>>8;
	notify[ES_MSG_OFT_HEAD + 1] = UART_MSG_HEAD&0x00FF;
	notify[ES_MSG_OFT_TYPE] = ESP_STM_SYS_STATUS;
	
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
		WIFI_INFO("connecting to ap...");
		esp_wifi_connect();
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
	{
		set_network_param(arg);
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
		g_retry_num += 1;
		notify[ES_MSG_OFT_DATA] = ESP32_WIFI_CONNECT_ERR;
		notify[ES_MSG_OFT_CRC] = crc_8bit_mask(notify, ES_MSG_OFT_CRC, UART_CRC_MASK);
		uart_data_transmit(notify, ESP_STM_MSG_LEN);
		WIFI_ERR("connect to ap fail, retry connecting %d", g_retry_num);
		esp_wifi_connect();
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		WIFI_DBG("ip:" IPSTR, IP2STR(&event->ip_info.ip));
		WIFI_DBG("mask:" IPSTR, IP2STR(&event->ip_info.netmask));
		WIFI_DBG("gw:" IPSTR, IP2STR(&event->ip_info.gw));
		wifi_record_network_params(event);
		g_retry_num = 0;
		xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

/*************************************************
 * @description			:	wifi站点模式初始化
 * @param 				:	无
 * @return 				:	无
**************************************************/
void wifi_init_sta(void)
{
	g_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());

	esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
	assert(sta_netif);

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
														ESP_EVENT_ANY_ID,
														&wifi_sta_event_handler,
														sta_netif,
														&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
														IP_EVENT_STA_GOT_IP,
														&wifi_sta_event_handler,
														sta_netif,
														&instance_got_ip));

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &g_wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start() );

	WIFI_INFO("station mode init finished...");

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	* number of re-tries (WIFI_FAIL_BIT). The bits are set by wifi_sta_event_handler() (see above) */
	EventBits_t bits = xEventGroupWaitBits(g_wifi_event_group,
			WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
			pdFALSE,
			pdFALSE,
			portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	* happened. */
	if (bits & WIFI_CONNECTED_BIT) {
		WIFI_INFO("connected to SSID:%s password:%s",
				(char *)&g_wifi_config.sta.ssid, (char *)&g_wifi_config.sta.password);
	} else if (bits & WIFI_FAIL_BIT) {
		WIFI_ERR("connect to SSID:%s, password:%s failed",
				(char *)&g_wifi_config.sta.ssid, (char *)&g_wifi_config.sta.password);
		//当前根据串口打印出来的信息，会自动重连5次。（还不清楚这五次是从哪里来的,如果可以的话可以增加到10-15次，测试过程中重新上电时会有几次重连失败的情况，5次不太保险）
		//找到了，这个retry是在event_handler函数中，当前已经将最大重试次数改为15次了
		//在当前的基础上，这里加上功能：连接failed之后，就直接清除nvs消息并重启就可以了
		//还需要加上一个延时以及LOG
		ESP_ERROR_CHECK(nvs_flash_erase());
		WIFI_INFO("erase flash and restart cpu...");
		vTaskDelay(2000 / portTICK_PERIOD_MS);
		esp_restart();
	} else {
		WIFI_ERR("unexpected event");
	}
#ifdef CONFIG_EXAMPLE_STATIC_DNS_RESOLVE_TEST
	struct addrinfo *address_info;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	int res = getaddrinfo(EXAMPLE_RESOLVE_DOMAIN, NULL, &hints, &address_info);
	if (res != 0 || address_info == NULL) {
		ESP_LOGE(TAG, "couldn't get hostname for :%s: "
					"getaddrinfo() returns %d, addrinfo=%p", EXAMPLE_RESOLVE_DOMAIN, res, address_info);
	} else {
		if (address_info->ai_family == AF_INET) {
			struct sockaddr_in *p = (struct sockaddr_in *)address_info->ai_addr;
			ESP_LOGI(TAG, "Resolved IPv4 address: %s", ipaddr_ntoa((const ip_addr_t*)&p->sin_addr.s_addr));
		}
#if CONFIG_LWIP_IPV6
		else if (address_info->ai_family == AF_INET6) {
			struct sockaddr_in6 *p = (struct sockaddr_in6 *)address_info->ai_addr;
			ESP_LOGI(TAG, "Resolved IPv6 address: %s", ip6addr_ntoa((const ip6_addr_t*)&p->sin6_addr));
		}
#endif
	}
#endif
	/* The event will not be processed after unregister */
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
	vEventGroupDelete(g_wifi_event_group);
}

/*************************************************
 * @description			:	AP模式处理函数
 * @param 				:	无
 * @return 				:	无
**************************************************/
static void wifi_ap_event_handler(void* arg, esp_event_base_t event_base,
									int32_t event_id, void* event_data)
{
	if (event_id == WIFI_EVENT_AP_STACONNECTED) {
		wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
		WIFI_INFO("sta: "MACSTR" join, aid:%d",
				MAC2STR(event->mac), event->aid);
		wifi_set_client_connect_status(AP_WIFI_CLIENT_CONNECTED);
	} else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
		wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
		WIFI_INFO("sta: "MACSTR" leave, aid: %d",
				MAC2STR(event->mac), event->aid);
		wifi_set_client_connect_status(AP_WIFI_CLIENT_NOT_CONNECT);
	}
}

/*************************************************
 * @description			:	AP模式初始化
 * @param 				:	无
 * @return 				:	无
**************************************************/
void wifi_init_ap(void)
{
	uint8_t mac[6] = {0};
	uint8_t ssid_str[32] = {0};

	ESP_ERROR_CHECK(esp_netif_init());						//初始化lwIP stack 协议栈
	ESP_ERROR_CHECK(esp_event_loop_create_default());		//初始化wifi事件
	esp_netif_create_default_wifi_ap();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	/* 获取mac以配置WIFI的SSID */
	if (ESP_OK != esp_read_mac(mac, ESP_MAC_WIFI_STA))
	{
		WIFI_ERR("read sta mac failed");
	}
	WIFI_INFO("mac: %02x-%02x-%02x-%02x-%02x-%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	snprintf((char *)ssid_str, 32, "BCI100_WIFI_%02X%02X%02X", mac[3], mac[4], mac[5]);
	ssid_str[31] = 0;

	//配置wifi
	wifi_config_t wifi_ap_config = {
		.ap = {
			.ssid_len = ESP_WIFI_AP_LEN,
			.password = ESP_WIFI_AP_PASSWORD,
			.authmode = ESP_WIFI_AP_AUTHMODE,
			.max_connection = ESP_WIFI_AP_MAX_CONNECT,
		},
	};
	memcpy(wifi_ap_config.ap.ssid, ssid_str, 32);

	//注册wifi事件的处理handle
	//向WIFI组件注册事件通知，通过回调函数event_handler实现相关功能
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
														ESP_EVENT_ANY_ID,
														&wifi_ap_event_handler,
														NULL,
														NULL));

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));       //设置ap模式
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config) );        //配置ap模式的参数
	ESP_ERROR_CHECK(esp_wifi_start());      //启动
}

/*************************************************
 * @description			:	记录ssid
 * @param - ssid		:	从flash读取的ssid
 * @return 				:	无
**************************************************/
void wifi_record_ssid(uint8_t *ssid)
{
	memcpy(g_wifi_config.sta.ssid, ssid, WIFI_SSID_STR_LEN);
}

/*************************************************
 * @description			:	记录ssid
 * @param - pw			:	从flash读取的password
 * @return 				:	无
**************************************************/
void wifi_record_password(uint8_t *pw)
{
	memcpy(g_wifi_config.sta.password, pw, WIFI_PW_STR_LEN);
}
