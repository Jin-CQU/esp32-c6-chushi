idf_component_register(SRCS "led.c"
					INCLUDE_DIRS "include"
					PRIV_INCLUDE_DIRS	"${IDF_PATH}/components/driver/gpio/include"
										"${IDF_PATH}/components/log/include"
										"${IDF_PATH}/components/freertos/FreeRTOS-Kernel/include/freertos"
					REQUIRES "")
