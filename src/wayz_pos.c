
#include "wayz_pos.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include <ntp_client.h>
#include <finsh.h>
#include <msh.h>
#include <math.h>

tapinfo aucApInfo = {0};
static twifi_info aucwlaninfo = {0};
static struct rt_semaphore net_ready;
static char access_key[KEY_LENGTH];
#define NET_READY_TIME_OUT       (rt_tick_from_millisecond(15 * 1000))

const double a = 6378245.0;
const double ee = 0.00669342162296594323;
const double pi = 3.14159265358979324;

static unsigned char outOfChina(double lat, double lon)
{
    if (lon < 72.004 || lon > 137.8347)
        return 1;
    if (lat < 0.8293 || lat > 55.8271)
        return 1;
    return 0;
}

static double transformLat(double x, double y)
{
    double ret = -100.0 + 2.0 * x + 3.0 * y + 0.2 * y * y + 0.1 * x * y + 0.2 * sqrt(abs(x));
    ret += (20.0 * sin(6.0 * x * pi) + 20.0 * sin(2.0 * x * pi)) * 2.0 / 3.0;
    ret += (20.0 * sin(y * pi) + 40.0 * sin(y / 3.0 * pi)) * 2.0 / 3.0;
    ret += (160.0 * sin(y / 12.0 * pi) + 320 * sin(y * pi / 30.0)) * 2.0 / 3.0;
    return ret;
}

static double transformLon(double x, double y)
{
    double ret = 300.0 + x + 2.0 * y + 0.1 * x * x + 0.1 * x * y + 0.1 * sqrt(abs(x));
    ret += (20.0 * sin(6.0 * x * pi) + 20.0 * sin(2.0 * x * pi)) * 2.0 / 3.0;
    ret += (20.0 * sin(x * pi) + 40.0 * sin(x / 3.0 * pi)) * 2.0 / 3.0;
    ret += (150.0 * sin(x / 12.0 * pi) + 300.0 * sin(x / 30.0 * pi)) * 2.0 / 3.0;
    return ret;
}

static void gps_transform( double wgLat, double wgLon, double * mgLat, double* mgLon)
{
    if (outOfChina(wgLat, wgLon))
    {
        *mgLat = wgLat;
        *mgLon = wgLon;
        return;
    }
    double dLat = transformLat(wgLon - 105.0, wgLat - 35.0);
    double dLon = transformLon(wgLon - 105.0, wgLat - 35.0);
    double radLat = wgLat / 180.0 * pi;
    double magic = sin(radLat);
    magic = 1 - ee * magic * magic;
    double sqrtMagic = sqrt(magic);
    dLat = (dLat * 180.0) / ((a * (1 - ee)) / (magic * sqrtMagic) * pi);
    dLon = (dLon * 180.0) / (a / sqrtMagic * cos(radLat) * pi);
    *mgLat = wgLat + dLat;
    *mgLon = wgLon + dLon;
};

// static void wgs84togcj02(double wgLat, double wgLon, double * mgLat, double* mgLon)
// {
//     gps_transform(wgLat, wgLon, mgLat, mgLon);
// }

static void gcj02towgs84(double wgLat, double wgLon, double * mgLat, double* mgLon)
{
    gps_transform(wgLat, wgLon, mgLat, mgLon);
    *mgLat = wgLat * 2 - *mgLat;
    *mgLon = wgLon * 2 - *mgLon;
}

// static void gcj02tobd09(double wgLat, double wgLon, double * mgLat, double* mgLon)
// {
//     double x = wgLon, y = wgLat;
//     double z = sqrt(x * x + y * y) + 0.00002 * sin(y * pi);
//     double theta = atan2(y, x) + 0.000003 * cos(x * pi);
//     *mgLon = z * cos(theta) + 0.0065;
//     *mgLat = z * sin(theta) + 0.006;
// }

/**
 * @param data: get mac address data, eg:XX:XX:XX:XX:XX:XX
 * 
 * @return 
 */
