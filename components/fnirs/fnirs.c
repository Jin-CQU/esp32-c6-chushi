/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: fnirs.c
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V2.0
描述	   	: 脑氧驱动
其他	   	: 无
日志	   	: V2.0 2025/1/24 刘有为创建
***********************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <sys/param.h>
#include <pthread.h>
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "fnirs.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/FreeRTOS.h"
#include "led.h"
#include "web_server.h"
#include "wifi.h"
#include "esp_timer.h"
#include "utils.h"
#include "esp_mac.h"
#include "fnirs.h"
#include "uart.h"
#include "key.h"
#include "utils.h"

#define FNIRS_TAG "FNIRS"
#define FNIRS_INFO(fmt, ...) ESP_LOGI(FNIRS_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define FNIRS_DBG(fmt, ...) ESP_LOGW(FNIRS_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define FNIRS_ERR(fmt, ...) ESP_LOGE(FNIRS_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)

typedef struct _FNIRS_CTX
{
	uint8_t state;							/* 软件状态位 */
	uint32_t serial;						/* 本机序列号 */
	uint8_t connect_type;					/* 配网方式 */
	uint8_t protocol;						/* 传输协议 */
	int data_sock;							/* 发送套接字 */
	int admin_sock;							/* 管理信息套接字 */
	uint16_t port;							/* 端口 */
	uint32_t host_ip;						/* 上位机IP */
	uint32_t mask;							/* 上位机IP掩码 */
	uint32_t client_ip;						/* 下位机IP */
	struct sockaddr_in dst;					/* 发送端地址 */
	struct sockaddr_in src;					/* 源地址 */
	struct sockaddr_in from;				/* 管理端地址 */
	struct sockaddr_storage dst_s;			/* 发送端地址（sockaddr_storage格式） */
	uint16_t buf_idx;						/* 当前环形缓冲区序号 */
	uint8_t *snd_buf;						/* 环形缓冲区地址 */
} FNIRS_CTX;

typedef enum {
	FNIRS_STATE_IDLE = 0,
	FNIRS_STATE_ON = 1,
} FNIRS_STATE_T;

FNIRS_CTX g_fnirs = {
	.state = 								FNIRS_STATE_IDLE,
	.serial =  								FNIRS_DEFAULT_SERIAL_NUM,
	.protocol =   							FNIRS_DEFAULT_PROTOCOL,
	.data_sock = 							-1,
	.admin_sock = 							-1,
	.port = 								FNIRS_DEFAULT_HOST_PORT,
	.host_ip = 								0,
	.mask = 								0,
	.client_ip = 							0,
	.dst = 									{0},
	.src = 									{0},
	.dst_s = 								{0},
	.from = 								{0},
	.buf_idx =  							0,
	.snd_buf =  							NULL,
};

void fnirs_data_process(uint8_t argc, uint8_t *argv);
static void fnirs_sync_params_from_flash(void);
static void fnirs_search_host_ip(void);
static int fnirs_record_host_ip(uint8_t *buf, int len);
static int fnirs_network_connect(void);

/*************************************************
 * @description			:	切换脑氧功能状态
 * @param 				:	无
 * @return 				:	无
**************************************************/
static void fnirs_toggle_state(void)
{
	g_fnirs.state = (FNIRS_STATE_IDLE == g_fnirs.state) ? FNIRS_STATE_ON : FNIRS_STATE_IDLE;
	if (FNIRS_STATE_IDLE == g_fnirs.state)
	{
		/* 关闭肌电功能 */
		FNIRS_INFO("stop fnirs");
	}
	else
	{
		/* 开启肌电功能 */
		FNIRS_INFO("start fnirs");
	}
}

/*************************************************
 * @description			:	按键短按处理函数
 * @param - argc		:	参数长度
 * @param - argv		:	参数地址
 * @return 				:	无
**************************************************/
void fnirs_key_short_press(uint8_t argc, void *argv)
{
	fnirs_toggle_state();
}

/*************************************************
 * @description			:	按键长按处理函数
 * @param - argc		:	参数长度
 * @param - argv		:	参数地址
 * @return 				:	无
**************************************************/
void fnirs_key_long_press(uint8_t argc, void *argv)
{
	FNIRS_INFO("stop fnirs");
	fnirs_set_state(FNIRS_STATE_IDLE);
}

