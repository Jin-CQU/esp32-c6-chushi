/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: uart.c
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V1.0
描述	   	: 串口驱动
其他	   	: 无
日志	   	: 初版V1.0 2025/1/24 刘有为创建
***********************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "errno.h"
#include "uart.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "utils.h"
#include "freertos/FreeRTOS.h"
#include "semphr.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"

#define UART_TAG "UART"
#define UART_INFO(fmt, ...) ESP_LOGI(UART_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define UART_DBG(fmt, ...) ESP_LOGW(UART_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define UART_ERR(fmt, ...) ESP_LOGE(UART_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)

/* 用于crc校验的buf格式 */
typedef struct _UART_MSG_CRC
{
	uint16_t head;
	uint8_t type;
	uint8_t len;
	uint8_t value[UART_MSG_MAX_VALUE_LEN];
} UART_MSG_CRC;

/* 字符接收队列 */
typedef struct _UART_RECV_QUEUE
{
	volatile uint32_t head;
	volatile uint32_t tail;
	volatile uint32_t size;
	uint8_t *base;
}UART_RECV_QUEUE;

/* 任务队列元素 */
typedef struct _UART_TASK_ELEM
{
	uint8_t type;								/* 串口消息类型 */
	uint8_t argc;								/* 数据长度 */
	uint8_t argv[UART_MSG_MAX_VALUE_LEN];		/* 串口数据地址 */
	UART_TASK_HANDLE_FUNC *func;				/* 处理函数地址 */
}UART_TASK_ELEM;

static UART_ERR_T recv_queue_init(UART_RECV_QUEUE *q, uint32_t size);
static void recv_queue_destroy(UART_RECV_QUEUE *q);
static bool recv_queue_is_empty(UART_RECV_QUEUE *q);
static bool recv_queue_is_full(UART_RECV_QUEUE *q);
static UART_ERR_T recv_en_queue(UART_RECV_QUEUE *q, uint8_t x);
static UART_ERR_T recv_de_queue(UART_RECV_QUEUE *q, uint8_t *x);
static uint32_t recv_queue_get_len(UART_RECV_QUEUE *q);

static void uart_recv_task(void *pvParameters);
static void uart_process_task(void *pvParameters);
void uart_process_task_inner(UART_TASK_ELEM *param);
void *uart_task_handler_pthread(void *arg);
static UART_ERR_T uart_recv_queue_parse(UART_RECV_QUEUE *q, UART_TASK_ELEM *param);

typedef struct _UART_GLOBAL
{
	UART_RECV_QUEUE queue;							/* 串口接收队列 */
	UART_TASK_HANDLER handlers[UART_TYPE_NUM];		/* 处理函数池 */
	uint8_t* recv_buf;								/* 串口接收数据地址 */
} UART_GLOBAL;

UART_GLOBAL g_uart = {0};

