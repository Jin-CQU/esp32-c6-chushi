/**********************************************************************
Copyright ? LIZ MEDICAL TECHNOLOGY Co., Ltd. 2022. All rights reserved.
文件名		: wifi.h
作者	  	: 刘有为 <458386139@qq.com>
版本	   	: V1.0
描述	   	: 配网页面头文件
			  用于处理从html中得到的post事件（及得到数据的那个信号）
			  参考网站：https://blog.csdn.net/q_fy_p/article/details/127175477
			  handler的编写参考网站：https://blog.csdn.net/sinat_36568888/article/details/118355836
其他	   	: 无
日志	   	: 初版V1.0 2024/12/03 刘有为创建
***********************************************************************/
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "esp_http_server.h"
#include "web_server.h"

#define WEB_TAG "WEB"
#define WEB_INFO(fmt, ...) ESP_LOGI(WEB_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define WEB_DBG(fmt, ...) ESP_LOGW(WEB_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define WEB_ERR(fmt, ...) ESP_LOGE(WEB_TAG, "(%s:%d) "fmt, __func__, __LINE__, ##__VA_ARGS__)

#define MIN(x, y) ((x) < (y) ? (x) : (y))

//这里的代码引用html文件，在CMakeLists.list已经嵌入了index.html文件
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

/*-----------------这里是my_nvs的程序_start------------------*/
//这部分代码为新的nvs程序，参考来源：https://blog.csdn.net/weixin_59288228/article/details/129493804

void NVS_write_data_to_flash(char *WIFI_Name_write_in, char *WIFI_Password_write_in, char *ConfirmString)
{
	//本函数用于保存收集到的wifi信息进flash中
	//输入参数ConfirmString没啥作用，但是去掉的话会报错：NVS_write_data_to_flash定义的参数太多
	//输入参数confirmstring的作用为设置校检字符串
	nvs_handle my_nvs_handle;
	wifi_config_t nvs_wifi_store;       //定义一个wifi_config_t的变量用于储存wifi信息
	
	// 要写入的WIFI信息，这部分对应将函数中的name password分别写入到wifi_config_to_store中
	strcpy((char *)&nvs_wifi_store.sta.ssid,WIFI_Name_write_in);            //将ssid_write_in参数写入到需要保存的变量中
	strcpy((char *)&nvs_wifi_store.sta.password,WIFI_Password_write_in);    //将password_write_in参数写入需要保存的变量中
	ESP_ERROR_CHECK( nvs_open("wifi", NVS_READWRITE, &my_nvs_handle) );     //打开nvs，以可读可写的方式
	ESP_ERROR_CHECK( nvs_set_str( my_nvs_handle, "check", ConfirmString) ); //储存校检符号
	ESP_ERROR_CHECK( nvs_set_blob( my_nvs_handle, "wifi_config", &nvs_wifi_store, sizeof(nvs_wifi_store)));     //将类型为wifi_config_t的变量nvs_wifi_store储存于flash中 
	if(ESP_OK != nvs_commit(my_nvs_handle)){                              //commit数据
		WEB_ERR("nvs_commit error!");
	}
	nvs_close(my_nvs_handle);                                             //关闭nvs
}

unsigned char NVS_read_data_from_flash(char *WIFI_Name,char *WIFI_Password, char *ConfirmString)
{
	//该函数的功能用于读取nvs中的wifi信息
	//输入参数ConfirmString没啥作用，但是去掉的话会报错：NVS_write_data_to_flash定义的参数太多
	//输入参数confirmstring的作用为设置校检字符串
	nvs_handle my_nvs_handle;
	size_t str_length = 50;
	char str_data[50] = {0};
	wifi_config_t nvs_wifi_stored;       //定义一个用于保存wifi信息的wifi_config_t类型的变量
	memset(&nvs_wifi_stored, 0x0, sizeof(nvs_wifi_stored));       //为已经保持的用户信息
	size_t wifi_len = sizeof(nvs_wifi_stored);
	ESP_ERROR_CHECK(nvs_open("wifi", NVS_READWRITE, &my_nvs_handle));
	//ESP_ERROR_CHECK( nvs_get_str(my_nvs_handle, "check", str_data, &str_length) );       //校验字符串，用于与read中的confirmstring相对应的
	//ESP_ERROR_CHECK( nvs_get_blob(my_nvs_handle, "wifi_config", &nvs_wifi_stored, &wifi_len));       //用于保存读取到的wifi信息，key为wifi_config
	nvs_get_str(my_nvs_handle, "check", str_data, &str_length);
	nvs_get_blob(my_nvs_handle, "wifi_config", &nvs_wifi_stored, &wifi_len);
	//上面两条读取nvs内容的代码不能使用ESP_ERROR_CHECK来检测错误，不然若NVS为空的时候，会一直报错并且一直重启
	//以下打印信息为字节长度及已保持的wifi信息
	//printf("[data1]: %s len:%u\r\n", str_data, str_length);
	//printf("[data3]: ssid:%s passwd:%s\r\n", wifi_config_stored.sta.ssid, wifi_config_stored.sta.password);
	//
	strcpy(WIFI_Name,(char *)&nvs_wifi_stored.sta.ssid);
	strcpy(WIFI_Password,(char *)&nvs_wifi_stored.sta.password);
	nvs_close(my_nvs_handle);
	//下面这部分代码是判断是否正确读写flash中的数据
	if(strcmp(ConfirmString,str_data) == 0)
	{
		return 0x00;
	}
	else
	{
		return 0xFF;
	}
}



