/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: fnirs.h
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V2.0
描述	   	: 脑氧驱动头文件
其他	   	: 无
日志	   	: V2.0 2025/1/24 刘有为创建
***********************************************************************/
#ifndef __FNIRS_H__
#define __FNIRS_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

typedef enum {
	FNIRS_FAIL = -1,
	FNIRS_OK = 0,
} FNIRS_ERR_T;

typedef enum {
	FNIRS_NET_AUTO_CONNECT,			/* 自动配网 */
	FNIRS_NET_MANUAL_CONNECT,		/* 手动配网 */
} FNIRS_NET_CONNECT_T;

#define FNIRS_DEFAULT_NET_CONNECT_TYPE 		FNIRS_NET_MANUAL_CONNECT
#define FNIRS_DEFAULT_HOST_IP 				"192.168.1.1"
#define FNIRS_DEFAULT_HOST_PORT 			30301
#define FNIRS_DEFAULT_SERIAL_NUM 			0
#define FNIRS_DEFAULT_PROTOCOL 				SOCK_DGRAM
#define FNIRS_HOST_BROADCAST_PORT 			30200
#define FNIRS_HOST_BROADCAST_MSG_HEAD 		0xFFFF
#define BROAD_CAST_RECV_BUF_LEN 			256

/* 广播报文偏移 */
enum {
	FNIRS_BROADCAST_HEAD = 0,
	FNIRS_BROADCAST_IP = 2,
	FNIRS_BROADCAST_MASK = 6,
	FNIRS_BROADCAST_TCP_PORT_NUM = 10,
};
#define PORT_SELECT_INDEX 					1 	/* 广播报文中获取第x个端口 */

typedef enum {
	FNIRS_UART_DF_OFT_HEAD = 	0,
	FNIRS_UART_DF_OFT_TYPE = 	2,
	FNIRS_UART_DF_OFT_DLEN = 	3,
	FNIRS_UART_DF_OFT_CHN = 	4,
	FNIRS_UART_DF_OFT_DATA = 	5,
	FNIRS_UART_DF_OFT_CRC = 	25,
	FNIRS_UART_DF_OFT_TAIL = 	26,
	FNIRS_UART_DATA_FRAME_LEN = 28,
} FNIRS_UART_MSG_OFFSET;

#define FNIRS_DATA_HEADER 						0xAEAE			//脑氧数据
#define FNIRS_CRC_MASK 							0xd5
typedef enum {
	FNIRS_DF_OFT_HEAD = 	0,					/* 首部 */
	FNIRS_DF_OFT_SERIAL = 	2,					/* 序列号 */
	FNIRS_DF_OFT_CHN = 		5,					/* 通道编号 */
	FNIRS_DF_OFT_DLEN = 	6,					/* 数据长度 */
	FNIRS_DF_OFT_DATA = 	8,					/* 数据起始位 */
	FNIRS_DF_OFT_TS = 		28,					/* 8字节时间戳 */
	FNIRS_DF_OFT_CRC = 		36,					/* crc校验位 */
	FNIRS_DATA_FRAME_LEN = 	37,
} FNIRS_DATA_FRAME_T;

#define FNIRS_SEND_BUF_NUM 256					/* 发送缓存区数量 */

typedef enum {
	NIRS_CHN_1 = 0,
	NIRS_CHN_2 = 1,
	NIRS_CHN_3 = 2,
	NIRS_CHN_4 = 3,
	NIRS_CHN_5 = 4,
	NIRS_CHN_NUM ,
} FNIRS_CHN_T;

/*************************************************
 * @description			:	初始化肌电模块
 * @param - 			:	无
 * @return 				:	FNIRS_ERR_T
**************************************************/
FNIRS_ERR_T fnirs_init(void);

/*************************************************
 * @description			:	设置脑氧功能状态
 * @param - state		:	要设置的状态
 * @return 				:	无
**************************************************/
void fnirs_set_state(uint8_t state);

#ifdef __cplusplus
}
#endif

#endif /* __FNIRS_H__ */
