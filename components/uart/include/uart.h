/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: uart.h
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V1.0
描述	   	: 串口驱动头文件
其他	   	: 无
日志	   	: 初版V1.0 2025/1/24 刘有为创建
***********************************************************************/
#ifndef __UART_H__
#define __UART_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#define UART_TXD_PIN 				(GPIO_NUM_5)
#define UART_RXD_PIN 				(GPIO_NUM_4)
#define UART_PORT 					(UART_NUM_1)
#define UART_BAUDRATE 				1000000
#define UART_DATA_BITS 				UART_DATA_8_BITS
#define UART_STOP_BITS 				UART_STOP_BITS_1
#define UART_HW_FLOW_CTRL 			UART_HW_FLOWCTRL_DISABLE
#define UART_PARITY 				UART_PARITY_DISABLE
#define UART_CLK_SRC 				UART_SCLK_DEFAULT
#define UART_RX_BUF_SIZE 			(2048)
#define UART_TX_BUF_SIZE 			(1024)
#define UART_RECV_QUEUE_SIZE 		(4096 - 1)
#define UART_CRC_MASK 				0xd5

typedef enum{
	UART_FAIL = -1,
	UART_OK = 0,
} UART_ERR_T;

#define QUEUE_MUTEX_TIMEOUT 		(1000)
#define UART_MSG_HEAD 				0x2325
#define UART_MSG_TAIL 				0x0D0A
#define UART_MSG_MAX_VALUE_LEN 		128
#define UART_MSG_MIN_LEN 			7
#define UART_MSG_CRC_TAIL_LEN 		3

typedef enum{
	UART_MSG_OFT_HEAD = 0,
	UART_MSG_OFT_TYPE = 2,
	UART_MSG_OFT_LEN = 3,
	UART_MSG_OFT_VALUE = 4,
}UART_MSG_OFFSET;

/* 定义串口消息处理函数类型 */
typedef void (UART_TASK_HANDLE_FUNC)(uint8_t, uint8_t*);

/* 存储不同串口消息类型的处理函数接口 */
#define UART_TASK_HANDLER_INFO_SIZE 64
typedef struct _UART_TASK_HANDLER
{
	uint8_t type;								/* 串口消息类型 */
	char info[UART_TASK_HANDLER_INFO_SIZE];		/* 相关消息 */
	UART_TASK_HANDLE_FUNC *func;				/* 处理函数指针 */
}UART_TASK_HANDLER;

/* 串口消息类型码 */
typedef enum {
	UART_TYPE_SEMG = 		0xC0,
	UART_TYPE_REDIR = 		0xC1,
	UART_TYPE_FNIRS = 		0xC2,
	UART_TYPE_BATTERY = 	0xC4,
	UART_TYPE_VERSION = 	0xC5,
	UART_TYPE_KEY = 		0xC6,
	UART_TYPE_ADS = 		0xC7,

	UART_TYPE_IDLE = 		0xFF,				/* 不使用 */
}UART_TASK_CMD;
#define UART_TYPE_NUM 		128					/* 支持的串口数据类型数量 */

/* esp32 -> stm32 报文消息类型 */
typedef enum {
	ESP_STM_SYS_STATUS = 0,
} ESP_STM_INFO_T;

/* esp32芯片工作状态 */
typedef enum {
	ESP32_IDLE = 0,
	ESP32_1KHZ = 1,
	ESP32_2KHZ = 2,
	ESP32_4KHZ = 3,
	ESP32_8KHZ = 4,
	ESP32_CONFIGARING_CONN_WAITING = 5,
	ESP32_CONFIGARING_CLIENT_CONNECT = 6,
	ESP32_WIFI_CONNECT_ERR = 7,
	ESP32_NETIF_TCP_CONN_FAIL = 8,
	ESP32_ADS_ERR = 9,
	ESP32_BROADCAST_TIME_OUT = 10,
} ESP32_WORK_STATE;

/* esp32->stm32 串口报文偏移 */
enum {
	ES_MSG_OFT_HEAD = 0,
	ES_MSG_OFT_TYPE = 2,
	ES_MSG_OFT_DATA = 3,
	ES_MSG_OFT_CRC  = 7,
	/* 固定8字节发送 */
	ESP_STM_MSG_LEN = 8,
};

/*************************************************
 * @description			:	串口驱动初始化
 * @param 				:	无
 * @return 				:	无
**************************************************/
UART_ERR_T uart_init(void);

/*************************************************
 * @description			:	注册串口消息处理函数
 * @param - type		:	串口消息类型
 * @param - info		:	有关信息
 * @param - func		:	处理函数
 * @return 				:	UART_ERR_T
**************************************************/
UART_ERR_T uart_handler_register(uint8_t type, const char *info, UART_TASK_HANDLE_FUNC *func);

/*************************************************
 * @description			:	注销串口消息处理函数
 * @param - type		:	串口消息类型
 * @return 				:	无
**************************************************/
void uart_handler_unregister(uint8_t type);

/*************************************************
 * @description			:	获取消息处理函数
 * @param - type		:	串口消息类型
 * @return 				:	处理函数地址
**************************************************/
UART_TASK_HANDLE_FUNC *uart_get_handler_func(uint8_t type);

/*************************************************
 * @description			:	打印已注册的处理函数
 * @param - 			:	无
 * @return 				:	无
**************************************************/
void uart_handler_dump(void);

/*************************************************
 * @description			:	获取消息处理函数
 * @param - data		:	待发送数据地址
 * @param - len			:	待发送数据长度
 * @return 				:	UART_ERR_T
**************************************************/
UART_ERR_T uart_data_transmit(uint8_t *data, uint8_t len);

/*************************************************
 * @description			:	配网状态串口驱动初始化
 * @param 				:	无
 * @return 				:	UART_ERR_T
**************************************************/
UART_ERR_T uart_net_configuring_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __UART_H__ */
