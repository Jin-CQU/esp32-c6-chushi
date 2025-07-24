idf_component_register(SRCS "main.c"
					INCLUDE_DIRS "../components"
					
					PRIV_INCLUDE_DIRS 
									"${IDF_PATH}/components/bt/host/nimble/nimble/nimble/host/include"
					# 				"${IDF_PATH}/components/driver/gpio/include" 
					# 				"${IDF_PATH}/components/driver/uart/include" 
					# 				"${IDF_PATH}/components/freertos/FreeRTOS-Kernel/include/freertos"
					# 				"${IDF_PATH}/examples/protocols/http_server/captive_portal/main/include"
					# 				"${IDF_PATH}/components/bt"
					# 				"${IDF_PATH}/components/bt/include"
					# 				"${IDF_PATH}/components/bt/host/nimble"
					# 				"${IDF_PATH}/components/bt/host/nimble/esp-hci/include"
					# 				"${IDF_PATH}/components/bt/host/nimble/nimble"
					# 				"${IDF_PATH}/components/bt/host/nimble/nimble/nimble/include"
					# 				"${IDF_PATH}/components/bt/host/nimble/port/include"
					REQUIRES 
						esp_wifi esp_netif nvs_flash esp_timer
						driver  # ✅ 添加 driver 依赖
                    	wifi led key battery version uart utils eeg fnirs dns httpd 
                        tcp_communication
                        my_ble 
					)
