/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: ads129x.h
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V1.0
描述	   	: ads129x驱动头文件
其他	   	: 无
日志	   	: 初版V1.0 2025/1/16 刘有为创建
***********************************************************************/
#ifndef __ADS129X_H__
#define __ADS129X_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#define CONFIG_ADS129X_TYPE 1299

/* 寄存器默认值 */
#if (CONFIG_ADS129X_TYPE == 1294)
#define ADS129X_REG_ID_DEFAULT 				0x90
#elif (CONFIG_ADS129X_TYPE == 1299)
#define ADS129X_REG_ID_DEFAULT 				0x3E
#endif

#if (CONFIG_ADS129X_TYPE == 1294)
#define ADS129X_REG_CONFIG1_DEFAULT 		0x06
#elif (CONFIG_ADS129X_TYPE == 1299)
#define ADS129X_REG_CONFIG1_DEFAULT 		0x96
#endif

#if (CONFIG_ADS129X_TYPE == 1294)
#define ADS129X_REG_CONFIG2_DEFAULT 		0x40
#elif (CONFIG_ADS129X_TYPE == 1299)
#define ADS129X_REG_CONFIG2_DEFAULT 		0xC0
#endif

#if (CONFIG_ADS129X_TYPE == 1294)
#define ADS129X_REG_CONFIG3_DEFAULT 		0x40
#elif (CONFIG_ADS129X_TYPE == 1299)
#define ADS129X_REG_CONFIG3_DEFAULT 		0x60
#endif

#define ADS129X_REG_LOFF_DEFAULT 			0x00
#if (CONFIG_ADS129X_TYPE == 1294)
#define ADS129X_REG_CHN1_DEFAULT 			0x00
#define ADS129X_REG_CHN2_DEFAULT 			0x00
#define ADS129X_REG_CHN3_DEFAULT 			0x00
#define ADS129X_REG_CHN4_DEFAULT 			0x00
#elif (CONFIG_ADS129X_TYPE == 1299)
#define ADS129X_REG_CHN1_DEFAULT 			0x61
#define ADS129X_REG_CHN2_DEFAULT 			0x61
#define ADS129X_REG_CHN3_DEFAULT 			0x61
#define ADS129X_REG_CHN4_DEFAULT 			0x61
#define ADS129X_REG_CHN5_DEFAULT 			0x61
#define ADS129X_REG_CHN6_DEFAULT 			0x61
#define ADS129X_REG_CHN7_DEFAULT 			0x61
#define ADS129X_REG_CHN8_DEFAULT 			0x61
#endif

#if (CONFIG_ADS129X_TYPE == 1294)
#define ADS1294_REG_RLD_SENSP_DEFAULT 		0x00
#define ADS1294_REG_RLD_SENSN_DEFAULT 		0x00
#elif (CONFIG_ADS129X_TYPE == 1299)
#define ADS1294_REG_BIAS_SENSP_DEFAULT 		0x00
#define ADS1294_REG_BIAS_SENSN_DEFAULT 		0x00
#endif

#define ADS129X_REG_LOFF_SENSP_DEFAULT 		0x00
#define ADS129X_REG_LOFF_SENSN_DEFAULT 		0x00
#define ADS129X_REG_LOFF_FLIP_DEFAULT 		0x00
#define ADS129X_REG_LOFF_STATP_DEFAULT 		0x00
#define ADS129X_REG_LOFF_STATN_DEFAULT 		0x00
#define ADS129X_REG_GPIO_DEFAULT 			0x0F

#if (CONFIG_ADS129X_TYPE == 1294)
#define ADS1294_REG_PACE_DEFAULT 			0x00
#define ADS1294_REG_RESP_DEFAULT 			0x00
#elif (CONFIG_ADS129X_TYPE == 1299)
#define ADS1294_REG_MISC1_DEFAULT 			0x00
#define ADS1294_REG_MISC2_DEFAULT 			0x00
#endif

#define ADS129X_REG_CONFIG4_DEFAULT 		0x00

#if (CONFIG_ADS129X_TYPE == 1294)
#define ADS1294_REG_WCT1_DEFAULT 			0x00
#define ADS1294_REG_WCT2_DEFAULT 			0x00
#endif

/* CONFIG1 */
#if (CONFIG_ADS129X_TYPE == 1294)
#define LOW_PWR_MODE 0				/* 低功耗模式（LP） */
#define HIGH_RATE_MODE 1			/* 高分辨率模式(HR) */

