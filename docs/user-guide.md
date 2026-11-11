# 使用指南

这里主要介绍 wayz_iotkit 程序的基本使用流程，并针对使用过程中经常涉及到的结构体和重要 API 进行简要说明。

wayz_iotkit 的基本工作流程如下所示：

- 初始化 wifi 参数
- 连接网络
- 上报扫描到的 wifi 信息（可选 GNSS/基站数据），取回定位结果(gcj02和wgs84标准)以及POI信息

> 组件只与云端的 **track_points** 接口交互。云端在收到第一个轨迹点时会自动创建设备，
> 因此**不需要**再调用设备注册接口。

## menuconfig 配置说明

获取 wayz_iotkit 软件包或者修改用户配置都需要使用 `menuconfig`。需要用户打开 ENV 工具，并将目录切换到您所用的 BSP 目录，使用 `menuconfig` 命令打开配置界面。

在 `RT-Thread online packages → IOT - internet of things` 中选择 **wayz_iotkit** 软件包，操作界面如下图所示：

详细的配置介绍如下所示：

```shell
RT-Thread online packages
    IOT - internet of things --->
        [*] wayz iotkit: wayz iot location   # 打开 wayz_iotkit 软件包
            (wayz123) wifi ssid              # wifi 名字
            (wayz1234) wifi password         # wifi 密码
            Version (latest)   --->          # 选择软件包版本，默认为最新版本
```

选择合适的配置项后，使用 `pkgs --update` 命令下载软件包并更新用户配置。

- 注：wifi 名字和密码 供软件包切换模式用

### 依赖组件

| 组件 | 是否必需 | 说明 |
| ---- | -------- | ---- |
| WLAN 框架 | 必需 | 需要有 WiFi 芯片；仿真环境可用 `wayz_set_scan_result()` 注入扫描结果绕过 |
| webclient | 必需 | HTTP(S) 客户端 |
| cJSON | 必需 | JSON 组包/解包 |
| mbedTLS | **仅启用 HTTPS 时必需** | 证书配置见 [HTTPS 与证书说明](https-certs.md) |

> **传输协议**：组件**默认使用 `http://`** —— 不依赖 TLS 组件，也不需要设备内置根证书。
> 需要 https 时，在 BSP 的 `rtconfig.h` 里定义 `PKG_WAYZ_IOTKIT_USE_HTTPS`，
> 并按 [HTTPS 与证书说明](https-certs.md) 准备证书（RT-Thread 的 mbedTLS 移植默认强制校验证书）。
>
> `access_key` 由云端接口规定放在 URL 的 query 参数里：即使启用 https，密钥也仍会出现在 URL 中
> （只是传输过程被加密）。
> ⚠️ 启用 https 时, webclient 在 TLS 连接失败时会把完整 URI(含 access_key) 打进串口日志,
> 请勿外传串口日志, 或给 webclient 打个日志脱敏补丁。

### 作为本地包使用

在线包索引会生成 `PKG_USING_WAYZ_IOTKIT` 等配置项；如果把本仓库直接拷贝到 BSP 的
`packages/` 目录下使用，需要自己在 `packages/Kconfig` 中加入：

```kconfig
menuconfig PKG_USING_WAYZ_IOTKIT
    bool "wayz iotkit: WAYZ location cloud client"
    default n
    select PKG_USING_WEBCLIENT
    select PKG_USING_CJSON
```

## 工作原理

设备中wifi模块获取周围环境中的wifi信息，组包通过http方式上传到WAYZ定位云平台，云平台进行分析后将经纬度等其他信息一并返回到软件包，软件包通过处理可以得到gcj02和wgs84标准的经纬度，和POI信息。也可以通过传入gnss、基站等相关数据获取定位结果及其POI信息

## wifi 参数初始化
```c
typedef struct _wifi_info_              // wifi 相关信息
{
    char *ssid;                         // 保存wifi名称
    char *passwd;                       // 保存wifi密码
}twifi_info;
```

`twifi_info` 用于保存建立连接的 wifi相关信息，在设备上传`周围wifi信息`时联网使用：