/*************************************************
 * @description			:	初始化肌电模块
 * @param - 			:	无
 * @return 				:	FNIRS_ERR_T
**************************************************/
FNIRS_ERR_T fnirs_init(void)
{
	int i = 0;
	uint8_t *msg = NULL;

	/* 读取flash参数 */
	fnirs_sync_params_from_flash();

	/* 分配发送缓存区并初始化 */
	g_fnirs.snd_buf = (uint8_t *)malloc(FNIRS_SEND_BUF_NUM * FNIRS_DATA_FRAME_LEN);
	if (NULL == g_fnirs.snd_buf)
	{
		FNIRS_ERR("malloc failed: %d (%s)", errno, strerror(errno));
		return FNIRS_FAIL;
	}
	memset(g_fnirs.snd_buf, 0, FNIRS_SEND_BUF_NUM * FNIRS_DATA_FRAME_LEN);
	for (i = 0; i < FNIRS_SEND_BUF_NUM; i++)
	{
		msg = &g_fnirs.snd_buf[i*FNIRS_DATA_FRAME_LEN];
		*(uint16_t *)&msg[FNIRS_DF_OFT_HEAD] = htons(FNIRS_DATA_HEADER);
		msg[FNIRS_DF_OFT_SERIAL] = (g_fnirs.serial&0x00FF0000) >> 16;
		msg[FNIRS_DF_OFT_SERIAL + 1] = (g_fnirs.serial&0x0000FF00) >> 8;
		msg[FNIRS_DF_OFT_SERIAL + 2] = g_fnirs.serial&0x000000FF;
		msg[FNIRS_DF_OFT_DLEN] = FNIRS_DF_OFT_TS - FNIRS_DF_OFT_DATA;
	}

	/* 连接到上位机 */
	if (FNIRS_OK != fnirs_network_connect())
	{
		FNIRS_ERR("connect to server failed");
		return FNIRS_FAIL;
	}

	/* 注册串口相应函数 */
	uart_handler_register(UART_TYPE_FNIRS, "fnirs", fnirs_data_process);

	/* 注册长短按处理函数 */
	key_press_handler_register(KEY_SHORT_PRESS, "fnirs short press", fnirs_key_short_press, 0, NULL);
	key_press_handler_register(KEY_LONG_PRESS, "fnirs long press", fnirs_key_long_press, 0, NULL);

	fnirs_set_state(FNIRS_STATE_ON);
	return FNIRS_OK;
}

/*************************************************
 * @description			:	设置脑氧功能状态
 * @param - state		:	要设置的状态
 * @return 				:	无
**************************************************/
void fnirs_set_state(uint8_t state)
{
	if (FNIRS_STATE_ON != state && FNIRS_STATE_IDLE != state)
	{
		FNIRS_ERR("unknown state: %u", state);
		return;
	}

	g_fnirs.state = state;
}

/*************************************************
 * @description			:	脑氧数据处理函数
 * @param - argc		:	参数长度
 * @param - argv		:	参数地址
 * @return 				:	无
**************************************************/
void fnirs_data_process(uint8_t argc, uint8_t *argv)
{
	uint8_t chn = 0;
	uint16_t buf_idx = 0;
	uint8_t *msg = NULL;
	int64_t time_stamp = 0;

	if (NULL == argv || (FNIRS_UART_DF_OFT_CRC - FNIRS_UART_DF_OFT_CHN) != argc)
	{
		FNIRS_ERR("param abnormal, argc: %u", argc);
		return;
	}

	if (FNIRS_STATE_ON != g_fnirs.state)
	{
		return;
	}

	chn = argv[0];
	if (NIRS_CHN_5 < chn)
	{
		FNIRS_ERR(" abnormal chn: %u", chn);
		return;
	}

	buf_idx = g_fnirs.buf_idx;
	msg = &g_fnirs.snd_buf[buf_idx * FNIRS_DATA_FRAME_LEN];
	msg[FNIRS_DF_OFT_CHN] = chn;
	memcpy(msg + FNIRS_DF_OFT_DATA, argv + (FNIRS_UART_DF_OFT_DATA - FNIRS_UART_DF_OFT_CHN),
		FNIRS_UART_DF_OFT_CRC - FNIRS_UART_DF_OFT_DATA);
	time_stamp = esp_timer_get_time();
	msg[FNIRS_DF_OFT_TS] = 	 (time_stamp&0xff00000000000000)>>56;
	msg[FNIRS_DF_OFT_TS + 1] = (time_stamp&0x00ff000000000000)>>48;
	msg[FNIRS_DF_OFT_TS + 2] = (time_stamp&0x0000ff0000000000)>>40;
	msg[FNIRS_DF_OFT_TS + 3] = (time_stamp&0x000000ff00000000)>>32;
	msg[FNIRS_DF_OFT_TS + 4] = (time_stamp&0x00000000ff000000)>>24;
	msg[FNIRS_DF_OFT_TS + 5] = (time_stamp&0x0000000000ff0000)>>16;
	msg[FNIRS_DF_OFT_TS + 6] = (time_stamp&0x000000000000ff00)>>8;
	msg[FNIRS_DF_OFT_TS + 7] = time_stamp&0x00000000000000ff;
	msg[FNIRS_DF_OFT_CRC] = crc_8bit_mask(msg, FNIRS_DF_OFT_CRC, FNIRS_CRC_MASK);

	/* 发送 */
	if (SOCK_STREAM == g_fnirs.protocol)
	{
		if (FNIRS_DATA_FRAME_LEN != safety_send(g_fnirs.data_sock, msg, FNIRS_DATA_FRAME_LEN))
		{
			FNIRS_ERR("send failed, errno: %d (%s)", errno, strerror(errno));
		}
	}
	else
	{
		sendto(g_fnirs.data_sock, msg, FNIRS_DATA_FRAME_LEN, 0,
			(struct sockaddr *)&g_fnirs.dst, sizeof(struct sockaddr_in));
	}

	g_fnirs.buf_idx += 1;
	if (FNIRS_SEND_BUF_NUM == g_fnirs.buf_idx)
	{
		g_fnirs.buf_idx = 0;
	}
}