/*************************************************
 * @description			:	串口驱动初始化
 * @param 				:	无
 * @return 				:	UART_ERR_T
**************************************************/
UART_ERR_T uart_init(void)
{
	int i = 0;
	const uart_config_t uart_config = {
		.baud_rate = 	UART_BAUDRATE,
		.data_bits = 	UART_DATA_BITS,
		.parity = 		UART_PARITY,
		.stop_bits = 	UART_STOP_BITS,
		.flow_ctrl = 	UART_HW_FLOW_CTRL,
		.source_clk = 	UART_CLK_SRC,
	};

	if (ESP_OK != uart_driver_install(UART_PORT, UART_RX_BUF_SIZE * 2, UART_TX_BUF_SIZE * 2, 0, NULL, 0))
	{
		UART_ERR("uart driver install failed");
		goto error_exit;
	}
	if (ESP_OK != uart_param_config(UART_PORT, &uart_config))
	{
		UART_ERR("set uart param failed");
		goto error_exit;
	}
	if (ESP_OK != uart_set_pin(UART_PORT, UART_TXD_PIN, UART_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE))
	{
		UART_ERR("set uart pin failed");
		goto error_exit;
	}

	/* 初始化数据队列 */
	if (UART_OK != recv_queue_init(&g_uart.queue, UART_RECV_QUEUE_SIZE))
	{
		UART_ERR("init queue failed errno: %d (%s)", errno, strerror(errno));
		goto error_exit;
	}

	g_uart.recv_buf = (uint8_t*) malloc(UART_RX_BUF_SIZE);
	if (NULL == g_uart.recv_buf)
	{
		UART_ERR("Unable to malloc: errno %d (%s)", errno, strerror(errno));
		goto error_exit;
	}
	bzero(g_uart.recv_buf, UART_RX_BUF_SIZE);

	/* 初始化处理函数池 */
	for (i = 0; i < UART_TYPE_NUM; i++)
	{
		g_uart.handlers[i].type = UART_TYPE_IDLE;
		bzero(g_uart.handlers[i].info, UART_TASK_HANDLER_INFO_SIZE);
		g_uart.handlers[i].func = NULL;
	}

	xTaskCreate(uart_recv_task, "uart recv task", 4096, NULL, 5, NULL);
	xTaskCreate(uart_process_task, "uart process task", 8192, NULL, 5, NULL);

	UART_INFO("uart init done...");

	return UART_OK;

error_exit:
	return UART_FAIL;
}

/*************************************************
 * @description			:	串口接收线程
 * @param - pvParameters:	线程参数
 * @return 				:	无
**************************************************/
static void uart_recv_task(void *pvParameters)
{
	uint8_t* data = NULL;
	UART_TASK_ELEM param = {0};
	int recv_len = 0;
	int i = 0;
	int ret = 0;
	uint8_t len = 0;
	uint8_t offset = 0;

	UART_INFO("monitoring uart...");
	data = g_uart.recv_buf;
	while (true)
	{
		/* 接收stm32数据并放入队列 */
		recv_len = uart_read_bytes(UART_PORT, data, UART_RX_BUF_SIZE, 100 / portTICK_PERIOD_MS);
		if (recv_len > 0) 
		{
			offset = 0;
			/* 低速状态下，串口可以收到整齐的报文 */
			if ((UART_MSG_HEAD>>8) == data[UART_MSG_OFT_HEAD] &&
				(UART_MSG_HEAD&0x00ff) == data[UART_MSG_OFT_HEAD + 1] &&
				UART_TYPE_SEMG != data[UART_MSG_OFT_TYPE] &&
				UART_TYPE_FNIRS != data[UART_MSG_OFT_TYPE] &&
				UART_MSG_MAX_VALUE_LEN >= data[UART_MSG_OFT_LEN])
			{
				//UART_INFO("uart recv %d bytes", recv_len);
				//hex_dump(data, recv_len, NULL);
				len = data[UART_MSG_OFT_LEN];
				/* crc offset */
				offset = UART_MSG_OFT_VALUE + len;
				if (data[offset] != crc_8bit_mask(data, offset, UART_CRC_MASK) || 
					(UART_MSG_TAIL>>8) != data[offset + 1] ||
					(UART_MSG_TAIL&0x00ff) != data[offset + 2])
				{
					UART_ERR("crc verify failed offset: %u tail: 0x%02x%02x", offset, data[offset + 1], data[offset + 2]);
					// hex_dump(data, recv_len, NULL);
				}
				else
				{
					// UART_INFO("get correct msg");
					offset += UART_MSG_CRC_TAIL_LEN;
					param.type = data[UART_MSG_OFT_TYPE];
					param.argc = data[UART_MSG_OFT_LEN];
					if (0 < len)
					{
						memcpy(param.argv, &data[UART_MSG_OFT_VALUE], len);
					}
					param.func = uart_get_handler_func(param.type);
					if (NULL == param.func)
					{
						UART_ERR("type: 0x%02x handler func did not register", param.type);
					}
					else
					{
						uart_process_task_inner(&param);
					}
				}
			}
			/* 按字节入队 */
			for (i = offset; i < recv_len; i++)
			{
				do
				{
					ret = recv_en_queue(&g_uart.queue, data[i]);
					if (UART_OK != ret)
					{
						vTaskDelay(1 / portTICK_PERIOD_MS);
					}
				}while(UART_OK != ret);
			}
			//UART_INFO("uart recv %d bytes", recv_len);
			//hex_dump(data, recv_len, NULL);
		}
	}

	vTaskDelete(NULL);
}