```c
twifi_info *wlan_info = RT_NULL;
if (RT_EOK != wifi_param_init(WIFI_SSID, WIFI_PWD, &wlan_info))
{
    /* 初始化失败 */
}
```

用完记得调用 `wifi_info_free(wlan_info)` 释放。

## 连接网络

```c
if (RT_EOK != wayz_connect(wlan_info))
{
    rt_kprintf("connect to the internet failure.\n");
}
```

`get_position_info()` 内部也会做一次连接检查（已连接则直接复用），所以如果只想定位一次，
可以不单独调用 `wayz_connect()`。

## 填充GNSS、基站信息定位
```c
typedef struct _gnss_unit_
{
    uint64_t timestamp;         // 数据收集的时间戳（UTC 时间，单位：毫秒）
    double lng;                 // 经度
    double lat;                 // 纬度
    float accuracy;             // 卫星定位水平精度，单位：米
    float altitude;             // 海拔，单位：米
}tgnss_unit;

typedef struct _cell_unit_
{
    uint64_t timestamp;         // 数据收集的时间戳（UTC 时间，单位：毫秒）
    uint32_t cell_id;           // 小区 ID，当 CDMA 时，为 BID（Base Station ID）
    char radio_type[7];         // 基站类型，只能是以下值：gsm, wcdma, lte, cdma
    uint32_t mcc;               // mobileCountryCode：MCC 码
    uint32_t mnc;               // mobileNetworkCode：当 CDMA 时，为 SID（System ID）码
    uint32_t lac;               // locationAreaCode：当 CDMA 时，为 NID（Network ID）；
                                                     当 LTE 时，为 TAC（Tracking Area code）
    rt_int32_t signal_strength; // 信号强度，单位：dBm
    rt_uint8_t connected;        // 是否为当前驻留的小区
}tcell_unit;
```
通过填充GNSS、基站等数据，传入定位接口即可获取定位结果信息


## 获取定位结果

应用程序使用`get_position_info`函数从平台端获取位置信息。**其中ACCESS_KEY需要在平台上申请**
`location_print`函数是打印位置相关信息

示例代码如下所示：
```c
tlocation_info location = {0};
ret = get_position_info(wlan_info, ACCESS_KEY, RT_NULL, &location);
if (RT_EOK != ret)
{
    rt_kprintf("\033[31;22mthe device failed to obtain latitude and longitude information.\033[0m\n");
}
else
{
    location_print(&location);
}
```
- 其中`get_position_info`函数第三个参数为填充的`GNSS和基站数据`，相关操作可以参照[示例文档](samples.md)

**打印位置信息结果**
```c
-------------------location result-------------------------------
timestamp: 1606293694990 ms
gcj02:
	latitude:30.515105
	longitude:114.401555
wgs84:
	latitude:30.517407914397
	longitude:114.396014616712
accuracy: 30 m
source: wifi, spatialReference: gcj02
address: 上海市闵行区莘庄镇秀文路600号2楼
context: Country = 中国 (CN)
context: Province = 上海市 (310000)
context: City = 上海市 (310100)
context: District = 闵行区 (310112)
context: Township = 莘庄镇 (310112101)
POI: {"id": , "type": Entity, "name": 上海市闵行区医疗保险事务中心}
-------------------location result end---------------------------
```

## 坐标系

云端支持 `gcj02` / `wgs84` / `bd09` 三种返回格式，由请求参数 `response_sprf` 指定，
**组件不做本地坐标换算**，拿到的就是云端给出的坐标：

```c
/* 默认 gcj02 */
wayz_set_response_sprf(WAYZ_SPRF_WGS84);   /* 之后云端直接返回 WGS-84 */
```

结果里的 `location->point.longitude/latitude` 是云端返回的坐标，
`location->spatial_reference` 是云端回填的实际坐标系。

需要应用侧自己换算时（例如把 GPS 的 WGS-84 转成 gcj02 显示在国内地图上），
可用独立工具模块 `inc/wayz_coord.h`：

```c
double gcj_lat, gcj_lng;
wayz_wgs84_to_gcj02(wgs_lat, wgs_lng, &gcj_lat, &gcj_lng);
```

## 时间源

组件**不再自带 NTP 客户端**，上报用的时间戳按以下顺序获取：