/* 高功率模式的采样率 */
typedef enum {
	HR_FREQ_32_KHZ = 0,
	HR_FREQ_16_KHZ = 1,
	HR_FREQ_8_KHZ = 2,
	HR_FREQ_4_KHZ = 3,
	HR_FREQ_2_KHZ = 4,
	HR_FREQ_1_KHZ = 5,
	HR_FREQ_500_HZ = 6,
	HR_FREQ_NOT_USE = 7,
} ADS1294_FREQ_HR;

/* 低功耗模式的采样率 */
typedef enum {
	LP_FREQ_16_KHZ = 0,
	LP_FREQ_8_KHZ = 1,
	LP_FREQ_4_KHZ = 2,
	LP_FREQ_2_KHZ = 3,
	LP_FREQ_1_KHZ = 4,
	LP_FREQ_500_HZ = 5,
	LP_FREQ_250_HZ = 6,
	LP_FREQ_NOT_USE = 7,
} ADS1294_FREQ_LP;
#elif (CONFIG_ADS129X_TYPE == 1299)
typedef enum {
	FREQ_16_KHZ = 0,
	FREQ_8_KHZ = 1,
	FREQ_4_KHZ = 2,
	FREQ_2_KHZ = 3,
	FREQ_1_KHZ = 4,
	FREQ_500_HZ = 5,
	FREQ_250_HZ = 6,
	FREQ_NOT_USE = 7,
} ADS1299_FREQ_T;
#endif

typedef enum {
	DAISY_CHAIN_MODE = 0,
	MULTI_READ_MODE = 1,
} ADS129X_DAISY_MODE_T;

typedef enum {
	OSC_EN_OFF = 0,
	OSC_EN_ON = 1,
} CLOCK_EN_T;
/* end of CONFIG1 */

/* CONFIG2 */
#if (CONFIG_ADS129X_TYPE == 1294)
/* WCT 斩波方案 */
typedef enum {
	WCT_FREQ_CHANGE = 0,
	WCT_FREQ_INVARIANT = 1,
} ADS1294_WCT_T;
#endif

/* 测试信号来源 */
typedef enum {
	DRIVE_EXTERNAL = 0,
	GENERATE_INTERNAL = 1,
} ADS129X_TEST_SRC_T;

/* 测试信号放大倍数 */
typedef enum {
	TEST_SIGNAL_PGA1 = 0,
	TEST_SIGNAL_PGA2 = 1,
} ADS129X_SIGNAL_PGA_T;

/* 测试信号频率 */
typedef enum {
	TEST_SIGNAL_FREQ1 = 0, 			/* Fclk/2^21 */
	TEST_SIGNAL_FREQ2, 				/* Fclk/2^20 */
	TEST_SIGNAL_FREQ_NOT_USE,
	TEST_SIGNAL_FREQ_AT_DC,			/* 直流 */
} ADS129X_SIGNAL_FREQ_T;
/* end of CONFIG2 */

/* CONFIG3 */
typedef enum {
	INTERNAL_BUF_OFF = 0,
	INTERNAL_BUF_ON = 1,
} ADS129X_INTER_BUF_EN_T;
#if (CONFIG_ADS129X_TYPE == 1294)
/* 基准电压 */
typedef enum {
	ADS1294_VERF_2P4V = 0,
	ADS1294_VERF_5V = 1,
} ADS1294_VREF_T;

/* RLD 测量 */
typedef enum {
	RLD_MEAS_OPEN = 0,
	RLD_MEAS_MUX = 1,
} ADS1294_RLD_MEAS_T;

/* RLDREF 信号源 */
typedef enum {
	RLD_REF_EXTERNAL = 0,
	RLD_REF_INTERNAL = 1,
} ADS1294_RLD_REF_SRC_T;

/* RLD 缓冲器电源 */
typedef enum {
	RLD_PWR_OFF = 0,
	RLD_PWR_ON = 1,
} ADS1294_RLD_PWR_T;

/* RLD 感应功能 */
typedef enum {
	RLD_LOFF_SENS_OFF = 0,
	RLD_LOFF_SENS_ON = 1,
} ADS1294_RLD_LOFF_SENS_T;
#elif (CONFIG_ADS129X_TYPE == 1299)
/* 启用内部缓冲区 */
typedef enum {
	BIAS_REFBUF_OFF = 0,
	BIAS_REFBUF_ON = 1,
} ADS1299_BIAS_REFBUF_EN_T;