/*-----------------这里是my_nvs的程序_end----------------------*/

//20230519的demo中的nvs部分
//下面这部分的nvs程序测试结果来说使用不了
//实际上打开write函数不知道有没有效果
//在main函数中打开read函数，esp32会不断重启
//因此还需要另外的代码来实现nvs储存功能
/* void NVS_write_data_to_flash(char *WIFI_Name, char *WIFI_Password, char *ConfirmString)
{
	//本函数用于保存收集到的wifi信息进flash中
	nvs_handle my_nvs_handle;

	// 写入一个整形数据，一个字符串，WIFI信息以及版本信息
	static const char *NVS_CUSTOMER = "customer data";
	static const char *DATA2 = "String";
	static const char *DATA3 = "blob_wifi";
	
	// 要写入的WIFI信息，这部分对应将函数中的name password分别写入到wifi_config_to_store中
	wifi_config_t wifi_config_to_store;
	strcpy((char *)&wifi_config_to_store.sta.ssid,WIFI_Name);
	strcpy((char *)&wifi_config_to_store.sta.password,WIFI_Password);
	ESP_LOGE(TAG,"set size : %u!/r/n",sizeof(wifi_config_to_store));    //打印出保持的字节数
	ESP_ERROR_CHECK( nvs_open( NVS_CUSTOMER, NVS_READWRITE, &my_nvs_handle) );
	ESP_ERROR_CHECK( nvs_set_str( my_nvs_handle, DATA2, ConfirmString) );       //暂不清楚这部分的作用
	ESP_ERROR_CHECK( nvs_set_blob( my_nvs_handle, DATA3, &wifi_config_to_store, sizeof(wifi_config_to_store)));     //暂不清楚这部分的作用
	ESP_ERROR_CHECK( nvs_commit(my_nvs_handle) );
	nvs_close(my_nvs_handle);
}
*/
/* unsigned char NVS_read_data_from_flash(char *ConfirmString,char *WIFI_Name,char *WIFI_Password)
{
	nvs_handle my_nvs_handle;
	// 写入一个整形数据，一个字符串，WIFI信息以及版本信息
	static const char *NVS_CUSTOMER = "customer data";
	static const char *DATA2 = "String";
	static const char *DATA3 = "blob_wifi";
	uint32_t str_length = 50;
	char str_data[50] = {0};
	wifi_config_t wifi_config_stored;
	memset(&wifi_config_stored, 0x0, sizeof(wifi_config_stored));       //为已经保持的用户信息
	uint32_t wifi_len = sizeof(wifi_config_stored);
	ESP_ERROR_CHECK( nvs_open(NVS_CUSTOMER, NVS_READWRITE, &my_nvs_handle) );

	ESP_ERROR_CHECK ( nvs_get_str(my_nvs_handle, DATA2, str_data, &str_length) );
	ESP_ERROR_CHECK ( nvs_get_blob(my_nvs_handle, DATA3, &wifi_config_stored, &wifi_len) );
	//以下打印信息为字节长度及已保持的wifi信息
	//printf("[data1]: %s len:%u\r\n", str_data, str_length);
	//printf("[data3]: ssid:%s passwd:%s\r\n", wifi_config_stored.sta.ssid, wifi_config_stored.sta.password);
	//
	strcpy(WIFI_Name,(char *)&wifi_config_stored.sta.ssid);
	strcpy(WIFI_Password,(char *)&wifi_config_stored.sta.password);
	nvs_close(my_nvs_handle);
	//下面这部分代码是判断是否正确读写flash中的数据
	if(strcmp(ConfirmString,str_data) == 0)
	{
		return 0x00;
	}
	else
	{
		return 0xFF;
	}
}
*/

