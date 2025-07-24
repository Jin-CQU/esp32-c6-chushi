/*
main.c 调用
    ↓
ble_communication_start()  ← 你只需要调用这个
    ↓ (内部自动)
nimble_port_freertos_init() 启动 BLE 任务
    ↓ (BLE协议栈自动调用)
ble_app_on_sync()
    ↓ (内部调用)
gatt_svr_init()
    ↓ (内部调用)
ble_gap_advertise_start()
    ↓ (客户端连接时自动调用)
ble_gap_event()
    ↓ (客户端读写数据时自动调用)
gatt_svr_chr_access_generic()
*/

#include <string.h>
#include "freertos/FreeRTOS.h"       // ← 新增
#include "freertos/task.h"           // ← 新增
#include "nvs_flash.h" 
#include "esp_nimble_hci.h"
#include "esp_bt.h"
#include "api/esp_bt_main.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "host/ble_store.h"          // ← 新增
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "my_ble.h"
#include "nimble/ble.h"
#include "host/ble_gap.h"

//#include "host/ble_hs.h"
//#include "host/ble_gap.h"
//#include "nimble/nimble_port.h"

// 标准心率服务和特征的UUID
#define SERVICE_UUID 0x180D         // 心率服务UUID
#define CHARACTERISTIC_UUID 0x2A37  // 心率测量特征UUID

#define Device_Name "ESP32 EEG Device" // 设备名称

#define BLE_HS_TIME 60000 // BLE广播持续时间，单位为毫秒

static uint8_t gap_addr_type; // GAP地址类型

struct ble_gap_adv_params adv_params = { // 广播参数
    .conn_mode = BLE_GAP_CONN_MODE_UND, // 连接模式：未连接
    .disc_mode = BLE_GAP_DISC_MODE_GEN, // 广播模式：通用
    .itvl_min = BLE_GAP_ADV_ITVL_MS(100), // 最小间隔：100毫秒
    .itvl_max = BLE_GAP_ADV_ITVL_MS(200), // 最大间隔：200毫秒
};

static int gatt_svr_init(void);
// static int ble_gap_event(struct ble_gap_event *event, void *arg);
static int gatt_svr_chr_access_generic(uint16_t conn_handle, uint16_t attr_handle, 
                                       struct ble_gatt_access_ctxt *ctxt,          
                                       void *arg); // 通用特征访问处理函数

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,        // 主服务
        .uuid = BLE_UUID16_DECLARE(SERVICE_UUID), // 声明这是心率服务
        .includes = NULL,                         // 没有包含其他服务
        .characteristics = (const struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(CHARACTERISTIC_UUID), // 声明这是心率测量特征
                .access_cb = gatt_svr_chr_access_generic,       // 告诉系统：有人访问特征值时，调用我这个函数
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY, // 可读和通知
            },
            {
                0, // 终止标志
            }
        },
    },
    {
        0, // 终止标志
    }
};

/*************************************************
 * @description			:	心率测量特征的读写请求处理
 * @param - conn_handle	:	连接句柄
 * @param - attr_handle	:	属性句柄
 * @param - ctxt		:	GATT访问上下文
 * @param - arg		    :	附加参数
 * @return 				:   0 成功，其他值表示错误
**************************************************/
static int gatt_svr_chr_access_generic(uint16_t conn_handle, uint16_t attr_handle, 
                                       struct ble_gatt_access_ctxt *ctxt,          
                                       void *arg)
{
    switch (ble_uuid_u16(ctxt->chr->uuid)) {
    case CHARACTERISTIC_UUID:
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) { //客户端请求读取特征值
            // <<<<< 处理读取心率测量特征的请求>>>>>

            char * send_data = "Hello from ESP32"; // 假设心率值为72

            // 把数据添加到 NimBLE 的内存缓冲区，然后系统自动发送给客户端
            os_mbuf_append(ctxt->om, send_data, strlen(send_data));
            return 0; // 成功
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {  // 客户端请求写入特征值
            char buf[128]; // 定义一个缓冲区用于存储从客户端写入的数据
            int len = os_mbuf_copydata(ctxt->om, 0, sizeof(buf) - 1, buf); // 从缓冲区复制数据到 buf
            buf[len] = '\0'; // 确保字符串以 null 结尾 
            printf("Received heart rate value: %s\n", buf);
            return 0; // 成功
        }
        return BLE_ATT_ERR_UNLIKELY; // 如果操作类型不是读/写，返回错误码
    default:
        return BLE_ATT_ERR_UNLIKELY; // 如果客户端访问的不是心率特征（UUID 不匹配），返回错误码
    }
}

