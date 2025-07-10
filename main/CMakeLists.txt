idf_component_register(SRCS "main.c"
					INCLUDE_DIRS "../components"
					
					PRIV_INCLUDE_DIRS "${IDF_PATH}/components/driver/gpio/include" 
									"${IDF_PATH}/components/driver/uart/include" 
									"${IDF_PATH}/components/freertos/FreeRTOS-Kernel/include/freertos"  
					)