//http_SendText_html函数是https://blog.csdn.net/q_fy_p/article/details/127175477中编写的
//代码功能应该是用于发送html至sta设备
static esp_err_t http_SendText_html(httpd_req_t *req)
{
	/* Get handle to embedded file upload script */

	const size_t upload_script_size = (index_html_end - index_html_start);
	
	/* Add file upload form and script which on execution sends a POST request to /upload */
	//这部分代码参考来源：https://blog.csdn.net/q_fy_p/article/details/127175477
	//这部分是demo的源代码部分，但是跑不通，弹不出网页来，估计是httpd_resp_send_chunk函数的问题
	// const char TxBuffer[] = "<h1> SSID1 other WIFI</h1>";
	// httpd_resp_send_chunk(req, (const char *)index_html_start, upload_script_size);
	// httpd_resp_send_chunk(req,(const char *)TxBuffer,sizeof(TxBuffer));
	
	//下面这部分代码参考来源：https://blog.csdn.net/qq_27114397/article/details/89643232
	//httpd_resp_set_type和httpd_resp_set_hdr这俩函数不是很清楚它们的功能，只保留httpd_reso_send函数就能自动弹出页面
	//httpd_resp_set_type(req, "text/html");
	//httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
	return httpd_resp_send(req, (const char *)index_html_start, upload_script_size);
}

//HTTP_FirstGet_handler函数是https://blog.csdn.net/q_fy_p/article/details/127175477中编写的
//代码功能:强制门户访问时连接wifi后的第一次任意GET请求
static esp_err_t HTTP_FirstGet_handler(httpd_req_t *req)
{
	http_SendText_html(req);
	return ESP_OK;
}

unsigned char CharToNum(unsigned char Data)
{
	if(Data >= '0' && Data <= '9')
	{
		return Data - '0';
	}
	else if(Data >= 'a' && Data <= 'f')
	{
		switch (Data)
		{
			case 'a':return 10;
			case 'b':return 11;
			case 'c':return 12;
			case 'd':return 13;
			case 'e':return 14;
			case 'f':return 15;
		default:
			break;
		}
	}
	else if(Data >= 'A' && Data <= 'F')
	{
		switch (Data)
		{
			case 'A':return 10;
			case 'B':return 11;
			case 'C':return 12;
			case 'D':return 13;
			case 'E':return 14;
			case 'F':return 15;
		default:
			break;
		}
	}
	return 0;
}


