# wayz_iotkit

## 1、介绍

wayz_iotkit 是上海图趣信息科技有限公司，针对RT-thread开发的能够实现定位功能的物联网组件。在使用传统上定位，如 GPS 等信号的同时，也支持使用基站、Wifi等数据，通过 WAYZ 定位云来进行定位。当前版本已实现支持wifi、gnss、基站定位，其中wifi定位为自动获取定位数据，gnss和基站需要开发者手动填写相关信息，后期会逐步实现对其他信号源的自动采集，进一步提升定位效果。

组件只与云端的 **track_points** 接口交互，云端在收到第一个轨迹点时会自动创建设备。

### 1.1 目录结构

| 名称 | 说明 |
| ---- | ---- |
| docs  | 文档目录 |
| samples  | 例子目录，并有相应的一些说明 |
| inc  | 头文件目录 |
| src  | 源代码目录 |
| tools  | 主机端测试台与云端联调脚本 |

### 1.2 许可证

wayz_iotkit package 遵循 LGPLv2.1 许可，详见 `LICENSE` 文件。

### 1.3 依赖

- RT-Thread 4.1.0+（含 5.x / master，WLAN 头文件 `wlan_mgnt.h` 与 `dev_wlan_mgnt.h` 自动兼容）
- RT-Thread 开发 WLAN 框架
- webclient软件包（2.0 及以上，2.2.0 前后的 session API 自动兼容）
- cJSON软件包
- mbedTLS 软件包（**仅在启用 HTTPS 时需要**，证书见 [docs/https-certs.md](docs/https-certs.md)）

### 1.4 版本说明

- **v1.1.0 起公开 API 返回 `int`**：成功 `RT_EOK`(0)、失败 `RT_ERROR`(-1)；
  `dev_register_init`/`dev_para_init` 已移除，改为 `wayz_connect()`；
  `wifi_param_init` 改为出参形式；新增 `wayz_set_time_provider()`（时间源）与
  `wayz_set_scan_result()`（仿真/单测注入扫描结果）。迁移示例见 [API 手册](docs/api.md)。
- **默认使用 `http://`**：不需要任何 TLS 组件，也不需要设备内置根证书。
  需要 https 时在 BSP 的 `rtconfig.h` 中定义 `PKG_WAYZ_IOTKIT_USE_HTTPS`，
  并按 [HTTPS 与证书说明](docs/https-certs.md) 准备证书。
- **坐标换算交给云端**：请求带 `?response_sprf=`，可用 `wayz_set_response_sprf()`
  选择 `gcj02` / `wgs84` / `bd09`，组件不做本地换算；
  需要自行换算时可用独立工具模块 `wayz_coord.h`（`wayz_gcj02_to_wgs84()` / `wayz_wgs84_to_gcj02()`）。
- 组件不再自带 NTP 客户端，时间戳取自系统时钟，或通过 `wayz_set_time_provider()` 接入
  RTC / 平台自带的 NTP-SNTP 组件。

## 2、如何打开 wayz_iotkit

使用 wayz_iotkit package 需要在 RT-Thread 的包管理器中选择它，具体路径如下：

```
RT-Thread online packages
    IOT - internet of things --->
        [*] wayz iotkit: wayz iot location
            (wayz123) wifi ssid
            (wayz1234) wifi password
            Version (latest)   --->
```

- 然后让 RT-Thread 的包管理器自动更新，或者使用 `pkgs --update` 命令更新包到 BSP 中。

## 3、使用 wayz_iotkit

- 如何从零开始使用，请参考 [用户手册](docs/user-guide.md)
- 完整的 API 文档，请参考 [API 手册](docs/api.md)
- 详细的示例介绍，请参考 [示例文档](docs/samples.md)
- HTTPS 与证书，请参考 [HTTPS 与证书说明](docs/https-certs.md)
- 自研实现与成熟组件的取舍，请参考 [标准组件替换盘点](docs/standard-components.md)

## 3.1、配套工具（PC 端，无需开发板）

| 工具 | 说明 |
| ---- | ---- |
| `tools/host_test/` | 主机端测试台：把组件本体编进 PC，验证上报请求体与响应解析是否符合云端契约 |
| `tools/mock_cloud.py` | 本地 mock 云：回放真实响应并记录组件发出的请求体，可做**零外发**的端到端回归 |
| `tools/wayz_track_points_test.py` | 直接对 WAYZ 定位云做一次 track_points 验证（需要 access_key） |
| `docs/testdata/` | 请求体 / 真实响应样例，作为离线回归基线 |

```sh
cd tools/host_test && ./build.sh && ./test_wayz          # 离线回归
python tools/mock_cloud.py --port 8099                   # 本地 mock 云
export WAYZ_ACCESS_KEY=<你的key>
python tools/wayz_track_points_test.py                   # 连云验证
```

## 4、注意事项

- 该组件是基于wlan框架上运用的wifi定位，前提条件必须要有wifi芯片；无 WiFi 硬件的仿真环境可用 `wayz_set_scan_result()` 注入扫描结果
- 利用该组件时，需要重新开启任务处理，防止内存过小引发堆栈溢出错误（示例线程栈为 12KB，接入 mbedTLS 时请勿调小）
- 云端接口规定 `access_key` 放在 URL 的 query 参数中，HTTPS 只能保证传输过程不裸奔

## 5、联系方式 & 感谢

* 维护：jianxiong.ye
* 主页：https://github.com/wayz-iot/wayz_iotkit
