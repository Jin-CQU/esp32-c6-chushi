/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: utils.h
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V1.0
描述	   	: 通用组件
其他	   	: 无
日志	   	: 初版V1.0 2024/6/21 刘有为创建
***********************************************************************/
#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>

/* 提供info、debug、err三种级别的打印 */
#define INFO_PRINT(fmt, ...)	printf(fmt"\r\n", ##__VA_ARGS__)
#define DBG_PRINT(fmt, ...)		printf("(%s, %d) " fmt"\r\n", __func__, __LINE__, ##__VA_ARGS__)
#define ERR_PRINT(fmt, ...)		printf("error!(%s, %d) " fmt"\r\n", __func__, __LINE__, ##__VA_ARGS__)

#define CRC_TABLE_SIZE 256

/* 右移 */
#define R_SHIFT(val, bit) ((val) << (bit))

/*
 * ARRAY_SIZE	求取数组长度
 * @arr:		数组首地址
 */
#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))

/* 短整型大小端互换 */
#define ENDIAN_SWAP_16B(x) ((((uint16_t)(x) & 0XFF00) >> 8) | \
							(((uint16_t)(x) & 0X00FF) << 8))

/* 整型大小端互换 */
#define ENDIAN_SWAP_32B(x) ((((uint32_t)(x) & 0XFF000000) >> 24) | \
							(((uint32_t)(x) & 0X00FF0000) >> 8) | \
							(((uint32_t)(x) & 0X0000FF00) << 8) | \
							(((uint32_t)(x) & 0X000000FF) << 24))

/*************************************************
 * @description			:	16进制内存打印函数
 * @param - adr			:	打印的初始地址
 * @param - len			:	打印地址长度
 * @param - info		:	附加信息
 * @return 				:	无
**************************************************/
void hex_dump(uint8_t *adr, uint32_t len, const char *info);

/*************************************************
 * @description			:	8位crc检验码计算
 * @param - data		:	数据地址
 * @param - len			:	数据长度
 * @return 				:	8位crc检验码
**************************************************/
uint8_t crc_8bit(uint8_t *data, uint8_t len);

/*************************************************
 * @description			:	CRC校验表初始化
 * @param 				:	无
 * @return 				:	无
**************************************************/
void crc_table_init(void);

/*************************************************
 * @description			:	crc校验
 * @param - buf			:	数据地址
 * @param - len			:	数据长度
 * @return 				:	校验结果
**************************************************/
uint8_t crc_cal(uint8_t *buf, int len);

/*************************************************
 * @description			:	8位crc检验码计算
 * @param - data		:	数据地址
 * @param - len			:	数据长度
 * @param - mask		:	掩码
 * @return 				:	8位crc检验码
**************************************************/
uint8_t crc_8bit_mask(uint8_t *data, uint8_t len, const uint8_t mask);

/*************************************************
 * @description			:	stm32调试信息重定向
 * @param - argc		:	参数长度
 * @param - argv		:	参数地址
 * @return 				:	无
**************************************************/
void stm32_debug_redirect(uint8_t argc, uint8_t *argv);

/*************************************************
 * @description			:	TCP发送函数
 * @param - fd			:	套接字
 * @param - buf			:	数据地址
 * @param - len			:	数据长度
 * @return 				:	已发送的字节个数
**************************************************/
ssize_t safety_send(int fd, uint8_t *buf, ssize_t len);

#endif /* __UTILS_H__ */