/*************************************************
 * @description			:	从flash中同步参数到
 * 							全局变量中
 * @param - 			:	无
 * @return 				:	无
**************************************************/
static void fnirs_sync_params_from_flash(void)
{
	NET_CONFIG net_cfg = {0};
	NET_CONFIG cfg_cmp = {0};

	if (ESP_OK != nvs_read_net_cfg_from_flash(&net_cfg, "OK"))
	{
		/* 读取失败，使用默认地址 */
		FNIRS_ERR("read from flash failed, use default network params");
		g_fnirs.host_ip = inet_addr(FNIRS_DEFAULT_HOST_IP);
		FNIRS_ERR("host: %s", FNIRS_DEFAULT_HOST_IP);

		g_fnirs.port = FNIRS_DEFAULT_HOST_PORT;
		FNIRS_ERR("port: %u", g_fnirs.port);

		g_fnirs.client_ip = inet_addr(DEFAULT_CLIENT_IP_ADDR);
		FNIRS_ERR("client ip: %s", DEFAULT_CLIENT_IP_ADDR);

		g_fnirs.serial = FNIRS_DEFAULT_SERIAL_NUM;
		FNIRS_ERR("serial num: %lu", g_fnirs.serial);

		g_fnirs.protocol = FNIRS_DEFAULT_PROTOCOL;
		FNIRS_ERR("protocol: %s", (SOCK_STREAM == g_fnirs.protocol) ? "TCP" : "UDP");

		g_fnirs.connect_type = FNIRS_DEFAULT_NET_CONNECT_TYPE;
		FNIRS_ERR("net connect type: %s", g_fnirs.connect_type ? "manual" : "auto");
	}
	else
	{
		/* 配网方式 */
		if (0 == memcmp(cfg_cmp.connect_type, net_cfg.connect_type, CONNECT_TYPE_STR_LEN))
		{
			/* 用户未设置，则使用默认参数 */
			g_fnirs.connect_type = FNIRS_DEFAULT_NET_CONNECT_TYPE;
			FNIRS_DBG("use default connect type: %s",  g_fnirs.connect_type ? "manual" : "auto");
		}
		else
		{
			if (0 == memcmp(net_cfg.connect_type, "auto", 4))
			{
				g_fnirs.connect_type = FNIRS_NET_AUTO_CONNECT;
			}
			else if (0 == memcmp(net_cfg.connect_type, "manual", 6))
			{
				g_fnirs.connect_type = FNIRS_NET_MANUAL_CONNECT;
			}
			else
			{
				g_fnirs.connect_type = FNIRS_DEFAULT_NET_CONNECT_TYPE;
			}
			FNIRS_INFO("connect type: %s", g_fnirs.connect_type ? "manual" : "auto");
		}

		/* 序列号 */
		if (0 == memcmp(cfg_cmp.client_serial, net_cfg.client_serial, SERIAL_STRING_LEN))
		{
			g_fnirs.serial = FNIRS_DEFAULT_SERIAL_NUM;
			FNIRS_DBG("use default serial num: %lu", g_fnirs.serial);
		}
		else
		{
			g_fnirs.serial = serial_num_str_parse(net_cfg.client_serial);
			FNIRS_INFO("serial num:" SERIAL_NUM_FORMAT,  SERIAL_NUM_PARSE(g_fnirs.serial));
		}

		/* 协议 */
		if (0 == memcmp(cfg_cmp.protocol, net_cfg.protocol, PROTOCOL_STR_LEN))
		{
			g_fnirs.protocol = FNIRS_DEFAULT_PROTOCOL;
			FNIRS_DBG("use default protocol: %s", (SOCK_STREAM == g_fnirs.protocol) ? "TCP" : "UDP");
		}
		else
		{
			if (0 == memcmp(net_cfg.protocol, "TCP", 3) || 0 == memcmp(net_cfg.protocol, "tcp", 3))
			{
				g_fnirs.protocol = SOCK_STREAM;
			}
			else if (0 == memcmp(net_cfg.protocol, "UDP", 3) || 0 == memcmp(net_cfg.protocol, "udp", 3))
			{
				g_fnirs.protocol = SOCK_DGRAM;
			}
			else
			{
				g_fnirs.protocol = FNIRS_DEFAULT_PROTOCOL;
				FNIRS_ERR("unknown protocol: %s use default: %s", net_cfg.protocol,
									(SOCK_STREAM == g_fnirs.protocol) ? "TCP" : "UDP");
			}
			FNIRS_INFO("protocol: %s", (SOCK_STREAM == g_fnirs.protocol) ? "TCP" : "UDP");
		}

		if (FNIRS_NET_MANUAL_CONNECT == g_fnirs.connect_type)
		{
			/* 上位机IP */
			if (0 == memcmp(cfg_cmp.host_ip, net_cfg.host_ip, IP_STRING_LEN))
			{
				/* 用户未设置，则使用默认参数 */
				g_fnirs.host_ip = inet_addr(FNIRS_DEFAULT_HOST_IP);
				FNIRS_DBG("use default host ip: %s", FNIRS_DEFAULT_HOST_IP);
			}
			else
			{
				g_fnirs.host_ip = inet_addr(net_cfg.host_ip);
				FNIRS_INFO("host: %s", net_cfg.host_ip);
			}

			/* 端口 */
			if (0 == memcmp(cfg_cmp.fnirs_port, net_cfg.fnirs_port, PORT_STRING_LEN))
			{
				g_fnirs.port = FNIRS_DEFAULT_HOST_PORT;
				FNIRS_DBG("use default port: %u", g_fnirs.port);
			}
			else
			{
				g_fnirs.port = atoi(net_cfg.fnirs_port);
				FNIRS_INFO("port: %u", g_fnirs.port);
			}

			/* 下位机IP */
			if (0 == memcmp(cfg_cmp.client_ip, net_cfg.client_ip, IP_STRING_LEN))
			{
				g_fnirs.client_ip = inet_addr(DEFAULT_CLIENT_IP_ADDR);
				FNIRS_DBG("use default client ip: %s", DEFAULT_CLIENT_IP_ADDR);
			}
			else
			{
				g_fnirs.client_ip = inet_addr(net_cfg.client_ip);
				FNIRS_INFO("client: %s", net_cfg.client_ip);
			}
		}
		else
		{
			/* 自动获取上位机IP、掩码、端口等信息 */
			fnirs_search_host_ip();
		}
	}
}

