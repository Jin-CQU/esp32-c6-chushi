idf_component_register(SRCS "fnirs.c"
					INCLUDE_DIRS "include"
					PRIV_INCLUDE_DIRS   "${IDF_PATH}/components/esp_ringbuf/include" 
										"${IDF_PATH}/components/driver/uart/include" 
										"${IDF_PATH}/components/driver/gpio/include"
										"${IDF_PATH}/components/log/include"
										"${IDF_PATH}/components/freertos/FreeRTOS-Kernel/include/freertos"
										"${IDF_PATH}/components/driver/spi/include"
										"${IDF_PATH}/components/esp_timer/include"

					# PRIV_REQUIRES "driver/uart"
					REQUIRES "led" "httpd" "wifi" "utils" "uart" "key")
