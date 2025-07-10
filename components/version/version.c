/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: version.h
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V1.0
描述	   	: 版本信息
其他	   	: 无
日志	   	: 初版V1.0 2024/12/30 刘有为创建
***********************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "esp_log.h"
#include "version.h"
#include "utils.h"

#define VER_TAG "VER"
#define VER_INFO(fmt, ...) ESP_LOGI(VER_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define VER_DBG(fmt, ...) ESP_LOGW(VER_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define VER_ERR(fmt, ...) ESP_LOGE(VER_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)

typedef struct _VERSION_INFO
{
	HARDWARE_VERSION hw;
	SOFTWARE_VERSION sw[MCU_NUM];
} VERSION_INFO;

VERSION_INFO g_version = {0};

/*************************************************
 * @description			:	本机版本信息初始化
 * @param 				:	无
 * @return 				:	无
**************************************************/
void version_init(void)
{
	SOFTWARE_VERSION *sw = NULL;
	char month_str[8] = {0};
	const char *months[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	uint16_t year = 0;
	uint8_t month = 0;
	uint8_t day = 0;
	uint8_t hour = 0;
	uint8_t min = 0;
	uint8_t sec = 0;
	uint8_t i = 0;

	/* 硬件版本信息，目前预留，后续应当从flash中读出 */

	sw = &g_version.sw[MCU_ESP32];
	sw->ver_a = ESP32_VERSION_A;
	sw->ver_b = ESP32_VERSION_B;
	sw->ver_c = ESP32_VERSION_C;

	/* 解析版本号D */
	sscanf(__DATE__, "%s %hhu %hu", month_str, &day, &year);
	sscanf(__TIME__, "%hhu:%hhu:%hhu", &hour, &min, &sec);
	for (i = 0; i < 12; i++)
	{
		if (0 == memcmp(month_str, months[i], 3))
		{
			month = i + 1;
			break;
		}
	}
	sw->ver_d_year = year;
	sw->ver_d_month = month;
	sw->ver_d_day = day;
	sw->ver_d_hour = hour;
	sw->ver_d_min = min;
	sw->ver_d_sec = sec;

	/* 分支号 */
	sw->branch_id = ESP32_VERSION_BR;

	VER_INFO("esp32 software version: v%u.%u.%u br.%u cp %u-%u-%u %u:%u:%u cpu: %u", 
		sw->ver_a, sw->ver_b, sw->ver_c, sw->branch_id,
		sw->ver_d_year, sw->ver_d_month, sw->ver_d_day,
		sw->ver_d_hour, sw->ver_d_min, sw->ver_d_sec, sw->type);
}

/*************************************************
 * @description			:	获取硬件版本信息
 * @param 				:	无
 * @return 				:	硬件版本信息地址
**************************************************/
HARDWARE_VERSION *get_hardware_version(void)
{
	return &g_version.hw;
}

/*************************************************
 * @description			:	获取软件版本信息
 * @param - mcu			:	mcu类型
 * @return 				:	软件版本信息地址
**************************************************/
SOFTWARE_VERSION *get_software_version(uint8_t mcu)
{
	if (MCU_NUM <= mcu)
	{
		VER_ERR("illegal mcu index: %u", mcu);
		return NULL;
	}

	return &g_version.sw[mcu];
}

/*************************************************
 * @description			:	stm32版本信息处理函数
 * @param - argc		:	参数长度
 * @param - argv		:	参数地址
 * @return 				:	无
**************************************************/
void stm32_version_data_process(uint8_t argc, uint8_t *argv)
{
	SOFTWARE_VERSION *stm32_sw = NULL;

	stm32_sw = (SOFTWARE_VERSION *)argv;
	memcpy(&g_version.sw[MCU_STM32], stm32_sw, sizeof(SOFTWARE_VERSION));
	stm32_sw = &g_version.sw[MCU_STM32];
	VER_INFO("stm32 software version: v%u.%u.%u br.%u cp: %u-%u-%u %u:%u:%u cpu: %u", 
		stm32_sw->ver_a, stm32_sw->ver_b, stm32_sw->ver_c, stm32_sw->branch_id,
		(stm32_sw->ver_d_year), stm32_sw->ver_d_month, stm32_sw->ver_d_day,
		stm32_sw->ver_d_hour, stm32_sw->ver_d_min, stm32_sw->ver_d_sec, stm32_sw->type);
}
