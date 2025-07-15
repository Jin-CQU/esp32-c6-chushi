/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: eeg.c
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V2.0
描述	   	: 脑电部分
其他	   	: 无
日志	   	: V2.0 2025/1/20 刘有为创建
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
#include "eeg.h"
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
#include "battery.h"
#include "version.h"
#include "esp_mac.h"
#include "key.h"
#include "uart.h"
//#if ESP_IDF_VERSION <= ESP_IDF_VERSION_VAL(5, 1, 0)
#include "freertos/timers.h"
//#endif

#define EEG_TAG "EEG"
#define EEG_INFO(fmt, ...) ESP_LOGI(EEG_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define EEG_DBG(fmt, ...) ESP_LOGW(EEG_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define EEG_ERR(fmt, ...) ESP_LOGE(EEG_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)

#define SET_BIT(reg, bit) 		((reg) |= (bit))
/* 右移 */
#define R_SHIFT(val, bit) 		((val) << (bit))

/* 发送状态 */
enum {
	EEG_DATA_NOT_SEND = 0,
	EEG_DATA_SEND = 1,
};

/* 函数声明 */
static void ads129x_send_cmd(uint8_t cmd);
static void ads129x_read_reg(uint8_t reg, uint8_t *data);
static void ads129x_write_reg(uint8_t reg, uint8_t *data);
static void ads129x_read_all_regs(uint8_t *regs);
static void ads129x_write_all_regs(uint8_t *regs);
static void ads129x_read_regs_from_context(uint8_t *reg_arr);
static void IRAM_ATTR drdy_isr_handler(void *arg);
static void eeg_monitor(TimerHandle_t xTimer);
static void eeg_crc_table_init(uint8_t mask);
static inline uint8_t eeg_crc_cal(uint8_t *buf, int len);
static int eeg_network_connect(void);
static void *eeg_send_task(void *arg);
static void eeg_sync_params_from_flash(void);
static void eeg_sync_params_1kHz(void);
static void eeg_sync_params_2kHz(void);
static void eeg_sync_params_4kHz(void);
static void eeg_sync_params_8kHz(void);
static void eeg_search_host_ip(void);
static int eeg_record_host_ip(uint8_t *buf, int len);
static void eeg_info_init(void);
static void eeg_info_report(void);
void ads129x_cmd_data_process(uint8_t argc, uint8_t *argv);

typedef struct _ADS129X_REGS
{
	ADS129X_CONFIG1 config1;
	ADS129X_CONFIG2 config2;
	ADS129X_CONFIG3 config3;
	ADS129X_LOFF loff;
	ADS129X_CHSET chset[ADS129X_CHN_NUM];
#if (CONFIG_ADS129X_TYPE == 1294)
	ADS1294_RLD_SENS rld_sens_p;
	ADS1294_RLD_SENS rld_sens_n;
#elif (CONFIG_ADS129X_TYPE == 1299)
	ADS1299_BIAS_SENS bias_sens_p;
	ADS1299_BIAS_SENS bias_sens_n;
#endif
	ADS129X_LOFF_SENS loff_sens_p;
	ADS129X_LOFF_SENS loff_sens_n;
	ADS129X_LOFF_FLIP loff_flip;
	ADS129X_GPIO gpio;
#if (CONFIG_ADS129X_TYPE == 1294)
	ADS1294_PACE pace;
	ADS1294_RESP resp;
#elif (CONFIG_ADS129X_TYPE == 1299)
	ADS1299_MISC1 misc1;
	ADS1299_MISC2 misc2;
#endif
	ADS129X_CONFIG4 config4;
#if (CONFIG_ADS129X_TYPE == 1294)
	ADS1294_WCT1 wct1;
	ADS1294_WCT2 wct2;
#endif
}ADS129X_REGS;

typedef struct _EEG_GLOBAL {
	uint32_t serial;						/* 本机序列号 */
	uint8_t state;							/* 功能状态 */
	uint8_t freq;							/* 采样率 */
	uint8_t protocol;						/* 传输协议 */
	uint8_t connect_type;					/* 配网方式 */
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
	pthread_t pid;							/* 发送线程 */
	uint8_t snd_state;						/* 发送状态 */
	uint8_t buf_idx;						/* 当前环形缓冲区序号 */
	uint8_t snd_buf_idx;					/* 待发送的缓冲区子区序号 */
	uint8_t *snd_buf;						/* 环形缓冲区地址 */
	uint32_t snd_rate;						/* 发送率 */
	spi_device_handle_t hspi;				/* SPI通信句柄 */
	uint16_t data_cnt;						/* 数据点累计值 */
	uint16_t patch_size;					/* 上报上位机的数据包中数据点的个数，随采样率变化 */
	uint16_t frame_total_len;				/* 上报上位机的数据包中一个通道数据帧总长 */
	uint16_t frame_oft_crc;					/* 数据帧内crc位的偏移 */
	uint16_t frame_oft_ts;					/* 数据帧内时间戳位的偏移 */
	uint8_t *sub_bufs[SEND_BUF_NUM];		/* 环形缓冲区子区地址 */
	uint8_t crc_table[EEG_CRC_TABLE_SIZE];	/* crc表 */
	ADS129X_REGS regs;						/* ads1294寄存器 */
	uint8_t bp_filter;						/* 带通滤波器开关状态 */
	uint8_t hp_filter;						/* 带通滤波器开关状态 */
	uint8_t lp_filter;						/* 带通滤波器开关状态 */
	uint8_t notch_filter;					/* 陷波滤波器开关状态 */
#if SUPPORT_NOTCH_FILTER
	double n_a[NOTCH_FILTER_DEGREE + 1];	/* 陷波滤波参数a项 */
	double n_b[NOTCH_FILTER_DEGREE + 1];	/* 陷波滤波参数b项 */
#endif
#if MONITOR_FILTER_RUN_TIME
	int64_t t_start;
	int64_t t_stop;
#endif
	uint8_t info_report;					/* 版本信息等上报周期 */
	uint8_t info_buf[INFO_TOTAL_LEN];		/* 版本信息等地址 */
	uint8_t led_color;						/* 记录电池对应的led颜色 */
} EEG_GLOBAL;

EEG_GLOBAL g_eeg = {
	.state = 								EEG_STATE_ON,
	.serial = 								EEG_DEFAULT_SERIAL_NUM,
	.freq = 								EEG_DEFAULT_FREQUENCY,
	.protocol = 							EEG_DEFAULT_PROTOCOL,
	.data_sock = 							-1,
	.admin_sock = 							-1,
	.port = 								EEG_DEFAULT_HOST_PORT,
	.host_ip = 								0,
	.mask = 								0,
	.client_ip = 							0,
	.dst = 									{0},
	.src = 									{0},
	.dst_s = 								{0},
	.snd_state = 							0,
	.buf_idx = 								0,
	.snd_buf_idx = 							0,
	.snd_buf = 								NULL,
	.snd_rate = 							0,
	.hspi = 								NULL,
	.data_cnt = 							0,
	.patch_size = 							0,
	.frame_total_len = 						0,
	.frame_oft_crc = 						0,
	.frame_oft_ts = 						0,
	.sub_bufs = 							{0},
	.crc_table = 							{0},
	.regs = {
		.config1 = {
			.date_rate = 				ADS129X_DATA_RATE_DEFAULT,
#if (CONFIG_ADS129X_TYPE == 1294)
			.reserve1 = 				0,
#elif (CONFIG_ADS129X_TYPE == 1299)
			.reserve1 = 				2,
#endif
			.clk_en = 					ADS129X_CLK_EN,
			.daisy_en = 				ADS129X_DAISY_MODE,
#if (CONFIG_ADS129X_TYPE == 1294)
			.hr = 						ADS1294_POWER_MODE,
#elif (CONFIG_ADS129X_TYPE == 1299)
			.reserve2 = 				1
#endif
		},
		.config2 = {
			.test_freq = 				ADS129X_TEST_FREQ_DEFAULT,
			.test_amp = 				ADS129X_TEST_AMP_DEFAULT,
			.reserve1 = 				0,
			.int_test = 				ADS129X_TEST_SIGNAL_SOURCE,
#if (CONFIG_ADS129X_TYPE == 1294)
			.wct_chop = 				ADS1294_WCT_CHOP,
			.reserve2 = 				0,
#elif (CONFIG_ADS129X_TYPE == 1299)
			.reserve2 = 				6
#endif
		},
		.config3 = {
#if (CONFIG_ADS129X_TYPE == 1294)
			.rld_stat = 				0,
			.rld_loff_sens = 			ADS1294_RLD_LOFF_SENS,
			.pd_rld = 					ADS1294_RLD_PWR,
			.rldref_int = 				ADS1294_RLD_REF_SRC,
			.rld_meas = 				ADS1294_RLD_MEAS,
			.vref_4v = 					ADS1294_VREF,
			.reserve = 					1,
			.pd_refbuf = 				ADS1294_INTER_BUF
#elif (CONFIG_ADS129X_TYPE == 1299)
			.bias_stat = 				0,
			.bias_loff_sens = 			ADS1299_BIAS_LOFF_SENS,
			.pd_bias = 					ADS1299_PD_BIAS,
			.biasref_int = 				ADS1299_BIAS_REF_SOURCE,
			.bias_meas = 				ADS1299_BIAS_MEAS,
			.reserve = 					3,
			.pd_refbuf = 				ADS1299_PD_REFBUF
#endif
		},
		.loff = {
			.flead_Off = 				ADS129X_FLEAD,
			.ilead_Off = 				ADS129X_ILEAD,
#if (CONFIG_ADS129X_TYPE == 1294)
			.vlead_off_en = 			ADS1294_VLEAD_EN,
#elif (CONFIG_ADS129X_TYPE == 1299)
			.reserve = 					0,
#endif
			.comp_th = 					ADS129X_COMP_TH
		},
		.chset = {
			/* chn1set */
			{
				.mux = 					ADS129X_CHN1_MUX,
				.gain = 				ADS129X_CHN1_PGA_GAIN,
				.pd = 					ADS129X_CHN1_PD,
#if (CONFIG_ADS129X_TYPE == 1294)
				.reserve = 				0
#elif (CONFIG_ADS129X_TYPE == 1299)
				.srb2 = 				ADS1299_CHN8_SRB2
#endif
			},
			/* chn2set */
			{
				.mux = 					ADS129X_CHN2_MUX,
				.gain = 				ADS129X_CHN2_PGA_GAIN,
				.pd = 					ADS129X_CHN2_PD,
#if (CONFIG_ADS129X_TYPE == 1294)
				.reserve = 				0
#elif (CONFIG_ADS129X_TYPE == 1299)
				.srb2 = 				ADS1299_CHN2_SRB2
#endif
			},
			/* chn3set */
			{
				.mux = 					ADS129X_CHN3_MUX,
				.gain = 				ADS129X_CHN3_PGA_GAIN,
				.pd = 					ADS129X_CHN3_PD,
#if (CONFIG_ADS129X_TYPE == 1294)
				.reserve = 				0
#elif (CONFIG_ADS129X_TYPE == 1299)
				.srb2 = 				ADS1299_CHN3_SRB2
#endif
			},
			/* chn4set */
			{
				.mux = 					ADS129X_CHN4_MUX,
				.gain = 				ADS129X_CHN4_PGA_GAIN,
				.pd = 					ADS129X_CHN4_PD,
#if (CONFIG_ADS129X_TYPE == 1294)
				.reserve = 				0
#elif (CONFIG_ADS129X_TYPE == 1299)
				.srb2 = 				ADS1299_CHN4_SRB2
#endif
			}
#if (CONFIG_ADS129X_TYPE == 1299)
			,
			/* chn5set */
			{
				.mux = 					ADS129X_CHN5_MUX,
				.gain = 				ADS129X_CHN5_PGA_GAIN,
				.pd = 					ADS129X_CHN5_PD,
				.srb2 = 				ADS1299_CHN5_SRB2
			},
			/* chn6set */
			{
				.mux = 					ADS129X_CHN6_MUX,
				.gain = 				ADS129X_CHN6_PGA_GAIN,
				.pd = 					ADS129X_CHN6_PD,
				.srb2 = 				ADS1299_CHN6_SRB2
			},
			/* chn7set */
			{
				.mux = 					ADS129X_CHN7_MUX,
				.gain = 				ADS129X_CHN7_PGA_GAIN,
				.pd = 					ADS129X_CHN7_PD,
				.srb2 = 				ADS1299_CHN7_SRB2
			},
			/* chn8set */
			{
				.mux = 					ADS129X_CHN8_MUX,
				.gain = 				ADS129X_CHN8_PGA_GAIN,
				.pd = 					ADS129X_CHN8_PD,
				.srb2 = 				ADS1299_CHN8_SRB2
			}
#endif
		},
#if (CONFIG_ADS129X_TYPE == 1294)
		.rld_sens_p = {
			.chn1 = 					ADS1294_CHN1_RLD_SENS_P,
			.chn2 = 					ADS1294_CHN2_RLD_SENS_P,
			.chn3 = 					ADS1294_CHN3_RLD_SENS_P,
			.chn4 = 					ADS1294_CHN4_RLD_SENS_P,
			.reserve = 					0
		},
		.rld_sens_n = {
			.chn1 = 					ADS1294_CHN1_RLD_SENS_N,
			.chn2 = 					ADS1294_CHN2_RLD_SENS_N,
			.chn3 = 					ADS1294_CHN3_RLD_SENS_N,
			.chn4 = 					ADS1294_CHN4_RLD_SENS_N,
			.reserve = 					0
		},
#elif (CONFIG_ADS129X_TYPE == 1299)
		.bias_sens_p = {
			.chn1 = 					ADS1294_CHN1_BIAS_SENS_P,
			.chn2 = 					ADS1294_CHN2_BIAS_SENS_P,
			.chn3 = 					ADS1294_CHN3_BIAS_SENS_P,
			.chn4 = 					ADS1294_CHN4_BIAS_SENS_P,
			.chn5 = 					ADS1294_CHN5_BIAS_SENS_P,
			.chn6 = 					ADS1294_CHN6_BIAS_SENS_P,
			.chn7 = 					ADS1294_CHN7_BIAS_SENS_P,
			.chn8 = 					ADS1294_CHN8_BIAS_SENS_P
		},
		.bias_sens_n = {
			.chn1 = 					ADS1294_CHN1_BIAS_SENS_N,
			.chn2 = 					ADS1294_CHN2_BIAS_SENS_N,
			.chn3 = 					ADS1294_CHN3_BIAS_SENS_N,
			.chn4 = 					ADS1294_CHN4_BIAS_SENS_N,
			.chn5 = 					ADS1294_CHN5_BIAS_SENS_N,
			.chn6 = 					ADS1294_CHN6_BIAS_SENS_N,
			.chn7 = 					ADS1294_CHN7_BIAS_SENS_N,
			.chn8 = 					ADS1294_CHN8_BIAS_SENS_N
		},
#endif
		.loff_sens_p = {
#if (CONFIG_ADS129X_TYPE == 1294)
			.chn1 = 					ADS1294_CHN1_LOFF_SEN_P,
			.chn2 = 					ADS1294_CHN2_LOFF_SEN_P,
			.chn3 = 					ADS1294_CHN3_LOFF_SEN_P,
			.chn4 = 					ADS1294_CHN1_LOFF_SEN_P,
			.reserve = 					0
#elif (CONFIG_ADS129X_TYPE == 1299)
			.chn1 = 					ADS1299_CHN1_LOFF_SEN_P,
			.chn2 = 					ADS1299_CHN2_LOFF_SEN_P,
			.chn3 = 					ADS1299_CHN3_LOFF_SEN_P,
			.chn4 = 					ADS1299_CHN4_LOFF_SEN_P,
			.chn5 = 					ADS1299_CHN5_LOFF_SEN_P,
			.chn6 = 					ADS1299_CHN6_LOFF_SEN_P,
			.chn7 = 					ADS1299_CHN7_LOFF_SEN_P,
			.chn8 = 					ADS1299_CHN8_LOFF_SEN_P
#endif
		},
		.loff_sens_n = {
#if (CONFIG_ADS129X_TYPE == 1294)
			.chn1 = 					ADS1294_CHN1_LOFF_SEN_N,
			.chn2 = 					ADS1294_CHN2_LOFF_SEN_N,
			.chn3 = 					ADS1294_CHN3_LOFF_SEN_N,
			.chn4 = 					ADS1294_CHN1_LOFF_SEN_N,
			.reserve = 					0
#elif (CONFIG_ADS129X_TYPE == 1299)
			.chn1 = 					ADS1299_CHN1_LOFF_SEN_N,
			.chn2 = 					ADS1299_CHN2_LOFF_SEN_N,
			.chn3 = 					ADS1299_CHN3_LOFF_SEN_N,
			.chn4 = 					ADS1299_CHN4_LOFF_SEN_N,
			.chn5 = 					ADS1299_CHN5_LOFF_SEN_N,
			.chn6 = 					ADS1299_CHN6_LOFF_SEN_N,
			.chn7 = 					ADS1299_CHN7_LOFF_SEN_N,
			.chn8 = 					ADS1299_CHN8_LOFF_SEN_N
#endif
		},
		.loff_flip = {
#if (CONFIG_ADS129X_TYPE == 1294)
			.chn1 = 					ADS1294_CHN1_LOFF_FLIP,
			.chn2 = 					ADS1294_CHN2_LOFF_FLIP,
			.chn3 = 					ADS1294_CHN3_LOFF_FLIP,
			.chn4 = 					ADS1294_CHN4_LOFF_FLIP,
			.reserve = 				0,
#elif (CONFIG_ADS129X_TYPE == 1299)
			.chn1 = 					ADS1299_CHN1_LOFF_FLIP,
			.chn2 = 					ADS1299_CHN2_LOFF_FLIP,
			.chn3 = 					ADS1299_CHN3_LOFF_FLIP,
			.chn4 = 					ADS1299_CHN4_LOFF_FLIP,
			.chn5 = 					ADS1299_CHN5_LOFF_FLIP,
			.chn6 = 					ADS1299_CHN6_LOFF_FLIP,
			.chn7 = 					ADS1299_CHN7_LOFF_FLIP,
			.chn8 = 					ADS1299_CHN8_LOFF_FLIP
#endif
		},
		.gpio = {
			.gpio_c = 					ADS129X_GPIO_C,
			.gpio_d = 					ADS129X_GPIO_D
		},
#if (CONFIG_ADS129X_TYPE == 1294)
		.pace = {
			.pd_pace = 					ADS1294_PACE_PD,
			.paceo = 					ADS1294_PACE_O,
			.pacee = 					ADS1294_PACE_E
		},
		.resp = {
			.resp_ctrl = 				ADS1294_RESP_CTRL,
			.resp_ph = 					ADS1294_RESP_PHASE,
			.resp_mod_en = 				ADS1294_RESP_MOD,
			.resp_demod_en = 			ADS1294_DEMOD_EN,
			.reserve = 					1
		},
#endif
#if (CONFIG_ADS129X_TYPE == 1299)
		.misc1 = {
			.srb1 = 					ADS1299_SRB1_SWITCH,
			.reserve1 = 				0,
			.reserve2 = 				0
		},
		.misc2 = {
			.reserve = 					0,
		},
#endif
		.config4 = {
			.reserve1 = 				0,
			.pd_loff_comp = 			ADS129X_LOFF_COMP_PD,
#if (CONFIG_ADS129X_TYPE == 1294)
			.wct_to_rld = 				ADS1294_WCT_TO_RLD,
#elif (CONFIG_ADS129X_TYPE == 1299)
			.reserve2 = 				0,
#endif
			.single_shot = 				ADS129X_SINGLE_SHORT,
#if (CONFIG_ADS129X_TYPE == 1294)
			.reserve2 = 				0,
			.resp_freq = 				ADS1294_RESP_FREQ
#elif (CONFIG_ADS129X_TYPE == 1299)
			.reserve3 = 				0
#endif
		},
#if (CONFIG_ADS129X_TYPE == 1294)
		.wct1 = {
			.wcta = 					ADS1294_WCTA,
			.pd_wcta = 					ADS1294_WCTA_PD,
			.avr_ch4 = 					ADS1294_AVR_CHN4,
			.avr_ch5 = 					ADS1294_AVR_CHN5,
			.avr_ch6 = 					ADS1294_AVR_CHN6,
			.avr_ch7 = 					ADS1294_AVR_CHN7,
		},
		.wct2 = {
			.wctb = 					ADS1294_WCTB_PD,
			.wctc = 					ADS1294_WCTC_PD,
			.pd_wctb = 					ADS1294_WCTB_PD,
			.pd_wctc = 					ADS1294_WCTC_PD
		}
#endif
	},
#if SUPPORT_NOTCH_FILTER
	.n_a = {0},
	.n_b = {0},
#endif
	.info_report = 0,
	.info_buf = {0},
	.led_color = 0,
	.bp_filter = BP_FILTER_DEFAULT_STATE,
	.hp_filter = HP_FILTER_DEFAULT_STATE,
	.lp_filter = LP_FILTER_DEFAULT_STATE,
	.notch_filter = NOTCH_FILTER_DEFAULT_STATE,
};

