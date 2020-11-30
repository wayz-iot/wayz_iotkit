#include <rtthread.h>
#include <wlan_mgnt.h>
#include <wifi_config.h>
#include <webclient.h>
#include <ntp_client.h>
#include <wayz_pos.h>


#ifdef PKG_WAYZ_IOTKIT_WIFI_SSID
#define  WAYZ_WIFI_SSID      "Honor9"
#else
#define  WAYZ_WIFI_SSID      "Honor9"
#endif

#ifdef PKG_WAYZ_IOTKIT_WIFI_PASSWORD
#define  WAYZ_WIFI_PWD      "316157666"
#else
#define  WAYZ_WIFI_PWD      "316157666"
#endif

#define  DEV_NAME       "PANDDRA"
#define  VENDER         "ALIENTEK"
#define  PRODUCT        "FINDU01"
#define  SN             "1234567"
#define  TENANT         "WAYZ"

#define  POINT_FRQ      3000

#define  ACCESS_KEY     "SCVXgNIdKCYBeNkWY4W1UGw6NCEcGq7K" // 需要申请

static void location_client_entry(void *parament)
{
    char mac_addr[17] = {0};
    char ret = 0;
    get_sta_mac_addr(mac_addr);
    rt_kprintf("station mac : %s \r\n", mac_addr);

    twifi_info *wlan_info;
    tdeviec_info *dev_info;

    wlan_info = wifi_param_init(WAYZ_WIFI_SSID, WAYZ_WIFI_PWD);
    dev_info = dev_para_init(DEV_NAME, VENDER, PRODUCT, SN, TENANT);

    ret = dev_register_init(wlan_info, dev_info, ACCESS_KEY);
    if (ret != DEV_REGISTER_OK)
    {
        rt_kprintf("\033[31;22mdevice register failure. \033[0m\n");
        return ;
    }

    tlocation_info location = {0};
    ret = get_position_info(wlan_info, ACCESS_KEY, &location); // 单词定位结果获取
    if (RT_ERROR == ret)
    {
        rt_kprintf("\033[31;22mthe device failed to obtain latitude and longitude information.\033[0m\n");
    }
    else
    {
        location_print(location);
    }
    
    while (1)
    {
        ret = get_position_info(wlan_info, ACCESS_KEY, &location); // 单词定位结果获取
        if (RT_ERROR == ret)
        {
            rt_kprintf("\033[31;22mthe device failed to obtain latitude and longitude information.\033[0m\n");
        }
        else
        {
            location_print(location);
        }
        rt_thread_mdelay(POINT_FRQ);
        rt_memset(&location, 0, sizeof (location));
    }
    
}

int location_client_start(void)
{
    rt_thread_t tid;
				//cJsonTask();
    tid = rt_thread_create("location_client", location_client_entry, RT_NULL, 6 * 1024, RT_THREAD_PRIORITY_MAX / 3 - 1, 5);
    if (tid)
    {
						
        rt_thread_startup(tid);
        return RT_EOK;
    }

    return -RT_ERROR;
}