/*************************************************
 * @description			:	监听并搜寻上位机广播，获
 * 							取其IP、
 * @param - 			:	无
 * @return 				:	无
**************************************************/
static void fnirs_search_host_ip(void)
{
	int ret = 0;
	int recv_len = 0;
	struct sockaddr_in addr;
	socklen_t from_len;
	struct timeval tv;
	fd_set rd_set;
	fd_set rd_set_cp;
	uint8_t buf[BROAD_CAST_RECV_BUF_LEN] = {0};
 
retry:
	g_fnirs.admin_sock = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (0 > g_fnirs.admin_sock)
	{
		FNIRS_ERR("create sock failed: %d (%s)", errno, strerror(errno));
		goto retry;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(FNIRS_HOST_BROADCAST_PORT);

	if (0 != lwip_bind(g_fnirs.admin_sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)))
	{
		FNIRS_ERR("bind failed: %d (%s)", errno, strerror(errno));
		lwip_shutdown(g_fnirs.admin_sock, 0);
		close(g_fnirs.admin_sock);
		g_fnirs.admin_sock = -1;
		vTaskDelay(pdMS_TO_TICKS(1000));
		goto retry;
	}

	FD_ZERO(&rd_set);
	FD_SET(g_fnirs.admin_sock, &rd_set);

	FNIRS_INFO("searching for host...");

	while (1)
	{
		vTaskDelay(pdMS_TO_TICKS(10));
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		rd_set_cp = rd_set;
		ret = select(g_fnirs.admin_sock + 1, &rd_set_cp, NULL, NULL, &tv);
		if (0 > ret && (EINTR != errno))
		{
			FNIRS_ERR("select failed: %d (%s)", errno, strerror(errno));
			lwip_shutdown(g_fnirs.admin_sock, 0);
			close(g_fnirs.admin_sock);
			g_fnirs.admin_sock = -1;
			vTaskDelay(pdMS_TO_TICKS(1000));
			goto retry;
		}
		else if (0 == ret)
		{
			FNIRS_DBG("time out");
			continue;
		}
		if (FD_ISSET(g_fnirs.admin_sock, &rd_set_cp))
		{
			recv_len = recvfrom(g_fnirs.admin_sock, buf, BROAD_CAST_RECV_BUF_LEN, 0, (struct sockaddr*)&g_fnirs.from, &from_len);
			if (0 < recv_len)
			{
				if (FNIRS_OK == fnirs_record_host_ip(buf, recv_len))
				{	
					break;
				}
			}
		}
	}
}

