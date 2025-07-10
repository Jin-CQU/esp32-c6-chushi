/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: key.h
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V1.0
描述	   	: 按键驱动头文件
其他	   	: 无
日志	   	: 初版V1.0 2024/11/30 刘有为创建
***********************************************************************/
#ifndef __KEY_H__
#define __KEY_H__

#ifdef __cplusplus
extern "C"
{
#endif

enum
{
	ESP_POWER_OFF = 0,
	ESP_POWER_ON = 1,
};

#define ESP_POWER_CTRL_GPIO 	(GPIO_NUM_15)
#define ESP_POWER_KEY_GPIO 		(GPIO_NUM_23)
#define NET_KEY_GPIO 			(GPIO_NUM_9)
#define NET_KEY_ISR_NUM 		1
#define NET_KEY_TIME			3000000
#define POWER_KEY_TIME			1000000

typedef enum {
	KEY_FAIL = -1,
	KEY_OK = 0,
} KEY_ERR_T;

typedef enum {
	KEY_LONG_PRESS = 0,
	KEY_SHORT_PRESS = 1,
} KEY_REGISTER_T;

typedef enum {
	KEY_ACT_SINGLE_CLICK = 0,
	KEY_ACT_DOUBLE_CLICK = 1,
	KEY_ACT_LONG_PRESS = 2,
} STM32_PWR_KEY_ACT_T;

/* 定义按键处理函数类型 */
typedef void (KEY_HANDLE_FUNC)(uint8_t, uint8_t*);
typedef void (STM32_KEY_HANDLE_FUNC)(void);

/*************************************************
 * @description			:	按键初始化
 * @param 				:	无
 * @return 				:	无
**************************************************/
void key_init(void);

/*************************************************
 * @description			:	启动按键监控
 * @param 				:	无
 * @return 				:	无
**************************************************/
void key_scan_start(void);

/*************************************************
 * @description			:	注册按键长短按处理函数
 * @param - type		:	长按/短按
 * @param - info		:	有关信息
 * @param - func		:	处理函数
 * @param - argc		:	参数长度
 * @param - argv		:	参数地址
 * @return 				:	KEY_ERR_T
**************************************************/
KEY_ERR_T key_press_handler_register(KEY_REGISTER_T type, const char *info, KEY_HANDLE_FUNC *func, uint8_t argc, void *argv);

/*************************************************
 * @description			:	stm32按键处理函数
 * @param - argc		:	参数长度
 * @param - argv		:	参数地址
 * @return 				:	无
**************************************************/
void stm32_key_data_process(uint8_t argc, uint8_t *argv);

/*************************************************
 * @description			:	注册stm32按键处理函数
 * @param - type		:	单击/双击/长按
 * @param - func		:	处理函数
 * @return 				:	KEY_ERR_T
**************************************************/
KEY_ERR_T stm32_key_handler_register(STM32_PWR_KEY_ACT_T type, STM32_KEY_HANDLE_FUNC *func);

#ifdef __cplusplus
}
#endif

#endif /* __KEY_H__ */
