#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "tcp_communication.h"

#define PORT 8080
#define TAG "TCP通讯"

static void tcp_server_task(void *pvParameters){
    char rx_buffer[128]; // 接收缓冲区
    char addr_str[128]; // 存储客户端地址
    int addr_family = AF_INET;// IPv4
    int ip_protocol = IPPROTO_TCP; // TCP协议
    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);// 创建监听套接字
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "套接字创建失败: errno %d", errno); // 错误日志
        vTaskDelete(NULL); // 删除任务
        return;
    }
    ESP_LOGI(TAG, "Socket created");

    struct sockaddr_in dest_addr; // IPv4地址结构体
    dest_addr.sin_family = addr_family; // 设置地址族
    dest_addr.sin_port = htons(PORT); // 设置端口
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 存放所有可用接口地址
    //memset(dest_addr.sin_zero, 0, sizeof(dest_addr.sin_zero)); // 清零

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)); // 绑定套接字
    if (err != 0) {
        ESP_LOGE(TAG, "套接字绑定失败: errno %d", errno); // 错误日志
        close(listen_sock); // 关闭套接字
        vTaskDelete(NULL); // 删除任务
        return;
    }
    ESP_LOGI(TAG, "套接字绑定成功 端口号： %d", PORT);
    err = listen(listen_sock, 5); // 开始监听，最大允许排队数为 5
    ESP_LOGI(TAG, "开始监听端口 %d", PORT);
    if (err != 0) {
        ESP_LOGE(TAG, "监听失败: errno %d", errno); // 错误日志
        close(listen_sock); // 关闭套接字
        vTaskDelete(NULL); // 删除任务
        return;
    }
    while(1){
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_len); // 接受客户端连接
        if (client_sock < 0) {
            ESP_LOGE(TAG, "接受连接失败: errno %d", errno); // 错误日志
            continue;
        }
        ESP_LOGI(TAG, "接受到连接");
        inet_ntoa_r(client_addr.sin_addr, addr_str, sizeof(addr_str) - 1); // 获取客户端地址，写入 addr_str
        ESP_LOGI(TAG, "客户端地址: %s", addr_str);

        // 接收数据
        while (1) {
            int len = recv(client_sock, rx_buffer, sizeof(rx_buffer) -1, 0); // 把收到的数据存入 rx_buffer
            if (len < 0){
                ESP_LOGE(TAG, "数据接收发生错误: errno %d", errno); // 错误日志
            } else if (0 == len){
                ESP_LOGI(TAG, "客户端断开连接");
                break; // 客户端断开连接
            } else {
                rx_buffer[len] = 0; // 确保字符串以 null 字符 ('\0')结尾
                ESP_LOGI(TAG, "接收到数据: %s", rx_buffer); // 打印接收到的数据
                // 发送回客户端
                const char *resp = "收到数据";
                int err = send(client_sock, resp, strlen(resp), 0);
                if (err < 0) {
                    ESP_LOGE(TAG, "发送数据失败: errno %d", errno);
                    break; // 发送失败，退出循环
                }
            }
        }

        if (client_sock != -1) {
            ESP_LOGI(TAG, "关闭套接字并断开连接");
            shutdown(client_sock, 0); // 关闭连接
            close(client_sock); // 关闭客户端套接字
        }
    }
    vTaskDelete(NULL); // 删除任务
}

void tcp_communication_start(void) {
    ESP_LOGI(TAG, "TCP 通信开始");
    xTaskCreate(tcp_server_task, "tcp_server_task", 4096, NULL, 5, NULL); // 创建 TCP 服务器任务
}