/*************************************************
 * @description			:	设置脑电功能状态
 * @param - state		:	要设置的状态
 * @return 				:	无
**************************************************/
void eeg_set_state(uint8_t state)
{
	if (EEG_STATE_IDLE != state && EEG_STATE_ON != state)
	{
		EEG_ERR("unknown state: %u", state);
		return;
	}

	g_eeg.state = state;
	if (EEG_STATE_IDLE == state)
	{
		led_switch(g_eeg.led_color, LED_ON);
		ads129x_send_cmd(ADS129X_STOP);
		vTaskDelay(100);
		gpio_intr_disable(ADS129X_DRDY_GPIO);
	}
	else
	{
		/* 开启肌电功能 */
		ads129x_send_cmd(ADS129X_START);
		vTaskDelay(100);
		gpio_intr_enable(ADS129X_DRDY_GPIO);
	}
}

/*************************************************
 * @description			:	切换肌电功能状态
 * @param 				:	无
 * @return 				:	无
**************************************************/
static inline void eeg_toggle_state(void)
{
	g_eeg.state = (EEG_STATE_IDLE == g_eeg.state) ? EEG_STATE_ON : EEG_STATE_IDLE;
	if (EEG_STATE_IDLE == g_eeg.state)
	{
		/* 关闭肌电功能 */
		EEG_INFO("stop eeg");
		led_switch(g_eeg.led_color, LED_ON);
		ads129x_send_cmd(ADS129X_STOP);
		vTaskDelay(100);
		gpio_intr_disable(ADS129X_DRDY_GPIO);
	}
	else
	{
		/* 开启肌电功能 */
		EEG_INFO("start eeg");
		ads129x_send_cmd(ADS129X_START);
		vTaskDelay(100);
		gpio_intr_enable(ADS129X_DRDY_GPIO);
	}
}

/*************************************************
 * @description			:	按键短按处理函数
 * @param - 			:	无
 * @return 				:	无
**************************************************/
void eeg_stm32_key_single_click_handler(void)
{
	eeg_toggle_state();
}

/*************************************************
 * @description			:	按键短按处理函数
 * @param - argc		:	参数长度
 * @param - argv		:	参数地址
 * @return 				:	无
**************************************************/
void eeg_key_short_press(uint8_t argc, void *argv)
{
	eeg_toggle_state();
}

/*************************************************
 * @description			:	按键长按处理函数
 * @param - argc		:	参数长度
 * @param - argv		:	参数地址
 * @return 				:	无
**************************************************/
void eeg_key_long_press(uint8_t argc, void *argv)
{
	EEG_INFO("stop eeg");
	eeg_set_state(EEG_STATE_IDLE);
}

/*************************************************
 * @description			:	初始化肌电模块
 * @param - 			:	无
 * @return 				:	EEG_ERR_T
**************************************************/
EEG_ERR_T eeg_init(void)
{
	int i = 0;
	int j = 0;
	uint32_t buf_size = 0;
	uint8_t *msg = NULL;
	uint8_t *buf = NULL;
	gpio_config_t cs_cfg = {0};
	gpio_config_t drdy_cfg = {0};
	spi_bus_config_t bus_cfg = {0};
	spi_device_interface_config_t dev_cfg = {0};
	TimerHandle_t ads_timer = NULL;
	uint8_t r_regs[ADS129X_REG_NUM] = {0};
	uint8_t w_regs[ADS129X_REG_NUM] = {0};
	char *reg_name[ADS129X_REG_NUM] = {
		"ID", "config1", "config2", "config3", "loff",
		"chn1set", "chn2set", "chn3set", "chn4set",
		"chn5set", "chn6set", "chn7set", "chn8set",
#if (CONFIG_ADS129X_TYPE == 1294)
		"rld_sensp", "rld_sensn",
#elif (CONFIG_ADS129X_TYPE == 1299)
		"bias_sensp", "bias_sensn",
#endif
		"loff_sensp", "loff_sensn", "loff_flip", "loff_statp", "loff_statn",
		"gpio",
#if (CONFIG_ADS129X_TYPE == 1294)
		"pace", "resp",
#elif (CONFIG_ADS129X_TYPE == 1299)
		"misc1", "misc2", "config4"
#endif
#if (CONFIG_ADS129X_TYPE == 1294)
		"config4",
		"wct1", "wct2"
#endif
	};
	uint8_t default_regs[ADS129X_REG_NUM] = {
		ADS129X_REG_ID_DEFAULT, ADS129X_REG_CONFIG1_DEFAULT, ADS129X_REG_CONFIG2_DEFAULT,
		ADS129X_REG_CONFIG3_DEFAULT, ADS129X_REG_LOFF_DEFAULT,
		ADS129X_REG_CHN1_DEFAULT, ADS129X_REG_CHN2_DEFAULT, ADS129X_REG_CHN3_DEFAULT, ADS129X_REG_CHN4_DEFAULT,
#if (CONFIG_ADS129X_TYPE == 1299)
		ADS129X_REG_CHN5_DEFAULT, ADS129X_REG_CHN6_DEFAULT, ADS129X_REG_CHN7_DEFAULT, ADS129X_REG_CHN8_DEFAULT,
#endif
#if (CONFIG_ADS129X_TYPE == 1294)
		ADS1294_REG_RLD_SENSP_DEFAULT, ADS1294_REG_RLD_SENSN_DEFAULT,
#elif (CONFIG_ADS129X_TYPE == 1299)
		ADS1294_REG_BIAS_SENSP_DEFAULT, ADS1294_REG_BIAS_SENSN_DEFAULT,
#endif
		ADS129X_REG_LOFF_SENSP_DEFAULT, ADS129X_REG_LOFF_SENSN_DEFAULT, ADS129X_REG_LOFF_FLIP_DEFAULT,
		ADS129X_REG_LOFF_STATP_DEFAULT, ADS129X_REG_LOFF_STATN_DEFAULT, ADS129X_REG_GPIO_DEFAULT,
#if (CONFIG_ADS129X_TYPE == 1294)
		ADS1294_REG_PACE_DEFAULT, ADS1294_REG_RESP_DEFAULT,
#elif (CONFIG_ADS129X_TYPE == 1299)
		ADS1294_REG_MISC1_DEFAULT, ADS1294_REG_MISC2_DEFAULT, ADS129X_REG_CONFIG4_DEFAULT
#endif
#if (CONFIG_ADS129X_TYPE == 1294)
		ADS129X_REG_CONFIG4_DEFAULT, ADS1294_REG_WCT1_DEFAULT, ADS1294_REG_WCT2_DEFAULT
#endif
	};
	bool ads_regs_flag = true;
#if 0
	g_eeg.led_color = led_get_color();
	EEG_DBG("color type: %u", g_eeg.led_color);
	led_switch(g_eeg.led_color, LED_ON);
#endif
	uint8_t notify[ESP_STM_MSG_LEN] = {0};

	EEG_INFO("init eeg-spi%d...", SPI2_HOST + 1);

	/* 初始化片选引脚 */
	cs_cfg.pin_bit_mask = (1ull << ADS129X_CS_GPIO),
	cs_cfg.mode = GPIO_MODE_OUTPUT,
	gpio_config(&cs_cfg);
	gpio_set_level(ADS129X_CS_GPIO, 0);

	/* 
	 * 初始化esp32-c6的SPI2
	 * 初始化SPI2总线
	 */
	bus_cfg.miso_io_num = ADS129X_SPI_MISO;
	bus_cfg.mosi_io_num = ADS129X_SPI_MOSI;
	bus_cfg.sclk_io_num = ADS129X_SPI_CLK;
	bus_cfg.quadwp_io_num = -1;
	bus_cfg.quadhd_io_num = -1;
	bus_cfg.max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE;
	bus_cfg.flags = SPICOMMON_BUSFLAG_MASTER;
	if (ESP_OK != spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_DISABLED))
	{
		EEG_ERR("init spi2 bus failed");
		goto error_exit1;
	}

	/* 设置ads129x属性，并添加到SPI2总线 */
	dev_cfg.command_bits = 0;						/* 不发送cmd字段 */
	dev_cfg.address_bits = 0;						/* 不发送address字段 */
	dev_cfg.dummy_bits = 0;
	dev_cfg.mode = 1;								// SPI mode 1 CPOL = 0, CPHA = 1
	dev_cfg.clock_speed_hz = SPI_MASTER_FREQ_8M;	// Clock out at 8 MHz,
	dev_cfg.spics_io_num = GPIO_NUM_0;
	dev_cfg.queue_size = 1024;						// 传输队列大小，决定了等待传输数据的数量
	dev_cfg.input_delay_ns = 1000;
	dev_cfg.duty_cycle_pos = 0;
	if (ESP_OK != spi_bus_add_device(SPI2_HOST, &dev_cfg, &g_eeg.hspi))
	{
		EEG_ERR("add ads1294 to spi2 bus failed");
		goto error_exit2;
	}
	EEG_INFO("spi2 bus init success, waiting %s power up...",
#if (CONFIG_ADS129X_TYPE == 1294)
			"1294"
#elif (CONFIG_ADS129X_TYPE == 1299)
			"1299"
#endif
	);

	/* 
	 * 初始化ads129x
	 * 上电复位，进入待机模式
	 */
	do {
		ads129x_send_cmd(ADS129X_SDATAC);
		vTaskDelay(pdMS_TO_TICKS(100));

		/* 复位 */
		ads129x_send_cmd(ADS129X_RESET);
		vTaskDelay(pdMS_TO_TICKS(100));

		ads129x_send_cmd(ADS129X_SDATAC);
		vTaskDelay(pdMS_TO_TICKS(100));

		memset(r_regs, 0, ADS129X_REG_NUM);
		ads129x_read_all_regs(r_regs);

		/* 检查寄存器默认值 */
		ads_regs_flag = true;
		for (i = ADS129X_REG_ID; i < ADS129X_REG_NUM; i++)
		{
			if (r_regs[i] != default_regs[i])
			{
				ads_regs_flag = false;
				EEG_ERR("%s expect: 0x%02x read: 0x%02x", reg_name[i], default_regs[i], r_regs[i]);
			}
		}
		if (false == ads_regs_flag)
		{
			notify[ES_MSG_OFT_HEAD] = (UART_MSG_HEAD&0xFF00)>>8;
			notify[ES_MSG_OFT_HEAD + 1] = UART_MSG_HEAD&0x00FF;
			notify[ES_MSG_OFT_TYPE] = ESP_STM_SYS_STATUS;
			notify[ES_MSG_OFT_DATA] = ESP32_ADS_ERR;
			notify[ES_MSG_OFT_CRC] = crc_8bit_mask(notify, ES_MSG_OFT_CRC, UART_CRC_MASK);
			vTaskDelay(pdMS_TO_TICKS(1000));
			uart_data_transmit(notify, ESP_STM_MSG_LEN);
		}
	} while(false == ads_regs_flag);

	eeg_crc_table_init(EEG_CRC_MASK);

	/* 读取flash中网络配置 */
	eeg_sync_params_from_flash();

	if (EEG_OK != eeg_network_connect())
	{
		EEG_ERR("connect to server failed");
		goto error_exit2;
	}

	if (0 != pthread_create(&g_eeg.pid, NULL, eeg_send_task, NULL))
	{
		EEG_ERR("create thread failed: %d (%s)", errno, strerror(errno));
		goto error_exit2;
	}
	pthread_detach(g_eeg.pid);

	/* 分配数据块 */
