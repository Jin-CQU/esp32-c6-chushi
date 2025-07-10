/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: key.c
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V1.0
描述	   	: 按键驱动
其他	   	: 无
日志	   	: 初版V1.0 2024/11/30 刘有为创建
***********************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "led.h"
#include "key.h"
#include "uart.h"
//#if ESP_IDF_VERSION <= ESP_IDF_VERSION_VAL(5, 1, 0)
#include "FreeRTOS.h"
#include "task.h"
//#endif

#define KEY_TAG "KEY"
#define KEY_INFO(fmt, ...) ESP_LOGI(KEY_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define KEY_DBG(fmt, ...) ESP_LOGW(KEY_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define KEY_ERR(fmt, ...) ESP_LOGE(KEY_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)

static void key_monitor_task(void* arg);

/* 存储不同串口消息类型的处理函数接口 */
#define KEY_TASK_HANDLER_INFO_SIZE 64
typedef struct _KEY_TASK_ELEM
{
	uint8_t state;								/* 状态 */
	char info[KEY_TASK_HANDLER_INFO_SIZE];		/* 相关消息 */
	KEY_HANDLE_FUNC *func;						/* 处理函数 */
	uint8_t argc;								/* 参数长度 */
	void *argv;									/* 参数地址 */
} KEY_TASK_ELEM;
#define KEY_TASK_ELEM_MAX_SIZE 					16

typedef enum {
	KEY_TASK_ELEM_IDLE = 0,
	KEY_TASK_ELEM_ON = 1,
} KEY_TASK_ELEM_STATE_T;

typedef struct _KEY_GLB
{
	KEY_TASK_ELEM lpress_handlers[KEY_TASK_ELEM_MAX_SIZE];		/* 长按处理函数 */
	KEY_TASK_ELEM spress_handlers[KEY_TASK_ELEM_MAX_SIZE];		/* 短按处理函数 */
	STM32_KEY_HANDLE_FUNC *stm32_single_click_func;				/* stm32单击处理函数 */
	STM32_KEY_HANDLE_FUNC *stm32_double_click_func;				/* stm32双击处理函数 */
	STM32_KEY_HANDLE_FUNC *stm32_long_press_func;				/* stm32长按处理函数 */
} KEY_GLB;

KEY_GLB g_key = {0};

/*************************************************
 * @description			:	按键初始化
 * @param 				:	无
 * @return 				:	无
**************************************************/
void key_init(void)
{
	int i = 0;
	gpio_config_t net_key = {0};
	gpio_config_t pow_key = {0};
	gpio_config_t pow_ctrl = {0};

	KEY_INFO("init key module...");

	memset(&g_key.lpress_handlers[0], 0, sizeof(KEY_TASK_ELEM));
	memset(&g_key.spress_handlers[0], 0, sizeof(KEY_TASK_ELEM));
	for (i = 0; i < KEY_TASK_ELEM_MAX_SIZE; i++)
	{
		g_key.lpress_handlers[i].state = KEY_TASK_ELEM_IDLE;
		g_key.lpress_handlers[i].argc = 0;
		g_key.lpress_handlers[i].argv = NULL;
		g_key.lpress_handlers[i].func = NULL;

		g_key.spress_handlers[i].state = KEY_TASK_ELEM_IDLE;
		g_key.spress_handlers[i].argc = 0;
		g_key.spress_handlers[i].argv = NULL;
		g_key.spress_handlers[i].func = NULL;
	}

	g_key.stm32_single_click_func = NULL;
	g_key.stm32_double_click_func = NULL;
	g_key.stm32_long_press_func = NULL;

	/* 初始化按键 */
	net_key.intr_type = GPIO_INTR_DISABLE;					//无中断
	net_key.mode = GPIO_MODE_INPUT;							//设置该引脚为输入模式
	net_key.pull_up_en = 1;									//上拉
	net_key.pin_bit_mask = (1ULL << (NET_KEY_GPIO));
	gpio_config(&net_key);

	pow_key.intr_type = GPIO_INTR_DISABLE;					//无中断
	pow_key.mode = GPIO_MODE_INPUT;							//设置该引脚为输入模式
	pow_key.pull_up_en = 1;									//上拉
	pow_key.pin_bit_mask = (1ULL << (ESP_POWER_KEY_GPIO));
	gpio_config(&pow_key);

	pow_ctrl.intr_type = GPIO_INTR_DISABLE;					//无中断
	pow_ctrl.mode = GPIO_MODE_OUTPUT;						//设置该引脚为输出模式
	pow_ctrl.pull_up_en = 1;									//上拉
	pow_ctrl.pin_bit_mask = (1ULL << (ESP_POWER_CTRL_GPIO));
	gpio_config(&pow_ctrl);

	/* 打开供电开关 */
	gpio_set_level(ESP_POWER_CTRL_GPIO, ESP_POWER_ON);

	key_scan_start();
}