//当前主要工作就是对这部分代码进行解析并处理，这里需要完成的内容：
//1、需要将ssid和password保存至nvs flash中
//2、然后需要在main中进行sta和ap之间的切换
//3、测试一下nvs flash保存的内容
/* 门户页面发回的，带有要连接的WIFI的名字和密码 */
static esp_err_t WIFI_Config_POST_handler(httpd_req_t *req)
{
	char buf[256];
	int ret, remaining = req->content_len;
	char wifi_name[50];                             //用于储存wifi ssid
	NET_CONFIG net_cfg = {0};
	char wifi_password[50];                         //用于储存password
	char wifi_passwordTransformation[50] = {0};     //这个数组用于储存转化后的password
	int i = 0;
	esp_err_t e = 0;

	while (remaining > 0)
	{
		/* Read the data for the request */
		//这部分代码用于读取到来自ipad端的post请求
		if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0)
		{
			if (ret == HTTPD_SOCK_ERR_TIMEOUT)
			{
				/* Retry receiving if timeout occurred */
				continue;
			}
			return ESP_FAIL;
		}
		remaining -= ret;

		/* Log data received */
		//这部分是打印出post method的内容
		WEB_INFO("=========== RECEIVED DATA ==========");
		WEB_INFO("%.*s", ret, buf);
		WEB_INFO("====================================");

		e = httpd_query_key_value(buf, "ssid", wifi_name, sizeof(wifi_name));
		if(ESP_OK == e) {
			WEB_INFO("SSID: %s", wifi_name);
		}
		else {
			WEB_ERR("error: %d", e);
		}

		/* 获取配网方式 */
		e = httpd_query_key_value(buf, "connect_type", net_cfg.connect_type, CONNECT_TYPE_STR_LEN);
		if(ESP_OK == e) {
			net_cfg.connect_type[CONNECT_TYPE_STR_LEN - 1] = 0;
			WEB_INFO("connect type: %s", net_cfg.connect_type);
		}
		else {
			WEB_ERR("error: 0x%x", e);
		}

		/* 获取主机IP */
		e = httpd_query_key_value(buf, "host_ip", net_cfg.host_ip, IP_STRING_LEN);
		if(ESP_OK == e) {
			net_cfg.host_ip[IP_STRING_LEN - 1] = 0;
			WEB_INFO("host ip: %s", net_cfg.host_ip);
		}
		else {
			WEB_ERR("error: %d", e);
		}

		/* 获取脑电端口号 */
		e = httpd_query_key_value(buf, "eeg_port", net_cfg.eeg_port, PORT_STRING_LEN);
		if(ESP_OK == e) {
			net_cfg.eeg_port[PORT_STRING_LEN - 1] = 0;
			WEB_INFO("host port: %s", net_cfg.eeg_port);
		}
		else {
			WEB_ERR("error: %d", e);
		}

		/* 获取脑氧端口号 */
		e = httpd_query_key_value(buf, "fnirs_port", net_cfg.fnirs_port, PORT_STRING_LEN);
		if(ESP_OK == e) {
			net_cfg.fnirs_port[PORT_STRING_LEN - 1] = 0;
			WEB_INFO("host port: %s", net_cfg.fnirs_port);
		}
		else {
			WEB_ERR("error: %d", e);
		}

		/* 获取子网掩码 */
		e = httpd_query_key_value(buf, "netmask", net_cfg.netmask, IP_STRING_LEN);
		if(ESP_OK == e) {
			net_cfg.netmask[IP_STRING_LEN - 1] = 0;
			WEB_INFO("net mask: %s", net_cfg.netmask);
		}
		else {
			WEB_ERR("error: %d", e);
		}

		/* 获取网关地址 */
		e = httpd_query_key_value(buf, "gateway", net_cfg.gateway, IP_STRING_LEN);
		if(ESP_OK == e) {
			net_cfg.gateway[IP_STRING_LEN - 1] = 0;
			WEB_INFO("gateway: %s", net_cfg.gateway);
		}
		else {
			WEB_ERR("error: %d", e);
		}

		/* 获取结点IP */
		e = httpd_query_key_value(buf, "client_ip", net_cfg.client_ip, IP_STRING_LEN);
		if(ESP_OK == e) {
			net_cfg.client_ip[IP_STRING_LEN - 1] = 0;
			WEB_INFO("client ip: %s", net_cfg.client_ip);
		}
		else {
			WEB_ERR("error: %d", e);
		}

		/* 获取结点序列号 */
		e = httpd_query_key_value(buf, "client_serial", net_cfg.client_serial, SERIAL_STRING_LEN);
		if(ESP_OK == e || ESP_ERR_HTTPD_RESULT_TRUNC == e) {
			WEB_INFO("serial num: %c%c%c%c%c%c",
				net_cfg.client_serial[0], net_cfg.client_serial[1], net_cfg.client_serial[2],
				net_cfg.client_serial[3], net_cfg.client_serial[4], net_cfg.client_serial[5]);
		}
		else {
			WEB_ERR("error: %d", e);
		}

		/* 获取传输协议 */
		e = httpd_query_key_value(buf, "protocol", net_cfg.protocol, PROTOCOL_STR_LEN);
		if(e == ESP_OK) {
			net_cfg.protocol[PROTOCOL_STR_LEN - 1] = 0;
			WEB_INFO("trans protocol: %s", net_cfg.protocol);
		}
		else {
			WEB_ERR("error: %d", e);
		}

		e = httpd_query_key_value(buf, "frequency", net_cfg.frequency, FREQUENCY_STR_LEN);
		if(e == ESP_OK) {
			net_cfg.frequency[FREQUENCY_STR_LEN - 1] = 0;
			WEB_INFO("frequency: %s", net_cfg.protocol);
		}
		else {
			WEB_ERR("error: %d", e);
		}

		e = httpd_query_key_value(buf, "bp_filter", net_cfg.bp_filter, FILTER_SWITCH_STR_LEN);
		if(e == ESP_OK) {
			net_cfg.bp_filter[FILTER_SWITCH_STR_LEN - 1] = 0;
			WEB_INFO("bandpass filter switch: %s", net_cfg.bp_filter);
		}
		else {
			WEB_ERR("error: %d", e);
		}

		e = httpd_query_key_value(buf, "hp_filter", net_cfg.hp_filter, FILTER_SWITCH_STR_LEN);
		if(e == ESP_OK) {
			net_cfg.hp_filter[FILTER_SWITCH_STR_LEN - 1] = 0;
			WEB_INFO("highpass filter switch: %s", net_cfg.hp_filter);
		}
		else {
			WEB_ERR("error: %d", e);
		}

		e = httpd_query_key_value(buf, "lp_filter", net_cfg.lp_filter, FILTER_SWITCH_STR_LEN);
		if(e == ESP_OK) {
			net_cfg.lp_filter[FILTER_SWITCH_STR_LEN - 1] = 0;
			WEB_INFO("lowpass filter switch: %s", net_cfg.lp_filter);
		}
		else {
			WEB_ERR("error: %d", e);
		}

		e = httpd_query_key_value(buf, "notch_filter", net_cfg.notch_filter, FILTER_SWITCH_STR_LEN);
		if(e == ESP_OK) {
			net_cfg.notch_filter[FILTER_SWITCH_STR_LEN - 1] = 0;
			WEB_INFO("notch filter switch: %s", net_cfg.notch_filter);
		}
		else {
			WEB_ERR("error: %d", e);
		}

		if(ESP_OK != nvs_write_net_cfg_to_flash(&net_cfg, "OK"))
		{
			WEB_ERR("nvs_commit error!");
		}

		if(ESP_OK == httpd_query_key_value(buf, "password", wifi_password, sizeof(wifi_password))) {
			/*对传回来的数据进行处理*/
			unsigned char Len = strlen(wifi_password);
			char tempBuffer[2];
			char *temp;
			unsigned char Cnt = 0;
			temp = wifi_password;
			for(i = 0; i < Len;){
				if(*temp == '%'){
					tempBuffer[0] = CharToNum(temp[1]);
					tempBuffer[1] = CharToNum(temp[2]);
					*temp = tempBuffer[0] * 16 + tempBuffer[1];
					wifi_passwordTransformation[Cnt] = *temp;
					temp+=3;
					i+=3;
					Cnt++;
				}
				else{
					wifi_passwordTransformation[Cnt] = *temp;
					temp++;
					i++;
					Cnt++;
				}
			}
			temp -= Len;
			printf("Len = %d\r\n",Len);
			printf("wifi_password = %s\r\n",wifi_password);
			printf("pswd = %s\r\n",wifi_passwordTransformation);
		}
		else {
			WEB_ERR("error: %d", e);
		}

		NVS_write_data_to_flash(wifi_name, wifi_password, "OK");

		//读取到数据后就重新启动esp32并切换成sta模式重新连接
		esp_restart();
	}

	return ESP_OK;
}