#if (CONFIG_ADS129X_TYPE == 1294)
	buf_size = SEND_BUF_NUM * ADS129X_CHN_NUM * g_eeg.frame_total_len;
#elif (CONFIG_ADS129X_TYPE == 1299)
	buf_size = SEND_BUF_NUM * ADS1299_EFFECTIVE_CHN_NUM * g_eeg.frame_total_len;
#endif
	g_eeg.snd_buf = (uint8_t *)malloc(buf_size);
	if (NULL == g_eeg.snd_buf)
	{
		EEG_ERR("malloc failed: %d (%s)", errno, strerror(errno));
		goto error_exit2;
	}
	EEG_DBG("data buf: 0x%p", g_eeg.snd_buf);
	for (i = 0; i < SEND_BUF_NUM; i++)
	{
#if (CONFIG_ADS129X_TYPE == 1294)
		g_eeg.sub_bufs[i] = &g_eeg.snd_buf[i * ADS129X_CHN_NUM * g_eeg.frame_total_len];
#elif (CONFIG_ADS129X_TYPE == 1299)
		g_eeg.sub_bufs[i] = &g_eeg.snd_buf[i * ADS1299_EFFECTIVE_CHN_NUM * g_eeg.frame_total_len];
#endif
		EEG_DBG("buf[%d]: %p", i + 1, g_eeg.sub_bufs[i]);
#if (CONFIG_ADS129X_TYPE == 1294)
		for (j = ADS129X_CHN1; j < ADS129X_CHN_NUM; j++)
#elif (CONFIG_ADS129X_TYPE == 1299)
		for (j = ADS129X_CHN1; j < ADS1299_EFFECTIVE_CHN_NUM; j++)
#endif
		{
			/* 初始化报文结构 */
			buf = g_eeg.sub_bufs[i];
			msg = &buf[j * g_eeg.frame_total_len];

			/* 首部结构 */
			if (ADS129X_FREQ_1KHZ == g_eeg.freq)
			{
				*(uint16_t*)&msg[DATA_FRAME_OFT_HEAD] = htons(EEG_DATA_FRAME_HEADER_LOW_FRE_MODE);
			}
			else if (ADS129X_FREQ_2KHZ == g_eeg.freq || ADS129X_FREQ_4KHZ == g_eeg.freq || ADS129X_FREQ_8KHZ == g_eeg.freq)
			{
				*(uint16_t*)&msg[DATA_FRAME_OFT_HEAD] = htons(EEG_DATA_FRAME_HEADER_HIGH_FRE_MODE);
			}
			else
			{
				*(uint16_t*)&msg[DATA_FRAME_OFT_HEAD] = htons(EEG_DATA_FRAME_HEADER_LOW_FRE_MODE);
			}
			
			msg[DATA_FRAME_OFT_SERIAL] = g_eeg.serial >> 16;
			msg[DATA_FRAME_OFT_SERIAL + 1] = g_eeg.serial >> 8;
			msg[DATA_FRAME_OFT_SERIAL + 2] = g_eeg.serial;
			msg[DATA_FRAME_OFT_CHN] = j;

			/* 有效数据长度 */
			*(uint16_t*)&msg[DATA_FRAME_OFT_LEN] = htons(g_eeg.frame_total_len - CRC_SIZE - TS_SIZE - DATA_FRAME_OFT_DATA);
		}
	}

	/* 初始化DRDY引脚 */
	drdy_cfg.intr_type = GPIO_INTR_NEGEDGE;					//下降沿触发
	drdy_cfg.pin_bit_mask = (1ull << ADS129X_DRDY_GPIO);	//设置中断的IO口
	drdy_cfg.mode = GPIO_MODE_INPUT;						//设置IO为输入功能
	drdy_cfg.pull_up_en = 0;
	gpio_config(&drdy_cfg);
	if (ESP_OK != gpio_install_isr_service(0))
	{
		EEG_ERR("register gpio isr service failed");
	}

	/* 注册中断函数 */
	if (ESP_OK != gpio_isr_handler_add(ADS129X_DRDY_GPIO, drdy_isr_handler, (void*)ADS129X_DRDY_GPIO))
	{
		EEG_ERR("add gpio isr func failed");
	}

	/* 设置正常采集模式 */
	memset(w_regs, 0, ADS129X_REG_NUM);
	ads129x_read_regs_from_context(w_regs);
	ads129x_write_all_regs(w_regs);
	vTaskDelay(100);

	/* 检查设置的寄存器 */
	memset(r_regs, 0, ADS129X_REG_NUM);
	ads129x_read_all_regs(r_regs);
	for (i = 0; i < ADS129X_REG_NUM; i++)
	{
		if (r_regs[i] == w_regs[i])
		{
			EEG_DBG("%s: 0x%02x", reg_name[i], r_regs[i]);
		}
		else
		{
			EEG_ERR("%s expected: 0x%02x actual: 0x%02x", reg_name[i], w_regs[i], r_regs[i]);
			goto error_exit2;
		}
	}

	/* 注册长短按处理函数 */
	key_press_handler_register(KEY_SHORT_PRESS, "eeg short press", eeg_key_short_press, 0, NULL);
	key_press_handler_register(KEY_LONG_PRESS, "eeg long press", eeg_key_long_press, 0, NULL);

	uart_handler_register(UART_TYPE_ADS, "ads cmd", ads129x_cmd_data_process);

	/* 启动连续模式 */
	ads129x_send_cmd(ADS129X_RDATAC);
	vTaskDelay(10);

	ads129x_send_cmd(ADS129X_START);
	vTaskDelay(10);

	ads_timer = xTimerCreate("ads1294 monitor", pdMS_TO_TICKS(1000), pdTRUE, 0, eeg_monitor);
	if (NULL != ads_timer)
	{
		EEG_INFO("start eeg monitor...");
		xTimerStart(ads_timer, 0);
	}

	eeg_info_init();

	/* 注册短按处理函数 */
	if (KEY_OK != stm32_key_handler_register(KEY_ACT_SINGLE_CLICK, eeg_stm32_key_single_click_handler))
	{
		EEG_ERR("register stm32 single press handler failed");
	}

	EEG_INFO("init eeg done...");

	gpio_intr_enable(ADS129X_DRDY_GPIO);

	return EEG_OK;

error_exit2:
	spi_bus_free(SPI2_HOST);
error_exit1:
	return EEG_FAIL;
}