/*************************************************
 * @description			:	串口数据处理线程
 * @param - pvParameters:	线程参数
 * @return 				:	无
**************************************************/
static void uart_process_task(void *pvParameters)
{
	UART_TASK_ELEM *param = NULL;

	param = (UART_TASK_ELEM *)malloc(sizeof(UART_TASK_ELEM));
	if (NULL == param)
	{
		UART_ERR("Unable to malloc: errno %d (%s)", errno, strerror(errno));
		goto exit;
	}

	UART_INFO("processing uart msg...");

	while (true)
	{
		/* 报文处理 */
		vTaskDelay(1);
		bzero(param, sizeof(UART_TASK_ELEM));
		if (UART_OK != uart_recv_queue_parse(&g_uart.queue, param))
		{
			continue;
		}
		// UART_INFO("usage rate %lu - %lu", recv_queue_get_len(&g_uart_recv_queue), g_uart_recv_queue.size);

		switch (param->type)
		{
			case UART_TYPE_SEMG:
			case UART_TYPE_FNIRS:
				/* 高频数据，直接处理，不额外创建线程 */
				if (param->func)
				{
					param->func(param->argc, param->argv);
				}
				break;

			case UART_TYPE_REDIR:
			case UART_TYPE_BATTERY:
			case UART_TYPE_VERSION:
				/* 创建线程进行处理 */
				uart_process_task_inner(param);
				break;

			default:
				continue;
				break;
		}
	}

exit:
	if (NULL != param)
	{
		free(param);
	}
	vTaskDelete(NULL);
}

/*************************************************
 * @description			:	处理串口消息
 * @param - param		:	线程参数
 * @return 				:	无
**************************************************/
void uart_process_task_inner(UART_TASK_ELEM *param)
{
	UART_TASK_ELEM *thread_param = NULL;
	pthread_t pid;

	if (NULL == param)
	{
		UART_ERR("param null");
		return;
	}

	if (NULL == param->func)
	{
		UART_ERR("handler function not found in g_uart_handlers type: 0x%02x", param->type);
		return;
	}

	/* 部分报文无需创建线程处理 */
	switch (param->type)
	{
		case UART_TYPE_FNIRS:
			param->func(param->argc, param->argv);
			return;
			break;
	}

	thread_param = (UART_TASK_ELEM *)malloc(sizeof(UART_TASK_ELEM));
	if (NULL != thread_param)
	{
		/* 保存参数 */
		memcpy(thread_param, param, sizeof(UART_TASK_ELEM));

		/* 创建线程进行处理 */
		if (0 != pthread_create(&pid, NULL, uart_task_handler_pthread, (void *)thread_param))
		{
			UART_ERR("create thread failed errno: %d (%s)", errno, strerror(errno));
			return;
		}
		else
		{
			pthread_detach(pid);
		}
	}
	else
	{
		UART_ERR("Unable to malloc: errno %d (%s)", errno, strerror(errno));
		return;
	}
}

/*************************************************
 * @description			:	处理串口消息(线程式)
 * @param - param		:	线程参数
 * @return 				:	无
**************************************************/
void *uart_task_handler_pthread(void *arg)
{
	UART_TASK_ELEM *param = NULL;

	if (NULL == arg)
	{
		UART_ERR("param null");
		return NULL;
	}

	param = (UART_TASK_ELEM *)arg;
	if (NULL == param->func)
	{
		UART_ERR("handler function is null, type: 0x%02x", param->type);
		return NULL;
	}
	param->func(param->argc, param->argv);
	
	free(arg);
	return NULL;
}

