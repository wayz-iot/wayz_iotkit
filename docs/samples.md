# wayz_iotkit 示例程序 #

| 示例程序路径                   | 说明 |
| ----                          | ---- |
| samples/location_client.c     | wayz_iotkit 测试例程 |

## 运行示例 ##

## 1、申请ACCESS_KEY

- 需要在平台申请ACCESS_KEY，目前只能提供人工申请的方式
在示例代码中的宏中填写申请的ACCESS_KEY字段，例如：
```c
#define  ACCESS_KEY     "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
```
申请入口：<https://lothub.newayz.com/>（开放平台，API 文档见 `pdf/api.pdf`）。

> 也可以不在源码里写死密钥，运行时用 finsh 命令设置：
> ```
> wayz_key <access_key>
> ```

## 2、填写wifi、设备相关信息
修改将所要连接网络的AP即用户名、密码等；也可以在选择组件时指定wifi相关信息。例如：
```c
#ifdef PKG_WAYZ_IOTKIT_WIFI_SSID
#define  WAYZ_WIFI_SSID      PKG_WAYZ_IOTKIT_WIFI_SSID
#else
#define  WAYZ_WIFI_SSID      "thread"
#endif

#ifdef PKG_WAYZ_IOTKIT_WIFI_PASSWORD
#define  WAYZ_WIFI_PWD      PKG_WAYZ_IOTKIT_WIFI_PASSWORD
#else
#define  WAYZ_WIFI_PWD      "12345678"
#endif
```

## 3、GNSS、基站数据的填充

```c
tpost_data post_data = {0};					// 定义上传数据结构体

post_data.gnss.timestamp = 1606729066000;
post_data.gnss.lng = 114.39583641301239;
post_data.gnss.lat = 30.51769862171484;
post_data.gnss.accuracy = 8;
post_data.gnss.altitude = 35.5;

post_data.cellulars.count = 1;
post_data.cellulars.cell[0].timestamp = 1515743846504;
post_data.cellulars.cell[0].cell_id = 149833211;
rt_snprintf(post_data.cellulars.cell[0].radio_type, sizeof(post_data.cellulars.cell[0].radio_type), "%s", "gsm");
post_data.cellulars.cell[0].mcc = 460;
post_data.cellulars.cell[0].mnc = 11;
post_data.cellulars.cell[0].lac = 36558;
post_data.cellulars.cell[0].signal_strength = 85;
post_data.cellulars.cell[0].connected = 1;

// 传入获取定位函数中
get_position_info(wlan_info, ACCESS_KEY, &post_data, &location);
```

## 4、在`finsh`命令行中输入`location_test`命令即可调用示例代码

## 5、仿真环境（没有 WiFi 硬件时）

先用 `wayz_scan_stub` 注入一组 AP 与设备 MAC，再执行 `location_test`：

```
wayz_dev_mac 02:00:00:00:00:01
wayz_scan_stub 88:25:93:6f:df:bb,-31,1 94:77:2b:65:1a:00,-43,6 \
              3c:1e:cc:1a:2b:3c,-55,11 a4:2b:8c:11:22:33,-67,6
wayz_key <access_key>
location_test
```

启用 `PKG_WAYZ_IOTKIT_SIM` 后，示例还支持无人值守运行：
- `WAYZ_ACCESS_KEY` 环境变量：读取访问密钥
- `WAYZ_SIM_APS` 环境变量：空格分隔的 `mac,rssi,channel` 列表，替代内置的合成 AP
- `WAYZ_TRACK_POINTS_URL` 环境变量：覆盖接口地址（可指向本地 mock 服务做离线回归）
- `WAYZ_SPRF` 环境变量：云端返回的坐标系（`gcj02` / `wgs84` / `bd09`，默认 `gcj02`）

```sh
WAYZ_ACCESS_KEY=<key> \
WAYZ_SIM_APS="4c:ef:56:08:35:8d,-73 4c:ef:56:00:35:8d,-74 00:19:70:a2:66:5d,-79" \
./application.exe
```

### 用本地 mock 云做离线端到端回归（不产生任何外发流量）

```sh
# 终端 1: 启动 mock 云(回放 docs/testdata/track_points_response.json 里的真实响应)
python tools/mock_cloud.py --port 8099

# 终端 2: 让组件指向它
WAYZ_TRACK_POINTS_URL="http://127.0.0.1:8099/location/hub/v1/track_points" \
WAYZ_ACCESS_KEY="LOCAL_TEST" \
./application.exe
```

预期输出（组件在 RT-Thread 内完整跑通 组包 → POST → 解析 → 打印）：

```
-------------------location result-------------------------------
timestamp: 1794370462195 ms
gcj02:
	latitude:31.125781
	longitude:121.369155
wgs84:
	latitude:31.127691314782
	longitude:121.364538624076
accuracy: 50 m
source: wifi, spatialReference: gcj02
address: 上海市闵行区莘庄镇秀文路600号2楼
context: Country = 中国 (CN)
...
POI: {"id": , "type": Entity, "name": 上海市闵行区医疗保险事务中心}
-------------------location result end---------------------------
```

mock 云会同时把组件发出的请求体打印出来，可与官方契约逐字段核对：

```json
{
  "timestamp": 1794384732294,
  "id": "38efe26e-bcd8-020000000001",
  "asset": { "id": "020000000001" },
  "location": {
    "timestamp": 1794384732294,
    "wifis": [
      { "macAddress": "88:25:93:6F:DF:BB", "timestamp": 1794384732294,
        "signalStrength": 31, "channel": 1, "frequency": 2412, "connected": false }
    ]
  }
}
```

加 `--no-position` 可以回放"云端未定位"的响应（只有时间戳、没有 `position`），用来验证错误分支。

## 示例结果 ##
```c
-------------------location result-------------------------------
timestamp: 1606293694990 ms
point(gcj02):
	latitude:31.125781
	longitude:121.369155
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
