/* 仅用于本地语法检查的 WLAN 桩头文件, 不属于仓库
 * 按 RT-Thread 真实 API 建模:
 *   - 4.1.0 有 rt_wlan_scan_sync() / rt_wlan_scan_result_clean()
 *   - 5.0.0+ 只有 rt_wlan_scan() / rt_wlan_scan_with_info()
 */
#ifndef __WLAN_STUB_H__
#define __WLAN_STUB_H__

#include <rtthread.h>

#define RT_WLAN_BSSID_MAX_LENGTH 6

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

struct rt_wlan_info
{
    int security;
    int band;
    rt_uint32_t datarate;
    rt_int16_t channel;
    rt_int16_t rssi;
    char ssid[33];
    rt_uint8_t bssid[RT_WLAN_BSSID_MAX_LENGTH];
    rt_uint8_t hidden;
};

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
rt_err_t rt_wlan_disconnect(void);
rt_bool_t rt_wlan_is_connected(void);
rt_err_t rt_wlan_get_mac(rt_uint8_t *mac);
rt_err_t rt_wlan_get_info(struct rt_wlan_info *info);
rt_err_t rt_wlan_scan(void);
rt_err_t rt_wlan_scan_with_info(struct rt_wlan_info *info);
void rt_wlan_config_autoreconnect(rt_bool_t enable);
rt_err_t rt_wlan_register_event_handler(rt_wlan_event_t event, rt_wlan_event_handler handler, void *parameter);
rt_err_t rt_wlan_unregister_event_handler(rt_wlan_event_t event);

/* RT-Thread 5.0.0 起该接口被移除 */
#if !defined(RT_VERSION_MAJOR) || (RT_VERSION_MAJOR < 5)
struct rt_wlan_scan_result *rt_wlan_scan_sync(void);
void rt_wlan_scan_result_clean(void);
#endif

#endif
