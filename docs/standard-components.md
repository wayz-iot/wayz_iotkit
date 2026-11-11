# 可替换为成熟组件的自研实现盘点

本组件（wayz_iotkit）最初是一个"什么都自己写"的包：自带 NTP 客户端、自带坐标换算、
自带 JSON 处理方式。本次改造把其中的自研部分逐条对照 RT-Thread 生态做了取舍。

## 一、总览

| # | 原自研实现 | 原位置 | 可用的成熟组件 | 本次处置 |
|---|-------------|--------|----------------|----------|
| 1 | **NTP 客户端**：UDP socket + DNS 解析 + NTP 协议 + 时间戳换算 + RTC 同步 + `ctime/localtime` | `src/ntp_client.c`（2018 年的拷贝） | ① 平台的 NTP/SNTP 组件<br>② libc / lwIP 的 `gettimeofday()`<br>③ RT-Thread 的 RTC 设备 | ✅ **已删除**，改为"系统时钟 + 可插拔时间源" |
| 2 | **JSON 组包/解析** | `src/wayz_pos.c` | `cJSON`（RT-Thread-packages/cJSON） | ✅ 一直在用成熟组件（本轮补了 `cJSON_InitHooks` 统一分配器） |
| 3 | **HTTP/HTTPS 客户端** | `src/wayz_pos.c` | `webclient`（RT-Thread-packages/webclient） | ✅ 一直在用成熟组件（本轮做新旧 API 兼容层） |
| 4 | **TLS** | 无 | `mbedTLS`（RT-Thread-packages/mbedtls）/ `SAL`（主线 components/net/sal） | ⏸ **本项目实际部署用 HTTP，不引入 TLS 组件**；组件保留了 `PKG_WAYZ_IOTKIT_USE_HTTPS` 开关，将来启用时需自备根证书（见 [https-certs.md](https-certs.md)） |
| 5 | **gcj02 ↔ wgs84 坐标换算** | `src/wayz_pos.c` 内联 | RT-Thread 生态**没有**现成组件；社区有 coord-transform 等第三方库 | ⚠️ **定位链路不再需要它**：云端接口支持 `response_sprf` 参数(gcj02/wgs84/bd09)，由云端直接返回指定坐标系；自实现保留为独立工具模块 `src/wayz_coord.c` + `inc/wayz_coord.h`，供应用侧自行换算 |
| 6 | **设备注册（assets 接口）** | `src/wayz_pos.c` | 云端在收到第一个轨迹点时自动建档 | ✅ **已移除**，组件只调 track_points |
| 7 | **设备身份结构（tdeviec_info）与初始化** | `inc/wayz_pos.h` | — | ✅ **已移除**（随注册链路一起删掉） |
| 8 | MAC 字符串解析 | `samples/location_client.c` | 无（仅示例代码） | 保留在示例中 |
| 9 | 日历换算（Unix ↔ 年月日） | `src/ntp_client.c` | libc `localtime` / RTC 驱动 | ✅ 随 NTP 客户端一起移除 |

## 二、为什么删掉自带的 NTP 客户端

它带来的是一连串问题，而不是"少一个依赖"那么简单：

1. **与 libc 强耦合**：用了 `time_t` / `localtime` / `ctime` / `select` / `fd_set`。
   在 MSVC + `_CRT_NO_TIME_T` 的环境里**连编译都过不去**（`time_t` 整个消失）；
   在 RT-Thread 5.x 的新 libc 下同样脆弱。
2. **两份拷贝**：它本身就是 RT-Thread 老版本 ntp 包的源码拷贝，跟着走但不跟着更新，
   属于"既不成熟也不独立"的中间态 —— 比依赖官方组件更糟。
3. **职责错位**：组件是"定位客户端"，校时是系统级职责。RT-Thread 生态里
   校时通常由 NTP/SNTP 组件或 RTC + 平台策略完成。
4. **每次上报都做一次 DNS + UDP 往返**：定位请求里同步等 NTP，弱网下白等 1~5 秒。

现在的做法：

```c
/* 默认: 系统时钟 */
gettimeofday(&tv, RT_NULL);          /* lwIP / SAL / newlib 都提供 */

/* 需要精确校时: 接平台自己的 NTP/SNTP，或接 RTC */
int my_time_provider(rt_uint64_t *ms);
wayz_set_time_provider(my_time_provider);
```

## 三、可以进一步引入的成熟组件（建议）

| 组件 | 用途 | 引入方式 |
| ---- | ---- | -------- |
| `utest`（主线 `components/utilities/utest`） | 给 `wayz_coord.c` 的坐标换算、JSON 组包加单元测试 | `packages/utest` + `utest_cmd` |
| `mbedtls` | HTTPS 所需的 TLS 与根证书 | 见 [https-certs.md](https-certs.md) |
| `SAL` | 模组 AT 方案下由模组完成 TLS，MCU 侧零证书 | `RT_USING_SAL` + `SAL_USING_TLS` |
| `cJSON_Utils` | 若后续需要更复杂的 JSON 操作 | 已在 cJSON 包内 |

## 四、判断标准（本次沿用的取舍原则）

一个能力要不要自己写，取决于：

1. **生态里有没有公认组件？** 有就用（cJSON / webclient / mbedTLS）。
2. **自己写会不会比用组件更稳？** 不会就用（自带 NTP 拷贝 → 换成系统时钟 + 钩子）。
3. **没有现成组件时**：保持在**独立文件 + 纯函数 + 可单测**的形态，而不是内联在业务代码里
   （坐标换算 → `wayz_coord.c`），这样将来换库时只动一个文件。