/*************************************************
 * @description			:	记录上位机广播，获
 * 							取其IP、
 * @param - buf			:	广播数据地址
 * @param - len			:	广播数据长度
 * @return 				:	无
**************************************************/
static int fnirs_record_host_ip(uint8_t *buf, int len)
{
	int i = 0;
	uint16_t head = 0;
	uint8_t crc = 0;
	uint32_t ip = 0;
	uint32_t mask = 0;
	uint8_t tcp_port_num = 0;
	uint8_t udp_port_num = 0;
	uint16_t port = 0;
	uint8_t offset = 0;
	uint16_t tcp_port = 0;
	uint16_t udp_port = 0;

	if (NULL == buf || 0 == len)
	{
		FNIRS_ERR("input null");
		return FNIRS_FAIL;
	}

	head = ntohs(*((uint16_t *)buf));
	if (FNIRS_HOST_BROADCAST_MSG_HEAD != head ||
		11 >= len)
	{
		return FNIRS_FAIL;
	}

	crc = crc_8bit_mask(buf, len - 1, 0xd5);
	if (crc != buf[len - 1])
	{
		FNIRS_ERR("crc check failed expect: 0x%02x calculated: 0x%02x", buf[len - 1], crc);
		return FNIRS_FAIL;
	}

	/* 报文解析 */
	ip = buf[FNIRS_BROADCAST_IP + 3] | (buf[FNIRS_BROADCAST_IP + 2]<<8) |
		(buf[FNIRS_BROADCAST_IP + 1]<<16) | (buf[FNIRS_BROADCAST_IP]<<24);
	FNIRS_DBG("host ip: %u.%u.%u.%u", buf[FNIRS_BROADCAST_IP], buf[FNIRS_BROADCAST_IP + 1],
						buf[FNIRS_BROADCAST_IP + 2], buf[FNIRS_BROADCAST_IP + 3]);
	g_fnirs.host_ip = ntohl(ip);

	mask = buf[FNIRS_BROADCAST_MASK + 3] | (buf[FNIRS_BROADCAST_MASK + 2]<<8) |
		(buf[FNIRS_BROADCAST_MASK + 1]<<16) | (buf[FNIRS_BROADCAST_MASK]<<24);
	FNIRS_DBG("mask: %u.%u.%u.%u", buf[FNIRS_BROADCAST_MASK], buf[FNIRS_BROADCAST_MASK + 1],
						buf[FNIRS_BROADCAST_MASK + 2], buf[FNIRS_BROADCAST_MASK + 3]);
	g_fnirs.mask = ntohl(mask);

	tcp_port_num = buf[FNIRS_BROADCAST_TCP_PORT_NUM];
	offset = FNIRS_BROADCAST_TCP_PORT_NUM + 1;
	FNIRS_DBG("tcp port num: %u", tcp_port_num);
	for (i = 0; i < tcp_port_num; i++)
	{
		port = *(uint16_t*)(buf + offset + i*sizeof(uint16_t));
		port = ntohs(port);
		FNIRS_DBG("port[%d]: %u", i + 1, port);
	}
	if (1 == tcp_port_num)
	{
		tcp_port = *(uint16_t*)(buf + offset);
	}
	else
	{
		tcp_port = *(uint16_t*)(buf + offset + PORT_SELECT_INDEX * sizeof(uint16_t));
	}

	udp_port_num = buf[offset + tcp_port_num*sizeof(uint16_t)];
	offset += tcp_port_num*sizeof(uint16_t) + 1;
	FNIRS_DBG("udp port num: %u", udp_port_num);
	for (i = 0; i < udp_port_num; i++)
	{
		port = *(uint16_t*)(buf + offset + i*sizeof(uint16_t));
		port = ntohs(port);
		FNIRS_DBG("port[%d]: %u", i + 1, port);
	}
	if (1 == udp_port_num)
	{
		udp_port = *(uint16_t*)(buf + offset);
	}
	else
	{
		udp_port = *(uint16_t*)(buf + offset + PORT_SELECT_INDEX * sizeof(uint16_t));
	}
	
	g_fnirs.port = (SOCK_DGRAM == g_fnirs.protocol) ? ntohs(udp_port) : ntohs(tcp_port);

	return FNIRS_OK;
}

