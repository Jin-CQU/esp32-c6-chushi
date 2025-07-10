/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: led.c
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V1.0
描述	   	: led驱动头
其他	   	: 无
日志	   	: 初版V1.0 2024/11/25 刘有为创建
***********************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "led.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

#define LED_TAG "LED"
#define LED_INFO(fmt, ...) ESP_LOGI(LED_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define LED_DBG(fmt, ...) ESP_LOGW(LED_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define LED_ERR(fmt, ...) ESP_LOGE(LED_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)

typedef struct _LED_CTX {
	uint8_t toggle;			/* 记录电平反相的状态：bit[0-6]为red,blue,green,purple,cyan,yellow,white */
	uint8_t color;			/* 记录不同电量的led颜色 */
} LED_CTX;

LED_CTX g_led = {0};

/* 获取当前led的toggle状态 */
#define LED_GET_TOGGLE_STATE(color) (g_led.toggle & (1 << (color)))

/* 设置led的toggle状态 */
#define LED_SET_TOGGLE_STATE(color, val) do { \
		if (val) \
		{ \
			g_led.toggle |= (1 << (color)); \
		} \
		else \
		{ \
			g_led.toggle &= ~(1 << (color)); \
		} \
	}while(0)

/*************************************************
 * @description			:	设置当前led颜色
 * @param - color		:	led颜色码
 * @return 				:	无
**************************************************/
void led_set_color(uint8_t color)
{
	if (LED_COLOR_TYPE_NUM <= color)
	{
		LED_ERR("unknown color type: %u", color);
		return;
	}
	g_led.color = color;
}

/*************************************************
 * @description			:	获取当前电量对应的led颜色
 * @param 				:	无
 * @return 				:	led颜色码
**************************************************/
inline uint8_t led_get_color(void)
{
	return g_led.color;
}

/*************************************************
 * @description			:	led初始化
 * @param 				:	无
 * @return 				:	无
**************************************************/
void led_init(void)
{
	gpio_config_t led_cfg = {0};

	led_cfg.pin_bit_mask = (1ull << LED_BLUE_GPIO) | (1ull << LED_RED_GPIO) | (1ull << LED_GREEN_GPIO);
	led_cfg.mode = GPIO_MODE_OUTPUT;
	gpio_config(&led_cfg);

	LED_GPIO_SWITCH(LED_BLUE_GPIO, LED_OFF);
	LED_GPIO_SWITCH(LED_RED_GPIO, LED_OFF);
	LED_GPIO_SWITCH(LED_GREEN_GPIO, LED_OFF);

	LED_INFO("led module init done...");
}

/*************************************************
 * @description			:	led点亮与熄灭
 * @param - color 		:	led颜色
 * @param - state 		:	开启或关闭
 * @return 				:	无
**************************************************/
void led_switch(LED_COLOR_TYPE color, LED_GPIO_STATE state)
{
	if (LED_GPIO_STATE_ASSERT_FAIL(state))
	{
		LED_ERR("unknown led state: %u", state);
		return;
	}

	if (LED_OFF == state)
	{
		LED_GPIO_SWITCH(LED_BLUE_GPIO, LED_OFF);
		LED_GPIO_SWITCH(LED_RED_GPIO, LED_OFF);
		LED_GPIO_SWITCH(LED_GREEN_GPIO, LED_OFF);
		return;
	}

	switch (color)
	{
		case LED_COLOR_RED:
			LED_GPIO_SWITCH(LED_BLUE_GPIO, LED_OFF);
			LED_GPIO_SWITCH(LED_RED_GPIO, LED_ON);
			LED_GPIO_SWITCH(LED_GREEN_GPIO, LED_OFF);
			break;

		case LED_COLOR_BLUE:
			LED_GPIO_SWITCH(LED_BLUE_GPIO, LED_ON);
			LED_GPIO_SWITCH(LED_RED_GPIO, LED_OFF);
			LED_GPIO_SWITCH(LED_GREEN_GPIO, LED_OFF);
			break;

		case LED_COLOR_GREEN:
			LED_GPIO_SWITCH(LED_BLUE_GPIO, LED_OFF);
			LED_GPIO_SWITCH(LED_RED_GPIO, LED_OFF);
			LED_GPIO_SWITCH(LED_GREEN_GPIO, LED_ON);
			break;

		case LED_COLOR_PURPLE:
			/* red + blue */
			LED_GPIO_SWITCH(LED_BLUE_GPIO, LED_ON);
			LED_GPIO_SWITCH(LED_RED_GPIO, LED_ON);
			LED_GPIO_SWITCH(LED_GREEN_GPIO, LED_OFF);
			break;

		case LED_COLOR_CYAN:
			/* blue + green */
			LED_GPIO_SWITCH(LED_BLUE_GPIO, LED_ON);
			LED_GPIO_SWITCH(LED_RED_GPIO, LED_OFF);
			LED_GPIO_SWITCH(LED_GREEN_GPIO, LED_ON);
			break;

		case LED_COLOR_YELLOW:
			/* red + green */
			LED_GPIO_SWITCH(LED_BLUE_GPIO, LED_OFF);
			LED_GPIO_SWITCH(LED_RED_GPIO, LED_ON);
			LED_GPIO_SWITCH(LED_GREEN_GPIO, LED_ON);
			break;

		case LED_COLOR_WHITE:
			/* red + green + green */
			LED_GPIO_SWITCH(LED_BLUE_GPIO, LED_ON);
			LED_GPIO_SWITCH(LED_RED_GPIO, LED_ON);
			LED_GPIO_SWITCH(LED_GREEN_GPIO, LED_ON);
			break;
		
		default:
			LED_ERR("unkown color type: %u", color);
			break;
	}
}

/*************************************************
 * @description			:	led切换电平
 * @param - color 		:	led颜色
 * @return 				:	无
**************************************************/
void led_toggle(LED_COLOR_TYPE color)
{
	if (LED_GET_TOGGLE_STATE(color))
	{
		/* led off */
		LED_SET_TOGGLE_STATE(color, 0);
		led_switch(color, LED_OFF);
	}
	else
	{
		/* led on */
		LED_SET_TOGGLE_STATE(color, 1);
		led_switch(color, LED_ON);
	}
}