void web_server_start(void)
{
	// xTaskCreate(&webserver, "webserver_task", 2048, NULL, 5, NULL);
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	/* Use the URI wildcard matching function in order to
	* allow the same handler to respond to multiple different
	* target URIs which match the wildcard scheme */
	config.uri_match_fn = httpd_uri_match_wildcard;
	
	//启动web服务器
	WEB_INFO("Starting HTTP Server on port: %d", config.server_port); 
	if (httpd_start(&server, &config) != ESP_OK) {
		WEB_ERR("Failed to start file server!");
		return ;
	}

	/* URI handler for getting uploaded files */
	//这部分代码的功能讲解：
	//当ipad端打开192.168.4.1时会向esp32(即server端)发送一个HTTP_GET method的一个请求
	//此时就会触发HTTP_FistGet_handler这个事件，事件的功能为跳转至配网页面
	httpd_uri_t file_download = {
		.uri       = "/*",  // Match all URIs of type /path/to/file
		.method    = HTTP_GET,
		.handler   = HTTP_FirstGet_handler,
		.user_ctx  = NULL,
	};
	httpd_register_uri_handler(server, &file_download);

	/* URI handler for uploading files to server */
	//这部分代码的功能讲解：
	//这部分则是与html中定义的configwifi语句相关，当在ipad端点击submit时触发该语句并向esp32端发送一个HTTP_POST method类型的请求
	//此时就会触发WIFI_Config_POST_handler函数，这个函数的功能还需要再确定修改一下，因为现在还不知道POST发送过来的数据是咋样的
	//当前部分的功能已经能够正常接收到POST的数据了
	httpd_uri_t file_upload = {
		.uri       = "/configwifi",   // Match all URIs of type /upload/path/to/file
		.method    = HTTP_POST,
		.handler   = WIFI_Config_POST_handler,
		.user_ctx  = NULL,
	};
	httpd_register_uri_handler(server, &file_upload);
}