/*************************************************
 * @description			:	监听并搜寻上位机广播，获
 * 							取其IP、
 * @param - 			:	无
 * @return 				:	无
**************************************************/
static void eeg_search_host_ip(void)
{
	int ret = 0;
	int recv_len = 0;
	struct sockaddr_in addr;
	socklen_t from_len;
	struct timeval tv;
	fd_set rd_set;
	fd_set rd_set_cp;
	uint8_t buf[BROAD_CAST_RECV_BUF_LEN] = {0};
	uint8_t notify[ESP_STM_MSG_LEN] = {0};
 
retry:
	g_eeg.admin_sock = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (0 > g_eeg.admin_sock)
	{
		EEG_ERR("create sock failed: %d (%s)", errno, strerror(errno));
		goto retry;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(EEG_HOST_BROADCAST_PORT);

	if (0 != lwip_bind(g_eeg.admin_sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)))
	{
		EEG_ERR("bind failed: %d (%s)", errno, strerror(errno));
		lwip_shutdown(g_eeg.admin_sock, 0);
		close(g_eeg.admin_sock);
		g_eeg.admin_sock = -1;
		vTaskDelay(pdMS_TO_TICKS(1000));
		goto retry;
	}

	FD_ZERO(&rd_set);
	FD_SET(g_eeg.admin_sock, &rd_set);

	notify[ES_MSG_OFT_HEAD] = (UART_MSG_HEAD&0xFF00)>>8;
	notify[ES_MSG_OFT_HEAD + 1] = UART_MSG_HEAD&0x00FF;
	notify[ES_MSG_OFT_TYPE] = ESP_STM_SYS_STATUS;

	EEG_INFO("searching for host...");

	while (1)
	{
		vTaskDelay(pdMS_TO_TICKS(10));
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		rd_set_cp = rd_set;
		ret = select(g_eeg.admin_sock + 1, &rd_set_cp, NULL, NULL, &tv);
		if (0 > ret && (EINTR != errno))
		{
			EEG_ERR("select failed: %d (%s)", errno, strerror(errno));
			lwip_shutdown(g_eeg.admin_sock, 0);
			close(g_eeg.admin_sock);
			g_eeg.admin_sock = -1;
			vTaskDelay(pdMS_TO_TICKS(1000));
			goto retry;
		}
		else if (0 == ret)
		{
			EEG_DBG("time out");
			notify[ES_MSG_OFT_DATA] = ESP32_BROADCAST_TIME_OUT;
			notify[ES_MSG_OFT_CRC] = crc_8bit_mask(notify, ES_MSG_OFT_CRC, UART_CRC_MASK);
			uart_data_transmit(notify, ESP_STM_MSG_LEN);
			continue;
		}
		if (FD_ISSET(g_eeg.admin_sock, &rd_set_cp))
		{
			recv_len = recvfrom(g_eeg.admin_sock, buf, BROAD_CAST_RECV_BUF_LEN, 0, (struct sockaddr*)&g_eeg.from, &from_len);
			if (0 < recv_len)
			{
				if (EEG_OK == eeg_record_host_ip(buf, recv_len))
				{
					lwip_shutdown(g_eeg.admin_sock, 0);
					close(g_eeg.admin_sock);
					g_eeg.admin_sock = -1;
					notify[ES_MSG_OFT_DATA] = ESP32_IDLE;
					notify[ES_MSG_OFT_CRC] = crc_8bit_mask(notify, ES_MSG_OFT_CRC, UART_CRC_MASK);
					uart_data_transmit(notify, ESP_STM_MSG_LEN);
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
static int eeg_record_host_ip(uint8_t *buf, int len)
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
		EEG_ERR("input null");
		return EEG_FAIL;
	}

	head = ntohs(*((uint16_t *)buf));
	if (EEG_HOST_BROADCAST_MSG_HEAD != head ||
		11 >= len)
	{
		return EEG_FAIL;
	}

	crc = crc_8bit_mask(buf, len - 1, 0xd5);
	if (crc != buf[len - 1])
	{
		EEG_ERR("crc check failed expect: 0x%02x calculated: 0x%02x", buf[len - 1], crc);
		return EEG_FAIL;
	}

	/* 报文解析 */
	ip = buf[EEG_BROADCAST_IP + 3] | (buf[EEG_BROADCAST_IP + 2]<<8) |
		(buf[EEG_BROADCAST_IP + 1]<<16) | (buf[EEG_BROADCAST_IP]<<24);
	EEG_DBG("host ip: %u.%u.%u.%u", buf[EEG_BROADCAST_IP], buf[EEG_BROADCAST_IP + 1],
						buf[EEG_BROADCAST_IP + 2], buf[EEG_BROADCAST_IP + 3]);
	g_eeg.host_ip = ntohl(ip);

	mask = buf[EEG_BROADCAST_MASK + 3] | (buf[EEG_BROADCAST_MASK + 2]<<8) |
		(buf[EEG_BROADCAST_MASK + 1]<<16) | (buf[EEG_BROADCAST_MASK]<<24);
	EEG_DBG("mask: %u.%u.%u.%u", buf[EEG_BROADCAST_MASK], buf[EEG_BROADCAST_MASK + 1],
						buf[EEG_BROADCAST_MASK + 2], buf[EEG_BROADCAST_MASK + 3]);
	g_eeg.mask = ntohl(mask);

	tcp_port_num = buf[EEG_BROADCAST_TCP_PORT_NUM];
	offset = EEG_BROADCAST_TCP_PORT_NUM + 1;
	EEG_DBG("tcp port num: %u", tcp_port_num);
	for (i = 0; i < tcp_port_num; i++)
	{
		port = *(uint16_t*)(buf + offset + i*sizeof(uint16_t));
		port = ntohs(port);
		EEG_DBG("port[%d]: %u", i + 1, port);
	}
	tcp_port = *(uint16_t*)(buf + offset);

	udp_port_num = buf[offset + tcp_port_num*sizeof(uint16_t)];
	offset += tcp_port_num*sizeof(uint16_t) + 1;
	EEG_DBG("udp port num: %u", udp_port_num);
	for (i = 0; i < udp_port_num; i++)
	{
		port = *(uint16_t*)(buf + offset + i*sizeof(uint16_t));
		port = ntohs(port);
		EEG_DBG("port[%d]: %u", i + 1, port);
	}
	udp_port = *(uint16_t*)(buf + offset);
	g_eeg.port = (SOCK_DGRAM == g_eeg.protocol) ? ntohs(udp_port) : ntohs(tcp_port);

	return EEG_OK;
}

/*************************************************
 * @description			:	连接上位机网络
 * @param - 			:	无
 * @return 				:	EEG_ERR_T
**************************************************/
static int eeg_network_connect(void)
{
	uint8_t *adr = NULL;
	int opt = 0;
	int flags = 0;
	struct timeval timeout = {0};
	uint8_t notify[ESP_STM_MSG_LEN] = {0};

	g_eeg.dst.sin_addr.s_addr = g_eeg.host_ip;
	g_eeg.dst.sin_family = AF_INET;
	g_eeg.dst.sin_port = htons(g_eeg.port);

	notify[ES_MSG_OFT_HEAD] = (UART_MSG_HEAD&0xFF00)>>8;
	notify[ES_MSG_OFT_HEAD + 1] = UART_MSG_HEAD&0x00FF;
	notify[ES_MSG_OFT_TYPE] = ESP_STM_SYS_STATUS;

retry:
	g_eeg.data_sock = lwip_socket(AF_INET, g_eeg.protocol, IPPROTO_IP);
	if (g_eeg.data_sock < 0)
	{
		EEG_ERR("create sock failed: %d (%s)", errno, strerror(errno));
		goto error_exit;
	}

	if (SOCK_STREAM == g_eeg.protocol)
	{
		if (0 != connect(g_eeg.data_sock, (struct sockaddr *)&g_eeg.dst, sizeof(struct sockaddr_in)))
		{
			EEG_ERR("connect failed: %d (%s)", errno, strerror(errno));
			lwip_shutdown(g_eeg.data_sock, 0);
			close(g_eeg.data_sock);
			g_eeg.data_sock = -1;
			notify[ES_MSG_OFT_DATA] = ESP32_NETIF_TCP_CONN_FAIL;
			notify[ES_MSG_OFT_CRC] = crc_8bit_mask(notify, ES_MSG_OFT_CRC, UART_CRC_MASK);
			uart_data_transmit(notify, ESP_STM_MSG_LEN);
			vTaskDelay(pdMS_TO_TICKS(1000));
			goto retry;
		}
		memcpy(&g_eeg.dst_s, &g_eeg.dst, sizeof(g_eeg.dst));

		if (ADS129X_FREQ_1KHZ == g_eeg.freq)
		{
			opt = 1;
			if (0 != setsockopt(g_eeg.data_sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)))
			{
				EEG_ERR("set tcp no delay errno: %d (%s)", errno, strerror(errno));
				goto error_exit;
			}
		}

		flags = fcntl(g_eeg.data_sock, F_GETFL);
		if (0 != fcntl(g_eeg.data_sock, F_SETFL, flags | O_NONBLOCK))
		{
			EEG_ERR("set socket non blocking errno: %d (%s)", errno, strerror(errno));
			goto error_exit;
		}

		timeout.tv_sec = 0;
		timeout.tv_usec = 10;
		setsockopt(g_eeg.data_sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
	}
	else
	{
		g_eeg.src.sin_addr.s_addr = g_eeg.client_ip;
		g_eeg.src.sin_family = AF_INET;
		g_eeg.src.sin_port = htons(g_eeg.port + 1);

		if (lwip_bind(g_eeg.data_sock, (struct sockaddr *)&g_eeg.src, sizeof(g_eeg.src)) != 0 )
		{
			EEG_ERR("socket bind error errno: %d (%s)", errno, strerror(errno));
		}
	}

	adr = (uint8_t*)&g_eeg.host_ip;
	EEG_INFO("connect to server %u.%u.%u.%u:%u success ", adr[0], adr[1], adr[2], adr[3], g_eeg.port);

	return EEG_OK;

error_exit:
	if (-1 < g_eeg.data_sock)
	{
		lwip_shutdown(g_eeg.data_sock, 0);
		close(g_eeg.data_sock);
	}
	return EEG_FAIL;
}

/*************************************************
 * @description			:	发送线程函数
 * @param - arg			:	线程入参
 * @return 				:	NULL
**************************************************/
static void *eeg_send_task(void *arg)
{
	uint32_t snd_cnt = 0;
#if (CONFIG_ADS129X_TYPE == 1294)
	uint8_t chn_num = ADS129X_CHN_NUM;
#elif (CONFIG_ADS129X_TYPE == 1299)
	uint8_t chn_num = ADS1299_EFFECTIVE_CHN_NUM;
#endif

	EEG_INFO("waiting for sending...");

	while (1)
	{
		vTaskDelay(1);
		if (EEG_DATA_SEND == g_eeg.snd_state)
		{
#if MONITOR_FILTER_RUN_TIME
			EEG_DBG("bp filter use time: %lld", g_eeg.stop - g_eeg.start);
#endif
			//EEG_DBG("buf[%u]", g_eeg.snd_buf_idx);
#if 1
			if (SOCK_STREAM == g_eeg.protocol)
			{
				if (ADS129X_CHN_NUM * g_eeg.frame_total_len != safety_send(
						g_eeg.data_sock, g_eeg.sub_bufs[g_eeg.snd_buf_idx], chn_num*g_eeg.frame_total_len))
				{
					EEG_ERR("send failed, errno: %d (%s)", errno, strerror(errno));
				}
			}
			else
			{
				sendto(g_eeg.data_sock, g_eeg.sub_bufs[g_eeg.snd_buf_idx], chn_num*g_eeg.frame_total_len, 0,
					(struct sockaddr *)&g_eeg.dst, sizeof(struct sockaddr_in));
			}
#endif
			g_eeg.snd_rate += g_eeg.patch_size;
			g_eeg.snd_state = EEG_DATA_NOT_SEND;

			snd_cnt += g_eeg.patch_size;
			if (1000 == snd_cnt)
			{
				//EEG_DBG("color type: %u", g_eeg.led_color);
				led_toggle(g_eeg.led_color);
				snd_cnt = 0;
			}
		}

		if (EEG_INFO_REPORT_PERIOD <= g_eeg.info_report)
		{
			g_eeg.info_report = 0;

			/* 定期上报电量、版本等信息 */
			eeg_info_report();
		}
	}
	
	return NULL;
}

/*************************************************
 * @description			:	ads中断处理函数
 * @param - arg			:	入参
 * @return 				:	无
**************************************************/
// IRAM_ATTR 是 ESP-IDF 框架中定义的一个宏，通常在 esp_attr.h 头文件中定义。
// 它用于将函数放置到 IRAM（内部 RAM）中，保证中断等高实时性代码的执行速度。
// 你已经在本文件顶部包含了 #include "esp_attr.h"，无需额外定义。
static void IRAM_ATTR drdy_isr_handler(void *arg)
{
	int i = 0;
	uint32_t gpio_num = 0;
	int32_t filted = 0;
	spi_transaction_t t = {0};
	uint32_t amp = 0;
	int32_t	ori_val[ADS129X_CHN_NUM] = {0};
	uint8_t data_buf[ADS129X_DATA_BUF_SIZE] = {0};
	uint8_t *msg = NULL;
	uint8_t *buf = NULL;
	uint16_t cnt = 0;
	uint8_t data = 0;
#if SUPPORT_BP_FILTER
#if (CONFIG_ADS129X_TYPE == 1294)
	static double w_bp_n1[ADS129X_CHN_NUM][3] = {0};
	static double w_bp_n2[ADS129X_CHN_NUM][3] = {0};
	static double w_bp_n3[ADS129X_CHN_NUM][3] = {0};
#elif (CONFIG_ADS129X_TYPE == 1299)
	static double w_bp_n1[ADS1299_EFFECTIVE_CHN_NUM][3] = {0};
	static double w_bp_n2[ADS1299_EFFECTIVE_CHN_NUM][3] = {0};
	static double w_bp_n3[ADS1299_EFFECTIVE_CHN_NUM][3] = {0};
#endif
#endif
#if SUPPORT_NOTCH_FILTER
#if (CONFIG_ADS129X_TYPE == 1294)
	static double w_n[ADS129X_CHN_NUM][NOTCH_FILTER_DEGREE + 1] = {0};
#elif (CONFIG_ADS129X_TYPE == 1299)
	static double w_n[ADS1299_EFFECTIVE_CHN_NUM][NOTCH_FILTER_DEGREE + 1] = {0};
#endif
#endif
	double y = 0;
	int64_t time_stamp = 0;
	static double w_hp_n1[ADS1299_EFFECTIVE_CHN_NUM][3] = {0};
	static double w_hp_n2[ADS1299_EFFECTIVE_CHN_NUM][3] = {0};
	static double w_lp_n1[ADS1299_EFFECTIVE_CHN_NUM][3] = {0};
	static double w_lp_n2[ADS1299_EFFECTIVE_CHN_NUM][3] = {0};

	if (NULL == arg || EEG_STATE_IDLE == g_eeg.state)
	{
		return;
	}

	gpio_num = (uint32_t) arg;
	if (ADS129X_DRDY_GPIO == gpio_num && 0 == gpio_get_level(ADS129X_DRDY_GPIO))
	{
		/* 接收数据 */
		gpio_set_level(ADS129X_CS_GPIO, 0);
		t.length = ADS129X_DATA_BUF_SIZE_BIT;
		t.rxlength = t.length;
		t.tx_buffer = &data;
		t.rx_buffer = data_buf;
		spi_device_polling_transmit(g_eeg.hspi, &t);
		gpio_set_level(ADS129X_CS_GPIO, 1);

		/* 数据处理 */
#if (CONFIG_ADS129X_TYPE == 1294)
		for (i = ADS129X_CHN1; i <= ADS129X_CHN4; i++)
#elif (CONFIG_ADS129X_TYPE == 1299)
		for (i = ADS129X_CHN1; i <= ADS129X_CHN4; i++)
#endif
		{
			/* 符号转换 */
			amp = ((data_buf[3+3*i]&0x7F)<<16) | (data_buf[4+3*i]<<8) | (data_buf[5+3*i]);
			ori_val[i] = (data_buf[3+3*i]&0x80) ? (0 - amp) : amp;

			/* 数据缩放 */
			y = ((double)ori_val[i]) * ADS129X_BASE_SCALE;

#if MONITOR_FILTER_RUN_TIME
			g_eeg.t_start = esp_timer_get_time();
#endif

#if (SUPPORT_BP_FILTER)
			/* 
			* w[0] = a0*x - a1*w[1] - a2*w[2] - a3*w[3] - ... - aj*w[j] , j=BP_FILTER_DEGREE
			* y = b0*w[0] + b1*w[1] + b2*w[2] + b3*w[3] + ... + bj*w[j] , j=BP_FILTER_DEGREE
			* w[j] = w[j - 1], w[j - 1] = w[j - 2],...,w[1] = w[0], , j=BP_FILTER_DEGREE
			*/
			if (ADS129X_FREQ_1KHZ == g_eeg.freq)
			{
				if (FILTER_SWITCH_STATE_ON == g_eeg.bp_filter)
				{
					/* node1 */
					w_bp_n1[i][0] = y - BP_A1_1KHZ_N1*w_bp_n1[i][1] - BP_A2_1KHZ_N1*w_bp_n1[i][2];
					y = BP_B0_1KHZ_N1*w_bp_n1[i][0] + BP_B1_1KHZ_N1*w_bp_n1[i][1] + BP_B2_1KHZ_N1*w_bp_n1[i][2];
					w_bp_n1[i][2] = w_bp_n1[i][1];
					w_bp_n1[i][1] = w_bp_n1[i][0];
					y = BP_GAIN_1KHZ_N1 * y;

					/* node2 */
					w_bp_n2[i][0] = y - BP_A1_1KHZ_N2*w_bp_n2[i][1] - BP_A2_1KHZ_N2*w_bp_n2[i][2];
					y = BP_B0_1KHZ_N2*w_bp_n2[i][0] + BP_B1_1KHZ_N2*w_bp_n2[i][1] + BP_B2_1KHZ_N2*w_bp_n2[i][2];
					w_bp_n2[i][2] = w_bp_n2[i][1];
					w_bp_n2[i][1] = w_bp_n2[i][0];
					y = BP_GAIN_1KHZ_N2 * y;

					/* node3 */
					w_bp_n3[i][0] = y - BP_A1_1KHZ_N3*w_bp_n3[i][1] - BP_A2_1KHZ_N3*w_bp_n3[i][2];
					y = BP_B0_1KHZ_N3*w_bp_n3[i][0] + BP_B1_1KHZ_N3*w_bp_n3[i][1] + BP_B2_1KHZ_N3*w_bp_n3[i][2];
					w_bp_n3[i][2] = w_bp_n3[i][1];
					w_bp_n3[i][1] = w_bp_n3[i][0];
					y = BP_GAIN_1KHZ_N3 * y;
				}

				if (FILTER_SWITCH_STATE_ON == g_eeg.hp_filter)
				{
					/* node1 */
					w_hp_n1[i][0] = y - HP_A1_1KHZ_N1*w_hp_n1[i][1] - HP_A2_1KHZ_N1*w_hp_n1[i][2];
					y = HP_B0_1KHZ_N1*w_hp_n1[i][0] + HP_B1_1KHZ_N1*w_hp_n1[i][1] + HP_B2_1KHZ_N1*w_hp_n1[i][2];
					w_hp_n1[i][2] = w_hp_n1[i][1];
					w_hp_n1[i][1] = w_hp_n1[i][0];
					y = HP_GAIN_1KHZ_N1 * y;

					/* node2 */
					w_hp_n2[i][0] = y - HP_A1_1KHZ_N2*w_hp_n2[i][1] - HP_A2_1KHZ_N2*w_hp_n2[i][2];
					y = HP_B0_1KHZ_N2*w_hp_n2[i][0] + HP_B1_1KHZ_N2*w_hp_n2[i][1] + HP_B2_1KHZ_N2*w_hp_n2[i][2];
					w_hp_n2[i][2] = w_hp_n2[i][1];
					w_hp_n2[i][1] = w_hp_n2[i][0];
					y = HP_GAIN_1KHZ_N2 * y;
				}

				if (FILTER_SWITCH_STATE_ON == g_eeg.lp_filter)
				{
					/* node1 */
					w_lp_n1[i][0] = y - LP_A1_1KHZ_N1*w_lp_n1[i][1] - LP_A2_1KHZ_N1*w_lp_n1[i][2];
					y = LP_B0_1KHZ_N1*w_lp_n1[i][0] + LP_B1_1KHZ_N1*w_lp_n1[i][1] + LP_B2_1KHZ_N1*w_lp_n1[i][2];
					w_lp_n1[i][2] = w_lp_n1[i][1];
					w_lp_n1[i][1] = w_lp_n1[i][0];
					y = LP_GAIN_1KHZ_N1 * y;

					/* node2 */
					w_lp_n2[i][0] = y - LP_A1_1KHZ_N2*w_lp_n2[i][1] - LP_A2_1KHZ_N2*w_lp_n2[i][2];
					y = LP_B0_1KHZ_N2*w_lp_n2[i][0] + LP_B1_1KHZ_N2*w_lp_n2[i][1] + LP_B2_1KHZ_N2*w_lp_n2[i][2];
					w_lp_n2[i][2] = w_lp_n2[i][1];
					w_lp_n2[i][1] = w_lp_n2[i][0];
					y = LP_GAIN_1KHZ_N2 * y;
				}
			}
			else if (ADS129X_FREQ_2KHZ == g_eeg.freq)
			{
				if (FILTER_SWITCH_STATE_ON == g_eeg.bp_filter)
				{
					/* node1 */
					w_bp_n1[i][0] = y - BP_A1_2KHZ_N1*w_bp_n1[i][1] - BP_A2_2KHZ_N1*w_bp_n1[i][2];
					y = BP_B0_2KHZ_N1*w_bp_n1[i][0] + BP_B1_2KHZ_N1*w_bp_n1[i][1] + BP_B2_2KHZ_N1*w_bp_n1[i][2];
					w_bp_n1[i][2] = w_bp_n1[i][1];
					w_bp_n1[i][1] = w_bp_n1[i][0];

					/* node2 */
					w_bp_n2[i][0] = y - BP_A1_2KHZ_N2*w_bp_n2[i][1] - BP_A2_2KHZ_N2*w_bp_n2[i][2];
					y = BP_B0_2KHZ_N2*w_bp_n2[i][0] + BP_B1_2KHZ_N2*w_bp_n2[i][1] + BP_B2_2KHZ_N2*w_bp_n2[i][2];
					w_bp_n2[i][2] = w_bp_n2[i][1];
					w_bp_n2[i][1] = w_bp_n2[i][0];
				}
			}
			else if (ADS129X_FREQ_4KHZ == g_eeg.freq)
			{
				if (FILTER_SWITCH_STATE_ON == g_eeg.bp_filter)
				{
					/* node1 */
					w_bp_n1[i][0] = y - BP_A1_4KHZ_N1*w_bp_n1[i][1] - BP_A2_4KHZ_N1*w_bp_n1[i][2];
					y = BP_B0_4KHZ_N1*w_bp_n1[i][0] + BP_B1_4KHZ_N1*w_bp_n1[i][1] + BP_B2_4KHZ_N1*w_bp_n1[i][2];
					w_bp_n1[i][2] = w_bp_n1[i][1];
					w_bp_n1[i][1] = w_bp_n1[i][0];

					/* node2 */
					w_bp_n2[i][0] = y - BP_A1_4KHZ_N2*w_bp_n2[i][1] - BP_A2_4KHZ_N2*w_bp_n2[i][2];
					y = BP_B0_4KHZ_N2*w_bp_n2[i][0] + BP_B1_4KHZ_N2*w_bp_n2[i][1] + BP_B2_4KHZ_N2*w_bp_n2[i][2];
					w_bp_n2[i][2] = w_bp_n2[i][1];
					w_bp_n2[i][1] = w_bp_n2[i][0];
				}
			}
			else if (ADS129X_FREQ_8KHZ == g_eeg.freq)
			{
				if (FILTER_SWITCH_STATE_ON == g_eeg.bp_filter)
				{
					/* node1 */
					w_bp_n1[i][0] = y - BP_A1_8KHZ_N1*w_bp_n1[i][1] - BP_A2_8KHZ_N1*w_bp_n1[i][2];
					y = BP_B0_8KHZ_N1*w_bp_n1[i][0] + BP_B1_8KHZ_N1*w_bp_n1[i][1] + BP_B2_8KHZ_N1*w_bp_n1[i][2];
					w_bp_n1[i][2] = w_bp_n1[i][1];
					w_bp_n1[i][1] = w_bp_n1[i][0];

					/* node2 */
					w_bp_n2[i][0] = y - BP_A1_8KHZ_N2*w_bp_n2[i][1] - BP_A2_8KHZ_N2*w_bp_n2[i][2];
					y = BP_B0_8KHZ_N2*w_bp_n2[i][0] + BP_B1_8KHZ_N2*w_bp_n2[i][1] + BP_B2_8KHZ_N2*w_bp_n2[i][2];
					w_bp_n2[i][2] = w_bp_n2[i][1];
					w_bp_n2[i][1] = w_bp_n2[i][0];
				}
			}
#endif /* SUPPORT_BP_FILTER */

#if SUPPORT_NOTCH_FILTER
			if (FILTER_SWITCH_STATE_ON == g_eeg.notch_filter)
			{
				/* 数据滤波：50Hz陷波滤波 */
				w_n[i][0] = g_eeg.n_a[0]*y - g_eeg.n_a[1]*w_n[i][1] - g_eeg.n_a[2]*w_n[i][2];
				y = g_eeg.n_b[0]*w_n[i][0] + g_eeg.n_b[1]*w_n[i][1] + g_eeg.n_b[2]*w_n[i][2];
				w_n[i][2] = w_n[i][1];
				w_n[i][1] = w_n[i][0];
			}
#endif /* SUPPORT_NOTCH_FILTER */

#if MONITOR_FILTER_RUN_TIME
			g_eeg.t_start = esp_timer_get_time();
#endif

			/* todo：去除基础噪音 */

			/* 装填报文 */
			filted = y;
			buf = g_eeg.sub_bufs[g_eeg.buf_idx];
			msg = &buf[g_eeg.frame_total_len*i];
			cnt = g_eeg.data_cnt;
			msg[DATA_FRAME_OFT_DATA + cnt*DATA_SIZE] = filted>>16;
			msg[DATA_FRAME_OFT_DATA + cnt*DATA_SIZE + 1] = filted>>8;
			msg[DATA_FRAME_OFT_DATA + cnt*DATA_SIZE + 2] = filted;
		}
		g_eeg.data_cnt += 1;

		/* 数据发送 */
		if (g_eeg.patch_size == g_eeg.data_cnt)
		{
			g_eeg.data_cnt = 0;

			time_stamp = esp_timer_get_time();
			/* 组装报文 */
#if (CONFIG_ADS129X_TYPE == 1294)
			for (i = ADS129X_CHN1; i <= ADS129X_CHN4; i++)
#elif (CONFIG_ADS129X_TYPE == 1299)
			for (i = ADS129X_CHN1; i < ADS1299_EFFECTIVE_CHN_NUM; i++)
#endif
			{
				buf = g_eeg.sub_bufs[g_eeg.buf_idx];
				msg = &buf[g_eeg.frame_total_len*i];
				msg[g_eeg.frame_oft_ts] = 	 (time_stamp&0xff00000000000000)>>56;
				msg[g_eeg.frame_oft_ts + 1] = (time_stamp&0x00ff000000000000)>>48;
				msg[g_eeg.frame_oft_ts + 2] = (time_stamp&0x0000ff0000000000)>>40;
				msg[g_eeg.frame_oft_ts + 3] = (time_stamp&0x000000ff00000000)>>32;
				msg[g_eeg.frame_oft_ts + 4] = (time_stamp&0x00000000ff000000)>>24;
				msg[g_eeg.frame_oft_ts + 5] = (time_stamp&0x0000000000ff0000)>>16;
				msg[g_eeg.frame_oft_ts + 6] = (time_stamp&0x000000000000ff00)>>8;
				msg[g_eeg.frame_oft_ts + 7] = time_stamp&0x00000000000000ff;
				msg[g_eeg.frame_oft_crc] = eeg_crc_cal(msg, g_eeg.frame_oft_crc);
			}

			/* 发送报文 */
			g_eeg.snd_buf_idx = g_eeg.buf_idx;
			g_eeg.buf_idx += 1;
			if (SEND_BUF_NUM == g_eeg.buf_idx)
			{
				g_eeg.buf_idx = 0;
			}
			g_eeg.snd_state = EEG_DATA_SEND;
		}
	}
}

/*************************************************
 * @description			:	定时监控函数
 * @param - xTimer		:	定时器句柄
 * @return 				:	无
**************************************************/
static void IRAM_ATTR eeg_monitor(TimerHandle_t xTimer)
{
	uint8_t notify[ESP_STM_MSG_LEN] = {0};

	//EEG_DBG("rate: %lu Hz", g_eeg.snd_rate);
	g_eeg.snd_rate = 0;

	g_eeg.info_report += 1;
	if (0xFF == g_eeg.info_report)
	{
		g_eeg.info_report = 0xFE;
	}

	notify[ES_MSG_OFT_HEAD] = (UART_MSG_HEAD&0xFF00)>>8;
	notify[ES_MSG_OFT_HEAD + 1] = UART_MSG_HEAD&0x00FF;
	notify[ES_MSG_OFT_TYPE] = ESP_STM_SYS_STATUS;

	if (EEG_STATE_IDLE == g_eeg.state)
	{
		notify[ES_MSG_OFT_DATA] = ESP32_IDLE;
	}
	else
	{
		switch (g_eeg.freq)
		{
			case ADS129X_FREQ_1KHZ:
				notify[ES_MSG_OFT_DATA] = ESP32_1KHZ;
				break;

			case ADS129X_FREQ_2KHZ:
				notify[ES_MSG_OFT_DATA] = ESP32_2KHZ;
				break;

			case ADS129X_FREQ_4KHZ:
				notify[ES_MSG_OFT_DATA] = ESP32_4KHZ;
				break;

			case ADS129X_FREQ_8KHZ:
				notify[ES_MSG_OFT_DATA] = ESP32_8KHZ;
				break;

			default:
				break;
		}
	}

	notify[ES_MSG_OFT_CRC] = crc_8bit_mask(notify, ES_MSG_OFT_CRC, UART_CRC_MASK);
	uart_data_transmit(notify, ESP_STM_MSG_LEN);
}

/*************************************************
 * @description			:	向ADS1294发送命令
 * @param - cmd			:	命令符
 * @return 				:	无
**************************************************/
static void ads129x_send_cmd(uint8_t cmd)
{
	spi_transaction_t t = {0};
	uint8_t data = cmd;

	/* CS */
	gpio_set_level(ADS129X_CS_GPIO, 0);
	usleep(100);
	t.length = 8;
	t.tx_buffer = &data;
	t.rx_buffer = NULL;
	t.user = NULL;
	if (ESP_OK != spi_device_polling_transmit(g_eeg.hspi, &t))
	{
		EEG_ERR("send cmd: 0x%02x failed", cmd);
	}
	usleep(100);
	gpio_set_level(ADS129X_CS_GPIO, 1);
}

/*************************************************
 * @description			:	写寄存器
 * @param - reg			:	要写入的寄存器
 * @param - data_buf	:	数据地址
 * @return 				:	无
**************************************************/
static void ads129x_write_reg(uint8_t reg, uint8_t *data_buf)
{
	spi_transaction_t t = {0};
	uint8_t data = 0;

	if (NULL == data_buf)
	{
		EEG_ERR("param null");
		return;
	}

	/* CS */
	gpio_set_level(ADS129X_CS_GPIO, 0);
	usleep(100);

	/* 发送寄存器地址 */
	data = (ADS129X_WREG | reg);
	t.length = 8;
	t.tx_buffer = &data;
	t.rx_buffer = NULL;
	t.user = NULL;
	if (ESP_OK != spi_device_polling_transmit(g_eeg.hspi, &t))
	{
		EEG_ERR("read reg failed");
	}
	//vTaskDelay(1);
	usleep(100);

	/* 发送写的寄存器个数 */
	memset(&t, 0, sizeof(spi_transaction_t));
	data = 0;
	t.length = 8;
	t.tx_buffer = &data;
	t.rx_buffer = NULL;
	t.user = NULL;
	if (ESP_OK != spi_device_polling_transmit(g_eeg.hspi, &t))
	{
		EEG_ERR("read reg failed");
	}
	//vTaskDelay(1);
	usleep(100);

	memset(&t, 0, sizeof(spi_transaction_t));
	data = data_buf[0];
	t.length = 8;
	t.tx_buffer = &data;
	t.rx_buffer = NULL;
	t.user = NULL;
	if (ESP_OK != spi_device_polling_transmit(g_eeg.hspi, &t))
	{
		EEG_ERR("read reg failed");
	}
	usleep(100);

	gpio_set_level(ADS129X_CS_GPIO, 1);
}

/*************************************************
 * @description			:	写多个寄存器
 * @param - regs		:	要读写的寄存器地址
 * @return 				:	无
**************************************************/
static void ads129x_write_all_regs(uint8_t *regs)
{
	uint8_t i = 0;

	if (NULL == regs)
	{
		EEG_ERR("input null");
		return;
	}

	for (i = ADS129X_REG_ID; i < ADS129X_REG_NUM; i++)
	{
		if (
#if (CONFIG_ADS129X_TYPE == 1294)
			(ADS129X_REG_CHN5 <= i && ADS129X_REG_CHN8 >= i) ||
#endif
			ADS129X_REG_ID == i ||						/* 只读 */
			ADS129X_REG_LOFF_STATP == i || 				/* 只读 */
			ADS129X_REG_LOFF_STATN == i 				/* 只读 */
		)
		{
			continue;
		}
		ads129x_write_reg(i, &regs[i]);
	}
}

/*************************************************
 * @description			:	读多个寄存器
 * @param - regs		:	要读写的寄存器地址
 * @return 				:	无
**************************************************/
static void ads129x_read_all_regs(uint8_t *regs)
{
	uint8_t i = 0;
	uint8_t reg_val = 0;

	if (NULL == regs)
	{
		EEG_ERR("input null");
		return;
	}

	for (i = ADS129X_REG_ID; i < ADS129X_REG_NUM; i++)
	{
#if (CONFIG_ADS129X_TYPE == 1294)
		if (ADS129X_REG_CHN5 <= i && ADS129X_REG_CHN8 >= i)
		{
			regs[i] = 0;
			continue;
		}
#endif
		reg_val = 0;
		ads129x_read_reg(i, &reg_val);
		regs[i] = reg_val;
	}
}

/*************************************************
 * @description			:	读寄存器
 * @param - reg			:	要读的寄存器
 * @param - data_buf	:	数据地址
 * @return 				:	无
**************************************************/
static void ads129x_read_reg(uint8_t reg, uint8_t *data_buf)
{
	spi_transaction_t t = {0};
	uint8_t data = 0;

	if (NULL == data_buf)
	{
		EEG_ERR("param null");
		return;
	}

	gpio_set_level(ADS129X_CS_GPIO, 0);

	/* 发送寄存器地址 */
	data = (ADS129X_RREG | reg);
	t.length = 8;
	t.tx_buffer = &data;
	t.rx_buffer = NULL;
	t.user = NULL;
	if (ESP_OK != spi_device_polling_transmit(g_eeg.hspi, &t))
	{
		EEG_ERR("read reg failed");
	}
	usleep(100);

	/* 发送读取的寄存器个数 */
	memset(&t, 0, sizeof(spi_transaction_t));
	data = 0;
	t.length = 8;
	t.tx_buffer = &data;
	t.rx_buffer = NULL;
	t.user = NULL;
	if (ESP_OK != spi_device_polling_transmit(g_eeg.hspi, &t))
	{
		EEG_ERR("read reg failed");
	}
	usleep(100);

	/* 接收数据 */
	memset(&t, 0, sizeof(spi_transaction_t));
	data = 0;
	t.length = 8;
	t.rxlength = 8;
	t.tx_buffer = NULL;
	t.rx_buffer = data_buf;
	if (ESP_OK != spi_device_polling_transmit(g_eeg.hspi, &t))
	{
		EEG_ERR("read reg failed");
	}
	usleep(100);
	gpio_set_level(ADS129X_CS_GPIO, 1);
}

/*************************************************
 * @description			:	从全局变量中设置寄存器数组
 * @param - reg_arr		:	寄存器数组地址
 * @return 				:	无
**************************************************/
static void ads129x_read_regs_from_context(uint8_t *reg_arr)
{
	uint8_t val = 0;
	int i = 0;

	if (NULL == reg_arr)
	{
		EEG_ERR("param null");
		return;
	}

	/* 1.ID 只读 */
	reg_arr[ADS129X_REG_ID] = ADS129X_REG_ID_DEFAULT;

	/* 2.config1  */
	val = 0x0;
#if (CONFIG_ADS129X_TYPE == 1294)
	SET_BIT(val, R_SHIFT(g_eeg.regs.config1.hr, 7));			/* [7] 0 低功耗模式 */
#elif (CONFIG_ADS129X_TYPE == 1299)
	SET_BIT(val, R_SHIFT(g_eeg.regs.config1.reserve2, 7));
#endif
	SET_BIT(val, R_SHIFT(g_eeg.regs.config1.daisy_en, 6));		/* [6] 0 菊花链模式 */
	SET_BIT(val, R_SHIFT(g_eeg.regs.config1.clk_en, 5));		/* [5] 0 CLK引脚不输出时钟信号 */
	SET_BIT(val, R_SHIFT(g_eeg.regs.config1.reserve1, 3));
	SET_BIT(val, R_SHIFT(g_eeg.regs.config1.date_rate, 0));		/* [2:0] 设置采样率 */
	reg_arr[ADS129X_REG_CONFIG1] = val;

	/* 3.config2  */
	val = 0x0;
#if (CONFIG_ADS129X_TYPE == 1294)
	SET_BIT(val, R_SHIFT(g_eeg.regs.config2.reserve2, 6));
	SET_BIT(val, R_SHIFT(g_eeg.regs.config2.wct_chop, 5));		/* [5] 0 WCT斩波频率可变 */
#elif (CONFIG_ADS129X_TYPE == 1299)
	SET_BIT(val, R_SHIFT(g_eeg.regs.config2.reserve2, 5));
#endif
	SET_BIT(val, R_SHIFT(g_eeg.regs.config2.int_test, 4));		/* [4] 1 测试信号来源 */
	SET_BIT(val, R_SHIFT(g_eeg.regs.config2.reserve1, 3));
	SET_BIT(val, R_SHIFT(g_eeg.regs.config2.test_amp, 2));		/* [2] 0 设置测试信号幅值 */
	SET_BIT(val, R_SHIFT(g_eeg.regs.config2.test_freq, 0));		/* [1:0] 设置测试信号频率 */
	reg_arr[ADS129X_REG_CONFIG2] = val;

	/* 4.config3 0xcc, 1100 1100	[6]必须为1 [0]为只读 */
	val = 0x0;
#if (CONFIG_ADS129X_TYPE == 1294)
	SET_BIT(val, R_SHIFT(g_eeg.regs.config3.pd_refbuf, 7));		/* [7] 1 启用内部引用缓冲区 */
	SET_BIT(val, R_SHIFT(g_eeg.regs.config3.reserve, 6));
	SET_BIT(val, R_SHIFT(g_eeg.regs.config3.vref_4v, 5));		/* [5] 0 内部参考电压2.4V */
	SET_BIT(val, R_SHIFT(g_eeg.regs.config3.rld_meas, 4));		/* [4] 0 RLD测量，不使用 */
	SET_BIT(val, R_SHIFT(g_eeg.regs.config3.rldref_int, 3));	/* [3] 1 内部产生参考信号 */
	SET_BIT(val, R_SHIFT(g_eeg.regs.config3.pd_rld, 2));		/* [2] 1 RLD缓冲区使能 */
	SET_BIT(val, R_SHIFT(g_eeg.regs.config3.rld_loff_sens, 1));	/* [1] 0 RLD导联脱落检测功能，关闭 */
	SET_BIT(val, R_SHIFT(g_eeg.regs.config3.rld_stat, 0));
#elif (CONFIG_ADS129X_TYPE == 1299)
	SET_BIT(val, R_SHIFT(g_eeg.regs.config3.pd_refbuf, 7));
	SET_BIT(val, R_SHIFT(g_eeg.regs.config3.reserve, 5));
	SET_BIT(val, R_SHIFT(g_eeg.regs.config3.bias_meas, 4));
	SET_BIT(val, R_SHIFT(g_eeg.regs.config3.biasref_int, 3));
	SET_BIT(val, R_SHIFT(g_eeg.regs.config3.pd_bias, 2));
	SET_BIT(val, R_SHIFT(g_eeg.regs.config3.bias_loff_sens, 1));
	SET_BIT(val, R_SHIFT(g_eeg.regs.config3.bias_stat, 0));
#endif
	reg_arr[ADS129X_REG_CONFIG3] = val;

	/* loff */
	val = 0x0;
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff.comp_th, 5));			/* [7:5] 000 导联脱落检测器阈值，95% */
#if (CONFIG_ADS129X_TYPE == 1294)
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff.vlead_off_en, 4));		/* [4]   0   电流源模式导联脱落 */
#elif (CONFIG_ADS129X_TYPE == 1299)
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff.reserve, 4));
#endif
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff.ilead_Off, 2));		/* [3:2] 00  导联脱落检测器导通的电流幅度，6nA */
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff.flead_Off, 0));		/* [1:0] 10  导联脱落检测器的导通频率，不使用 */
	reg_arr[ADS129X_REG_LOFF] = val;

	/* ch1set-ch4set [3]必须为0 */
	for (i = ADS129X_CHN1; i < ADS129X_CHN_NUM; i++)
	{
		val = 0;
		SET_BIT(val, R_SHIFT(g_eeg.regs.chset[i].pd, 7));		/* [7] 0 通道通电 */
		SET_BIT(val, R_SHIFT(g_eeg.regs.chset[i].gain, 4));		/* [6:4] 设置PGA增益 */
#if (CONFIG_ADS129X_TYPE == 1294)
		SET_BIT(val, R_SHIFT(g_eeg.regs.chset[i].reserve, 3));
#elif (CONFIG_ADS129X_TYPE == 1299)
		SET_BIT(val, R_SHIFT(g_eeg.regs.chset[i].srb2, 3));
#endif
		SET_BIT(val, R_SHIFT(g_eeg.regs.chset[i].mux, 0));		/* [2:0] 设置通道输入方式，正常模式 */
		reg_arr[ADS129X_REG_CHN1 + i] = val;
	}