/* BIAS 测量 */
typedef enum {
	BIAS_MEAS_OPEN = 0,
	BIAS_MEAS_MUX = 1,
} ADS1299_BIAS_MEAS_T;

/* BIASREF 信号源 */
typedef enum {
	BIAS_REF_EXTERNAL = 0,
	BIAS_REF_INTERNAL = 1,
} ADS1299_BIAS_REF_SRC_T;

/* BIAS 缓冲器电源 */
typedef enum {
	BIAS_PWR_OFF = 0,
	BIAS_PWR_ON = 1,
} ADS1299_BIAS_PWR_T;

/* BIAS 感应功能 */
typedef enum {
	BIAS_LOFF_SENS_OFF = 0,
	BIAS_LOFF_SENS_ON = 1,
} ADS1299_BIAS_LOFF_SENS_T;
#endif
/* end of CONFIG3 */

/* LOFF */
/* 导联脱落比较器阈值 */
typedef enum {
	POS_95_NEG_5 = 0,
	POS_92P5_NEG_7P5 = 1,
	POS_90_NEG_10 = 2,
	POS_87P5_NEG_12P5 = 3,
	POS_85_NEG_15 = 4,
	POS_80_NEG_20 = 5,
	POS_75_NEG_25 = 6,
	POS_70_NEG_30 = 7,
} ADS129X_COMP_TH_T;
#if (CONFIG_ADS129X_TYPE == 1294)
/* 导联脱落检测模式 */
typedef enum {
	CURRENT_MODE = 0,
	RESISTANCE_MODE = 1,
} LOFF_DETECT_MODE_T;
#endif

/* 导联脱落电流幅度 */
typedef enum {
#if (CONFIG_ADS129X_TYPE == 1294)
	LOFF_CURR_6NA = 0,
	LOFF_CURR_12NA = 1,
	LOFF_CURR_18NA = 2,
	LOFF_CURR_24NA = 3,
#elif (CONFIG_ADS129X_TYPE == 1299)
	LOFF_CURR_6NA = 0,
	LOFF_CURR_24NA = 1,
	LOFF_CURR_6UA = 2,
	LOFF_CURR_24UA = 3,
#endif
} ADS129X_LOFF_CUR_AMP_T;

typedef enum {
	AC_DETECT_OPEN = 0,
	AC_DETECT_DR_QUARTER = 1,
	AC_DETECT_NOT_USE = 2,
	DC_DETECT_OPEN = 3,
} ADS129X_LOFF_FREQ_T;
/* end of LOFF */

/* CHnSET */
/* 通道检测开关 */
typedef enum {
	CHN_PD_ON = 0,
	CHN_PD_OFF = 1,
} ADS129X_CHN_PWR_T;

/* 通道PGA放大系数 */
typedef enum {
#if (CONFIG_ADS129X_TYPE == 1294)
	PGA_GAIN_6 = 0,
	PGA_GAIN_1 = 1,
	PGA_GAIN_2 = 2,
	PGA_GAIN_3 = 3,
	PGA_GAIN_4 = 4,
	PGA_GAIN_8 = 5,
	PGA_GAIN_12 = 6,
#elif (CONFIG_ADS129X_TYPE == 1299)
	PGA_GAIN_1 = 0,
	PGA_GAIN_2 = 1,
	PGA_GAIN_4 = 2,
	PGA_GAIN_6 = 3,
	PGA_GAIN_8 = 4,
	PGA_GAIN_12 = 5,
	PGA_GAIN_24 = 6,
	PGA_GAIN_NOT_USE = 7,
#endif
} ADS129X_PGA_GAIN_T;

#if (CONFIG_ADS129X_TYPE == 1299)
typedef enum {
	SRB2_CONNECT_OPEN = 0,
	SRB2_CONNECT_CLOSE = 1,
} ADS1299_SRB2_CONNECT_T;
#endif