int nvs_write_net_cfg_to_flash(NET_CONFIG *cfg, char *confirm)
{
	nvs_handle nvs_h;
	int ret = 0;

	if (NULL == cfg || NULL == confirm)
	{
		WEB_ERR("input null");
		return ESP_FAIL;
	}

	/* 写入flash */
	ESP_ERROR_CHECK(nvs_open("net_config", NVS_READWRITE, &nvs_h));		//打开nvs，以可读可写的方式
	ESP_ERROR_CHECK(nvs_set_str(nvs_h, "check", confirm)); 				//储存校检符号
	ESP_ERROR_CHECK(nvs_set_blob(nvs_h, "net_config", cfg, sizeof(NET_CONFIG)));
	ret = nvs_commit(nvs_h);
	if(ESP_OK != ret)
	{
		WEB_ERR("nvs_commit error!");
	}
	nvs_close(nvs_h);

	return ret;
}

int nvs_read_net_cfg_from_flash(NET_CONFIG *cfg, char *confirm)
{
	nvs_handle nvs_h;
	size_t data_len = 128;
	size_t cfg_len = sizeof(NET_CONFIG);
	char data_buf[128] = {0};

	if (NULL == cfg || NULL == confirm)
	{
		WEB_ERR("input null");
		return ESP_FAIL;
	}

	memset(cfg, 0, sizeof(NET_CONFIG));
	ESP_ERROR_CHECK(nvs_open("net_config", NVS_READWRITE, &nvs_h));
	nvs_get_str(nvs_h, "check", data_buf, &data_len);
	nvs_get_blob(nvs_h, "net_config", cfg, &cfg_len);
	nvs_close(nvs_h);
	
	if (0 == strcmp(confirm, data_buf))
	{
		cfg->host_ip[IP_STRING_LEN - 1] = 0;
		cfg->eeg_port[PORT_STRING_LEN - 1] = 0;
		cfg->fnirs_port[PORT_STRING_LEN - 1] = 0;
		cfg->netmask[IP_STRING_LEN - 1] = 0;
		cfg->gateway[IP_STRING_LEN - 1] = 0;
		cfg->client_ip[IP_STRING_LEN - 1] = 0;
		cfg->protocol[PROTOCOL_STR_LEN - 1] = 0;
		cfg->frequency[FREQUENCY_STR_LEN - 1] = 0;
		cfg->bp_filter[FILTER_SWITCH_STR_LEN - 1] = 0;
		cfg->hp_filter[FILTER_SWITCH_STR_LEN - 1] = 0;
		cfg->lp_filter[FILTER_SWITCH_STR_LEN - 1] = 0;
		cfg->notch_filter[FILTER_SWITCH_STR_LEN - 1] = 0;
		return ESP_OK;
	}
	
	return ESP_FAIL;
}

static uint8_t decStr_to_hex(char c)
{
	uint8_t ret = 0;

	if ('0' <= c && c <= '9')
	{
		ret = c - '0';
	}
	else if ('a' <= c && c <= 'f')
	{
		ret = c - 'a' + 10;
	}
	else if ('A' <= c && c <= 'F')
	{
		ret = c - 'A' + 10;
	}
	else if ('f' < c && c <= 'z')
	{
		ret = 'f' - 'a' + 10;
	}
	else if ('F' < c && c <= 'Z')
	{
		ret = 'F' - 'A' + 10;
	}
	else
	{
		ret = 0;
	}

	return ret;
}

uint32_t serial_num_str_parse(char *s)
{
	uint32_t serial = 0;
	int i = 0;
	int str_len = 0;
	uint8_t b[SERIAL_STRING_LEN - 1] = {0};

	if (NULL == s)
	{
		WEB_ERR("param null");
		return 0;
	}
	if (SERIAL_STRING_LEN - 1 < strlen(s))
	{
		WEB_ERR("serial num string is longer than %d bytes, actual length: %d", SERIAL_STRING_LEN - 1, strlen(s));
		return 0;
	}

	str_len = strlen(s);
	if (0 == str_len)
	{
		return 0;
	}

	for (i = SERIAL_STRING_LEN - 1 - str_len; i < SERIAL_STRING_LEN - 1; i++)
	{
		b[i] = decStr_to_hex(s[i]);
		serial |= (b[i]<<(20 - 4*i));
	}

	return serial;
}
