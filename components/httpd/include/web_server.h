/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: wifi.h
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V1.0
描述	   	: 配网页面头文件
其他	   	: 无
日志	   	: 初版V1.0 2024/12/03 刘有为创建
***********************************************************************/
#ifndef __WEB_SERVER_H__
#define __WEB_SERVER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define SERIAL_NUM_FORMAT 		"%01x%01x%01x%01x%01x%01x"
#define SERIAL_NUM_PARSE(s) 	(uint8_t)(((s)&0x00F00000)>>20), (uint8_t)(((s)&0x000F0000)>>16), (uint8_t)(((s)&0x0000F000)>>12), (uint8_t)(((s)&0x00000F00)>>8), (uint8_t)(((s)&0x000000F0)>>4), (uint8_t)(((s)&0x0000000F))

#define IP_STRING_LEN 16
#define PORT_STRING_LEN 6
#define SERIAL_STRING_LEN 7
#define PROTOCOL_STR_LEN 16
#define FREQUENCY_STR_LEN 16
#define CONNECT_TYPE_STR_LEN 16
#define FILTER_SWITCH_STR_LEN 4
typedef struct _NET_CONFIG {
	char host_ip[IP_STRING_LEN];
	char eeg_port[PORT_STRING_LEN];
	char fnirs_port[PORT_STRING_LEN];
	char netmask[IP_STRING_LEN];
	char gateway[IP_STRING_LEN];
	char client_ip[IP_STRING_LEN];
	char client_serial[SERIAL_STRING_LEN];
	char protocol[PROTOCOL_STR_LEN];
	char frequency[FREQUENCY_STR_LEN];
	char connect_type[CONNECT_TYPE_STR_LEN];
	char bp_filter[FILTER_SWITCH_STR_LEN];
	char hp_filter[FILTER_SWITCH_STR_LEN];
	char lp_filter[FILTER_SWITCH_STR_LEN];
	char notch_filter[FILTER_SWITCH_STR_LEN];
} NET_CONFIG;

void web_server_start(void);
unsigned char NVS_read_data_from_flash(char *ConfirmString,char *WIFI_Name,char *WIFI_Password);
void NVS_write_data_to_flash(char *WIFI_Name, char *WIFI_Password, char *ConfirmString);
int nvs_write_net_cfg_to_flash(NET_CONFIG *cfg, char *confirm);
int nvs_read_net_cfg_from_flash(NET_CONFIG *cfg, char *confirm);
uint32_t serial_num_str_parse(char *s);

#ifdef __cplusplus
}
#endif

#endif /* __WEB_SERVER_H__ */