/*************************************************
 * @description			:   BLE 协议栈初始化完成后的回调函数
 * @param - void		:	无
 * @return 				:   无
 * @note				:   完成 BLE 设备的基础配置和启动
**************************************************/
void ble_app_on_sync(void)
{
    int rc; // 返回值

    rc = ble_hs_id_infer_auto(0, &gap_addr_type); // 推断自动地址类型并写入 gap_addr_type
    assert(rc == 0); //确保地址类型推断成功

    rc = ble_svc_gap_device_name_set(Device_Name); // 设置设备名称
    assert(rc == 0); // 确保设备名称设置成功

    rc = gatt_svr_init(); // 初始化 GATT 服务器
    assert(rc == 0); // 确保 GATT 服务器初始化成功

    // rc = ble_gap_advertise_start(
    //     BLE_OWN_ADDR_RANDOM, NULL, NULL, NULL, NULL); // 开始广播
    // 设置广播包内容
    uint8_t adv_data[] = {
        0x02, 0x01, 0x06,
        0x03, 0x03, 0x0D, 0x18,
        0x12, 0x09, 'E','S','P','3','2',' ','E','E','G',' ','D','e','v','i','c','e'
    };
    // 广播数据包，包含设备名称和服务UUID
    ble_gap_adv_set_data(adv_data, sizeof(adv_data)); // 设置广播数据

    rc = ble_gap_adv_start(gap_addr_type, NULL, BLE_HS_FOREVER,
                                 &adv_params, ble_gap_event, NULL); // 开始广播，让其他设备能够发现和连接 ESP32
    //gap_addr_type 是 GAP 地址类型，NULL 表示没有额外的参数传递给回调函数 RANDOM 表示使用随机地址
    //&adv_params 是广告参数，BLE_HS_FOREVER 是广播持续时间（无限期）
    // ble_gap_event 是处理 GAP 事件的回调函数
    // NULL 表示没有额外的参数传递给回调函数
    if (rc != 0) {
        printf("ble_gap_adv_start failed, rc=%d\n", rc);
    }
    assert(rc == 0); // 确保广播启动成功
        
    printf("BLE device started, name: %s\n", Device_Name);
}

/*************************************************
 * @description			:   GAP事件回调函数
 * @param - event		:	GAP事件结构体
 * @param - arg		    :	附加参数
 * @return - int        :   0 成功，其他值表示错误
 * @note                :   处理连接、断开连接、广播等事件
**************************************************/
int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT: // 连接事件
        printf("✅ BLE Device connected successfully!\n");
        printf("Connection handle: %d\n", event->connect.conn_handle);
        printf("Connection status: %d\n", event->connect.status);
        break;

    case BLE_GAP_EVENT_DISCONNECT: // 断开连接事件
        printf("⚠️  BLE Device disconnected\n");
        printf("Disconnect reason: %d\n", event->disconnect.reason);
        printf("Restarting advertising...\n");
        
        // ✅ 重新开始广播，添加错误检查
        int rc = ble_gap_adv_start(gap_addr_type, NULL, BLE_HS_FOREVER,
                                  &adv_params, ble_gap_event, NULL);
        if (rc != 0) {
            printf("ERROR: Failed to restart advertising: %d\n", rc);
        } else {
            printf("✅ Advertising restarted successfully\n");
        }
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE: // 广播完成事件
        printf("! Advertising completed\n");
        break;
        
    case BLE_GAP_EVENT_CONN_UPDATE_REQ: // 连接参数更新请求
        printf("! Connection update requested\n");
        break;

    default:
        printf("! BLE GAP event: %d\n", event->type);
        break;
    }
    return 0;
}

/*************************************************
 * @description			:   初始化 GATT 服务器
 * @param - void		:	无
 * @return - int		:	0 成功，其他值表示错误
**************************************************/
static int gatt_svr_init(void)
{
    int rc;

    ble_svc_gap_init(); // 初始化 GAP 服务
    ble_svc_gatt_init(); // 初始化 GATT 服务

    rc = ble_gatts_count_cfg(gatt_svr_svcs); // 计算 GATT 服务的配置
    if (rc != 0) {
        return rc; // 如果计算失败，返回错误码
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs); // // 添加服务到 GATT 服务器
    if (rc != 0) {
        return rc; // 如果添加失败，返回错误码
    }

    return 0; // 初始化成功
}

/*************************************************
 * @description			:   启动 BLE 通信
 * @param - void		:	无
 * @return - void		:	无
 * @note                :   初始化 NVS Flash、NimBLE HCI、NimBLE 端口等
 **************************************************/
void ble_communication_start(void)
{
    esp_err_t ret;
    
    printf("Starting BLE initialization...\n");

    // ✅ 步骤1：释放经典蓝牙内存
    printf("Step 1: Releasing classic BT memory...\n");
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        printf("Warning: Failed to release BT memory: %s (continuing anyway)\n", esp_err_to_name(ret));
    }

    // ✅ 步骤2：直接初始化NimBLE端口
    printf("Step 2: Initializing NimBLE port...\n");
    ret = nimble_port_init(); 
    if (ret != ESP_OK) {
        printf("ERROR: NimBLE port init failed: %s\n", esp_err_to_name(ret));
        printf("BLE initialization FAILED at step 2\n");
        return;
    }
    printf("NimBLE port initialized successfully\n");

    // ✅ 步骤3：设置回调函数
    printf("Step 3: Setting up callbacks...\n");
    ble_hs_cfg.sync_cb = ble_app_on_sync; 
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    printf("Callbacks configured successfully\n");

    // ✅ 步骤4：手动创建 NimBLE 任务（避免崩溃）
    printf("Step 4: Creating NimBLE task manually...\n");
    vTaskDelay(pdMS_TO_TICKS(100));  // 延时确保系统稳定

    // 创建NimBLE任务，指定栈大小和优先级
    TaskHandle_t nimble_task_handle;
    BaseType_t task_created = xTaskCreate(
        nimble_port_run,           // 任务函数
        "nimble_host",             // 任务名称
        4096,                      // 栈大小 (4KB)
        NULL,                      // 任务参数
        5,                         // 优先级
        &nimble_task_handle        // 任务句柄
    );

    if (task_created == pdPASS) {
        printf("NimBLE task created successfully\n");
    } else {
        printf("ERROR: Failed to create NimBLE task\n");
        return;
    }
    
    printf("✅ BLE communication initialization completed successfully!\n");
    printf("Device should now be discoverable as: %s\n", Device_Name);
}