/*************************************************
 * @description			:	串口数据解析函数
 * @param - q			:	队列句柄
 * @param - param		:	队列任务地址
 * @return 				:	UART_ERR_T
**************************************************/
static UART_ERR_T uart_recv_queue_parse(UART_RECV_QUEUE *q, UART_TASK_ELEM *param)
{
	uint8_t d0 = 0;
	uint8_t d1 = 0;
	uint8_t type = 0;
	uint8_t len = 0;
	uint8_t crc = 0;
	uint8_t crc_act = 0;
	uint8_t pos = 0;
	uint32_t queue_len = 0;
	UART_MSG_CRC msg_crc = {0};

	if (NULL == q || NULL == param)
	{
		UART_ERR("param null");
		return UART_FAIL;
	}

	queue_len = recv_queue_get_len(q);
	if (UART_MSG_MIN_LEN > queue_len)
	{
		return UART_FAIL;
	}

	/* 检测首部 */
	recv_de_queue(q, &d1);
	if ((UART_MSG_HEAD>>8) != d1)
	{
		return UART_FAIL;
	}
	recv_de_queue(q, &d0);
	if ((UART_MSG_HEAD&0x00ff) != d0)
	{
		return UART_FAIL;
	}

	/* 检测到首部 */
	recv_de_queue(q, &type);
	recv_de_queue(q, &len);

	if (0 == len)
	{
		/* 直接检测crc+尾部 */
		recv_de_queue(q, &crc);
		msg_crc.head = htons(UART_MSG_HEAD);
		msg_crc.type = type;
		msg_crc.len = len;
		if (crc != crc_8bit_mask((uint8_t*)&msg_crc, (UART_MSG_MIN_LEN - UART_MSG_CRC_TAIL_LEN), UART_CRC_MASK))
		{
			UART_ERR("crc verify failed");
			return UART_FAIL;
		}
		recv_de_queue(q, &d1);
		if ((UART_MSG_TAIL>>8) == d1)
		{
			recv_de_queue(q, &d0);
			if ((UART_MSG_TAIL&0x00ff) == d0)
			{
				/* 检测到帧尾部 */
				param->type = type;
				param->argc = len;
				param->func = uart_get_handler_func(type);
				if (NULL == param->func)
				{
					UART_ERR("type: 0x%02x has not register", type);
					return UART_FAIL;
				}
				return UART_OK;
			}
		}
		return UART_FAIL;
	}
	else if (UART_MSG_MAX_VALUE_LEN < len)
	{
		UART_ERR("the message is too large: %d", len);
		return UART_FAIL;
	}
	else
	{
		/* 解析参数 */
		queue_len -= (UART_MSG_MIN_LEN - UART_MSG_CRC_TAIL_LEN);	/* head + type + len的长度 */
		if (len + UART_MSG_CRC_TAIL_LEN > queue_len)
		{
			return UART_FAIL;
		}
		recv_de_queue(q, &msg_crc.value[0]);
		for (pos = 1; pos < len; pos++)
		{
			recv_de_queue(q, &msg_crc.value[pos]);
			if ((UART_MSG_TAIL>>8) == msg_crc.value[pos - 1] &&
				(UART_MSG_TAIL&0x00ff) == msg_crc.value[pos])
			{
				/* 提前检测到尾部 */
				//UART_ERR("get short msg pos: %u", pos);
				// hex_dump((uint8_t*)msg_crc, pos + UART_MSG_MIN_LEN - UART_MSG_CRC_TAIL_LEN + 1, NULL);
				return UART_FAIL;
			}
		}
		recv_de_queue(q, &crc);
		msg_crc.head = htons(UART_MSG_HEAD);
		msg_crc.type = type;
		msg_crc.len = len;
		crc_act = crc_8bit_mask((uint8_t*)&msg_crc, UART_MSG_MIN_LEN - UART_MSG_CRC_TAIL_LEN + len, UART_CRC_MASK);
		if (crc != crc_act)
		{
			UART_ERR("crc verify failed len: %u expect 0x%02x actual: 0x%02x", len, crc, crc_act);
			return UART_FAIL;
		}
		recv_de_queue(q, &d1);
		if ((UART_MSG_TAIL>>8) == d1)
		{
			recv_de_queue(q, &d0);
			if ((UART_MSG_TAIL&0x00ff) == d0)
			{
				/* 检测到帧尾部 */
				param->type = type;
				param->argc = len;
				param->func = uart_get_handler_func(type);
				if (NULL == param->func)
				{
					UART_ERR("type: 0x%02x has not register", type);
					return UART_FAIL;
				}
				memcpy(param->argv, msg_crc.value, len);
				return UART_OK;
			}
		}
		return UART_FAIL;
	}
}

