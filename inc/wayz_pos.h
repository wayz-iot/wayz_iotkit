
#ifndef  __WAYZ_POS_H__
#define  __WAYZ_POS_H__

#include <rtthread.h>

/*
 * 说明: 本头文件刻意不包含 <wlan_mgnt.h> / <dev_wlan_mgnt.h> / <webclient.h>。
 *   1) 没有 WiFi 芯片的 BSP(仿真器、qemu)也能编译本组件;
 *   2) RT-Thread 5.2.0 起 WLAN 头文件更名, 不让公共头依赖具体的头文件名。
 * WLAN 与 webclient 的依赖只在 src/wayz_pos.c 内部按需引入。
 */

#define  VER_H               1
#define  VER_M               1
#define  VER_L               1        /* v1.1.1 */

#define  AP_MAX_NUMBER       100
#define  AP_MIN_UPLOAD       4       /* 云端要求至少 4 个 WiFi */
#define  AP_MAX_UPLOAD       10      /* 云端要求最多 10 个 WiFi */
#define  PRINT( buf, i )     buf[i + 0], buf[i + 1], buf[i + 2], buf[i + 3], buf[i + 4], buf[i + 5]
#define  MACPRINT            "%02X:%02X:%02X:%02X:%02X:%02X"
#define  MACPRINTID          "%02X%02X%02X%02X%02X%02X"
#define  MAC_LEN             (18)    /* "xx:xx:xx:xx:xx:xx" + '\0' */

#define  FREQ_START          2412    /* 单位：MHz */
#define  SEGMEMTATION        5       /* 单位：MHz */

#define  MAC_COUNT           6
#define  TIMESTAMP_DEL       (1605668910969LL) /* 无系统时钟时的兜底时间戳(UTC 毫秒) */

/**
 * 组件返回码
 *
 * 成功返回 RT_EOK(0), 失败返回下列负值。
 * WAYZ_ERR_PARAM 与 RT_ERROR 同值(-1), 因此原有的 `!= RT_EOK` 判断依然有效。
 */
#define  WAYZ_ERR_PARAM        (-1)  /* RT_ERROR: 参数非法 */
#define  WAYZ_ERR_NET          (-2)  /* 网络 / DNS / TLS 失败 */
#define  WAYZ_ERR_AUTH         (-3)  /* 云端 401/403: access_key 无效或无权限(不可重试) */
#define  WAYZ_ERR_RATE_LIMIT   (-4)  /* 云端 429: 触发限流, 应退避后重试 */
#define  WAYZ_ERR_SERVER       (-5)  /* 云端 5xx: 服务端异常, 可重试 */
#define  WAYZ_ERR_NO_SIGNAL    (-6)  /* 云端返回 200 但没有定位结果(无线信号不足) */
#define  WAYZ_ERR_PARSE        (-7)  /* 响应格式异常 */
#define  WAYZ_ERR_INTERNAL     (-8)  /* 内部错误(内存不足等) */
#define  NAME_LENGTH         32
#define  LONG_NAME_LENGTH    64      /* POI 名 / 行政区划名: 中文 UTF-8 一个字 3 字节 */
#define  SN_LENGTH           32
#define  KEY_LENGTH          64
#define  CELL_COUNT          32
#define  CONTEXT_COUNT       8
#define  ADDRESS_LENGTH      128

/*
 * 传输协议选择:
 *
 * 默认使用 http —— 不依赖任何 TLS 组件, 也不需要设备内置根证书, 开箱即用。
 * 需要 https 时定义 PKG_WAYZ_IOTKIT_USE_HTTPS, 并自行准备证书:
 *   - 内置根证书(推荐, 约 1.5KB), 或
 *   - 给 mbedtls 包打补丁关闭证书校验(不推荐, 需要维护 fork)。
 * 详见 docs/https-certs.md。
 *
 * 注意: access_key 由云端接口规定放在 URL 的 query 参数中,
 *       即使启用 https, 密钥也会出现在 URL 里(仅传输过程被加密)。
 */
#if defined(PKG_WAYZ_IOTKIT_USE_HTTPS)
#  define WAYZ_USE_HTTPS     1
#else
#  define WAYZ_USE_HTTPS     0
#endif

