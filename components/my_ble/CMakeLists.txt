idf_component_register(
    SRCS "my_ble.c"
    
    INCLUDE_DIRS "include"
    
    REQUIRES 
        nvs_flash 
        bt
        freertos
        console
        esp_timer
        
    PRIV_INCLUDE_DIRS 
        "${IDF_PATH}/components/bt/include"
        "${IDF_PATH}/components/bt/include/esp32c6/include"
        "${IDF_PATH}/components/bt/host/nimble/esp-hci/include"
        "${IDF_PATH}/components/bt/host/nimble/nimble/nimble/include"
        "${IDF_PATH}/components/bt/host/nimble/nimble/porting/nimble/include"
        "${IDF_PATH}/components/bt/host/nimble/nimble/porting/npl/freertos/include"
        "${IDF_PATH}/components/bt/host/nimble/port/include"
        "${IDF_PATH}/components/bt/host/nimble/nimble/nimble/host/include"
        "${IDF_PATH}/components/bt/host/nimble/nimble/nimble/host/services/gap/include"
        "${IDF_PATH}/components/bt/host/nimble/nimble/nimble/host/services/gatt/include"
        "${IDF_PATH}/components/bt/host/nimble/nimble/nimble/transport/include"
        "${IDF_PATH}/components/bt/host/nimble/nimble/nimble/host/util/include"
        "${IDF_PATH}/components/bt/host/bluedroid/api/include"
        "${IDF_PATH}/components/bt/common/include"
)