typedef enum {
	CHN_NORMAL_INPUT = 0,
	CHN_SHORT_INPUT = 1,
#if (CONFIG_ADS129X_TYPE == 1294)
	CHN_RLD_MEAS = 2,
#elif (CONFIG_ADS129X_TYPE == 1299)
	CHN_BIAS_MEAS = 2,
#endif
	CHN_MVDD_INPUT = 3,
	CHN_TEMPERATURE_SENS = 4,
	CHN_TEST_SIGNAL = 5,
#if (CONFIG_ADS129X_TYPE == 1294)
	CHN_RLD_DRP = 6,
	CHN_RLD_DRN = 7,
#elif (CONFIG_ADS129X_TYPE == 1299)
	CHN_BIAS_DRP = 6,
	CHN_BIAS_DRN = 7,
#endif
} ADS129X_CHN_MUX_T;
/* end of CHnSET */

#if (CONFIG_ADS129X_TYPE == 1294)
/* RLD_SENS */
typedef enum {
	RLD_SENS_DISABLED = 0,
	RLD_SENS_ENABLED = 1,
} ADS1294_RLD_SENS_EN_T;
#elif (CONFIG_ADS129X_TYPE == 1299)
/* BIAS_SENS */
typedef enum {
	BIAS_SENS_DISABLED = 0,
	BIAS_SENS_ENABLED = 1,
} ADS1299_BIAS_SENS_EN_T;
#endif

/* LOFF_SENS */
typedef enum {
	LOFF_SENS_DISABLED = 0,
	LOFF_SENS_ENABLED
} ADS129X_LOFF_SENS_EN_T;

/* LOFF_FLIP */
typedef enum {
	LOFF_NOT_FLIP = 0,
	LOFF_FLIPPED = 1,
} ADS129X_LOFF_FLIP_EN_T;

/* GPIO */
typedef enum {
	OUTPUT_MODE = 0,
	INTPUT_MODE = 1,
} ADS129X_GPIO_MODE_T;

#if (CONFIG_ADS129X_TYPE == 1294)
/* PACE */
typedef enum {
	PACE_CHN2 = 0,
	PACE_CHN4 = 1,
} ADS1294_PACE_CHN_EVEN_T;	/* 偶数通道 */
typedef enum {
	PACE_CHN1 = 0,
	PACE_CHN3 = 1,
} ADS1294_PACE_CHN_ODD_T;	/* 奇数通道 */

typedef enum {
	PD_PACE_OFF = 0,
	PD_PACE_ON = 1,
} ADS1294_PD_PACE_T;

/* RESP */
typedef enum {
	RESP_DEMOD_OFF = 0,
	RESP_DEMOD_ON = 1,
} ADS1294_RESP_DEMOD_EN_T;

typedef enum {
	RESP_MOD_OFF = 0,
	RESP_MOD_ON = 1,
} ADS1294_RESP_MOD_EN_T;

typedef enum {
	PHASE_22P5 = 0,
	PHASE_45 = 1,
	PHASE_67P5 = 2,
	PHASE_90 = 3,
	PHASE_112P5 = 4,
	PHASE_135 = 5,
	PHASE_157P5 = 6,
	PHASE_NOT_USE = 7,
} ADS1294_RESP_PHASE_T;

typedef enum {
	RESP_CTRL_NOT_USE = 0,
	RESP_CTRL_EXTERNAL = 1,
	RESP_CTRL_INTERNAL = 2,
	RESP_CTRL_USER = 3,
} ADS1294_RESP_CTRL_T;
#elif (CONFIG_ADS129X_TYPE == 1299)
typedef enum {
	SRB1_SWITCH_OPEN = 0,
	SRB1_SWITCH_CLOSE = 1,
} ADS1299_SRB1_SWITCH_T;
#endif

/* CONFIG4 */
typedef enum {
	LOFF_COMP_DISABLE = 0,
	LOFF_COMP_ENABLE = 1,
} ADS129X_LOFF_COMP_T;

typedef enum {
	CONTINUOUS_MODE = 0,
	SINGLE_SHOT_MODE = 1,
} DAS129X_CONVERSION_MODE_T;

#if (CONFIG_ADS129X_TYPE == 1294)
typedef enum {
	RESP_FREQ_64KHZ = 0,
	RESP_FREQ_32KHZ = 1,
	RESP_FREQ_16KHZ = 2,
	RESP_FREQ_8KHZ = 3,
	RESP_FREQ_4KHZ = 4,
	RESP_FREQ_2KHZ = 5,
	RESP_FREQ_1KHZ = 6,
	RESP_FREQ_500HZ = 7,
} ADS1294_RESP_FREQ_T;

