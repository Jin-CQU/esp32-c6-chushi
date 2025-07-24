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
#include "freertos/timers.h" // FreeRTOS 定时器
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
//#include "timers.h"
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

// ✅ 添加EEG初始化任务函数声明和定义
// static void eeg_init_task(void *pvParameters)
// {
//     MAIN_INFO("Starting EEG initialization in background...");
    
//     if (EEG_OK != eeg_init())
//     {
//         MAIN_ERR("EEG initialization failed! Continuing with BLE only...");
//     } else {
//         MAIN_INFO("✅ EEG initialization completed successfully!");
//     }
    
//     // 任务完成后删除自己
//     // vTaskDelete(NULL);
// }


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

		// // ✅ 先启动 BLE，不管WiFi和EEG
		// MAIN_INFO("Starting BLE communication...");
		// ble_communication_start();

		// // ✅ 检查BLE是否真的启动成功
		// vTaskDelay(pdMS_TO_TICKS(10000));  // 等待10秒让BLE完全启动

		// // 检查BLE是否真的在运行
		// MAIN_INFO("BLE initialization completed, continuing...");

		// // ✅ 添加BLE状态检查
		// if (ble_hs_is_enabled()) {
		// 	MAIN_INFO("✅ BLE Host Stack is enabled and running");
		// } else {
		// 	MAIN_ERR("❌ BLE Host Stack is NOT running!");
		// }

		// // ✅ 检查广播状态
		// if (ble_gap_adv_active()) {
		// 	MAIN_INFO("✅ BLE advertising is active");
		// } else {
		// 	MAIN_ERR("❌ BLE advertising is NOT active!");
		// 	// 尝试重新启动广播
		// 	MAIN_INFO("Attempting to restart BLE advertising...");
		// 	ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
		// }

		if (EEG_OK != eeg_init())
		{
			MAIN_ERR("EEG initialization failed! Continuing with BLE only...");
		} else {
			MAIN_INFO("✅ EEG initialization completed successfully!");
		}

		// ✅ 将EEG初始化放到独立任务中，避免阻塞主线程
		//  xTaskCreate(eeg_init_task, "eeg_init", 4096, NULL, 3, NULL);

		//  MAIN_INFO("Main thread continuing, EEG initializing in background...");

#if 0
		if (FNIRS_OK != fnirs_init())
		{
			MAIN_ERR("FNIRS initialization failed!");
			esp_restart();
		}
#endif

		// ✅ 增强的主循环
		static int status_count = 0;
		while (true) {
			vTaskDelay(pdMS_TO_TICKS(1000));
			
			// 每10秒打印一次系统状态
			if (++status_count >= 10) {
				status_count = 0;
				
				// ✅ 检查系统状态
				size_t free_heap = esp_get_free_heap_size();
				MAIN_INFO("System Status: Free memory: %d bytes", free_heap);
				
				// ✅ 如果内存过低，可能需要重启BLE
				if (free_heap < 30000) {
					MAIN_ERR("Low memory warning! Consider restarting BLE...");
				}
				
				MAIN_INFO("BLE+WiFi+EEG system running...");
			}
		}
	}
}


