idf_component_register(SRCS "key.c"
					INCLUDE_DIRS "include"
					PRIV_INCLUDE_DIRS	"${IDF_PATH}/components/driver/gpio/include"
										"${IDF_PATH}/components/log/include"
										"${IDF_PATH}/components/freertos/FreeRTOS-Kernel/include/freertos"
										"${IDF_PATH}/components/esp_timer/include"
										"${IDF_PATH}/components/nvs_flash/include"
										"${IDF_PATH}/components/esp_partition/include"
					REQUIRES "led" "uart")
