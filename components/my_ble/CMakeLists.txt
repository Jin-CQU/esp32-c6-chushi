idf_component_register(SRCS "my_ble.c" "ble_communication.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "nvs_flash" "esp_nimble_hci" "nimble" "ble_svc_gap")