/*
 * 接口基址(不含 access_key)。密钥由 wayz_pos.c 统一以
 * "?access_key=<key>" 拼接到基址后面, 因此这里不能再带 %s。
 */
#if WAYZ_USE_HTTPS
#  undef  DEV_POSTION_URL
#  define DEV_POSTION_URL     "https://api.newayz.com/location/hub/v1/track_points"
#else
#  undef  DEV_POSTION_URL
#  define DEV_POSTION_URL     "http://api.newayz.com/location/hub/v1/track_points"
#endif

#if WAYZ_USE_HTTPS && !defined(WEBCLIENT_USING_MBED_TLS) && !defined(WEBCLIENT_USING_SAL_TLS)
#warning "wayz_iotkit: HTTPS is enabled but no TLS component (mbedTLS/SAL) found, the connection will fail."
#endif

/*
 * 设备注册链路已在 v1.1.0 移除(云端在收到首个轨迹点时自动建档),
 * 下面两个枚举仅为兼容旧代码保留, 不要再使用。
 * 注意: WIFI_CONNECT_FAIL == 0 与 RT_EOK 语义相反, 沿用它会得到相反结论。
 */
enum
{
    WIFI_CONNECT_FAIL = 0,   /* @deprecated 已废弃 */
    DEV_REGISTER_OK,         /* @deprecated 已废弃 */
    DEV_REGISTER_FAIL        /* @deprecated 已废弃 */
};

typedef struct _ap_struct_info_
{
    rt_uint8_t mac[MAC_COUNT];
    rt_int8_t rssi;
    rt_uint8_t channel;
}tapStructInfo;

typedef struct _ap_info_
{
    tapStructInfo tinfoAp[AP_MAX_NUMBER];
    rt_uint8_t count;
    rt_uint8_t sta_mac[MAC_COUNT];
}tapinfo;

typedef struct _wifi_info_
{
    char *ssid;
    char *passwd;
}twifi_info;

/**
 * 定位坐标
 *
 * 坐标系由云端按请求参数 response_sprf 决定(见 wayz_set_response_sprf),
 * 组件不做本地坐标换算, 返回值即云端给出的坐标, 坐标系见
 * tlocation_info.spatial_reference。
 */
typedef struct _point_info_
{
    double longitude;
    double latitude;
}tpoint_info;

typedef struct _category_info_
{
    char id[LONG_NAME_LENGTH];
    char name[LONG_NAME_LENGTH];
}tcategory_info;

typedef struct _place_info_
{
    char id[LONG_NAME_LENGTH];
    char type[NAME_LENGTH];
    char name[LONG_NAME_LENGTH];
    tcategory_info category;
    double distance;      /* place.distance.line, 单位: 米 */
}tplace_info;

/* 行政区划上下文: Country / Province / City / District / Township */
typedef struct _context_info_
{
    char type[16];
    char name[LONG_NAME_LENGTH];
    char code[16];
}tcontext_info;

typedef struct _location_info_
{
    char timestamp[20];       /* 定位结果时间戳, UTC 毫秒 */
    tpoint_info point;
    tplace_info place;
    char address[ADDRESS_LENGTH];  /* address.name, 门牌级地址 */
    char source[16];              /* position.source: wifi / gnss / cell */
    char spatial_reference[8];    /* position.spatialReference: gcj02 / wgs84 / bd09 */
    float accuracy;               /* position.accuracy, 单位: 米 */
    rt_uint8_t context_count;
    tcontext_info context[CONTEXT_COUNT];
}tlocation_info;

typedef struct _gnss_unit_
{
    uint64_t timestamp;
    double lng;               /* longitude */
    double lat;               /* latitude */
    float accuracy;           /* 卫星定位水平精度, 单位: 米 */
    float altitude;           /* 海拔, 单位: 米 */
}tgnss_unit;