typedef enum {
	WCT_TO_RLD_OFF = 0,
	WCT_TO_RLD_ON = 1,
} ADS1294_WCT_RLD_CONNECT_T;
#endif

/* WCT1 */
#if (CONFIG_ADS129X_TYPE == 1294)
typedef enum {
	AVR_CHN_OFF = 0,
	AVR_CHN_ON = 1,
} ADS1294_AVR_CHN_EN_T;

typedef enum {
	PD_WCTA_OFF = 0,
	PD_WCTA_ON = 1,
} ADS1294_PD_WCTA_EN_T;

/* WCT2 */
typedef enum {
	PD_WCTB_OFF = 0,
	PD_WCTB_ON = 1,
} ADS1294_PD_WCTB_EN_T;

typedef enum {
	PD_WCTC_OFF = 0,
	PD_WCTC_ON = 1,
} ADS1294_PD_WCTC_EN_T;

typedef enum {
	WCTA_CHN1P = 0,
	WCTA_CHN1N = 1,
	WCTA_CHN2P = 2,
	WCTA_CHN2N = 3,
	WCTA_CHN3P = 4,
	WCTA_CHN3N = 5,
	WCTA_CHN4P = 6,
	WCTA_CHN4N = 7,
} ADS1294_WCTA_T;

typedef enum {
	WCTB_CHN1P = 0,
	WCTB_CHN1N = 1,
	WCTB_CHN2P = 2,
	WCTB_CHN2N = 3,
	WCTB_CHN3P = 4,
	WCTB_CHN3N = 5,
	WCTB_CHN4P = 6,
	WCTB_CHN4N = 7,
} ADS1294_WCTB_T;

typedef enum {
	WCTC_CHN1P = 0,
	WCTC_CHN1N = 1,
	WCTC_CHN2P = 2,
	WCTC_CHN2N = 3,
	WCTC_CHN3P = 4,
	WCTC_CHN3N = 5,
	WCTC_CHN4P = 6,
	WCTC_CHN4N = 7,
} ADS1294_WCTC_T;
#endif

/* 系统命令 */
#define ADS129X_WAKEUP		0X02		//从待机模式唤醒
#define ADS129X_STANDBY		0X04		//进入待机模式
#define ADS129X_RESET		0X06		//复位
#define ADS129X_START		0X08		//启动或转换
#define ADS129X_STOP		0X0A		//停止转换
#define ADS129X_CALIBRATE	0X1A		//通道偏移校准

/* 数据读取命令 */
#define ADS129X_RDATAC		0X10	//启用连续的数据读取模式,默认使用此模式
#define ADS129X_SDATAC		0X11	//停止连续的数据读取模式
#define ADS129X_RDATA		0X12	//通过命令读取数据;支持多种读回

/* 寄存器读取命令
 * r rrrr=要读写的寄存器首地址
 * n nnnn=要读写的寄存器数量
 */
#define ADS129X_RREG		0X20	//读取  001r rrrr(首字节) 000n nnnn(2字节) 
#define ADS129X_WREG		0X40	//写入  010r rrrr(首字节) 000n nnnn(2字节)

