idf_component_register(SRCS "web_server.c"
					INCLUDE_DIRS "include" 
					PRIV_INCLUDE_DIRS   "${IDF_PATH}/components/esp_http_server/include"
										"${IDF_PATH}/components/nvs_flash/include"
										"${IDF_PATH}/components/esp_partition/include"
										"${IDF_PATH}/components/esp_wifi/include"
										"${IDF_PATH}/components/esp_event/include"
										"${IDF_PATH}/components/esp_netif/include"
										"${IDF_PATH}/components/http_parser"
					REQUIRES ""
					EMBED_FILES "index.html" #EMBED_FILES用于嵌入html页面，记得加EMBED_FILES,不然web_server.c中调用的html会报错
					)