#if (CONFIG_ADS129X_TYPE == 1294)
	/* RLD_SENSP */
	val = 0;
	SET_BIT(val, R_SHIFT(g_eeg.regs.rld_sens_p.chn1, 0));
	SET_BIT(val, R_SHIFT(g_eeg.regs.rld_sens_p.chn2, 1));
	SET_BIT(val, R_SHIFT(g_eeg.regs.rld_sens_p.chn3, 2));
	SET_BIT(val, R_SHIFT(g_eeg.regs.rld_sens_p.chn4, 3));
	SET_BIT(val, R_SHIFT(g_eeg.regs.rld_sens_p.reserve, 4));
	reg_arr[ADS1294_REG_RLD_SENSP] = val;

	/* RLD_SENSN */
	val = 0;
	SET_BIT(val, R_SHIFT(g_eeg.regs.rld_sens_n.chn1, 0));
	SET_BIT(val, R_SHIFT(g_eeg.regs.rld_sens_n.chn2, 1));
	SET_BIT(val, R_SHIFT(g_eeg.regs.rld_sens_n.chn3, 2));
	SET_BIT(val, R_SHIFT(g_eeg.regs.rld_sens_n.chn4, 3));
	SET_BIT(val, R_SHIFT(g_eeg.regs.rld_sens_n.reserve, 4));
	reg_arr[ADS1294_REG_RLD_SENSN] = val;
