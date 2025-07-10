/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: battery.h
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V1.0
描述	   	: 电池电量驱动
其他	   	: 无
日志	   	: 初版V1.0 2024/12/29 刘有为创建
***********************************************************************/
#ifndef __BATTERY_H__
#define __BATTERY_H__

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
	BAT_FAIL = -1, 
	BAT_OK = 0,
} BAT_ERR_T;

#define BAT_ADC_GPIO 	(GPIO_NUM_0)
#define BAT_ADC_CHN 	(ADC_CHANNEL_0)
#define BAT_ADC_UNIT 	(ADC_UNIT_1)

#define BATTERY_VALUE_MAX 2450
#define BATTERY_VALUE_MIN 1850

/*************************************************
 * @description			:	获取电量百分比
 * @param 				:	无
 * @return 				:	百分比
**************************************************/
uint8_t battery_get_ratio(void);

/*************************************************
 * @description			:	电源模块初始化
 * @param 				:	无
 * @return 				:	BAT_ERR_T
**************************************************/
int battery_init(void);

/*************************************************
 * @description			:	电池电量读取并更新
 * @param 				:	无
 * @return 				:	无
**************************************************/
void battery_value_detect(void);

/*************************************************
 * @description			:	电量处理函数
 * @param - argc		:	参数长度
 * @param - argv		:	参数地址
 * @return 				:	无
**************************************************/
void battery_data_process(uint8_t argc, uint8_t *argv);

#ifdef __cplusplus
}
#endif

#endif /* __BATTERY_H__ */