/*************************************************
 * @description			:	连接上位机网络
 * @param - 			:	无
 * @return 				:	FNIRS_ERR_T
**************************************************/
static int fnirs_network_connect(void)
{
	uint8_t *adr = NULL;
	int opt = 0;
	int flags = 0;
	struct timeval timeout = {0};

	g_fnirs.dst.sin_addr.s_addr = g_fnirs.host_ip;
	g_fnirs.dst.sin_family = AF_INET;
	g_fnirs.dst.sin_port = htons(g_fnirs.port);

retry:
	g_fnirs.data_sock = lwip_socket(AF_INET, g_fnirs.protocol, IPPROTO_IP);
	if (g_fnirs.data_sock < 0)
	{
		FNIRS_ERR("create sock failed: %d (%s)", errno, strerror(errno));
		goto error_exit;
	}

	if (SOCK_STREAM == g_fnirs.protocol)
	{
		if (0 != connect(g_fnirs.data_sock, (struct sockaddr *)&g_fnirs.dst, sizeof(struct sockaddr_in)))
		{
			FNIRS_ERR("connect failed: %d (%s)", errno, strerror(errno));
			lwip_shutdown(g_fnirs.data_sock, 0);
			close(g_fnirs.data_sock);
			g_fnirs.data_sock = -1;
			vTaskDelay(pdMS_TO_TICKS(1000));
			goto retry;
		}
		memcpy(&g_fnirs.dst_s, &g_fnirs.dst, sizeof(g_fnirs.dst));

		opt = 1;
		if (0 != setsockopt(g_fnirs.data_sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)))
		{
			FNIRS_ERR("set tcp no delay errno: %d (%s)", errno, strerror(errno));
			goto error_exit;
		}

		flags = fcntl(g_fnirs.data_sock, F_GETFL);
		if (0 != fcntl(g_fnirs.data_sock, F_SETFL, flags | O_NONBLOCK))
		{
			FNIRS_ERR("set socket non blocking errno: %d (%s)", errno, strerror(errno));
			goto error_exit;
		}

		timeout.tv_sec = 0;
		timeout.tv_usec = 10;
		setsockopt(g_fnirs.data_sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
	}
	else
	{
		g_fnirs.src.sin_addr.s_addr = g_fnirs.client_ip;
		g_fnirs.src.sin_family = AF_INET;
		g_fnirs.src.sin_port = htons(g_fnirs.port + 1);

		if (lwip_bind(g_fnirs.data_sock, (struct sockaddr *)&g_fnirs.src, sizeof(g_fnirs.src)) != 0 )
		{
			FNIRS_ERR("socket bind error errno: %d (%s)", errno, strerror(errno));
		}
	}

	adr = (uint8_t*)&g_fnirs.host_ip;
	FNIRS_INFO("connect to server %u.%u.%u.%u:%u success ", adr[0], adr[1], adr[2], adr[3], g_fnirs.port);

	return FNIRS_OK;

error_exit:
	if (-1 < g_fnirs.data_sock)
	{
		lwip_shutdown(g_fnirs.data_sock, 0);
		close(g_fnirs.data_sock);
	}
	return FNIRS_FAIL;
}