#elif (CONFIG_ADS129X_TYPE == 1299)
	val = 0;
	SET_BIT(val, R_SHIFT(g_eeg.regs.bias_sens_p.chn1, 0));
	SET_BIT(val, R_SHIFT(g_eeg.regs.bias_sens_p.chn2, 1));
	SET_BIT(val, R_SHIFT(g_eeg.regs.bias_sens_p.chn3, 2));
	SET_BIT(val, R_SHIFT(g_eeg.regs.bias_sens_p.chn4, 3));
	SET_BIT(val, R_SHIFT(g_eeg.regs.bias_sens_p.chn5, 4));
	SET_BIT(val, R_SHIFT(g_eeg.regs.bias_sens_p.chn6, 5));
	SET_BIT(val, R_SHIFT(g_eeg.regs.bias_sens_p.chn7, 6));
	SET_BIT(val, R_SHIFT(g_eeg.regs.bias_sens_p.chn8, 7));
	reg_arr[ADS1299_REG_BIAS_SENSP] = val;

	val = 0;
	SET_BIT(val, R_SHIFT(g_eeg.regs.bias_sens_n.chn1, 0));
	SET_BIT(val, R_SHIFT(g_eeg.regs.bias_sens_n.chn2, 1));
	SET_BIT(val, R_SHIFT(g_eeg.regs.bias_sens_n.chn3, 2));
	SET_BIT(val, R_SHIFT(g_eeg.regs.bias_sens_n.chn4, 3));
	SET_BIT(val, R_SHIFT(g_eeg.regs.bias_sens_n.chn5, 4));
	SET_BIT(val, R_SHIFT(g_eeg.regs.bias_sens_n.chn6, 5));
	SET_BIT(val, R_SHIFT(g_eeg.regs.bias_sens_n.chn7, 6));
	SET_BIT(val, R_SHIFT(g_eeg.regs.bias_sens_n.chn8, 7));
	reg_arr[ADS1299_REG_BIAS_SENSN] = val;
#endif

	/* LOFF_SENSP */
	val = 0;
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_sens_p.chn1, 0));
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_sens_p.chn2, 1));
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_sens_p.chn3, 2));
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_sens_p.chn4, 3));
#if (CONFIG_ADS129X_TYPE == 1294)
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_sens_p.reserve, 4));
#elif (CONFIG_ADS129X_TYPE == 1299)
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_sens_p.chn5, 4));
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_sens_p.chn6, 5));
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_sens_p.chn7, 6));
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_sens_p.chn8, 7));
#endif
	reg_arr[ADS129X_REG_LOFF_SENSP] = val;

	/* LOFF_SENSN */
	val = 0;
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_sens_n.chn1, 0));
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_sens_n.chn2, 1));
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_sens_n.chn3, 2));
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_sens_n.chn4, 3));
#if (CONFIG_ADS129X_TYPE == 1294)
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_sens_n.reserve, 4));
#elif (CONFIG_ADS129X_TYPE == 1299)
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_sens_n.chn5, 4));
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_sens_n.chn6, 5));
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_sens_n.chn7, 6));
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_sens_n.chn8, 7));
#endif
	reg_arr[ADS129X_REG_LOFF_SENSN] = val;

	/* LOFF_FLIP */
	val = 0;
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_flip.chn1, 0));
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_flip.chn2, 1));
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_flip.chn3, 2));
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_flip.chn4, 3));
#if (CONFIG_ADS129X_TYPE == 1294)
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_flip.reserve, 4));
#elif (CONFIG_ADS129X_TYPE == 1299)
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_flip.chn5, 4));
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_flip.chn6, 5));
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_flip.chn7, 6));
	SET_BIT(val, R_SHIFT(g_eeg.regs.loff_flip.chn8, 7));
#endif
	reg_arr[ADS129X_REG_LOFF_FLIP] = val;

	/* LOFF_STATP 只读 */
	reg_arr[ADS129X_REG_LOFF_STATP] = ADS129X_REG_LOFF_STATP_DEFAULT;

	/* LOFF_STATN 只读 */
	reg_arr[ADS129X_REG_LOFF_STATN] = ADS129X_REG_LOFF_STATN_DEFAULT;

	/* GPIO */
	val = 0;
	SET_BIT(val, R_SHIFT(g_eeg.regs.gpio.gpio_d, 4));		/* [7:4] 用于向GPIO端口读写数据 */
	SET_BIT(val, R_SHIFT(g_eeg.regs.gpio.gpio_c, 0));
	reg_arr[ADS129X_REG_GPIO] = val;
#if (CONFIG_ADS129X_TYPE == 1294)
	/* PACE [7:5]必须为0 */
	val = 0;
	SET_BIT(val, R_SHIFT(g_eeg.regs.pace.reserve, 5));
	SET_BIT(val, R_SHIFT(g_eeg.regs.pace.pacee, 3));		/* [4:3] 起搏信号通道 */
	SET_BIT(val, R_SHIFT(g_eeg.regs.pace.paceo, 1));		/* [2:1] 起搏信号通道 */
	SET_BIT(val, R_SHIFT(g_eeg.regs.pace.pd_pace, 0));		/* [0] 0 起搏信号寄存器 */
	reg_arr[ADS1294_REG_PACE] = val;

	/* RESP [5] 必须为1 */
	val = 0x0;
	SET_BIT(val, R_SHIFT(g_eeg.regs.resp.resp_demod_en, 7));
	SET_BIT(val, R_SHIFT(g_eeg.regs.resp.resp_mod_en, 6));
	SET_BIT(val, R_SHIFT(g_eeg.regs.resp.reserve, 5));
	SET_BIT(val, R_SHIFT(g_eeg.regs.resp.resp_ph, 2));
	SET_BIT(val, R_SHIFT(g_eeg.regs.resp.resp_ctrl, 0));
	reg_arr[ADS1294_REG_RESP] = val;
#elif (CONFIG_ADS129X_TYPE == 1299)
	/* misc1 */
	val = 0x0;
	SET_BIT(val, R_SHIFT(g_eeg.regs.misc1.reserve2, 6));
	SET_BIT(val, R_SHIFT(g_eeg.regs.misc1.srb1, 5));
	SET_BIT(val, R_SHIFT(g_eeg.regs.misc1.reserve1, 0));
	reg_arr[ADS1299_REG_MISC1] = val;

	/* misc1 */
	val = 0x0;
	SET_BIT(val, R_SHIFT(g_eeg.regs.misc2.reserve, 0));
	reg_arr[ADS1299_REG_MISC2] = val;
#endif

	/* CONFIG4 */
	val = 0;
#if (CONFIG_ADS129X_TYPE == 1294)
	SET_BIT(val, R_SHIFT(g_eeg.regs.config4.resp_freq, 5));			/* [7:5] 000 呼吸调制频率64Hz */
	SET_BIT(val, R_SHIFT(g_eeg.regs.config4.reserve2, 4));
#elif (CONFIG_ADS129X_TYPE == 1299)
	SET_BIT(val, R_SHIFT(g_eeg.regs.config4.reserve3, 4));
#endif
	SET_BIT(val, R_SHIFT(g_eeg.regs.config4.single_shot, 3));		/* [3] 0	连续转换模式 */
#if (CONFIG_ADS129X_TYPE == 1294)
	SET_BIT(val, R_SHIFT(g_eeg.regs.config4.wct_to_rld, 2));		/* [2] 0	不将WCT连接到RLD */
#elif (CONFIG_ADS129X_TYPE == 1299)
	SET_BIT(val, R_SHIFT(g_eeg.regs.config4.reserve2, 2));
#endif
	SET_BIT(val, R_SHIFT(g_eeg.regs.config4.pd_loff_comp, 1));		/* [1] 0	导联脱落检测使能关闭 */
	SET_BIT(val, R_SHIFT(g_eeg.regs.config4.reserve1, 0));
	reg_arr[ADS129X_REG_CONFIG4] = val;

#if (CONFIG_ADS129X_TYPE == 1294)
	/* WCT1 */
	val = 0;
	SET_BIT(val, R_SHIFT(g_eeg.regs.wct1.avr_ch6, 7));
	SET_BIT(val, R_SHIFT(g_eeg.regs.wct1.avr_ch5, 6));
	SET_BIT(val, R_SHIFT(g_eeg.regs.wct1.avr_ch7, 5));
	SET_BIT(val, R_SHIFT(g_eeg.regs.wct1.avr_ch4, 4));
	SET_BIT(val, R_SHIFT(g_eeg.regs.wct1.pd_wcta, 3));
	SET_BIT(val, R_SHIFT(g_eeg.regs.wct1.wcta, 0));
	reg_arr[ADS1294_REG_WCT1] = val;

	/* WCT1 */
	val = 0;
	SET_BIT(val, R_SHIFT(g_eeg.regs.wct2.pd_wctc, 7));
	SET_BIT(val, R_SHIFT(g_eeg.regs.wct2.pd_wctb, 6));
	SET_BIT(val, R_SHIFT(g_eeg.regs.wct2.wctb, 3));
	SET_BIT(val, R_SHIFT(g_eeg.regs.wct2.wctc, 0));
	reg_arr[ADS1294_REG_WCT2] = val;
#endif
}

/*************************************************
 * @description			:	crc校验表初始化
 * @param - mask		:	掩码
 * @return 				:	无
**************************************************/
static void eeg_crc_table_init(uint8_t mask)
{
	int i = 0;
	int j = 0;
	uint8_t cur = 0;

	for (i  =0; i < EEG_CRC_TABLE_SIZE; i++)
	{
		cur = i;
		for (j = 0; j < 8; j++)
		{
			if ((0x80 & cur) != 0)
			{
				cur = cur << 1^(mask);
			}
			else
			{
				cur <<= 1;
			}
		}
		g_eeg.crc_table[i] = cur;
	}
}

/*************************************************
 * @description			:	crc校验
 * @param - buf			:	数据地址
 * @param - len			:	数据长度
 * @return 				:	校验结果
**************************************************/
static inline uint8_t eeg_crc_cal(uint8_t *buf, int len)
{
	int i = 0;
	uint8_t crc = 0;

	for (i = 0; i < len; i++)
	{
		crc = g_eeg.crc_table[crc ^ buf[i]];
	}

	return crc;
}

