# wayz_iotkit 主机端测试台

把组件本体编进 PC 运行，在没有开发板、没有网络的情况下验证：

- 上报请求体是否符合云端契约（字段名、格式、数量裁剪、排序）；
- 响应解析是否正确，**畸形响应是否被拒绝**；
- HTTP 链路（URL 拼接、请求头、状态码分级）；
- 内存是否有泄漏或越界。

## 使用

```sh
cd tools/host_test
./build.sh          # Linux / macOS / Git Bash
build.bat           # Windows
./test_wayz         # 运行
```

> **首次运行需要联网一次**：`build.sh` 会自动下载 cJSON（RT-Thread 官方软件包，MIT 许可）到
> `stub/cJSON.c`。该文件已在 `.gitignore` 中，不入库。
> 离线环境可以自行把 cJSON 源码放到 `stub/` 下（文件名 `cJSON.c` / `cJSON.h`）。

## 设计要点

这套测试台刻意避免了"桩比真实实现更宽松"导致的假阳性：

| 问题 | 做法 |
| ---- | ---- |
| 桩的版本语义与目标平台不一致 | 按 RT-Thread **5.x** 实现（`RTTHREAD_VERSION=50200`、`RT_NAME_MAX=12`），`rt_wlan_info` 与 5.x 对齐（`ssid` 是 `rt_wlan_ssid_t`、`channel` 是 `int16_t`） |
| 分配器与真实实现一致导致错配不可见 | **私有分配器**：magic 头 + 前后哨兵字节 + 记账，测试结束断言 `live allocations == 0`、`guard broken == 0` |
| HTTP 链路完全没被覆盖 | 实现**最小 webclient 桩**：记录请求 URL / 头 / 体，并返回预置响应，使 `get_position_info()` 全链路进入覆盖 |
| 断言太弱（只判范围） | **字段级精确比对**：时间戳位数、经纬度逐位、POI/地址字节数、`context` 条目内容 |
| 畸形输入没人测 | 12 组畸形响应用例，全部要求返回非 `RT_EOK`，且输出结构体必须被清零 |

## 覆盖的检查项

| 检查项 | 断言方式 |
| ------ | -------- |
| 响应解析 | 时间戳 13 位；经纬度逐位比对；`source` / `spatialReference` / `accuracy` / `place.type` / `place.distance` / 5 级行政区划逐项比对；POI 名与地址按字节数校验（防截断） |
| 畸形响应 | 12 组用例（缺 location / position 非对象 / 无 position / point 为空 / 缺经纬度 / 类型不符 / 越界 / (0,0) / 非法 JSON …）必须全部被拒绝，且输出结构体被清零 |
| 请求体 | `id` 26 字符且前缀正确；`asset.id` 与设备 MAC 一致；只上报 10 个 AP 且是**信号最强**的 10 个；`macAddress` 大写冒号格式；`frequency` 存在 |
| HTTP 链路 | URL 含 `access_key` 与 `response_sprf`；请求头含 `Content-Type: application/json` 与 `Content-Length`；请求体是合法 JSON 且含 `wifis` |
| 状态码分级 | 403→`WAYZ_ERR_AUTH`、429→`WAYZ_ERR_RATE_LIMIT`、500→`WAYZ_ERR_SERVER`、200 无 position→`WAYZ_ERR_NO_SIGNAL` |
| 内存 | 全部用例结束后 `live allocations == 0`，哨兵字节未被破坏 |

## 变异测试（验证断言本身有效）

`mutation_check.py` 会往组件源码里注入一个缺陷，看回归是否变红 —— 全部变异都应被抓住；
有逃逸就说明断言太弱。

```sh
python tools/host_test/mutation_check.py
```

当前覆盖 10 个变异体：id 截断、时间戳恒 0、MAC 去冒号、frequency 写死、字符串拷贝改成无界（堆溢出）、
云端没定位也算成功、AP 不裁剪、经纬度互换、AP 不按强度排序、状态码不分类。

> 该脚本会在 `C:/wz` 下建立临时副本（Windows 的链接器无法处理含中文的输出路径，
> 所以临时目录名用 ASCII）。