/*************************************************
 * @description			:	启动按键监控
 * @param 				:	无
 * @return 				:	无
**************************************************/
void key_scan_start(void)
{
	/* 注册按键处理函数 */
	do
	{
		vTaskDelay(100);
	} while (0 == gpio_get_level(ESP_POWER_KEY_GPIO));
	xTaskCreate(key_monitor_task, "key monitor", 2048, NULL, 10, NULL);
	KEY_INFO("monitoring keys");
}

/*************************************************
 * @description			:	按键监控函数
 * @param - arg			:	线程入参
 * @return 				:	无
**************************************************/
static void key_monitor_task(void* arg)
{
	int64_t time = 0;
	bool state = 0;
	int i = 0;

	KEY_INFO("scanning key...");

	while (1)
	{
		vTaskDelay(1);
		//KEY_DBG("gpio: %d", gpio_get_level(NET_KEY_GPIO));
#if 0
		if (0 == gpio_get_level(ESP_POWER_KEY_GPIO))
		{
			/* 
			 * 0：按键按下
			 * 1：按键松开
			 */
			time = esp_timer_get_time();
			while (0 == gpio_get_level(ESP_POWER_KEY_GPIO))
			{
				vTaskDelay(1);
				if (time < esp_timer_get_time() - POWER_KEY_TIME)
				{
					KEY_INFO("long press");
					time = esp_timer_get_time();
					state = true;
					/* 关机 */
					led_switch(LED_COLOR_RED, LED_ON);
					vTaskDelay(2000 / portTICK_PERIOD_MS);
					gpio_set_level(ESP_POWER_CTRL_GPIO, ESP_POWER_OFF);
				}
			}
			if (false == state && time >= esp_timer_get_time() - POWER_KEY_TIME)
			{
				KEY_INFO("short press");
				esp_restart();
				state = false;
			}
		}
#endif
		if (0 == gpio_get_level(NET_KEY_GPIO))
		{
			/* 
			 * 0：按键按下
			 * 1：按键松开
			 */
			time = esp_timer_get_time();
			while (0 == gpio_get_level(NET_KEY_GPIO))
			{
				vTaskDelay(1);
				if (time < esp_timer_get_time() - NET_KEY_TIME)
				{
					KEY_INFO("long press");
					/* 长按处理函数 */
					for (i = 0; i < KEY_TASK_ELEM_MAX_SIZE; i++)
					{
						if (KEY_TASK_ELEM_ON == g_key.lpress_handlers[i].state && g_key.lpress_handlers[i].func)
						{
							g_key.lpress_handlers[i].func(g_key.lpress_handlers[i].argc, g_key.lpress_handlers[i].argv);
						}
					}
					led_switch(LED_COLOR_YELLOW, LED_ON);
					time = esp_timer_get_time();
					state = true;
					ESP_ERROR_CHECK(nvs_flash_erase());
					KEY_INFO("erase flash");
					vTaskDelay(2000 / portTICK_PERIOD_MS);
					esp_restart();
				}
			}
			if (false == state && time >= esp_timer_get_time() - NET_KEY_TIME)
			{
				KEY_INFO("short press");
				/* 短按处理函数 */
				for (i = 0; i < KEY_TASK_ELEM_MAX_SIZE; i++)
				{
					if (KEY_TASK_ELEM_ON == g_key.spress_handlers[i].state && g_key.spress_handlers[i].func)
					{
						g_key.spress_handlers[i].func(g_key.spress_handlers[i].argc, g_key.spress_handlers[i].argv);
					}
				}
				state = false;
			}
		}
	}
}

