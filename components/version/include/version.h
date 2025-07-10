/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: version.h
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V1.0
描述	   	: 版本信息
其他	   	: 无
日志	   	: 初版V1.0 2024/12/30 刘有为创建
***********************************************************************/
#ifndef __VERSION_H__
#define __VERSION_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#define ESP32_VERSION_A 	1
#define ESP32_VERSION_B 	0
#define ESP32_VERSION_C 	5
#define ESP32_VERSION_BR 	5
#define ESP32_MCU_TYPE 		0

/* 本机有两款mcu */
enum {
	MCU_ESP32 = 0,
	MCU_STM32 = 1,
	MCU_NUM = 2,
};

typedef struct _HARDWARE_VERSION
{
	uint16_t ver_year;					/* 硬件版本：年份 */
	uint8_t ver_month;					/* 硬件版本：月份 */
	uint8_t ver_day;					/* 硬件版本：日期 */
	uint8_t main_ver;					/* 硬件版本：主版本号 */
	uint8_t sub_ver;					/* 硬件版本：次版本号 */
} HARDWARE_VERSION;

typedef struct _SOFTWARE_VERSION
{
	uint8_t type;							/* mcu类型 */
	uint8_t ver_a;							/* 版本号A */
	uint8_t ver_b;							/* 版本号B */
	uint8_t ver_c;							/* 版本号C */
	uint16_t ver_d_year;					/* 版本号D：年份 */
	uint8_t ver_d_month;					/* 版本号D：月份 */
	uint8_t ver_d_day;						/* 版本号D：日期 */
	uint8_t ver_d_hour;						/* 版本号D：时（24小时制） */
	uint8_t ver_d_min;						/* 版本号D：分 */
	uint8_t ver_d_sec;						/* 版本号D：秒 */
	uint8_t branch_id;						/* 分支号 */
} SOFTWARE_VERSION;

/*************************************************
 * @description			:	本机版本信息初始化
 * @param 				:	无
 * @return 				:	无
**************************************************/
void version_init(void);

/*************************************************
 * @description			:	获取硬件版本信息
 * @param 				:	无
 * @return 				:	硬件版本信息地址
**************************************************/
HARDWARE_VERSION *get_hardware_version(void);

/*************************************************
 * @description			:	获取软件版本信息
 * @param - mcu			:	mcu类型
 * @return 				:	软件版本信息地址
**************************************************/
SOFTWARE_VERSION *get_software_version(uint8_t mcu);

/*************************************************
 * @description			:	stm32版本信息处理函数
 * @param - argc		:	参数长度
 * @param - argv		:	参数地址
 * @return 				:	无
**************************************************/
void stm32_version_data_process(uint8_t argc, uint8_t *argv);

#ifdef __cplusplus
}
#endif

#endif /* __VERSION_H__ */