/* ADS129X内部寄存器地址定义 */
typedef enum {
	ADS129X_REG_ID = 0,					/* ID控制寄存器(只读) */
	ADS129X_REG_CONFIG1,				/* 配置寄存器1 */
	ADS129X_REG_CONFIG2,				/* 配置寄存器2 */
	ADS129X_REG_CONFIG3,				/* 配置寄存器3 */
	ADS129X_REG_LOFF,					/* 导联脱落控制寄存器 */
	ADS129X_REG_CHN1,					/* 通道1设置寄存器 */
	ADS129X_REG_CHN2,					/* 通道2设置寄存器 */
	ADS129X_REG_CHN3,					/* 通道3设置寄存器 */
	ADS129X_REG_CHN4,					/* 通道4设置寄存器 */
	ADS129X_REG_CHN5,					/* 通道5设置寄存器 */
	ADS129X_REG_CHN6,					/* 通道6设置寄存器 */
	ADS129X_REG_CHN7,					/* 通道7设置寄存器 */
	ADS129X_REG_CHN8,					/* 通道8设置寄存器 */
#if (CONFIG_ADS129X_TYPE == 1294)
	ADS1294_REG_RLD_SENSP,				/* 右腿驱动选择寄存器(正相输入) */
	ADS1294_REG_RLD_SENSN,				/* 右腿驱动选择寄存器(反相输入) */
#elif (CONFIG_ADS129X_TYPE == 1299)
	ADS1299_REG_BIAS_SENSP,
	ADS1299_REG_BIAS_SENSN,
#endif
	ADS129X_REG_LOFF_SENSP,
	ADS129X_REG_LOFF_SENSN,
	ADS129X_REG_LOFF_FLIP,
	ADS129X_REG_LOFF_STATP,
	ADS129X_REG_LOFF_STATN,
	ADS129X_REG_GPIO,
#if (CONFIG_ADS129X_TYPE == 1294)
	ADS1294_REG_PACE,
	ADS1294_REG_RESP,
#elif (CONFIG_ADS129X_TYPE == 1299)
	ADS1299_REG_MISC1,
	ADS1299_REG_MISC2,
#endif
	ADS129X_REG_CONFIG4,
#if (CONFIG_ADS129X_TYPE == 1294)
	ADS1294_REG_WCT1,
	ADS1294_REG_WCT2,
#endif
	ADS129X_REG_NUM,
}ADS_REG_T;

/* 配置寄存器 1 */
typedef struct _ADS129X_CONFIG1
{
	uint8_t date_rate: 3;			/* 输出数据速率 */
	uint8_t reserve1: 2;
	uint8_t clk_en: 1;				/* CLK源 */
	uint8_t daisy_en: 1;			/* 菊花链或多读回模式 */
#if (CONFIG_ADS129X_TYPE == 1294)
	uint8_t hr: 1;					/* 高分辨率或低功耗模式 */
#elif (CONFIG_ADS129X_TYPE == 1299)
	uint8_t reserve2: 1;			/* 预留 */
#endif
}ADS129X_CONFIG1;

/* 配置寄存器 2 */
typedef struct _ADS129X_CONFIG2
{
	uint8_t test_freq: 2;			/* 测试信号频率 */
	uint8_t test_amp: 1;			/* 测试信号振幅 */
	uint8_t reserve1: 1;
	uint8_t int_test: 1;			/* 测试源 */
#if (CONFIG_ADS129X_TYPE == 1294)
	uint8_t wct_chop: 1;			/* WCT 斩波方案 */
	uint8_t reserve2: 2;
#elif (CONFIG_ADS129X_TYPE == 1299)
	uint8_t reserve2: 3;
#endif
}ADS129X_CONFIG2;

/* 配置寄存器 3 */
typedef struct _ADS129X_CONFIG3
{
#if (CONFIG_ADS129X_TYPE == 1294)
	uint8_t rld_stat: 1;			/* RLD 导联脱落状态 */
	uint8_t rld_loff_sens: 1;		/* RLD 感应功能 */
	uint8_t pd_rld: 1;				/* RLD 缓冲器电源 */
	uint8_t rldref_int: 1;			/* RLDREF 信号 */
	uint8_t rld_meas: 1;			/* RLD 测量 */
	uint8_t vref_4v: 1;				/* 基准电压 */
	uint8_t reserve: 1;
	uint8_t pd_refbuf: 1;			/* 关闭基准缓冲器 */
#elif (CONFIG_ADS129X_TYPE == 1299)
	uint8_t bias_stat: 1;			/* bias导联脱落状态 */
	uint8_t bias_loff_sens: 1;		/* bias 感应功能 */
	uint8_t pd_bias: 1;				/* bias 缓冲器电源 */
	uint8_t biasref_int: 1;			/* biasref 信号 */
	uint8_t bias_meas: 1;			/* bias 测量 */
	uint8_t reserve: 2;
	uint8_t pd_refbuf: 1;			/* 关闭基准缓冲器 */
#endif
}ADS129X_CONFIG3;

/* 导联脱落控制寄存器 */
typedef struct _ADS129X_LOFF
{
	uint8_t flead_Off: 2;			/* 导联脱落频率 */
	uint8_t ilead_Off: 2;			/* 导联脱落电流幅度 */
#if (CONFIG_ADS129X_TYPE == 1294)
	uint8_t vlead_off_en: 1;		/* 导联脱落检测模式 */
#elif (CONFIG_ADS129X_TYPE == 1299)
	uint8_t reserve: 1;
#endif
	uint8_t comp_th: 3;				/* 导联脱落比较器阈值 */
}ADS129X_LOFF;