/*************************************************
 * @description			:	注册按键长短按处理函数
 * @param - type		:	长按/短按
 * @param - info		:	有关信息
 * @param - func		:	处理函数
 * @param - argc		:	参数长度
 * @param - argv		:	参数地址
 * @return 				:	KEY_ERR_T
**************************************************/
KEY_ERR_T key_press_handler_register(KEY_REGISTER_T type, const char *info, KEY_HANDLE_FUNC *func, uint8_t argc, void *argv)
{
	int i = 0;
	uint8_t idx = 0;
	KEY_TASK_ELEM *handlers = NULL;

	if (NULL == info || NULL == func)
	{
		KEY_ERR("info or func null");
		return KEY_FAIL;
	}
	if (KEY_LONG_PRESS != type && KEY_SHORT_PRESS != type)
	{
		KEY_ERR("illegal type: %d", type);
		return KEY_FAIL;
	}
	if ((0 == argc && NULL != argv) || (0 != argc && NULL == argv))
	{
		KEY_ERR("argc(%u) not match with argv", argc);
		return KEY_FAIL;
	}
	if (KEY_TASK_HANDLER_INFO_SIZE <= strlen(info))
	{
		KEY_ERR("info is too large(%u bytes), request less than %d bytes", strlen(info), KEY_TASK_HANDLER_INFO_SIZE);
		return KEY_FAIL;
	}

	handlers = (KEY_LONG_PRESS == type) ? (g_key.lpress_handlers) : (g_key.spress_handlers);
	/* 查找空闲结点 */
	for (i = 0; i < KEY_TASK_ELEM_MAX_SIZE; i++)
	{
		if (KEY_TASK_ELEM_IDLE == handlers[i].state)
		{
			idx = i;
			break;
		}
	}

	if (0 != argc)
	{
		handlers[idx].argv = (void *)malloc(argc);
		if (NULL == handlers[idx].argv)
		{
			KEY_ERR("malloc failed errno: %d (%s)", errno, strerror(errno));
			goto error_exit;
		}
		memset(handlers[idx].argv, 0, argc);
		memcpy(handlers[idx].argv, argv, argc);
		handlers[idx].argc = argc;
	}
	handlers[idx].func = func;
	memcpy(handlers->info, info, strlen(info));
	handlers[idx].state = KEY_TASK_ELEM_ON;

	return KEY_OK;

error_exit:
	return KEY_FAIL;
}

/*************************************************
 * @description			:	stm32按键处理函数
 * @param - argc		:	参数长度
 * @param - argv		:	参数地址
 * @return 				:	无
**************************************************/
void stm32_key_data_process(uint8_t argc, uint8_t *argv)
{
	if (1 != argc || NULL == argv)
	{
		KEY_ERR("illegal param");
		return;
	}

	switch (*argv)
	{
		case KEY_ACT_SINGLE_CLICK:
			if (NULL != g_key.stm32_single_click_func)
			{
				g_key.stm32_single_click_func();
			}
			break;

		case KEY_ACT_DOUBLE_CLICK:
			if (NULL != g_key.stm32_double_click_func)
			{
				g_key.stm32_double_click_func();
			}

			/* 进入配网模式 */
			ESP_ERROR_CHECK(nvs_flash_erase());
			KEY_INFO("erase flash");
			vTaskDelay(2000 / portTICK_PERIOD_MS);
			esp_restart();
			break;

		case KEY_ACT_LONG_PRESS:
			if (NULL != g_key.stm32_long_press_func)
			{
				g_key.stm32_long_press_func();
			}
			break;
	}
}

/*************************************************
 * @description			:	注册stm32按键处理函数
 * @param - type		:	单击/双击/长按
 * @param - func		:	处理函数
 * @return 				:	KEY_ERR_T
**************************************************/
KEY_ERR_T stm32_key_handler_register(STM32_PWR_KEY_ACT_T type, STM32_KEY_HANDLE_FUNC *func)
{
	if (NULL == func)
	{
		KEY_ERR("param null");
		return KEY_FAIL;
	}

	switch (type)
	{
		case KEY_ACT_SINGLE_CLICK:
			if (NULL == g_key.stm32_single_click_func)
			{
				g_key.stm32_single_click_func = func;
			}
			else
			{
				KEY_ERR("stm32 single click handler has been registered");
				goto err_exit;
			}
			break;
		
		case KEY_ACT_DOUBLE_CLICK:
			if (NULL == g_key.stm32_double_click_func)
			{
				g_key.stm32_double_click_func = func;
			}
			else
			{
				KEY_ERR("stm32 double click handler has been registered");
				goto err_exit;
			}
			break;

		case KEY_ACT_LONG_PRESS:
			if (NULL == g_key.stm32_long_press_func)
			{
				g_key.stm32_long_press_func = func;
			}
			else
			{
				KEY_ERR("stm32 long press handler has been registered");
				goto err_exit;
			}
			break;
	}

	return KEY_OK;

err_exit:
	return KEY_FAIL;
}
