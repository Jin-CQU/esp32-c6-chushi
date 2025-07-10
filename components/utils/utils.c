/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: utils.c
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V1.0
描述	   	: 通用组件
其他	   	: 无
日志	   	: 初版V1.0 2024/6/21 刘有为创建
***********************************************************************/
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <sys/param.h>
#include <pthread.h>
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "utils.h"
#include "esp_log.h"

typedef enum{
	ESP_STM_INFO = 0,
	ESP_STM_ERR = 1,
	ESP_STM_DBG = 2,
} STM32_LEVEL_T;

uint8_t g_crc_table[CRC_TABLE_SIZE] = {0};

/*************************************************
 * @description			:	16进制内存打印函数
 * @param - adr			:	打印的初始地址
 * @param - len			:	打印地址长度
 * @param - info		:	附加信息
 * @return 				:	无
**************************************************/
void hex_dump(uint8_t *adr, uint32_t len, const char *info)
{
	int i = 0;
	uint16_t ofs = 0;

	printf("\r\n- - - - - - - - - - - - - - - - - - - -\r\n");
	if (NULL != info)
	{
		printf("%s\r\n", info);
	}
	for (i = 0; i < len; i++)
	{
		if (0 == i%8)
		{
			printf("0x%04x:", ofs);
			ofs += 8;
		}
		printf("%02x ", adr[i]);
		if (0 == (i + 1) % 8)
		{
			printf("\r\n");
		}
		if (3 ==i % 8) /* 0 == (i - 3)%4 */
		{
			printf(" ");
		}
	}
	printf("\r\n- - - - - - - - - - - - - - - - - - - -\r\n");
}

/*************************************************
 * @description			:	8位crc检验码计算
 * @param - data		:	数据地址
 * @param - len			:	数据长度
 * @return 				:	8位crc检验码
**************************************************/
uint8_t crc_8bit(uint8_t *data, uint8_t len)
{
	uint16_t crc = 0;
	int i = 0;
	int j = 0;

	for (i = 0; i < len; i++)
	{
		crc ^= data[i] << 8;
		for (j = 0; j < 8; j++)
		{
			if (crc & 0x8000)
				crc ^= (0x07 << 7);
			crc <<= 1;
		}
	}

	return ((crc >> 8) & 0xFF);
}

/*************************************************
 * @description			:	crc校验
 * @param - buf			:	数据地址
 * @param - len			:	数据长度
 * @return 				:	校验结果
**************************************************/
uint8_t crc_cal(uint8_t *buf, int len)
{
	int i = 0;
	uint8_t crc = 0;

	for (i = 0; i < len; i++)
	{
		crc = g_crc_table[crc ^ buf[i]];
	}

	return crc;
}

/*************************************************
 * @description			:	CRC校验表初始化
 * @param 				:	无
 * @return 				:	无
**************************************************/
void crc_table_init(void)
{
	int i = 0;
	int j = 0;
	uint8_t cur = 0;

	for (i  =0; i < CRC_TABLE_SIZE; i++)
	{
		cur = i;
		for (j = 0; j < 8; j++)
		{
			if ((0x80 & cur) != 0)
			{
				cur = cur << 1^(0xd5);
			}
			else
			{
				cur <<= 1;
			}
		}
		g_crc_table[i] = cur;
	}
}

/*************************************************
 * @description			:	8位crc检验码计算
 * @param - data		:	数据地址
 * @param - len			:	数据长度
 * @return 				:	8位crc检验码
**************************************************/
uint8_t crc_8bit_mask(uint8_t *data, uint8_t len, const uint8_t mask)
{
	uint16_t crc = 0;
	int i = 0;
	int j = 0;

	for (i = 0; i < len; i++)
	{
		crc ^= data[i] << 8;
		for (j = 0; j < 8; j++)
		{
			if (crc & 0x8000)
				crc ^= (mask << 7);
			crc <<= 1;
		}
	}

	return ((crc >> 8) & 0xFF);
}

/*************************************************
 * @description			:	stm32调试信息重定向
 * @param - argc		:	参数长度
 * @param - argv		:	参数地址
 * @return 				:	无
**************************************************/
void stm32_debug_redirect(uint8_t argc, uint8_t *argv)
{
	uint8_t level = 0;
	char *msg = NULL;

	if (NULL == argv || 0 == argc)
	{
		ESP_LOGE(__func__, "input null");
		return;
	}

	level = argv[0];
	msg = (char*)&argv[1];
	switch (level)
	{
		case ESP_STM_INFO:
			ESP_LOGI("STM32", "%s", msg);
			break;

		case ESP_STM_ERR:
			ESP_LOGE("STM32", "%s", msg);
			break;
		
		case ESP_STM_DBG:
			ESP_LOGW("STM32", "%s", msg);
			break;

		default:
			ESP_LOGE(__func__, "unsupported level: %u", level);
			break;
	}
}

/*************************************************
 * @description			:	TCP发送函数
 * @param - fd			:	套接字
 * @param - buf			:	数据地址
 * @param - len			:	数据长度
 * @return 				:	已发送的字节个数
**************************************************/
ssize_t safety_send(int fd, uint8_t *buf, ssize_t len)
{
	ssize_t left = len;
	ssize_t nsend = 0;

	while (0 < left)
	{
		nsend = send(fd, buf, left, 0);
		if (0 == nsend)
		{
			/* server调用close()关闭 */
			return -1;
		}
		else if (0 > nsend)
		{
			if (EINTR  == errno || EAGAIN == errno || EWOULDBLOCK == errno)
			{
				/* 
				 * 参考链接：https://blog.csdn.net/modi000/article/details/106783572
				 */
				continue;
			}
			return -1;
		}
		left -= nsend;
		buf += nsend;
	}

	return (len - left);
}
