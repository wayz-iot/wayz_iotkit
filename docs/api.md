# wayz_iotkit API

> **v1.1.0 起**：组件只与云端的 **track_points** 接口交互（设备注册由云端在收到第一个轨迹点时自动完成），
> 不再调用 `assets` 注册/查询接口；返回值统一为 `int`，成功 `RT_EOK`(0)、失败 `RT_ERROR`(-1)。
> 迁移说明见文末。

```c
/**
 * Wifi parameter initialized
 *
 * @param ssid: Wifi name
 *
 * @param passwd: Wifi password
 *
 * @param out: output the allocated wifi info structure, RT_NULL means failed
 *
 * @return =RT_EOK: success
 *         =RT_ERROR: failed
*/
int wifi_param_init(const char *ssid, const char *passwd, twifi_info **out);

/**
 * Release the wifi info structure
 *
 * @param wlan_info: wifi info, RT_NULL is allowed
 *
 * @return void
*/
void wifi_info_free(twifi_info *wlan_info);

/**
 * Wifi station mac address
 *
 * @param data: Wifi station mac address data
 * @note  the buffer must be at least MAC_LEN(18) bytes:
 *        "xx:xx:xx:xx:xx:xx" + '\0'
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
 * @param wlan_info: wifi name , wifi passwd
 *
 * @return =RT_EOK: connected
 *         =RT_ERROR: connect failed
 */
int wayz_connect(twifi_info *wlan_info);

/**
 * Get the positioning result function
 *
 * @param wlan_info Wifi related information
 *
 * @param key Visiting the website key
 *
 * @param post_data post gnss and cellulars data, obtain positioning results, RT_NULL means only wifi
 *
 * @param location get location result
 *
 * @return =RT_EOK: success
 *         =RT_ERROR: location failure
 *
*/
int get_position_info(twifi_info *wlan_info, const char *key, const tpost_data *post_data, tlocation_info *location);

/**
 * print location result
 *
 * @param location location info
 *
 * @return void:
 *
*/
void location_print(const tlocation_info *location);

/**
 * Set the timestamp provider
 *
 * The component does not carry an NTP client. By default the system clock
 * (gettimeofday) is used; this hook allows plugging in the RTC driver or a
 * platform specific NTP/SNTP component.
 *
 * @param provider the provider, RT_NULL restores the default strategy
 *
 * @return =RT_EOK: success
 *         =RT_ERROR: failed
*/
int wayz_set_time_provider(wayz_time_provider_t provider);

/**
 * Set the coordinate system returned by the cloud (response_sprf)
 *
 * The cloud supports gcj02 / wgs84 / bd09. The component does NOT convert
 * coordinates locally: the returned coordinates are exactly what the cloud
 * returns, and tlocation_info.spatial_reference tells which system it is.
 *
 * @param sprf one of WAYZ_SPRF_GCJ02 / WAYZ_SPRF_WGS84 / WAYZ_SPRF_BD09,
 *             RT_NULL restores the default(gcj02)
 *
 * @return =RT_EOK: success
 *         =RT_ERROR: failed
*/
int wayz_set_response_sprf(const char *sprf);

/**
 * Inject the wifi scan result, used by simulation and unit test.
 * The real hardware path keeps using rt_wlan_scan_sync().
 *
 * @param ap_info the scan result to inject, RT_NULL to restore the real scan
 *
 * @return =RT_EOK: success
 *         =RT_ERROR: failed
*/
int wayz_set_scan_result(const tapinfo *ap_info);
```

## 定位结果结构