/*************************************************
 * @description			:	从flash中同步参数到
 * 							全局变量中
 * @param - 			:	无
 * @return 				:	无
**************************************************/
static void eeg_sync_params_from_flash(void)
{
	int i = 0;
	NET_CONFIG net_cfg = {0};
	NET_CONFIG cfg_cmp = {0};
	const char *fre_info[ADS129X_FREQ_NUM] = {"1kHz", "2kHz", "4kHz", "8kHz"};
	bool match = false;

	if (ESP_OK != nvs_read_net_cfg_from_flash(&net_cfg, "OK"))
	{
		/* 读取失败，使用默认地址 */
		EEG_ERR("read from flash failed, use default network params");
		g_eeg.host_ip = inet_addr(EEG_DEFAULT_HOST_IP);
		EEG_ERR("host: %s", EEG_DEFAULT_HOST_IP);

		g_eeg.port = EEG_DEFAULT_HOST_PORT;
		EEG_ERR("port: %u", g_eeg.port);

		g_eeg.client_ip = inet_addr(DEFAULT_CLIENT_IP_ADDR);
		EEG_ERR("client ip: %s", DEFAULT_CLIENT_IP_ADDR);

		g_eeg.serial = EEG_DEFAULT_SERIAL_NUM;
		EEG_ERR("serial num: %lu", g_eeg.serial);

		g_eeg.protocol = EEG_DEFAULT_PROTOCOL;
		EEG_ERR("protocol: %s", (SOCK_STREAM == g_eeg.protocol) ? "TCP" : "UDP");

		g_eeg.freq = EEG_DEFAULT_FREQUENCY;
		EEG_ERR("frequency: %s", fre_info[g_eeg.freq]);

		g_eeg.connect_type = EEG_DEFAULT_NET_CONNECT_TYPE;
		EEG_ERR("net connect type: %s", g_eeg.connect_type ? "manual" : "auto");

		g_eeg.bp_filter = BP_FILTER_DEFAULT_STATE;
		EEG_ERR("bandpass filter state: %s", (FILTER_SWITCH_STATE_ON == g_eeg.bp_filter) ? "on" : "off");

		g_eeg.hp_filter = HP_FILTER_DEFAULT_STATE;
		EEG_ERR("highpass filter state: %s", (FILTER_SWITCH_STATE_ON == g_eeg.hp_filter) ? "on" : "off");

		g_eeg.lp_filter = LP_FILTER_DEFAULT_STATE;
		EEG_ERR("lowpass filter state: %s", (FILTER_SWITCH_STATE_ON == g_eeg.lp_filter) ? "on" : "off");

		g_eeg.notch_filter = NOTCH_FILTER_DEFAULT_STATE;
		EEG_ERR("notch filter state: %s", g_eeg.notch_filter ? "on" : "off");
	}
	else
	{
		/* 配网方式 */
		if (0 == memcmp(cfg_cmp.connect_type, net_cfg.connect_type, CONNECT_TYPE_STR_LEN))
		{
			/* 用户未设置，则使用默认参数 */
			g_eeg.connect_type = EEG_DEFAULT_NET_CONNECT_TYPE;
			EEG_DBG("use default connect type: %s",  g_eeg.connect_type ? "manual" : "auto");
		}
		else
		{
			if (0 == memcmp(net_cfg.connect_type, "auto", 4))
			{
				g_eeg.connect_type = EEG_NET_AUTO_CONNECT;
			}
			else if (0 == memcmp(net_cfg.connect_type, "manual", 6))
			{
				g_eeg.connect_type = EEG_NET_MANUAL_CONNECT;
			}
			else
			{
				g_eeg.connect_type = EEG_DEFAULT_NET_CONNECT_TYPE;
			}
			EEG_INFO("connect type: %s",  g_eeg.connect_type ? "manual" : "auto");
		}

		/* 序列号 */
		if (0 == memcmp(cfg_cmp.client_serial, net_cfg.client_serial, SERIAL_STRING_LEN))
		{
			g_eeg.serial = EEG_DEFAULT_SERIAL_NUM;
			EEG_DBG("use default serial num: %lu", g_eeg.serial);
		}
		else
		{
			g_eeg.serial = serial_num_str_parse(net_cfg.client_serial);
			EEG_INFO("serial num:" SERIAL_NUM_FORMAT,  SERIAL_NUM_PARSE(g_eeg.serial));
		}

		/* 协议 */
		if (0 == memcmp(cfg_cmp.protocol, net_cfg.protocol, PROTOCOL_STR_LEN))
		{
			g_eeg.protocol = EEG_DEFAULT_PROTOCOL;
			EEG_DBG("use default protocol: %s", (SOCK_STREAM == g_eeg.protocol) ? "TCP" : "UDP");
		}
		else
		{
			if (0 == memcmp(net_cfg.protocol, "TCP", 3) || 0 == memcmp(net_cfg.protocol, "tcp", 3))
			{
				g_eeg.protocol = SOCK_STREAM;
			}
			else if (0 == memcmp(net_cfg.protocol, "UDP", 3) || 0 == memcmp(net_cfg.protocol, "udp", 3))
			{
				g_eeg.protocol = SOCK_DGRAM;
			}
			else
			{
				g_eeg.protocol = EEG_DEFAULT_PROTOCOL;
				EEG_ERR("unknown protocol: %s use default: %s", net_cfg.protocol,
									(SOCK_STREAM == g_eeg.protocol) ? "TCP" : "UDP");
			}
			EEG_INFO("protocol: %s", (SOCK_STREAM == g_eeg.protocol) ? "TCP" : "UDP");
		}

		/* 采样率 */
		if (0 == memcmp(cfg_cmp.frequency, net_cfg.frequency, FREQUENCY_STR_LEN))
		{
			g_eeg.freq = EEG_DEFAULT_FREQUENCY;
			EEG_DBG("use default frequency: %s", fre_info[g_eeg.freq]);
		}
		else
		{
			for (i = ADS129X_FREQ_1KHZ; i < ADS129X_FREQ_NUM; i++)
			{
				if (0 == strcmp(net_cfg.frequency, fre_info[i]))
				{
					g_eeg.freq = i;
					match = true;
					break;
				}
			}
			if (false == match)
			{
				g_eeg.freq = EEG_DEFAULT_FREQUENCY;
				EEG_DBG("unknown frequency: %s use default frequency: %s", net_cfg.frequency, fre_info[g_eeg.freq]);
			}
			EEG_INFO("frequency: %s", fre_info[g_eeg.freq]);
		}

		/* 滤波器开关状态 */
		if (0 == memcmp(cfg_cmp.bp_filter, net_cfg.bp_filter, FILTER_SWITCH_STR_LEN))
		{
			g_eeg.bp_filter = BP_FILTER_DEFAULT_STATE;
			EEG_DBG("set bandpass filter to default: %s", (FILTER_SWITCH_STATE_ON == g_eeg.bp_filter) ? "on" : "off");
		}
		else
		{
			if (0 == memcmp(net_cfg.bp_filter, "on", 2) || 0 == memcmp(net_cfg.bp_filter, "ON", 2))
			{
				g_eeg.bp_filter = FILTER_SWITCH_STATE_ON;
			}
			else if (0 == memcmp(net_cfg.bp_filter, "off", 3) || 0 == memcmp(net_cfg.bp_filter, "OFF", 3))
			{
				g_eeg.bp_filter = FILTER_SWITCH_STATE_OFF;
			}
			else
			{
				g_eeg.bp_filter = BP_FILTER_DEFAULT_STATE;
				EEG_ERR("unknown bandpass filter state: %s use default: %s", net_cfg.bp_filter, (FILTER_SWITCH_STATE_ON == g_eeg.bp_filter) ? "on" : "off");
			}
			EEG_INFO("bandpass filter: %s", (FILTER_SWITCH_STATE_ON == g_eeg.bp_filter) ? "on" : "off");
		}

		if (0 == memcmp(cfg_cmp.hp_filter, net_cfg.hp_filter, FILTER_SWITCH_STR_LEN))
		{
			g_eeg.hp_filter = HP_FILTER_DEFAULT_STATE;
			EEG_DBG("set highpass filter to default: %s", (FILTER_SWITCH_STATE_ON == g_eeg.hp_filter) ? "on" : "off");
		}
		else
		{
			if (0 == memcmp(net_cfg.hp_filter, "on", 2) || 0 == memcmp(net_cfg.hp_filter, "ON", 2))
			{
				g_eeg.hp_filter = FILTER_SWITCH_STATE_ON;
			}
			else if (0 == memcmp(net_cfg.hp_filter, "off", 3) || 0 == memcmp(net_cfg.hp_filter, "OFF", 3))
			{
				g_eeg.hp_filter = FILTER_SWITCH_STATE_OFF;
			}
			else
			{
				g_eeg.hp_filter = HP_FILTER_DEFAULT_STATE;
				EEG_ERR("unknown highpass filter state: %s use default: %s", net_cfg.hp_filter, (FILTER_SWITCH_STATE_ON == g_eeg.hp_filter) ? "on" : "off");
			}
			EEG_INFO("highpass filter: %s", (FILTER_SWITCH_STATE_ON == g_eeg.hp_filter) ? "on" : "off");
		}

		if (0 == memcmp(cfg_cmp.lp_filter, net_cfg.lp_filter, FILTER_SWITCH_STR_LEN))
		{
			g_eeg.lp_filter = LP_FILTER_DEFAULT_STATE;
			EEG_DBG("set lowpass filter to default: %s", (FILTER_SWITCH_STATE_ON == g_eeg.lp_filter) ? "on" : "off");
		}
		else
		{
			if (0 == memcmp(net_cfg.lp_filter, "on", 2) || 0 == memcmp(net_cfg.lp_filter, "ON", 2))
			{
				g_eeg.lp_filter = FILTER_SWITCH_STATE_ON;
			}
			else if (0 == memcmp(net_cfg.lp_filter, "off", 3) || 0 == memcmp(net_cfg.lp_filter, "OFF", 3))
			{
				g_eeg.lp_filter = FILTER_SWITCH_STATE_OFF;
			}
			else
			{
				g_eeg.lp_filter = LP_FILTER_DEFAULT_STATE;
				EEG_ERR("unknown lowpass filter state: %s use default: %s", net_cfg.lp_filter, (FILTER_SWITCH_STATE_ON == g_eeg.lp_filter) ? "on" : "off");
			}
			EEG_INFO("lowpass filter: %s", (FILTER_SWITCH_STATE_ON == g_eeg.lp_filter) ? "on" : "off");
		}

		if (0 == memcmp(cfg_cmp.notch_filter, net_cfg.notch_filter, FILTER_SWITCH_STR_LEN))
		{
			g_eeg.notch_filter = BP_FILTER_DEFAULT_STATE;
			EEG_DBG("set notch filter to default: %s", (FILTER_SWITCH_STATE_ON == g_eeg.notch_filter) ? "on" : "off");
		}
		else
		{
			if (0 == memcmp(net_cfg.notch_filter, "on", 2) || 0 == memcmp(net_cfg.notch_filter, "ON", 2))
			{
				g_eeg.notch_filter = FILTER_SWITCH_STATE_ON;
			}
			else if (0 == memcmp(net_cfg.notch_filter, "off", 3) || 0 == memcmp(net_cfg.notch_filter, "OFF", 3))
			{
				g_eeg.notch_filter = FILTER_SWITCH_STATE_OFF;
			}
			else
			{
				g_eeg.notch_filter = FILTER_SWITCH_STATE_ON;
				EEG_ERR("unknown notch filter state: %s use default: %s", net_cfg.notch_filter, (FILTER_SWITCH_STATE_ON == g_eeg.notch_filter) ? "on" : "off");
			}
			EEG_INFO("notch filter: %s", (FILTER_SWITCH_STATE_ON == g_eeg.notch_filter) ? "on" : "off");
		}

		if (EEG_NET_MANUAL_CONNECT == g_eeg.connect_type)
		{
			/* 上位机IP */
			if (0 == memcmp(cfg_cmp.host_ip, net_cfg.host_ip, IP_STRING_LEN))
			{
				/* 用户未设置，则使用默认参数 */
				g_eeg.host_ip = inet_addr(EEG_DEFAULT_HOST_IP);
				EEG_DBG("use default host ip: %s", EEG_DEFAULT_HOST_IP);
			}
			else
			{
				g_eeg.host_ip = inet_addr(net_cfg.host_ip);
				EEG_INFO("host: %s", net_cfg.host_ip);
			}

			/* 端口 */
			if (0 == memcmp(cfg_cmp.eeg_port, net_cfg.eeg_port, PORT_STRING_LEN))
			{
				g_eeg.port = EEG_DEFAULT_HOST_PORT;
				EEG_DBG("use default port: %u", g_eeg.port);
			}
			else
			{
				g_eeg.port = atoi(net_cfg.eeg_port);
				EEG_INFO("port: %u", g_eeg.port);
			}

			/* 下位机IP */
			if (0 == memcmp(cfg_cmp.client_ip, net_cfg.client_ip, IP_STRING_LEN))
			{
				g_eeg.client_ip = inet_addr(DEFAULT_CLIENT_IP_ADDR);
				EEG_DBG("use default client ip: %s", DEFAULT_CLIENT_IP_ADDR);
			}
			else
			{
				g_eeg.client_ip = inet_addr(net_cfg.client_ip);
				EEG_INFO("client: %s", net_cfg.client_ip);
			}
		}
		else
		{
			/* 自动获取上位机IP、掩码、端口等信息 */
			eeg_search_host_ip();
		}

/* 初始化管理端口 */
retry:
		EEG_DBG("init admin sock");
		g_eeg.admin_sock = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
		if (0 > g_eeg.admin_sock)
		{
			EEG_ERR("create sock failed: %d (%s)", errno, strerror(errno));
			goto retry;
		}

		g_eeg.from.sin_family = AF_INET;
		g_eeg.from.sin_addr.s_addr = g_eeg.host_ip;
		g_eeg.from.sin_port = htons(EEG_HOST_BROADCAST_PORT);
	}

	switch (g_eeg.freq)
	{
		case ADS129X_FREQ_1KHZ:
			eeg_sync_params_1kHz();
			break;

		case ADS129X_FREQ_2KHZ:
			eeg_sync_params_2kHz();
			break;

		case ADS129X_FREQ_4KHZ:
			eeg_sync_params_4kHz();
			break;

		case ADS129X_FREQ_8KHZ:
			eeg_sync_params_8kHz();
			break;
		
		default:
			eeg_sync_params_2kHz();
			break;
	}

#if SUPPORT_NOTCH_FILTER
	EEG_INFO("notch filter params:");
	for (i = 0; i <= NOTCH_FILTER_DEGREE; i++)
	{
		EEG_INFO("A%d: %.16f", i, g_eeg.n_a[i]);
	}
	for (i = 0; i <= NOTCH_FILTER_DEGREE; i++)
	{
		EEG_INFO("B%d: %.16f", i, g_eeg.n_b[i]);
	}
#endif

	g_eeg.frame_oft_crc = g_eeg.frame_total_len - CRC_SIZE;
	g_eeg.frame_oft_ts = g_eeg.frame_oft_crc - TS_SIZE;
}

/*************************************************
 * @description			:	中同步1kHz模式的参数到
 * 							全局变量中
 * @param - 			:	无
 * @return 				:	无
**************************************************/
static void eeg_sync_params_1kHz(void)
{
	g_eeg.patch_size = PATCH_SIZE_LOW_FRE_MODE;
#if (CONFIG_ADS129X_TYPE == 1294)
#if (ADS1294_POWER_MODE == HIGH_RATE_MODE)
	g_eeg.regs.config1.date_rate = HR_FREQ_1_KHZ;
#else
	g_eeg.regs.config1.date_rate = LP_FREQ_1_KHZ;
#endif
#elif (CONFIG_ADS129X_TYPE == 1299)
	g_eeg.regs.config1.date_rate = FREQ_1_KHZ;
#endif
	g_eeg.frame_total_len = DATA_FRAME_TOTAL_LEN_LOW_FRE_MODE;

#if (SUPPORT_BP_FILTER)
	EEG_INFO("bandpass filter node1:");
	EEG_INFO("a0: %.16f a1: %.16f a2: %.16f", BP_A0_1KHZ_N1, BP_A1_1KHZ_N1, BP_A2_1KHZ_N1);
	EEG_INFO("b0: %.16f b1: %.16f b2: %.16f", BP_B0_1KHZ_N1, BP_B1_1KHZ_N1, BP_B2_1KHZ_N1);

	EEG_INFO("bandpass filter node2:");
	EEG_INFO("a0: %.16f a1: %.16f a2: %.16f", BP_A0_1KHZ_N2, BP_A1_1KHZ_N2, BP_A2_1KHZ_N2);
	EEG_INFO("b0: %.16f b1: %.16f b2: %.16f", BP_B0_1KHZ_N2, BP_B1_1KHZ_N2, BP_B2_1KHZ_N2);

	EEG_INFO("bandpass filter node3:");
	EEG_INFO("a0: %.16f a1: %.16f a2: %.16f", BP_A0_1KHZ_N3, BP_A1_1KHZ_N3, BP_A2_1KHZ_N3);
	EEG_INFO("b0: %.16f b1: %.16f b2: %.16f", BP_B0_1KHZ_N3, BP_B1_1KHZ_N3, BP_B2_1KHZ_N3);
#endif

#if SUPPORT_NOTCH_FILTER
	g_eeg.n_a[0] = N_A0_1KHZ;
	g_eeg.n_a[1] = N_A1_1KHZ;
	g_eeg.n_a[2] = N_A2_1KHZ;
	g_eeg.n_b[0] = N_B0_1KHZ;
	g_eeg.n_b[1] = N_B1_1KHZ;
	g_eeg.n_b[2] = N_B2_1KHZ;
#endif
}

