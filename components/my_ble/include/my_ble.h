#ifndef MY_BLE_H
#define MY_BLE_H

#ifdef __cplusplus
extern "C" {
#endif

// #include "host/ble_hs.h"
// #include "host/ble_gap.h"
// #include "nimble/nimble_port.h"
// ✅ 外部变量声明
extern struct ble_gap_adv_params adv_params;
extern int ble_gap_event(struct ble_gap_event *event, void *arg);

/**
 * @brief 启动 BLE 通信
 */
void ble_communication_start(void);
void ble_app_on_sync(void);

#ifdef __cplusplus
}
#endif

#endif