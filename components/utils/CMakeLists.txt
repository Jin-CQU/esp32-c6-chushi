idf_component_register(SRCS "utils.c"
					INCLUDE_DIRS "include"
					PRIV_INCLUDE_DIRS   "${IDF_PATH}/components/driver/uart/include" 
										"${IDF_PATH}/components/log/include"

					# PRIV_REQUIRES "driver/uart"
					REQUIRES "")