/*************************************************
 * @description			:	队列初始化
 * @param - q			:	队列句柄
 * @param - size		:	队列大小
 * @return 				:	UART_ERR_T
**************************************************/
static UART_ERR_T recv_queue_init(UART_RECV_QUEUE *q, uint32_t size)
{
	q->base = (uint8_t *)malloc(size + 1);
	if (NULL == q->base)
	{
		return UART_FAIL;
	}
	q->head = 0;
	q->tail = 0;
	q->size = size;

	return UART_OK;
}

/*************************************************
 * @description			:	队列注销
 * @param - q			:	队列句柄
 * @return 				:	UART_ERR_T
**************************************************/
static void recv_queue_destroy(UART_RECV_QUEUE *q)
{
	q->head = 0;
	q->tail = 0;
	q->size = 0;
	free(q->base);
}

/*************************************************
 * @description			:	队列判空
 * @param - q			:	队列句柄
 * @return 				:	bool
**************************************************/
static bool recv_queue_is_empty(UART_RECV_QUEUE *q)
{
	return (q->head == q->tail);
}

/*************************************************
 * @description			:	队列判满
 * @param - q			:	队列句柄
 * @return 				:	bool
**************************************************/
static bool recv_queue_is_full(UART_RECV_QUEUE *q)
{
	return ((q->tail + 1)%(q->size + 1) == q->head);
}

/*************************************************
 * @description			:	数据入队
 * @param - q			:	队列句柄
 * @param - x			:	入队字节
 * @return 				:	UART_ERR_T
**************************************************/
static UART_ERR_T recv_en_queue(UART_RECV_QUEUE *q, uint8_t x)
{
	if (true == recv_queue_is_full(q))
	{
		return UART_FAIL;
	}
	q->base[q->tail] = x;
	q->tail = (q->tail + 1)%(q->size + 1);

	return UART_OK;
}

/*************************************************
 * @description			:	数据出队
 * @param - q			:	队列句柄
 * @param - x			:	出队数据存放地址
 * @return 				:	UART_ERR_T
**************************************************/
static UART_ERR_T recv_de_queue(UART_RECV_QUEUE *q, uint8_t *x)
{
	if (true == recv_queue_is_empty(q))
	{
		return UART_FAIL;
	}
	*x = q->base[q->head];
	q->head = (q->head + 1)%(q->size + 1);

	return UART_OK;
}

/*************************************************
 * @description			:	获取队列有效数据
 * @param - q			:	队列句柄
 * @return 				:	数据长度
**************************************************/
static uint32_t recv_queue_get_len(UART_RECV_QUEUE *q)
{
	return (q->tail >= q->head) ? (q->tail - q->head) : (q->size - q->head + q->tail);
}