```c
typedef struct _point_info_
{
    double longitude;   /* 云端返回的坐标(坐标系见 spatial_reference) */
    double latitude;
}tpoint_info;

typedef struct _location_info_
{
    char timestamp[20];       /* 定位结果时间戳, UTC 毫秒 */
    tpoint_info point;        /* 云端按 response_sprf 返回的坐标 */
    tplace_info place;        /* POI 名称/类型/距离 */
    char address[128];        /* address.name, 门牌级地址 */
    char source[16];          /* position.source: wifi / gnss / cell */
    char spatial_reference[8];/* position.spatialReference: gcj02 / wgs84 / bd09 */
    float accuracy;           /* position.accuracy, 单位: 米 */
    rt_uint8_t context_count;
    tcontext_info context[CONTEXT_COUNT]; /* Country/Province/City/District/Township */
}tlocation_info;
```

**坐标系由云端决定，组件不做本地换算**：

- 通过 `wayz_set_response_sprf(WAYZ_SPRF_GCJ02 / _WGS84 / _BD09)` 选择云端返回哪种坐标系
  （默认 `gcj02`），请求会带上 `?response_sprf=<坐标系>`；
- `point` 就是云端给出的坐标，`spatial_reference` 是云端回填的实际坐标系；
- 实测：同一组 AP，`response_sprf=wgs84` 返回 `31.127691 / 121.364539`，
  与组件原先自己做的 gcj02→wgs84 换算结果一致；
- 需要应用侧自行换算（例如把 GPS 的 WGS-84 转成 gcj02 显示在国内地图上）时，
  可直接使用独立工具模块 `inc/wayz_coord.h` / `src/wayz_coord.c`：
  `wayz_gcj02_to_wgs84()` / `wayz_wgs84_to_gcj02()`。

## 从 v1.0.x 迁移到 v1.1.0

| v1.0.x | v1.1.0 | 说明 |
| ------ | ------ | ---- |
| `twifi_info *wifi_param_init(ssid, pwd)` | `int wifi_param_init(ssid, pwd, &wlan_info)` | 改为出参，失败可判 |
| `tdeviec_info *dev_para_init(...)` | — | **已移除**：不再需要设备注册信息 |
| `dev_info_free(dev_info)` | — | **已移除** |
| `char dev_register_init(wlan_info, dev_info, key)` | `int wayz_connect(wlan_info)` | **语义变化**：只联网，不再注册设备（云端自动建档） |
| `char get_position_info(...)` | `int get_position_info(...)` | 成功 `RT_EOK`(0)、失败 `RT_ERROR`(-1) |
| `void location_print(tlocation_info location)` | `void location_print(const tlocation_info *location)` | 改为指针传参 |
| — | `int wayz_set_time_provider(...)` | 新增：接入 RTC / 平台 NTP 组件 |
| — | `int wayz_set_scan_result(...)` | 新增：仿真与单测注入扫描结果 |
| `point.gcj02` / `point.wgs84` 两套坐标 | `point.longitude` / `point.latitude` + `spatial_reference` | 坐标换算改由云端 `response_sprf` 参数完成，组件不再本地换算 |
| 内置 `ntp_client.c` | — | **已移除**：改用系统时钟或平台自带 NTP/SNTP |

```c
/* v1.0.x */
twifi_info *wlan_info = wifi_param_init(SSID, PWD);
tdeviec_info *dev_info = dev_para_init(NAME, VEND, PROD, SN, TEN);
char ret = dev_register_init(wlan_info, dev_info, ACCESS_KEY);
if (ret != DEV_REGISTER_OK) { /* handle */ }
ret = get_position_info(wlan_info, ACCESS_KEY, RT_NULL, &location);
if (RT_ERROR == ret) { /* handle */ }

/* v1.1.0 */
twifi_info *wlan_info = RT_NULL;
int ret;

if (RT_EOK != wifi_param_init(SSID, PWD, &wlan_info)) { /* handle */ }

ret = wayz_connect(wlan_info);
if (RT_EOK != ret) { /* handle */ }

ret = get_position_info(wlan_info, ACCESS_KEY, RT_NULL, &location);
if (RT_EOK != ret) { /* handle */ }

/* 退出前释放 */
wifi_info_free(wlan_info);
```
