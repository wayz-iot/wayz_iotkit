
#ifndef  __WAYZ_POS_H__
#define  __WAYZ_POS_H__

#include <rtthread.h>
#include <wlan_mgnt.h>
#include <webclient.h>

#define  VER_H               1
#define  VER_M               0
#define  VER_L               0

#define  AP_MAX_NUMBER       100
#define  PRINT( buf, i )     buf[i + 0], buf[i + 1], buf[i + 2], buf[i + 3], buf[i + 4], buf[i + 5]
#define  MACPRINT            "%02X:%02X:%02X:%02X:%02X:%02X"
#define  MACPRINTID          "%02X%02X%02X%02X%02X%02X"
#define  MAC_LEN             (17)

#define  FREQ_START          2412 // 单位：M
#define  SEGMEMTATION        5    // 单位：M

#define  DEV_REGISTER_URL    "http://api.newayz.com/location/thing/v1/assets?access_key=%s"
#define  DEV_QUERY_URL       "http://api.newayz.com/location/thing/v1/assets/%s?access_key=%s"
#define  DEV_POSTION_URL     "http://api.newayz.com/location/hub/v1/track_points?access_key=%s"

#define  MAC_COUNT           6
#define  TIMESTAMP_DEL       (1605668910969) // ll
#define  NAME_LENGTH         32
#define  SN_LENGTH           32
#define  KEY_LENGTH          64

#define  STR_ERROR           "1"

#define  CELL_COUNT          32

#pragma pack(push)
#pragma pack(1)

enum
{
    WIFI_CONNECT_FAIL = 0,
    DEV_REGISTER_OK,
    DEV_REGISTER_FAIL
};

enum rd_type
{
    GSM = 0,
    WCDMA,
    LTE,
    CDMA
};

typedef struct _ap_struct_info_
{
    rt_uint8_t mac[MAC_COUNT];
    rt_int8_t rssi;
    rt_uint8_t channel;
    // rt_uint8_t ssid[33];
}tapStructInfo;

typedef struct _ap_info_
{
    tapStructInfo tinfoAp[AP_MAX_NUMBER];
    rt_uint8_t count;
    rt_uint8_t sta_mac[MAC_COUNT];
    // rt_int32_t length;
}tapinfo;

typedef struct _device_info_
{
    char *dev_name;
    char *manufacturer;
    char *SN;
    char *product;
    char *tenant;
}tdeviec_info;

typedef struct _wifi_info_
{
    char *ssid;
    char *passwd;
}twifi_info;

typedef struct _gcj02_
{
    double longitude;
    double latitude;
}tgcj02;

typedef struct _wgs84_
{
    double longitude;
    double latitude;
}twgs84;

typedef struct _point_info_
{
    tgcj02 gcj02;
    twgs84 wgs84;
}tpoint_info;

typedef struct _category_info_
{
    char id[NAME_LENGTH];
    char name[NAME_LENGTH];
}tcategory_info;


typedef struct _place_info_
{
    char id[NAME_LENGTH];
    char type[NAME_LENGTH];
    char name[NAME_LENGTH];
    tcategory_info category;
}tplace_info;

typedef struct _location_info_
{
    char timestamp[20];  // When to locate the result
    tpoint_info point;
    tplace_info place;
}tlocation_info;

typedef struct _gnss_unit_
{
    uint64_t timestamp;
    double lng; // longitude
    double lat; // latitude 
    float accuracy; // Horizontal accuracy of satellite positioning, unit: meter
}tgnss_unit;

typedef struct _cell_unit_
{
    uint64_t timestamp;
    uint32_t cell_id;
    char radio_type[7]; // Base station type, can only be the following values: GSM, WCDMA, LTE, CDMA
    uint32_t mcc; // mobileCountryCode
    uint32_t mnc; // mobileNetworkCode
    uint32_t lac; // locationAreaCode
}tcell_unit;

typedef struct _cell_
{
    tcell_unit cell[CELL_COUNT];
    uint8_t count;
}tcell;

typedef struct _post_data_
{
    tgnss_unit gnss;
    tcell cellulars;
}tpost_data;


#pragma pack(pop)

/**
 * Wifi parameter initialized
 * 
 * @param ssid Wifi name
 * 
 * @param passwd Wifi password
 * 
 * @return twifi_info: Dynamic allocation wifi info structure
*/
twifi_info *wifi_param_init(const char *ssid, const char *passwd);

/**
 * device parameter initialized
 * 
 * @param dev_name device name
 * 
 * @param manufacturer device manufacturer
 * 
 * @param product product name
 * 
 * @param SN product serial number
 * 
 * @param tenant tenant device
 * 
 * @return tdeviec_info: Dynamic allocation device info structure
*/
tdeviec_info *dev_para_init(const char *dev_name, const char *manufacturer, const char *product, \
                const char *SN, const char *tenant);

/**
 * Wifi station mac address
 * 
 * @param data Wifi station mac address data
 * 
 * @return void
*/
void get_sta_mac_addr(char *data);

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
char dev_register_init(twifi_info *wlan_info, tdeviec_info *dev_info, char *key);

/**
 * Get the positioning result function
 * 
 * @param wlan_info Wifi related information
 * 
 * @param key Visiting the website key
 * 
 * @param post_data post gnss and cellulars data, obtain positioning results
 * 
 * @param location get location result
 * 
 * @return >0: success
 *         =0: location failure
 * 
*/
char get_position_info(twifi_info *wlan_info, char *key, tpost_data *post_data, tlocation_info *location);

/**
 * print location result
 * 
 * @param location location info
 * 
 * @return void:
 * 
*/
void location_print(tlocation_info location);

#endif
