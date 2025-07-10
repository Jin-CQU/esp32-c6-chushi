idf_component_register(SRCS "wifi.c"
					INCLUDE_DIRS "include"
					PRIV_INCLUDE_DIRS   "${IDF_PATH}/components/esp_wifi/include"
										"${IDF_PATH}/components/esp_event/include"
										"${IDF_PATH}/components/esp_netif/include"
										"${IDF_PATH}/components/nvs_flash/include"
										"${IDF_PATH}/components/esp_partition/include"
										# "${IDF_PATH}/components/http_parser"
					PRIV_REQUIRES dns httpd uart utils
					)