/*************************************************
 * @description			:	中同步2kHz模式的参数到
 * 							全局变量中
 * @param - 			:	无
 * @return 				:	无
**************************************************/
static void eeg_sync_params_2kHz(void)
{
	g_eeg.patch_size = PATCH_SIZE_HIGH_FRE_MODE;
#if (CONFIG_ADS129X_TYPE == 1294)
#if (ADS1294_POWER_MODE == HIGH_RATE_MODE)
	g_eeg.regs.config1.date_rate = HR_FREQ_2_KHZ;
#else
	g_eeg.regs.config1.date_rate = LP_FREQ_2_KHZ;
#endif
#elif (CONFIG_ADS129X_TYPE == 1299)
	g_eeg.regs.config1.date_rate = FREQ_2_KHZ;
#endif
	g_eeg.frame_total_len = DATA_FRAME_TOTAL_LEN_HIGH_FRE_MODE;

#if (SUPPORT_BP_FILTER)
	EEG_INFO("bandpass filter node1:");
	EEG_INFO("a0: %.16f a1: %.16f a2: %.16f", BP_A0_2KHZ_N1, BP_A1_2KHZ_N1, BP_A2_2KHZ_N1);
	EEG_INFO("b0: %.16f b1: %.16f b2: %.16f", BP_B0_2KHZ_N1, BP_B1_2KHZ_N1, BP_B2_2KHZ_N1);

	EEG_INFO("bandpass filter node2:");
	EEG_INFO("a0: %.16f a1: %.16f a2: %.16f", BP_A0_2KHZ_N2, BP_A1_2KHZ_N2, BP_A2_2KHZ_N2);
	EEG_INFO("b0: %.16f b1: %.16f b2: %.16f", BP_B0_2KHZ_N2, BP_B1_2KHZ_N2, BP_B2_2KHZ_N2);
#endif

#if SUPPORT_NOTCH_FILTER
	g_eeg.n_a[0] = N_A0_2KHZ;
	g_eeg.n_a[1] = N_A1_2KHZ;
	g_eeg.n_a[2] = N_A2_2KHZ;
	g_eeg.n_b[0] = N_B0_2KHZ;
	g_eeg.n_b[1] = N_B1_2KHZ;
	g_eeg.n_b[2] = N_B2_2KHZ;
#endif
}

/*************************************************
 * @description			:	中同步4kHz模式的参数到
 * 							全局变量中
 * @param - 			:	无
 * @return 				:	无
**************************************************/
static void eeg_sync_params_4kHz(void)
{
	g_eeg.patch_size = PATCH_SIZE_HIGH_FRE_MODE;
#if (CONFIG_ADS129X_TYPE == 1294)
#if (ADS1294_POWER_MODE == HIGH_RATE_MODE)
	g_eeg.regs.config1.date_rate = HR_FREQ_4_KHZ;
#else
	g_eeg.regs.config1.date_rate = LP_FREQ_4_KHZ;
#endif
#elif (CONFIG_ADS129X_TYPE == 1299)
	g_eeg.regs.config1.date_rate = FREQ_4_KHZ;
#endif
	g_eeg.frame_total_len = DATA_FRAME_TOTAL_LEN_HIGH_FRE_MODE;

#if (SUPPORT_BP_FILTER)
	EEG_INFO("bandpass filter node1:");
	EEG_INFO("a0: %.16f a1: %.16f a2: %.16f", BP_A0_4KHZ_N1, BP_A1_4KHZ_N1, BP_A2_4KHZ_N1);
	EEG_INFO("b0: %.16f b1: %.16f b2: %.16f", BP_B0_4KHZ_N1, BP_B1_4KHZ_N1, BP_B2_4KHZ_N1);

	EEG_INFO("bandpass filter node2:");
	EEG_INFO("a0: %.16f a1: %.16f a2: %.16f", BP_A0_4KHZ_N2, BP_A1_4KHZ_N2, BP_A2_4KHZ_N2);
	EEG_INFO("b0: %.16f b1: %.16f b2: %.16f", BP_B0_4KHZ_N2, BP_B1_4KHZ_N2, BP_B2_4KHZ_N2);
#endif

#if SUPPORT_NOTCH_FILTER
	g_eeg.n_a[0] = N_A0_4KHZ;
	g_eeg.n_a[1] = N_A1_4KHZ;
	g_eeg.n_a[2] = N_A2_4KHZ;
	g_eeg.n_b[0] = N_B0_4KHZ;
	g_eeg.n_b[1] = N_B1_4KHZ;
	g_eeg.n_b[2] = N_B2_4KHZ;
#endif
}

/*************************************************
 * @description			:	中同步8kHz模式的参数到
 * 							全局变量中
 * @param - 			:	无
 * @return 				:	无
**************************************************/
static void eeg_sync_params_8kHz(void)
{
	g_eeg.patch_size = PATCH_SIZE_HIGH_FRE_MODE;
#if (CONFIG_ADS129X_TYPE == 1294)
#if (ADS1294_POWER_MODE == HIGH_RATE_MODE)
	g_eeg.regs.config1.date_rate = HR_FREQ_8_KHZ;
#else
	g_eeg.regs.config1.date_rate = LP_FREQ_8_KHZ;
#endif
#elif (CONFIG_ADS129X_TYPE == 1299)
	g_eeg.regs.config1.date_rate = FREQ_8_KHZ;
#endif
	g_eeg.frame_total_len = DATA_FRAME_TOTAL_LEN_HIGH_FRE_MODE;

#if (SUPPORT_BP_FILTER)
	EEG_INFO("bandpass filter node1:");
	EEG_INFO("a0: %.16f a1: %.16f a2: %.16f", BP_A0_8KHZ_N1, BP_A1_8KHZ_N1, BP_A2_8KHZ_N1);
	EEG_INFO("b0: %.16f b1: %.16f b2: %.16f", BP_B0_8KHZ_N1, BP_B1_8KHZ_N1, BP_B2_8KHZ_N1);

	EEG_INFO("bandpass filter node2:");
	EEG_INFO("a0: %.16f a1: %.16f a2: %.16f", BP_A0_8KHZ_N2, BP_A1_8KHZ_N2, BP_A2_8KHZ_N2);
	EEG_INFO("b0: %.16f b1: %.16f b2: %.16f", BP_B0_8KHZ_N2, BP_B1_8KHZ_N2, BP_B2_8KHZ_N2);
#endif

#if SUPPORT_NOTCH_FILTER
	g_eeg.n_a[0] = N_A0_8KHZ;
	g_eeg.n_a[1] = N_A1_8KHZ;
	g_eeg.n_a[2] = N_A2_8KHZ;
	g_eeg.n_b[0] = N_B0_8KHZ;
	g_eeg.n_b[1] = N_B1_8KHZ;
	g_eeg.n_b[2] = N_B2_8KHZ;
#endif
}

/*************************************************
 * @description			:	上报信息初始化
 * @param 				:	无
 * @return 				:	无
**************************************************/
static void eeg_info_init(void)
{
	uint8_t i = 0;
	uint8_t mac[MAC_ADDR_LEN] = {0};
	uint8_t *msg = NULL;
	EEG_INFO_PKG *info_pkg = NULL;
	WIFI_NETWORK_PARAM *net_param = NULL;
	HARDWARE_VERSION *hw = NULL;
	SOFTWARE_VERSION *sw = NULL;

	msg = g_eeg.info_buf;

	/* 首部 */
	*((uint16_t *)(msg + INFO_OFT_HEAD)) = htons(EEG_INFO_MSG_HEAD);

	/* 序列号 */
	msg[INFO_OFT_SERIAL] = g_eeg.serial >> 16;
	msg[INFO_OFT_SERIAL + 1] = g_eeg.serial >> 8;
	msg[INFO_OFT_SERIAL + 2] = g_eeg.serial;

	/* 通道号，固定为0xFF */
	msg[INFO_OFT_CHN] = EEG_INFO_MSG_CHN_NUM;
	
	/* 数据长度 */
	*((uint16_t *)(msg + INFO_OFT_LEN)) = htons(sizeof(EEG_INFO_PKG));

	info_pkg = (EEG_INFO_PKG *)&msg[INFO_OFT_DATA];
	/* 系统信息部分 */
	info_pkg->device_id = 0;		/* 暂时固定为0，后续需要从flash读出 */
	net_param = wifi_get_network_params();
	info_pkg->ip = htonl(net_param->ip);
	if (ESP_OK != esp_read_mac(mac, ESP_MAC_WIFI_STA))
	{
		EEG_ERR("read sta mac failed");
	}
	memcpy(info_pkg->mac, mac, MAC_ADDR_LEN);

	/* 硬件版本 */
	hw = get_hardware_version();
	if (NULL != hw)
	{
		memcpy(&info_pkg->hw, hw, sizeof(HARDWARE_VERSION));
	}
	else
	{
		EEG_ERR("hw null");
	}

	/* 软件版本 */
	info_pkg->mcu_num = htonl(MCU_NUM);
	for (i = 0; i < MCU_NUM; i++)
	{
		sw = get_software_version(i);
		if (NULL != sw)
		{
			memcpy(&info_pkg->sw[i], sw, sizeof(SOFTWARE_VERSION));
		}
		else
		{
			EEG_ERR("sw null cpu: %u", i);
		}
	}

	/* 电池电量 */
	info_pkg->battery_ratio = htonl(battery_get_ratio());
}

/*************************************************
 * @description			:	信息定时上报函数
 * @param 				:	无
 * @return 				:	无
**************************************************/
static void eeg_info_report(void)
{
	uint8_t *msg = NULL;
	EEG_INFO_PKG *info_pkg = NULL;
	socklen_t adr_size = sizeof(struct sockaddr_in);
	int64_t time_stamp = 0;
	SOFTWARE_VERSION *sw = NULL;

	msg = g_eeg.info_buf;
	info_pkg = (EEG_INFO_PKG *)&msg[INFO_OFT_DATA];

	/* 更新电池电量 */
	info_pkg->battery_ratio = htonl(battery_get_ratio());

	/* 实时更新cpu1的版本 */
	sw = get_software_version(MCU_STM32);
	if (NULL != sw)
	{
		memcpy(&info_pkg->sw[MCU_STM32], sw, sizeof(SOFTWARE_VERSION));
	}

	time_stamp = esp_timer_get_time();
	msg[INFO_OFT_IDX] = 	(time_stamp&0xff00000000000000)>>56;
	msg[INFO_OFT_IDX + 1] = (time_stamp&0x00ff000000000000)>>48;
	msg[INFO_OFT_IDX + 2] = (time_stamp&0x0000ff0000000000)>>40;
	msg[INFO_OFT_IDX + 3] = (time_stamp&0x000000ff00000000)>>32;
	msg[INFO_OFT_IDX + 4] = (time_stamp&0x00000000ff000000)>>24;
	msg[INFO_OFT_IDX + 5] = (time_stamp&0x0000000000ff0000)>>16;
	msg[INFO_OFT_IDX + 6] = (time_stamp&0x000000000000ff00)>>8;
	msg[INFO_OFT_IDX + 7] = time_stamp&0x00000000000000ff;

	/* crc */
	msg[INFO_OFT_CRC] = crc_8bit_mask(msg, INFO_OFT_CRC, 0xd5);

	/* 发送 */
	sendto(g_eeg.admin_sock, msg, INFO_TOTAL_LEN, 0, (struct sockaddr*)&g_eeg.from, adr_size);
}

/*************************************************
 * @description			:	ads129x复位
 * @param -				:	无
 * @param - 			:	无
 * @return 				:	无
**************************************************/
static void ads129x_reset_handler(void)
{
	int i = 0;
	uint8_t r_regs[ADS129X_REG_NUM] = {0};
	uint8_t w_regs[ADS129X_REG_NUM] = {0};
	char *reg_name[ADS129X_REG_NUM] = {
		"ID", "config1", "config2", "config3", "loff",
		"chn1set", "chn2set", "chn3set", "chn4set",
		"chn5set", "chn6set", "chn7set", "chn8set",
#if (CONFIG_ADS129X_TYPE == 1294)
		"rld_sensp", "rld_sensn",
#elif (CONFIG_ADS129X_TYPE == 1299)
		"bias_sensp", "bias_sensn",
#endif
		"loff_sensp", "loff_sensn", "loff_flip", "loff_statp", "loff_statn",
		"gpio",
#if (CONFIG_ADS129X_TYPE == 1294)
		"pace", "resp",
#elif (CONFIG_ADS129X_TYPE == 1299)
		"misc1", "misc2", "config4"
#endif
#if (CONFIG_ADS129X_TYPE == 1294)
		"config4",
		"wct1", "wct2"
#endif
	};

	EEG_INFO("reset ads");

	gpio_intr_disable(ADS129X_DRDY_GPIO);
	ads129x_send_cmd(ADS129X_SDATAC);
	vTaskDelay(pdMS_TO_TICKS(100));

	/* 复位 */
	ads129x_send_cmd(ADS129X_RESET);
	vTaskDelay(pdMS_TO_TICKS(100));

	ads129x_send_cmd(ADS129X_SDATAC);
	vTaskDelay(pdMS_TO_TICKS(100));

	/* 设置正常采集模式 */
	memset(w_regs, 0, ADS129X_REG_NUM);
	ads129x_read_regs_from_context(w_regs);
	ads129x_write_all_regs(w_regs);
	vTaskDelay(100);

	/* 检查设置的寄存器 */
	memset(r_regs, 0, ADS129X_REG_NUM);
	ads129x_read_all_regs(r_regs);
	for (i = 0; i < ADS129X_REG_NUM; i++)
	{
		if (r_regs[i] == w_regs[i])
		{
			EEG_DBG("%s: 0x%02x", reg_name[i], r_regs[i]);
		}
		else
		{
			EEG_ERR("%s expected: 0x%02x actual: 0x%02x", reg_name[i], w_regs[i], r_regs[i]);
		}
	}
	/* 启动连续模式 */
	ads129x_send_cmd(ADS129X_RDATAC);
	vTaskDelay(10);

	ads129x_send_cmd(ADS129X_START);
	vTaskDelay(10);
	gpio_intr_enable(ADS129X_DRDY_GPIO);
}

/*************************************************
 * @description			:	stm32发来的ads相关控制命令
 * @param - argc		:	参数长度
 * @param - argv		:	参数地址
 * @return 				:	无
**************************************************/
void ads129x_cmd_data_process(uint8_t argc, uint8_t *argv)
{
	uint8_t cmd = 0;

	if (NULL == argv || 0 == argc)
	{
		EEG_ERR("illegel params");
		return;
	}

	cmd = argv[0];
	switch (cmd)
	{
		case ADS129X_RESET:
			ads129x_reset_handler();
			break;
	}
}