typedef struct _cell_unit_
{
    uint64_t timestamp;
    uint32_t cell_id;
    char radio_type[7];       /* GSM / WCDMA / LTE / CDMA */
    uint32_t mcc;             /* mobileCountryCode */
    uint32_t mnc;             /* mobileNetworkCode */
    uint32_t lac;             /* locationAreaCode */
    rt_int32_t signal_strength;  /* dBm */
    rt_uint8_t connected;        /* 是否为当前驻留的小区 */
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

/**
 * 云端返回的坐标系(response_sprf)
 *
 * 云端支持 gcj02 / wgs84 / bd09 三种, 由请求参数 response_sprf 指定,
 * 组件不做本地坐标换算, 返回值即云端给出的坐标。
 */
#define  WAYZ_SPRF_GCJ02     "gcj02"
#define  WAYZ_SPRF_WGS84     "wgs84"
#define  WAYZ_SPRF_BD09      "bd09"

/**
 * 时间戳提供者: 返回毫秒级 UTC 时间戳
 *
 * 组件不再自带 NTP 客户端(那是平台/SDK 的职责), 默认按以下顺序取时间:
 *   1) 用户通过 wayz_set_time_provider() 注册的提供者;
 *   2) RT-Thread 的 RTC 设备(节点名 "rtc");
 *   3) 系统的 gettimeofday();
 *   4) 都没有时用"基准时间 + rt_tick 增量"保证时间戳单调递增。
 *
 * @param timestamp_ms 输出: UTC 毫秒时间戳
 *
 * @return RT_EOK: success
 *         RT_ERROR: failed
 */
typedef int (*wayz_time_provider_t)(rt_uint64_t *timestamp_ms);

/**
 * 设置时间戳提供者
 *
 * @param provider 提供者, 传 RT_NULL 恢复默认策略
 *
 * @return RT_EOK: success
 *         RT_ERROR: failed
 */
int wayz_set_time_provider(wayz_time_provider_t provider);

/**
 * Set the coordinate system returned by the cloud (response_sprf)
 *
 * @param sprf one of WAYZ_SPRF_GCJ02 / WAYZ_SPRF_WGS84 / WAYZ_SPRF_BD09,
 *             RT_NULL restores the default(gcj02)
 *
 * @return =RT_EOK: success
 *         =RT_ERROR: failed
*/
int wayz_set_response_sprf(const char *sprf);

/**
 * Wifi parameter initialized
 *
 * @param ssid Wifi name
 * @param passwd Wifi password
 * @param out  output the allocated wifi info structure
 *
 * @return RT_EOK: success
 *         RT_ERROR: failed
*/
int wifi_param_init(const char *ssid, const char *passwd, twifi_info **out);

/**
 * Release the wifi info structure
 *
 * @param wlan_info wifi info, RT_NULL is allowed
 *
 * @return void
*/
void wifi_info_free(twifi_info *wlan_info);

/**
 * Wifi station mac address
 *
 * @param data Wifi station mac address data, the buffer must be at least MAC_LEN(18) bytes
 *
 * @return void
*/
void get_sta_mac_addr(char *data);

/**
 * Connect the device to the Internet
 *
 * The WAYZ cloud creates the asset automatically when the first track point is
 * uploaded, so no device registration request is needed.
 *
 * @param wlan_info wifi name , wifi passwd
 *
 * @return =RT_EOK: connected
 *         =RT_ERROR: connect failed
 */
int wayz_connect(twifi_info *wlan_info);

/**
 * Get the positioning result function
 *
 * @param wlan_info Wifi related information
 * @param key Visiting the website key
 * @param post_data post gnss and cellulars data, obtain positioning results, RT_NULL means only wifi
 * @param location get location result
 *
 * @return RT_EOK: success
 *         RT_ERROR: location failure
 */
int get_position_info(twifi_info *wlan_info, const char *key, const tpost_data *post_data, \
                      tlocation_info *location);

/**
 * print location result
 *
 * @param location location info
 *
 * @return void
*/
void location_print(const tlocation_info *location);

/**
 * Inject the wifi scan result, used by simulation and unit test.
 * The real hardware path keeps using rt_wlan_scan_sync().
 *
 * @param ap_info the scan result to inject, RT_NULL to restore the real scan
 *
 * @return RT_EOK: success
 *         RT_ERROR: failed
*/
int wayz_set_scan_result(const tapinfo *ap_info);

#endif
