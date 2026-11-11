/*
 * 主机端测试台的 WLAN 桩头文件
 *
 * 结构体定义对齐 RT-Thread 5.x(dev_wlan.h):
 *   - ssid 是 rt_wlan_ssid_t{len, val[RT_WLAN_SSID_MAX_LENGTH]}
 *   - channel 是 rt_int16_t(驱动的无效值是 -1)
 */
#ifndef __WLAN_STUB_H__
#define __WLAN_STUB_H__

#include <rtthread.h>

#define RT_WLAN_SSID_MAX_LENGTH   34
#define RT_WLAN_BSSID_MAX_LENGTH  6
#define RT_WLAN_INFO_INIT          (-1)

typedef struct rt_wlan_ssid
{
    rt_uint8_t len;
    rt_uint8_t val[RT_WLAN_SSID_MAX_LENGTH];
} rt_wlan_ssid_t;

struct rt_wlan_info
{
    int security;
    int band;
    rt_uint32_t datarate;
    rt_int16_t channel;
    rt_int16_t rssi;
    rt_wlan_ssid_t ssid;
    rt_uint8_t bssid[RT_WLAN_BSSID_MAX_LENGTH];
    rt_uint8_t hidden;
};

typedef enum
{
    RT_WLAN_EVT_READY = 0,
    RT_WLAN_EVT_SCAN_DONE,
    RT_WLAN_EVT_SCAN_REPORT,
    RT_WLAN_EVT_STA_CONNECTED,
    RT_WLAN_EVT_STA_CONNECTED_FAIL,
    RT_WLAN_EVT_STA_DISCONNECTED,
    RT_WLAN_EVT_MAX
} rt_wlan_event_t;

struct rt_wlan_scan_result
{
    rt_int32_t num;
    struct rt_wlan_info *info;
};

struct rt_wlan_buff
{
    void *data;
    rt_int32_t len;
};

typedef void (*rt_wlan_event_handler)(int event, struct rt_wlan_buff *buff, void *parameter);

rt_err_t rt_wlan_connect(const char *ssid, const char *password);
rt_bool_t rt_wlan_is_connected(void);
rt_err_t rt_wlan_get_mac(rt_uint8_t *mac);
rt_err_t rt_wlan_get_info(struct rt_wlan_info *info);
rt_err_t rt_wlan_scan(void);
void rt_wlan_config_autoreconnect(rt_bool_t enable);
rt_err_t rt_wlan_register_event_handler(rt_wlan_event_t event, rt_wlan_event_handler handler, void *parameter);
rt_err_t rt_wlan_unregister_event_handler(rt_wlan_event_t event);

#endif