/*************************************************
 * @description			:	注册串口消息处理函数
 * @param - type		:	串口消息类型
 * @param - info		:	有关信息
 * @param - func		:	处理函数
 * @return 				:	UART_ERR_T
**************************************************/
UART_ERR_T uart_handler_register(uint8_t type, const char *info, UART_TASK_HANDLE_FUNC *func)
{
	int i = 0;

	if (NULL == info || NULL == func || UART_TYPE_IDLE == type)
	{
		UART_ERR("param null type: 0x%02x", type);
		return UART_FAIL;
	}
	if (UART_MSG_MAX_VALUE_LEN <= strlen(info))
	{
		UART_ERR("info is too large(%u bytes), request less than %d bytes", strlen(info), UART_MSG_MAX_VALUE_LEN);
		return UART_FAIL;
	}

	/* 检查是否已经注册 */
	for (i = 0; i < UART_TYPE_NUM; i++)
	{
		if (type == g_uart.handlers[i].type)
		{
			UART_ERR("type 0x%02x has been registered", type);
			return UART_FAIL;
		}
	}

	for (i = 0; i < UART_TYPE_NUM; i++)
	{
		/* 查找空闲结点并注册 */
		if (UART_TYPE_IDLE == g_uart.handlers[i].type && NULL == g_uart.handlers[i].func)
		{
			/* 注册到空闲元素 */
			g_uart.handlers[i].type = type;
			bzero(g_uart.handlers[i].info, UART_MSG_MAX_VALUE_LEN);
			memcpy(g_uart.handlers[i].info, info, strlen(info));
			g_uart.handlers[i].func = func;
			UART_INFO("register type: 0x%02x handler func into pool success index: %d", type, i);
			return UART_OK;
		}
	}

	UART_ERR("uart handler pool is full");
	return UART_FAIL;
}

/*************************************************
 * @description			:	注销串口消息处理函数
 * @param - type		:	串口消息类型
 * @return 				:	无
**************************************************/
void uart_handler_unregister(uint8_t type)
{
	int i  =0;

	if (UART_TYPE_IDLE == type)
	{
		UART_ERR("illegal uart type: 0x%02x", type);
		return;
	}

	for (i = 0; i < UART_TYPE_NUM; i++)
	{
		if (type == g_uart.handlers[i].type)
		{
			g_uart.handlers[i].type = UART_TYPE_IDLE;
			bzero(g_uart.handlers[i].info, UART_TASK_HANDLER_INFO_SIZE);
			g_uart.handlers[i].func = NULL;
			UART_INFO("unregister type 0x%02x handler func", type);
			return;
		}
	}

	UART_INFO("type 0x%02x handler func has been unregister", type);
}

/*************************************************
 * @description			:	获取消息处理函数
 * @param - type		:	串口消息类型
 * @return 				:	处理函数地址
**************************************************/
UART_TASK_HANDLE_FUNC *uart_get_handler_func(uint8_t type)
{
	int i  =0;

	for (i = 0; i < UART_TYPE_NUM; i++)
	{
		if (type == g_uart.handlers[i].type)
		{
			return g_uart.handlers[i].func;
		}
	}

	return NULL;
}

/*************************************************
 * @description			:	打印已注册的处理函数
 * @param - 			:	无
 * @return 				:	无
**************************************************/
void uart_handler_dump(void)
{
	int i  =0;

	for (i = 0; i < UART_TYPE_NUM; i++)
	{
		if (UART_TYPE_IDLE != g_uart.handlers[i].type)
		{
			UART_INFO("[%d]: type: 0x%02x func: %p info: %s",
				i, g_uart.handlers[i].type, g_uart.handlers[i].func, g_uart.handlers[i].info);
		}
	}
}

/*************************************************
 * @description			:	获取消息处理函数
 * @param - data		:	待发送数据地址
 * @param - len			:	待发送数据长度
 * @return 				:	UART_ERR_T
**************************************************/
UART_ERR_T uart_data_transmit(uint8_t *data, uint8_t len)
{
	if (NULL == data || 0 == len)
	{
		UART_ERR("param null or led: %u", len);
		return UART_FAIL;
	}

	uart_write_bytes(UART_PORT, data, len);
	return UART_OK;
}
