# wayz_iotkit 主机端测试台

在 PC 上直接编译组件本体，验证：

1. **定位上报请求体**是否符合 WAYZ 官方 API 契约（字段名、类型、数量裁剪）
2. **响应解析**是否能正确处理云端返回的结构（经纬度、POI、地址、行政区划、坐标系）

不需要开发板、不需要联网、不需要 RT-Thread 环境。

## 使用

```sh
cd tools/host_test
./build.sh          # Linux / macOS / Git Bash
# 或 Windows:
build.bat
./test_wayz         # 运行
```

Windows 下也可以直接用 gcc 命令：

```sh
gcc -Wall -Wextra -Wno-unused-parameter \
    -I stub -I ../../inc -I ../../src \
    test_wayz.c stub/cJSON.c -o test_wayz.exe
```

## 说明

- `stub/` 下是 RT-Thread / webclient / WLAN 的**最小桩头文件**与实现，只为让组件能在主机上编译运行，
  不参与固件构建（固件走 `src/SConscript`）。
- `test_wayz.c` 通过 `#include "wayz_pos.c"` 把组件本体编进来，以便访问其内部静态函数
  （`point_cJson_handler` / `parse_point_cJson_handler`）。
- `cJSON.c` / `cJSON.h` 来自 RT-Thread 官方软件包仓库（MIT 许可）：
  <https://github.com/RT-Thread-packages/cJSON>，`build.sh` 会自动下载。
- 响应样例取自 WAYZ 官方 API 文档 `https://lothub.newayz.com/pdf/api.pdf`，
  保存在 `docs/testdata/track_points_response.sample.json`，可用真实响应覆盖后做回归。

## 覆盖的检查项

| 检查项 | 说明 |
| ------ | ---- |
| 请求体字段 | `timestamp` / `id` / `asset` / `location` / `wifis` / `macAddress` / `signalStrength` / `frequency` / `channel` / `connected` |
| id 完整性 | `id` = `38efe26e-bcd8-<MAC12>`（26 字符，不能被截断） |
| asset 一致性 | `asset.id` 与设备注册时的 id 形式一致（无冒号 MAC） |
| AP 数量 | 云端要求 4~10 个，注入 20 个时应只上报 10 个 |
| 经纬度 | 与官方样例逐位比对 |
| 坐标系 | `spatialReference` 为 `wgs84` 时不重复换算，为 `gcj02` 时做 gcj02→wgs84 换算 |
| POI / 地址 | `place.name` / `place.type` / `place.distance` / `address.name` / `address.context[]` |
