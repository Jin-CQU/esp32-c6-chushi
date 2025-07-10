/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: battery.c
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V1.0
描述	   	: 按键驱动
其他	   	: 无
日志	   	: 初版V1.0 2024/12/29 刘有为创建
***********************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "errno.h"
#include "hal/adc_types.h"
#include "esp_log.h"
#include "battery.h"
#include "esp_adc/adc_oneshot.h"
#include "led.h"

#define BAT_TAG "BAT"
#define BAT_INFO(fmt, ...) ESP_LOGI(BAT_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define BAT_DBG(fmt, ...) ESP_LOGW(BAT_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define BAT_ERR(fmt, ...) ESP_LOGE(BAT_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)

typedef struct _BATTERY_GLOBAL
{
	int32_t val;							/* 电量值 */
	int8_t ratio;							/* 百分比（%） */
	adc_oneshot_unit_handle_t adc;			/* adc句柄 */
	adc_oneshot_unit_init_cfg_t unit_cfg;	/* adc配置 */
	adc_oneshot_chan_cfg_t chn_cfg;			/* 通道配置 */
} BATTERY_GLOBAL;

BATTERY_GLOBAL g_battery = {0};

/*************************************************
 * @description			:	电源模块初始化
 * @param 				:	无
 * @return 				:	BAT_ERR_T
**************************************************/
int battery_init(void)
{
	int ret = 0;
	int value = 0;

	g_battery.unit_cfg.unit_id = BAT_ADC_UNIT;
	g_battery.unit_cfg.clk_src = 0;
	g_battery.unit_cfg.ulp_mode = ADC_ULP_MODE_DISABLE;

	if (ESP_OK != adc_oneshot_new_unit(&g_battery.unit_cfg, &g_battery.adc))
	{
		BAT_ERR("alloc new unit for adc failed errno: %d (%s)", errno, strerror(errno));
		return BAT_FAIL;
	}

	g_battery.chn_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
	g_battery.chn_cfg.atten = ADC_ATTEN_DB_6;
	if (ESP_OK != adc_oneshot_config_channel(g_battery.adc, BAT_ADC_CHN, &g_battery.chn_cfg))
	{
		BAT_ERR("set adc channel config failed errno: %d (%s)", errno, strerror(errno));
		goto exit;
	}

	battery_value_detect();

	return BAT_OK;

exit:
	adc_oneshot_del_unit(g_battery.adc);
	return BAT_FAIL;
}

/*************************************************
 * @description			:	电池电量读取并更新
 * @param 				:	无
 * @return 				:	无
**************************************************/
inline void battery_value_detect(void)
{
	int ret = 0;
	int value = 0;
	static uint64_t cnt = 0;

	ret = adc_oneshot_read(g_battery.adc, BAT_ADC_CHN, &value);
	if (ESP_OK != ret)
	{
		BAT_ERR("adc read failed ret: %d(0x%x)", ret, ret);
		return;
	}
	g_battery.val = value;
	if (BATTERY_VALUE_MAX <= g_battery.val)
	{
		g_battery.ratio = 100;
	}
	else if (BATTERY_VALUE_MIN >= g_battery.val)
	{
		g_battery.ratio = 0;
	}
	else
	{
		g_battery.ratio = 100*(g_battery.val - BATTERY_VALUE_MIN)/(BATTERY_VALUE_MAX - BATTERY_VALUE_MIN);
	}
	cnt += 1;

	if (80 <= g_battery.ratio)
	{
		/* 80%以上的电量，绿灯闪烁 */
		led_set_color(LED_COLOR_GREEN);
	}
	else if (50 <= g_battery.ratio && 80 > g_battery.ratio)
	{
		/* 50%~80%的电量，青灯闪烁 */
		led_set_color(LED_COLOR_CYAN);
	}
	else if (20 <= g_battery.ratio && 50 > g_battery.ratio)
	{
		/* 20%~50%的电量，紫灯闪烁 */
		led_set_color(LED_COLOR_PURPLE);
	}
	else
	{
		/* 20%以下的电量，红灯闪烁 */
		led_set_color(LED_COLOR_RED);
	}

	BAT_INFO("value: %ld ratio: %d (%llu)", g_battery.val, g_battery.ratio, cnt);
}

/*************************************************
 * @description			:	获取电量百分比
 * @param 				:	无
 * @return 				:	百分比
**************************************************/
uint8_t battery_get_ratio(void)
{
	return g_battery.ratio;
}

/*************************************************
 * @description			:	电量处理函数
 * @param - argc		:	参数长度
 * @param - argv		:	参数地址
 * @return 				:	无
**************************************************/
void battery_data_process(uint8_t argc, uint8_t *argv)
{
	static uint32_t print_cnt = 0;
	if (sizeof(uint32_t) != argc)
	{
		BAT_ERR("argv error");
		return;
	}

	g_battery.val = (argv[0]<<24) | (argv[1]<<16) | (argv[2]<<8) | (argv[3]);
	if (BATTERY_VALUE_MAX <= g_battery.val)
	{
		g_battery.ratio = 100;
	}
	else if (BATTERY_VALUE_MIN >= g_battery.val)
	{
		g_battery.ratio = 0;
	}
	else
	{
		g_battery.ratio = 100*(g_battery.val - BATTERY_VALUE_MIN)/(BATTERY_VALUE_MAX - BATTERY_VALUE_MIN);
	}
	print_cnt += 1;
	BAT_INFO("battery val: %ld ratio: %u (%lu)", g_battery.val, g_battery.ratio, print_cnt);
}