void get_sta_mac_addr(char *data)
{
    rt_uint8_t mac[6] = {0};
    rt_wlan_get_mac(mac);

    rt_sprintf(data, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void wayz_error(char* msg)
{
    rt_kprintf("\033[31;22m[E/wayz]: ERROR %s\033[0m\n", msg); // Print the error message to stderr.
}

static void print_scan_result(struct rt_wlan_scan_result *scan_result)
{
    int index, num;

    num = scan_result->num;
    aucApInfo.count = scan_result->num;
    for (index = 0; index < num; index ++)
    {
        aucApInfo.tinfoAp[index].mac[0] = scan_result->info[index].bssid[0];
        aucApInfo.tinfoAp[index].mac[1] = scan_result->info[index].bssid[1];
        aucApInfo.tinfoAp[index].mac[2] = scan_result->info[index].bssid[2];
        aucApInfo.tinfoAp[index].mac[3] = scan_result->info[index].bssid[3];
        aucApInfo.tinfoAp[index].mac[4] = scan_result->info[index].bssid[4];
        aucApInfo.tinfoAp[index].mac[5] = scan_result->info[index].bssid[5];
        aucApInfo.tinfoAp[index].rssi = scan_result->info[index].rssi;
        aucApInfo.tinfoAp[index].channel = scan_result->info[index].channel;
    }
}

static void get_wifi_scan_info()
{
    if (RT_TRUE == rt_wlan_is_connected())
    {
        wayz_error("ready to disconect from ap ...");
        rt_wlan_disconnect();
    }
    /* wait 500 milliseconds for wifi low level initialize complete */
    //rt_hw_wlan_wait_init_done(500);
    /* Configuring WLAN device working mode */
    rt_wlan_set_mode(RT_WLAN_DEVICE_STA_NAME, RT_WLAN_STATION);
        /* scan ap */
    struct rt_wlan_scan_result *scan_result = RT_NULL;
    rt_kprintf("\nstart to scan ap ...\n");
    /* execute synchronous scan function */
    scan_result = rt_wlan_scan_sync();
    if (scan_result)
    {
        rt_kprintf("the scan is complete, results is as follows: \n");
        /* print scan results */
        print_scan_result(scan_result);
        /* clean scan results */
        rt_wlan_scan_result_clean();
    }
    else
    {
        wayz_error("not found ap information ");
    }


}

static void wifi_ready_handler(int event, struct rt_wlan_buff *buff, void *parameter)
{
    rt_sem_release(&net_ready);
}

/**
 * The callback of wlan disconected event
 */
static void wifi_station_disconnect_handler(int event, struct rt_wlan_buff *buff, void *parameter)
{
    rt_kprintf("disconnect from the network!\n");
}

// static void print_wlan_information(struct rt_wlan_info *info)
// {
//     rt_kprintf("SSID : %-.32s\n", &info->ssid.val[0]);
//     rt_kprintf("MAC Addr: %02x:%02x:%02x:%02x:%02x:%02x\n", info->bssid[0],
//                info->bssid[1],
//                info->bssid[2],
//                info->bssid[3],
//                info->bssid[4],
//                info->bssid[5]);
//     rt_kprintf("Channel: %d\n", info->channel);
//     rt_kprintf("DataRate: %dMbps\n", info->datarate / 1000000);
//     rt_kprintf("RSSI: %d\n", info->rssi);
// }

twifi_info *wifi_param_init(const char *ssid, const char *passwd)
{
    twifi_info *wlan_info = (twifi_info *) rt_calloc(1, sizeof (twifi_info));
    if (RT_NULL == wlan_info)
    {
        wayz_error("wifi param malloc fail.");
        return RT_NULL;
    }

    wlan_info->ssid = strdup(ssid);
    wlan_info->passwd = strdup(passwd);

    return wlan_info;
}

tdeviec_info * dev_para_init(const char *dev_name, const char *manufacturer, const char *product, \
                const char *SN, const char *tenant)
{

    tdeviec_info *dev_info = (tdeviec_info *) rt_calloc(1, sizeof (tdeviec_info));
    if (RT_NULL == dev_info)
    {
        wayz_error("wifi param malloc fail.");
        return RT_NULL;
    }

    dev_info->dev_name = strdup(dev_name);
    dev_info->manufacturer = strdup(manufacturer);
    dev_info->product = strdup(product);
    dev_info->SN = strdup(SN);
    dev_info->tenant = strdup(tenant);
    return dev_info;
}

static char wifi_init(const char *ssid, const char *password)
{
    struct rt_wlan_info info;
    int result = RT_EOK;
    char ret = RT_EOK;
    rt_wlan_get_mac(aucApInfo.sta_mac);

    rt_memcpy(aucwlaninfo.ssid, ssid, rt_strlen(ssid));
    rt_memcpy(aucwlaninfo.passwd, password, rt_strlen(password));

    if (RT_TRUE == rt_wlan_is_connected())
    {
        ret = RT_EOK;
        goto _connected;
    }
    rt_sem_init(&net_ready, "net_ready", 0, RT_IPC_FLAG_FIFO);
    /* register network ready event callback */
    rt_wlan_register_event_handler(RT_WLAN_EVT_READY, wifi_ready_handler, RT_NULL);
    /* register wlan disconnect event callback */
    rt_wlan_register_event_handler(RT_WLAN_EVT_STA_DISCONNECTED, wifi_station_disconnect_handler, RT_NULL);
    result = rt_wlan_connect(ssid, password);
    if (result == RT_EOK)
    {
        rt_memset(&info, 0, sizeof(struct rt_wlan_info));
        /* Get the information of the current connection AP */
        rt_wlan_get_info(&info);
        // rt_kprintf("station information:\n");
        // print_wlan_information(&info);
        /* waiting for IP to be got successfully  */
        result = rt_sem_take(&net_ready, NET_READY_TIME_OUT);
        if (result == RT_EOK)
        {
            rt_kprintf("networking ready!\n");
        }
        else
        {
            wayz_error("wait ip got timeout!");
        }
        /* unregister network ready event */
        rt_wlan_unregister_event_handler(RT_WLAN_EVT_READY);
        rt_sem_detach(&net_ready);
    }
    else
    {
        rt_kprintf("\033[31;22mThe AP(%s) is connect failed!\033[0m\n", ssid);
        ret = RT_ERROR;
    }

_connected:
    rt_wlan_config_autoreconnect(RT_TRUE);
    return ret;
}

// static rt_uint16_t chnTofreq(rt_uint8_t channel)
// {
//     return (FREQ_START + (channel - 1) * SEGMEMTATION);
// }

static char *point_cJson_handler(void)
{
 /* declare a few. */
    cJSON *root = NULL;
    cJSON *fmt = NULL;
    cJSON *img = NULL;
    cJSON *thm = NULL;
    cJSON *fld = NULL;
    int i = 0, j = 0;
    char macBuf[MAC_LEN] = {0};
    char *cJsonBuffer = NULL;
    char *buffer = NULL;

    root = cJSON_CreateObject();

    time_t cur_time;
    uint64_t time = 0;
    static uint64_t pre_time = 0;

    cur_time = wayz_get_time_by_ntp(NULL);
    if (cur_time)
    {
        time = (uint64_t)cur_time * 1000;
        // rt_kprintf("cur_time: %ld, %ld \r\n", cur_time, time);
        pre_time = time;
    }
    else
    {
        time = (pre_time == 0 ? TIMESTAMP_DEL : (pre_time + 5000));
    }
    
    char buftemp[40] = {0};
    rt_sprintf(buftemp, "38efe26e-bcd8-"MACPRINTID, PRINT(aucApInfo.sta_mac, 0));

    cJSON_AddItemToObject(root, "id", cJSON_CreateString(buftemp));
    cJSON_AddItemToObject(root, "asset", fmt = cJSON_CreateObject());
    rt_sprintf(macBuf, ""MACPRINT, PRINT(aucApInfo.sta_mac, 0));
    cJSON_AddItemToObject(fmt, "id", cJSON_CreateString(macBuf));
    cJSON_AddItemToObject(root, "location", img = cJSON_CreateObject());
    cJSON_AddItemToObject(img, "wifis", thm = cJSON_CreateArray());

    for (i = 0, j = 0; i < aucApInfo.count; i++)
    {
        rt_sprintf(macBuf, ""MACPRINT, PRINT(aucApInfo.tinfoAp[i].mac, 0));
        cJSON_AddItemToArray(thm, fld = cJSON_CreateObject());
        cJSON_AddStringToObject(fld, "macAddress", macBuf);
        // cJSON_AddStringToObject(fld, "ssid", "");
        // cJSON_AddNumberToObject(fld, "frequency", chnTofreq(aucApInfo.tinfoAp[i].channel));
        cJSON_AddNumberToObject(fld, "signalStrength", abs(aucApInfo.tinfoAp[i].rssi));
        j ++;
        if (j > 10)
        {
            //break;
        }
    }
    
    cJsonBuffer = cJSON_Print(root);
    buffer = (char *)rt_malloc(rt_strlen(cJsonBuffer) + 4);
    if (RT_NULL == buffer)
    {
        wayz_error("point_cJson_handler create malloc failure.");
        return STR_ERROR;
    }

    rt_sprintf(buffer, "%s", cJsonBuffer);
    cJSON_Delete(root);
    rt_free(cJsonBuffer);

    return buffer; // 需要free
}

/**
 * @param location: print location info
 * 
 * @warning rt_kprintf no print float/double
 *          printf is ok
 * 
*/
void location_print(tlocation_info location)
{
    rt_kprintf("-------------------location result-------------------------------\r\n");
    rt_kprintf("timestamp: %lld \r\n", location.timestamp);
    printf("gcj02:\r\n\tlatitude:%f\r\n\tlongitude:%f\r\n", location.point.gcj02.latitude, location.point.gcj02.longitude);
    printf("wgs84:\r\n\tlatitude:%0.12f\r\n\tlongitude:%0.12f\r\n", location.point.wgs84.latitude, location.point.wgs84.longitude);

    rt_kprintf("POI: {\"id\": %s,\"type\": %s,\"name\": %s,\"categories\":[{\"id\": %s,\"name\": %s}]}\r\n", 
        location.place.id, location.place.type, location.place.name, location.place.category.id, location.place.category.name);
    rt_kprintf("-------------------location result end---------------------------\r\n");
}

/**
 * parse point cjson handler
*/
static void parse_point_cJson_handler(char *data, tlocation_info *location)
{
    cJSON *root, *object, *position, *point, *place, *category;
	cJSON *item;
	cJSON *temp = RT_NULL;
    char *buffer = RT_NULL;

    root = cJSON_Parse(data);
    if (!root) {
        rt_kprintf("\033[31;22m[E/wayz]Error before: [%s]\033[0m\n", cJSON_GetErrorPtr());
        goto _root_fail;
    }

    object = cJSON_GetObjectItem(root, "location");
    if (!object)
    {
        wayz_error("object failure\r\n");
        goto _root_fail;
    }

    position = cJSON_GetObjectItem(object, "position");
    if (!position)
    {
        wayz_error("position failure\r\n");
        goto _root_fail;
    }

    item = cJSON_GetObjectItem(position, "timestamp");
    buffer = cJSON_Print(item);
    location->timestamp = atoll(buffer);
    rt_free(buffer);
    buffer = RT_NULL;

    point = cJSON_GetObjectItem(position, "point");
    if (!point)
    {
        wayz_error("point failure\r\n");
        goto _root_fail;
    }

    item = cJSON_GetObjectItem(point, "longitude");
    buffer = cJSON_Print(item);
    location->point.gcj02.longitude = atof(buffer);
    rt_free(buffer);
    buffer = RT_NULL;
    // printf("longitude: %s, %3.6f\r\n", cJSON_Print(item), location.point.gcj02.longitude);
    item = cJSON_GetObjectItem(point, "latitude");
    buffer = cJSON_Print(item);
    location->point.gcj02.latitude = atof(buffer);
    rt_free(buffer);
    buffer = RT_NULL;
  
    gcj02towgs84(location->point.gcj02.latitude, location->point.gcj02.longitude, &location->point.wgs84.latitude, &location->point.wgs84.longitude);

    place = cJSON_GetObjectItem(object, "place");
    if (!place)
    {
        wayz_error("place failure\r\n");
        goto _root_fail;
    }

    item = cJSON_GetObjectItem(place, "id");
    buffer = cJSON_Print(item);
    rt_sprintf(location->place.id , "%s", buffer);
    rt_free(buffer);
    buffer = RT_NULL;

    item = cJSON_GetObjectItem(place, "type");
    buffer = cJSON_Print(item);
    rt_sprintf(location->place.type , "%s", buffer);
    rt_free(buffer);
    buffer = RT_NULL;

    item = cJSON_GetObjectItem(place, "name");
    buffer = cJSON_Print(item);
    rt_sprintf(location->place.name , "%s", buffer);
    rt_free(buffer);
    buffer = RT_NULL;

    category = cJSON_GetObjectItem(place, "categories");
    
    temp = cJSON_GetArrayItem(category, 0);
    item = cJSON_GetObjectItem(temp, "id");
    buffer = cJSON_Print(item);
    rt_sprintf(location->place.category.id , "%s", buffer);
    rt_free(buffer);
    buffer = RT_NULL;
    item = cJSON_GetObjectItem(temp, "name");
    buffer = cJSON_Print(item);
    rt_sprintf(location->place.category.name , "%s", buffer);
    rt_free(buffer);
    buffer = RT_NULL;

_root_fail:
    cJSON_Delete(root);
}

static char * register_dev_cjson_handler(tdeviec_info *dev_info)
{
    cJSON *root = NULL;
    cJSON *fmt = NULL;
    cJSON *img = NULL;
    char macBuf[MAC_LEN] = {0};
    char *cJsonBuffer = NULL;
    char *buffer = NULL;
    time_t cur_time;
    uint64_t time = 0;
    static uint64_t pre_time = 0;
    char temp[10] = {0};

    root = cJSON_CreateArray();

    rt_sprintf(temp, "%01d.%01d.%01d", VER_H, VER_M, VER_L);
    rt_sprintf(macBuf, ""MACPRINT, PRINT(aucApInfo.sta_mac, 0));
    cJSON_AddItemToArray(root, fmt = cJSON_CreateObject());
    cJSON_AddStringToObject(fmt, "id", macBuf);
    cJSON_AddStringToObject(fmt, "name", dev_info->dev_name);
    cJSON_AddStringToObject(fmt, "manufacturer", dev_info->manufacturer);
    cJSON_AddStringToObject(fmt, "macAddress", macBuf);
    cJSON_AddStringToObject(fmt, "serialNumber", dev_info->SN);
    cJSON_AddItemToObject(fmt, "firmware", img = cJSON_CreateObject());
    cJSON_AddStringToObject(img, "version", temp);

    cur_time = wayz_get_time_by_ntp(NULL);
    if (cur_time)
    {
        time = (uint64_t)cur_time * 1000;
        // rt_kprintf("cur_time: %ld, %ld \r\n", cur_time, time);
        pre_time = time;
    }
    else
    {
        time = (pre_time == 0 ? TIMESTAMP_DEL : (pre_time + 5000));
    }

    cJSON_AddNumberToObject(fmt, "createTime", time);
    cJSON_AddNumberToObject(fmt, "updateTime", time);
    cJSON_AddNumberToObject(fmt, "manufactureTime", time);
    cJSON_AddStringToObject(fmt, "product", dev_info->product);
    cJSON_AddStringToObject(fmt, "tenant", dev_info->tenant);

    cJsonBuffer = cJSON_Print(root);
    buffer = (char *)rt_malloc(rt_strlen(cJsonBuffer));
    if (RT_NULL == buffer)
    {
        rt_kprintf("create malloc failure.\r\n");
        return STR_ERROR;
    }

    rt_sprintf(buffer, "%s", cJsonBuffer);

    cJSON_Delete(root);
    rt_free(cJsonBuffer);

    return buffer; // free
}

static unsigned char * wayz_webclient_get_data(const char *URI)
{
    unsigned char *buffer = RT_NULL;
    int length = 0;
    
    length = webclient_request(URI, RT_NULL, RT_NULL, &buffer);
    if (length < 0)
    {
        wayz_error("webclient GET request response data error.");
        web_free(buffer);
        return STR_ERROR;
    }

    return buffer;
}

/* HTTP client upload data to server by POST request */
static unsigned char * wayz_webclient_post_data(const char *URI, const char *post_data)
{
    unsigned char *buffer = RT_NULL;
    int length = 0;

    length = webclient_request(URI, RT_NULL, post_data, &buffer);
    if (length < 0)
    {
        rt_kprintf("\033[31;22m[E/wayz]:webclient POST request response data error.%s\033[0m\r\n", URI);
        web_free(buffer);
        return STR_ERROR;
    }

    return buffer;
}

static rt_uint8_t query_device()
{
    char *url = RT_NULL;
    char mac_addr[MAC_LEN] = {0};
    unsigned char *buffer = RT_NULL;
    char *temp = RT_NULL;
    char result = 0;
    url = (char *)rt_malloc(rt_strlen(DEV_QUERY_URL) + MAC_LEN + rt_strlen(access_key));
    if (RT_NULL == url)
    {
        result = RT_ERROR;
        goto _malloc_fail;
    }
    rt_memset(url, 0, rt_strlen(DEV_QUERY_URL) + MAC_LEN + rt_strlen(access_key));
    rt_sprintf(mac_addr, ""MACPRINT, PRINT(aucApInfo.sta_mac, 0));
    rt_sprintf(url, ""DEV_QUERY_URL, mac_addr, access_key);
    buffer = wayz_webclient_get_data(url);
    if (rt_memcmp(buffer, STR_ERROR, rt_strlen(STR_ERROR)) == 0)
    {
        rt_kprintf("\033[31;22m[E/wayz]: visiting %s failure\033[0m\r\n", url);
        result = RT_ERROR;
        goto _url_fail;
    }

    temp = rt_strstr((char *)buffer, mac_addr);
    if (RT_NULL == temp)
    {
        result = RT_ERROR;
        goto _query_fail;
    }
    result = RT_EOK;

_query_fail:
    rt_free(buffer);
_url_fail:
    rt_free(url);
    url = RT_NULL;
_malloc_fail:
    return result;
}

static rt_uint8_t register_device(tdeviec_info *dev_info)
{
    char *url = RT_NULL;
    unsigned char *buffer = RT_NULL;
    char *temp = RT_NULL;
    char result = 0;
    char *cJsonBuffer = NULL;
    char mac_addr[MAC_LEN] = {0};
    url = (char *)rt_malloc(rt_strlen(DEV_REGISTER_URL) + rt_strlen(access_key));
    if (RT_NULL == url)
    {
        result = RT_ERROR;
        goto _malloc_fail;
    }
    rt_memset(url, 0, rt_strlen(DEV_REGISTER_URL) + rt_strlen(access_key));
    rt_sprintf(url, ""DEV_REGISTER_URL, access_key);
    cJsonBuffer = register_dev_cjson_handler(dev_info);
    buffer = wayz_webclient_post_data(url, cJsonBuffer);
    if (rt_memcmp(buffer, STR_ERROR, rt_strlen(STR_ERROR)) == 0)
    {
        rt_kprintf("\033[31;22m[E/wayz]: visiting %s failure\033[0m\r\n", url);
        result = RT_ERROR;
        goto _url_fail;
    }
    rt_sprintf(mac_addr, ""MACPRINT, PRINT(aucApInfo.sta_mac, 0));
    temp = rt_strstr((char *)buffer, mac_addr);
    if (RT_NULL == temp)
    {
        result = RT_ERROR;
        goto _query_fail;
    }
    result = RT_EOK;

_query_fail:
    rt_free(buffer);
    buffer = RT_NULL;
_url_fail:
    rt_free(url);
    rt_free(cJsonBuffer);
    url = RT_NULL;
    cJsonBuffer = RT_NULL;
_malloc_fail:
    return result;
}

static void dev_free(tdeviec_info *dev_info)
{
    rt_free(dev_info->dev_name);
    rt_free(dev_info->manufacturer);
    rt_free(dev_info->SN);
    rt_free(dev_info->product);
    rt_free(dev_info->tenant);
}

/**
 * Connected to the Internet to register
 *
 * @param wlan_info wifi name , wifi passwd
 *
 * @param dev_info device info ,(dev_name、manufacturer、SN[SN_LENGTH]、product[NAME_LENGTH]、tenant)
 * 
 * @param key Visiting the website key
 *
 * @return =0: wifi connect failure
 *         =1: device register success
 *         =2: device register failure
 */
char dev_register_init(twifi_info *wlan_info, tdeviec_info *dev_info, char *key)
{
#ifdef FINSH_USING_MSH
    rt_kprintf("dev_register_init is start:\r\n");
    msh_exec("free", rt_strlen("free"));
#endif

    if (RT_NULL == key && 0 == rt_strcmp(key, ""))
    {
        wayz_error("Visiting the website key failure");
    }
    else
    {
        rt_memcpy(access_key, key, rt_strlen(key));
    }
    
    // 1 connected to the Internet 
    if (RT_ERROR == wifi_init(wlan_info->ssid, wlan_info->passwd))
    {
        wayz_error("connect to the internet failure\r\n");
        return WIFI_CONNECT_FAIL;
    }
    // 2 Check whether the device is registered
    if (RT_EOK == query_device())
    {
        wayz_error("the device has been registered, no need to register again");
        return DEV_REGISTER_OK;
    }
    // 3 registered handler
    if (RT_EOK == register_device(dev_info))
    {
        rt_kprintf("register device success.\r\n");
    }
    else
    {
        wayz_error("register device failure.\r\n");
        return DEV_REGISTER_FAIL;
    }

    dev_free(dev_info);
    return DEV_REGISTER_OK;
}

/**
 * Get the positioning result function
 * 
 * @param wlan_info Wifi related information
 * 
 * @param key Visiting the website key
 * 
 * @param location get location result
 * 
 * @return >0: success
 *         =0: location failure
 * 
*/
char get_position_info(twifi_info *wlan_info, char *key, tlocation_info *location)
{
    char *url = RT_NULL;
    unsigned char *buffer = RT_NULL;
    char result = 0;
    char *cJsonBuffer = NULL;
    
    if (RT_NULL == key && 0 == rt_strcmp(key, ""))
    {
        wayz_error("Visiting the website key failure");
    }
    else
    {
        rt_memcpy(access_key, key, rt_strlen(key));
    }
    // 1 scan wifi info 
    get_wifi_scan_info();
    // 2 connect to the internet
    if (RT_ERROR == wifi_init(wlan_info->ssid, wlan_info->passwd))
    {
        wayz_error("connect to the internet failure\r\n");
        result = RT_ERROR;
        goto _malloc_fail;
    }

    // 3 Upload data to get positioning results
    url = (char *)rt_malloc(rt_strlen(DEV_POSTION_URL) + rt_strlen(access_key));
    if (RT_NULL == url)
    {
        result = RT_ERROR;
        goto _malloc_fail;
    }
    rt_memset(url, 0, rt_strlen(DEV_POSTION_URL) + rt_strlen(access_key));
    rt_sprintf(url, ""DEV_POSTION_URL, access_key);
    cJsonBuffer = point_cJson_handler();
    buffer = wayz_webclient_post_data(url, cJsonBuffer);
    if (rt_memcmp(buffer, STR_ERROR, rt_strlen(STR_ERROR)) == 0)
    {
        rt_kprintf("\033[31;22m[E/wayz]: visiting %s failure\033[0m\r\n", url);
        result = RT_ERROR;
        goto _url_fail;
    }

    result = RT_EOK;

    parse_point_cJson_handler((char *)buffer, location);

    rt_free(buffer);
    buffer = RT_NULL;

_url_fail:
    rt_free(url);
    rt_free(cJsonBuffer);
    url = RT_NULL;
    cJsonBuffer = RT_NULL;
_malloc_fail:

#ifdef FINSH_USING_MSH
    rt_kprintf("get_position_info is end:\r\n");
    msh_exec("free", rt_strlen("free"));
#endif
    return result;
}