/* 通道设置寄存器 */
typedef struct _ADS129X_CHSET
{
	uint8_t mux: 3;					/* 通道输入 */
#if (CONFIG_ADS129X_TYPE == 1294)
	uint8_t reserve: 1;
#elif (CONFIG_ADS129X_TYPE == 1299)
	uint8_t srb2: 1;				/* srbs连接 */
#endif
	uint8_t gain: 3;				/* PGA 增益 */
	uint8_t pd: 1;					/* 开关 */
}ADS129X_CHSET;

/* 信号导出寄存器 */
#if (CONFIG_ADS129X_TYPE == 1294)
typedef struct _ADS1294_RLD_SENS
{
	/* 信号导出开关 */
	uint8_t chn1: 1;
	uint8_t chn2: 1;
	uint8_t chn3: 1;
	uint8_t chn4: 1;
	uint8_t reserve: 4;				/* 寄存器位位 [7:6] [5:4]不适于ADS1294 */
}ADS1294_RLD_SENS;
#elif (CONFIG_ADS129X_TYPE == 1299)
typedef struct _ADS1299_BIAS_SENS
{
	/* 信号导出开关 */
	uint8_t chn1: 1;
	uint8_t chn2: 1;
	uint8_t chn3: 1;
	uint8_t chn4: 1;
	uint8_t chn5: 1;
	uint8_t chn6: 1;
	uint8_t chn7: 1;
	uint8_t chn8: 1;
}ADS1299_BIAS_SENS;
#endif

/* 导联脱落检测寄存器 */
typedef struct _ADS129X_LOFF_SENS
{
	/* 导联脱落检测开关 */
	uint8_t chn1: 1;
	uint8_t chn2: 1;
	uint8_t chn3: 1;
	uint8_t chn4: 1;
#if (CONFIG_ADS129X_TYPE == 1294)
	uint8_t reserve: 4;				/* 寄存器位位 [7:6] [5:4]不适于ADS1294 */
#elif (CONFIG_ADS129X_TYPE == 1299)
	uint8_t chn5: 1;
	uint8_t chn6: 1;
	uint8_t chn7: 1;
	uint8_t chn8: 1;
#endif
}ADS129X_LOFF_SENS;

/* 导联脱落翻转寄存器 */
typedef struct _ADS129X_LOFF_FLIP
{
	/* 导联脱落导出的电流的方向 */
	uint8_t chn1: 1;
	uint8_t chn2: 1;
	uint8_t chn3: 1;
	uint8_t chn4: 1;
#if (CONFIG_ADS129X_TYPE == 1294)
	uint8_t reserve: 4;				/* 寄存器位位 [7:6] [5:4]不适于ADS1294 */
#elif (CONFIG_ADS129X_TYPE == 1299)
	uint8_t chn5: 1;
	uint8_t chn6: 1;
	uint8_t chn7: 1;
	uint8_t chn8: 1;
#endif
}ADS129X_LOFF_FLIP;

/* 通用 I/O 寄存器 */
typedef struct _ADS129X_GPIO
{
	uint8_t	gpio_c: 4;				/* GPIO 控制 */
	uint8_t	gpio_d: 4;				/* GPIO 数据 */
}ADS129X_GPIO;

#if (CONFIG_ADS129X_TYPE == 1294)
/* 起搏信号检测寄存器 */
typedef struct _ADS1294_PACE
{
	uint8_t pd_pace: 1;				/* 起搏信号检测寄存器 */
	uint8_t paceo: 2;				/* 起搏信号奇数通道 */
	uint8_t pacee: 2;				/* 起搏信号偶数通道 */
	uint8_t reserve: 3;
}ADS1294_PACE;
#endif

#if (CONFIG_ADS129X_TYPE == 1299)
typedef struct _ADS1299_MISC1
{
	uint8_t reserve1: 5;
	uint8_t srb1: 1;
	uint8_t reserve2: 2;
}ADS1299_MISC1;
#endif

