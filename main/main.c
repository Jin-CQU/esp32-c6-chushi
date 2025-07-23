/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: main.c
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V1.0
描述	   	: 主函数，系统初始化
其他	   	: 无
日志	   	: 初版V1.0 2024/12/03 刘有为创建
***********************************************************************/
#include <string.h>
#include <netdb.h>
#include "stdio.h"
#include "stdlib.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_types.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "lwip/dns.h"
#include "esp_attr.h"
#include "dns_server.h"
#include "web_server.h"
#include "wifi.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "eeg.h"
#include "fnirs.h"
#include "timers.h"
#include "led.h"
#include "key.h"
#include "battery.h"
#include "version.h"
#include "uart.h"
#include "utils.h"
#include "my_ble.h"  // 引入自定义的 BLE 组件头文件

#define MAIN_TAG "ESP32"
#define MAIN_INFO(fmt, ...) ESP_LOGI(MAIN_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define MAIN_DBG(fmt, ...) ESP_LOGW(MAIN_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define MAIN_ERR(fmt, ...) ESP_LOGE(MAIN_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)

void app_main(void)
{
	int ret = 0;
	char ssid[WIFI_SSID_STR_LEN] = {0};
	char password[WIFI_PW_STR_LEN] = {0};
	uint8_t notify[ESP_STM_MSG_LEN] = {0};

	/* 初始化netif */
	ESP_ERROR_CHECK(esp_netif_init());

	/* 初始化nvs用于保存配网信息 */
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		// Retry nvs_flash_init
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	version_init();
	led_init();
	key_init();
#if 0
	if (BAT_OK != battery_init())
	{
		esp_restart();
	}
#endif

	uart_init();
	uart_handler_register(UART_TYPE_VERSION, "inner version ctrl", stm32_version_data_process);
	uart_handler_register(UART_TYPE_BATTERY, "battery", battery_data_process);
	uart_handler_register(UART_TYPE_REDIR, "stm32 dbg redirect", stm32_debug_redirect);
	uart_handler_register(UART_TYPE_KEY, "stm32 key", stm32_key_data_process);

	ret = NVS_read_data_from_flash(ssid, password, "OK");
	if(0 != ret)
	{
		MAIN_INFO("run ap mode...");
		wifi_init_ap();
		dns_server_start();
		web_server_start();

		notify[ES_MSG_OFT_HEAD] = (UART_MSG_HEAD&0xFF00)>>8;
		notify[ES_MSG_OFT_HEAD + 1] = UART_MSG_HEAD&0x00FF;
		notify[ES_MSG_OFT_TYPE] = ESP_STM_SYS_STATUS;

		while (true)
		{
			vTaskDelay(pdMS_TO_TICKS(1000));
			if (AP_WIFI_CLIENT_NOT_CONNECT == wifi_get_client_connect_status())
			{
				notify[ES_MSG_OFT_DATA] = ESP32_CONFIGARING_CONN_WAITING;
			}
			else
			{
				notify[ES_MSG_OFT_DATA] = ESP32_CONFIGARING_CLIENT_CONNECT;
			}
			notify[ES_MSG_OFT_CRC] = crc_8bit_mask(notify, ES_MSG_OFT_CRC, UART_CRC_MASK);
			uart_data_transmit(notify, ESP_STM_MSG_LEN);
		}
	}
	else{
		MAIN_INFO("run sta mode connecting to ssid: %s pw: %s", ssid, password);
		wifi_record_ssid((uint8_t *)ssid);
		wifi_record_password((uint8_t *)password);
		wifi_init_sta();

		if (EEG_OK != eeg_init())
		{
			esp_restart();
		}
#if 0
		if (FNIRS_OK != fnirs_init())
		{
			esp_restart();
		}
#endif
	}
}
