/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: led.h
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V1.0
描述	   	: led驱动头文件
其他	   	: 无
日志	   	: 初版V1.0 2024/11/25 刘有为创建
***********************************************************************/
#ifndef __LED_H__
#define __LED_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define LED_BLUE_GPIO 		GPIO_NUM_22
#define LED_RED_GPIO 		GPIO_NUM_21
#define LED_GREEN_GPIO 		GPIO_NUM_1

/* led点亮电平 */
typedef enum {
	LED_ON = 0,
	LED_OFF = 1,
}LED_GPIO_STATE;

/* 状态码入参判断宏 */
#define LED_GPIO_STATE_ASSERT_FAIL(state) ((LED_ON != (state)) && (LED_OFF != (state)))

/* led的gpio开关宏 */
#define LED_GPIO_SWITCH(gpio, state) do { \
		gpio_set_level(gpio, state); \
	} while(0)

/* 颜色种类码 */
typedef enum {
	LED_COLOR_RED = 0,
	LED_COLOR_BLUE = 1,
	LED_COLOR_GREEN = 2,
	LED_COLOR_PURPLE = 3,
	LED_COLOR_CYAN = 4,
	LED_COLOR_YELLOW = 5,
	LED_COLOR_WHITE = 6,
	LED_COLOR_TYPE_NUM,
} LED_COLOR_TYPE;

/*************************************************
 * @description			:	led初始化
 * @param 				:	无
 * @return 				:	无
**************************************************/
void led_init(void);

/*************************************************
 * @description			:	led点亮与熄灭
 * @param - color 		:	led颜色
 * @param - state 		:	开启或关闭
 * @return 				:	无
**************************************************/
void led_switch(LED_COLOR_TYPE color, LED_GPIO_STATE state);

/*************************************************
 * @description			:	led切换电平
 * @param - color 		:	led颜色
 * @return 				:	无
**************************************************/
void led_toggle(LED_COLOR_TYPE color);

/*************************************************
 * @description			:	设置当前led颜色
 * @param - color		:	led颜色码
 * @return 				:	无
**************************************************/
void led_set_color(uint8_t color);

/*************************************************
 * @description			:	获取当前电量对应的led颜色
 * @param 				:	无
 * @return 				:	led颜色码
**************************************************/
uint8_t led_get_color(void);

#ifdef __cplusplus
}
#endif

#endif /* __LED_H__ */