#if (CONFIG_ADS129X_TYPE == 1294)
/* 呼吸控制寄存器 */
typedef struct _ADS1294_RESP
{
	uint8_t resp_ctrl: 2;			/* 呼吸控制 */
	uint8_t resp_ph: 3;				/* 呼吸相位 */
	uint8_t reserve: 1;
	uint8_t resp_mod_en: 1;		/* 启用呼吸调制电路 */
	uint8_t resp_demod_en: 1;		/* 启用呼吸解调电路 */
}ADS1294_RESP;
#endif

#if (CONFIG_ADS129X_TYPE == 1299)
typedef struct _ADS1299_MISC2
{
	uint8_t reserve;
}ADS1299_MISC2;
#endif

/* 配置寄存器 4 */
typedef struct _ADS129X_CONFIG4
{
	uint8_t reserve1: 1;
	uint8_t pd_loff_comp: 1;		/* 导联脱落比较器断电 */
#if (CONFIG_ADS129X_TYPE == 1294)
	uint8_t wct_to_rld: 1;			/* 将 WCT 连接到 RLD */
#elif (CONFIG_ADS129X_TYPE == 1299)
	uint8_t reserve2: 1;
#endif
	uint8_t single_shot: 1;			/* 单冲转换 */
#if (CONFIG_ADS129X_TYPE == 1294)
	uint8_t reserve2: 1;
	uint8_t resp_freq: 3;			/* 呼吸调制频率 */
#elif (CONFIG_ADS129X_TYPE == 1299)
	uint8_t reserve3: 4;
#endif
} ADS129X_CONFIG4;

#if (CONFIG_ADS129X_TYPE == 1294)
/* 威尔逊中心端子和增强导联控制寄存器 */
typedef struct _ADS1294_WCT1
{
	uint8_t wcta: 3;				/* WCT 放大器 A 通道选择，通常连接到 RA 电极 */
	uint8_t pd_wcta: 1;				/* 使 WCTA 断电 */
	uint8_t avr_ch4: 1;				/* 使 (WCTB + WCTC)/2 成为通道 4 的负输入 */
	uint8_t avr_ch7: 1;				/* 使 (WCTB + WCTC)/2 成为通道 7 */
	uint8_t avr_ch5: 1;				/* 使 (WCTA + WCTC)/2 成为通道 5（ADS1296、ADS1296R、ADS1298 和 ADS1298R）的负输入 */
	uint8_t avr_ch6: 1;				/* 使 (WCTA + WCTB)/2 成为通道 6（ADS1296、ADS1296R、ADS1298 和 ADS1298R）的负输入 */
}ADS1294_WCT1;

/* 威尔逊中心端子控制寄存器 */
typedef struct _ADS1294_WCT2
{
	uint8_t wctc: 3;				/* WCT 放大器 C 通道选择，通常连接到 LL 电极 */
	uint8_t wctb: 3;				/* WCT 放大器 B 通道选择，通常连接到 LA 电极 */
	uint8_t pd_wctb: 1;				/* 使 WCTB 断电 */
	uint8_t pd_wctc: 1;				/* 使 WCTC 断电 */
}ADS1294_WCT2;
#endif

typedef enum {
	ADS129X_CHN1 = 0,
	ADS129X_CHN2 = 1,
	ADS129X_CHN3 = 2,
	ADS129X_CHN4 = 3,
#if (CONFIG_ADS129X_TYPE == 1299)
	ADS129X_CHN5 = 4,
	ADS129X_CHN6 = 5,
	ADS129X_CHN7 = 6,
	ADS129X_CHN8 = 7,
#endif
	ADS129X_CHN_NUM,
} ADS_CHN_T;

/* 
 * PGA放大系数12, VREF = 2400000000nv
 * scale = VREF/[(2^23 - 1)*PGA]
 */
#if (CONFIG_ADS129X_TYPE == 1294)
#define ADS129X_BASE_SCALE (23.841860752)
#elif (CONFIG_ADS129X_TYPE == 1299)
#define ADS129X_BASE_SCALE (89.579835645)
#endif

/* 数据缓冲区长度 */
#if (CONFIG_ADS129X_TYPE == 1294)
#define ADS129X_DATA_BUF_SIZE 15
#define ADS129X_DATA_BUF_SIZE_BIT 120
#elif (CONFIG_ADS129X_TYPE == 1299)
#define ADS129X_DATA_BUF_SIZE 27
#define ADS129X_DATA_BUF_SIZE_BIT 216
#endif

#ifdef __cplusplus
}
#endif

#endif /* __ADS129X_H__ */