1. `wayz_set_time_provider()` 注册的提供者；
2. 系统时钟 `gettimeofday()`（RT-Thread 下它读的是 RTC 设备；没有 RTC、或 RTC 还没校过时直接失败）；
3. 都没有时用"基准时间 + rt_tick 增量"兜底，保证时间戳单调递增。

> ⚠️ **第 3 步的兜底基准是一个固定值（2020-12-16）**，只是"看起来合法"，不是真实时间。
> 没有 RTC 的设备请务必用 `wayz_set_time_provider()` 接入真实时间源
> （平台自带的 NTP/SNTP 组件，或自己联网同步），否则云端收到的时间戳是错的。

需要精确校时的设备，请用平台自带的 NTP/SNTP 组件校准系统时钟，或注册一个自己的时间源：

```c
static int my_time_provider(rt_uint64_t *timestamp_ms)
{
    /* 例如从 RTC 设备读日历时间再换算成 Unix 时间戳 */
    *timestamp_ms = my_rtc_epoch_ms();
    return RT_EOK;
}

wayz_set_time_provider(my_time_provider);
```

## 定位轮询频次

该引用可以通过循环的模式来设置定位频次，**其中POINT_FRQ为定位频次的设置**

示例代码如下所示：
```c
while (1)
{
    ret = get_position_info(wlan_info, ACCESS_KEY, RT_NULL, &location);
    if (RT_EOK != ret)
    {
        rt_kprintf("\033[31;22mthe device failed to obtain latitude and longitude information.\033[0m\n");
    }
    else
    {
        location_print(&location);
    }
    rt_thread_mdelay(POINT_FRQ);
    rt_memset(&location, 0, sizeof (location));
}
```

## 已知限制与注意事项

- **WLAN 事件回调是全局单槽**：RT-Thread 的 `rt_wlan_register_event_handler()` 每个事件类型只保存一个回调，
  组件在扫描期间会占用并随后清除 `RT_WLAN_EVT_SCAN_REPORT` / `SCAN_DONE` / `READY` / `STA_DISCONNECTED` 槽位。
  因此**不要**同时使用 finsh 的 `wifi scan` 命令，也不要注册自己的同名回调 —— 它们会互相顶掉。
  该限制来自 RT-Thread 框架设计（已建议上游改为支持多回调）。
- **轮询频率**：云端配额为 日 3000 次 / 秒 5 次，示例的 3 秒轮询一天约 2.9 万次会超限。
  量产建议放到 30 秒以上，并根据 `get_position_info()` 的返回值退避
  （`WAYZ_ERR_RATE_LIMIT` 退避重试，`WAYZ_ERR_AUTH` 直接停止重试）。
- **线程安全**：`get_position_info()` 内部已串行化，并发调用安全；但不要在组件回调里再调用它。
- **扫描结果由驱动决定**：组件会上报信号最强的 10 个 AP；云端要求 4~10 个，不足 4 个时会上报失败
  （`WAYZ_ERR_NO_SIGNAL`）。

## 无 WiFi 硬件时的仿真

仿真环境（`bsp/qemu-vexpress-a9`、`bsp/simulator`）没有 WiFi 芯片，
`rt_wlan_scan_sync()` 无法工作。可以使用示例提供的 `wayz_scan_stub` 命令注入一组
AP，从而在不依赖硬件的情况下驱动完整的定位流程：

```
wayz_scan_stub 88:25:93:6f:df:bb,-31,1 94:77:2b:65:1a:00,-43,6 \
              3c:1e:cc:1a:2b:3c,-55,11 a4:2b:8c:11:22:33,-67,6
wayz_dev_mac 02:00:00:00:00:01    # 无 WiFi 芯片时设置设备 MAC
wayz_key <access_key>              # 运行时设置密钥, 避免写死在源码里
wayz_scan_stub off                 # 恢复真实扫描
```

参数格式为 `mac,rssi,channel`，云端要求一次上报 4~10 个 WiFi。

也可以在代码中直接调用：

```c
tapinfo ap = {0};
/* 填写 ap.tinfoAp[] 与 ap.count 后 */
wayz_set_scan_result(&ap);
```
