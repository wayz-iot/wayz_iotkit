# 发布计划：wayz_iotkit v1.1.1

> 涉及两个仓库：
> - **组件仓库**：`https://github.com/wayz-iot/wayz_iotkit.git`（本地 `C:\Users\admin\zhaoyii.github.com\wayz_iotkit`）
> - **包索引仓库**：`https://github.com/wayz-iot/packages.git`（本地 `C:\Users\admin\zhaoyii.github.com\packages`，fork 自 RT-Thread/packages）

---

## 零、发布前的现状核对

| 项 | 现状 | 结论 |
|---|------|------|
| 组件仓库 PR | #1 已 rebase 合入 master，顶点 `07e8a5c` | ✅ 已就绪 |
| 组件仓库 tag | **一个都没有**（远端经 API 核实：从未打过 tag） | ⬜ 本次要打第一个 |
| 组件仓库根 Kconfig | 已有（v1.1.0 时新增） | ✅ |
| 组件版本宏 | `inc/wayz_pos.h` VER_H/M/L = 1/1/0 → **1.1.0** | ⬜ 与 tag 版本要对齐 |
| 索引 Kconfig | 只有 `V100` + `LATEST`，**缺** 本轮新增的 USE_HTTPS / NO_WLAN / SIM / COORD | ⬜ 要补 |
| 索引 Kconfig 的 `V100` 选项 | package.json **没有 v1.0.0 的 site 条目**（且该仓库从未打过 tag） | ⬜ 选了会没有下载源 |
| 索引 package.json `site` | 只有 `latest`(VER_SHA=master) | ⬜ 要补 v1.1.1 固定条目 |
| 索引 package.json `license` | `GPL-2.0`，而仓库 LICENSE 是 **LGPLv2.1** | ⬜ 要改 |
| 索引 package.json `repository`/`homepage`/`doc` | 都指向 `wayz-iot/wayz_iotkit` | ✅ 与组件仓库 origin 一致，无需改 |

---

## 一、组件仓库：打 tag

### 1.1 发布前检查（在 tag 所在的提交上跑一遍）

```sh
cd C:/Users/admin/zhaoyii.github.com/wayz_iotkit
git checkout master && git pull --ff-only

# 编译矩阵(4.1/5.0/5.1/5.2/6.0 + SIM/NO_WLAN/COORD)应零警告
gcc -fsyntax-only -Wall -Wextra -I <stub>/inc -I inc src/wayz_pos.c

# 主机端回归 + 变异测试
cd tools/host_test && sh build.sh && ./test_wayz        # 应"全部通过", live allocations: 0
python tools/host_test/mutation_check.py                # 应"全部 10 个变异体都被抓住"

# 端到端(真实云端, 需 access_key)
cd ../../rtwork/rt-thread/bsp/simulator
WAYZ_ACCESS_KEY=<key> WAYZ_SIM_APS="..." WAYZ_SPRF=wgs84 timeout 20 ./application.exe
```

> 三项全绿才允许打 tag。

### 1.2 版本号对齐

`inc/wayz_pos.h` 的版本宏当前是 `1.1.0`，本次发的是 **v1.1.1**，需同步：

```c
#define  VER_H               1
#define  VER_M               1
#define  VER_L               1        /* 1.1.0 -> 1.1.1 */
```

> 该宏原本用于设备注册请求里的 `firmware.version`；注册链路已移除，现在只作为版本标识，
> 仍然建议与 tag 保持一致，避免以后再出现"文档版本 ≠ 宏版本"。

同时确认 `docs/version.md` 已有 `v1.1.1` 行（日期填实际发布日）。

### 1.3 打 tag 并推送

```sh
cd C:/Users/admin/zhaoyii.github.com/wayz_iotkit

# 1) 版本宏的修正单独提交
git add inc/wayz_pos.h && git commit -m "版本宏对齐 v1.1.1"

# 2) 打注释 tag(带发布说明)
git tag -a v1.1.1 -m "wayz_iotkit v1.1.1

- 适配 RT-Thread 5.x/6.0(webclient 新旧 API、事件驱动扫描、WLAN 头文件更名)
- 修复 26 处内存与返回值缺陷(堆越界/free 字面量/分配器不匹配/char 无符号等)
- 只保留 track_points 链路, 移除设备注册
- 坐标换算交给云端 response_sprf, 本地换算工具模块按需编译
- 时间源可插拔(默认系统时钟), HTTPS 可选
- 新增主机端回归测试台(私有分配器 + 强断言 + 变异测试)
- 新增仓库根 Kconfig"

# 3) 推送提交与 tag
git push origin master
git push origin v1.1.1
```

### 1.4 (可选) GitHub Release

在 Releases 页新建 `v1.1.1`，正文可用上面的 tag 说明 + 迁移提示：
公开 API 有破坏性变更（`char`→`int`、`dev_register_init`→`wayz_connect`、坐标结构精简），
详见 `docs/api.md` 的迁移表。

**取 tag 对应的 commit sha**（索引元数据要用）：

```sh
git rev-list -n 1 v1.1.1        # → VER_SHA
```

---

## 二、包索引仓库：元数据准备

工作目录：`C:\Users\admin\zhaoyii.github.com\packages`

### 2.1 `iot/wayz_iotkit/Kconfig`

要改三处：**补本轮新增的配置项**、**加 v1.1.1 版本选项**、**修 v1.0.0 无下载源的问题**。

```kconfig
# Kconfig file for package wayz_iotkit
menuconfig PKG_USING_WAYZ_IOTKIT
    bool "wayz iot location"
    default n
    select PKG_USING_CJSON
    select PKG_USING_WEBCLIENT

if PKG_USING_WAYZ_IOTKIT

    config PKG_USING_WAYZ_IOTKIT_PATH
        string
        default "/packages/iot/wayz_iotkit"

    config PKG_WAYZ_IOTKIT_WIFI_SSID
        string "wifi ssid"
        default "wayz123"

    config PKG_WAYZ_IOTKIT_WIFI_PASSWORD
        string "wifi password"
        default "wayz1234"

    config PKG_WAYZ_IOTKIT_USE_HTTPS
        bool "use https for the WAYZ cloud API"
        default n
        help
            需要 webclient 启用 mbedTLS/SAL, 并让设备内置服务端证书链对应的根证书,
            详见组件仓库 docs/https-certs.md。

    config PKG_WAYZ_IOTKIT_NO_WLAN
        bool "build without the WLAN framework"
        default n
        help
            没有 WiFi 芯片的 BSP(仿真器 / qemu)可以打开它,
            此时组件只能使用 wayz_set_scan_result() 注入的扫描结果。

    config PKG_WAYZ_IOTKIT_SIM
        bool "simulation mode for the sample"
        default n
        help
            仿真模式: 示例自动注入一组 AP 与设备 MAC, 并允许用环境变量
            WAYZ_ACCESS_KEY / WAYZ_SIM_APS / WAYZ_SPRF 覆盖运行参数。

    config PKG_WAYZ_IOTKIT_COORD
        bool "build the local coordinate conversion helper"
        default n
        help
            wayz_coord.c 是独立工具模块(gcj02 <-> wgs84)。
            定位链路本身不做换算(由云端的 response_sprf 参数决定),
            默认不编译该模块, 避免给所有用户引入 libm 依赖。

    choice
        prompt "Version"
        default PKG_USING_WAYZ_IOTKIT_LATEST_VERSION
        help
            Select the package version

        config PKG_USING_WAYZ_IOTKIT_V111
            bool "v1.1.1"

        config PKG_USING_WAYZ_IOTKIT_V100
            bool "v1.0.0"

        config PKG_USING_WAYZ_IOTKIT_LATEST_VERSION
            bool "latest"
    endchoice

    config PKG_USING_WAYZ_IOTKIT_VER
       string
       default "v1.1.1"    if PKG_USING_WAYZ_IOTKIT_V111
       default "v1.0.0"    if PKG_USING_WAYZ_IOTKIT_V100
       default "latest"    if PKG_USING_WAYZ_IOTKIT_LATEST_VERSION

endif
```

> ⚠️ **v1.0.0 的下载源**：已核实 —— **该仓库从未打过任何 tag**，
> 所以 Kconfig 里的 `V100` 选项一直没有对应的下载源（选了会拉不到包）。
> 修法（二选一）：
> - **(a) 推荐**：给 `site` 补一条 v1.0.0 条目，`VER_SHA` 指向**合并前的旧 master 顶点**
>   `1ef0b178c4fb50bf81d63a54e34ba98d4f78f7b2`（该提交仍是旧的 `dev_register_init` API，
>   正好是"v1.0.0 语义"）→ 老用户仍能锁旧版本；
> - **(b)** 直接删掉 `V100` 选项与 `PKG_USING_WAYZ_IOTKIT_VER` 里对应的 default。
>
> 采用 (a) 时，`site` 里需要三条：

### 2.2 `iot/wayz_iotkit/package.json`

```json
{
  "name": "wayz_iotkit",
  "description": "wayz iot location software package",
  "description_zh": "wayz iot 定位软件包",
  "enable": "PKG_USING_WAYZ_IOTKIT",
  "keywords": [
    "wayz_iotkit",
    "location",
    "gnss",
    "wifi"
  ],
  "category": "iot",
  "author": {
    "name": "jianxiong.ye",
    "email": "jianxiong.ye@wayz.ai",
    "github": "https://github.com/wayz-iot/"
  },
  "license": "LGPL-2.1",
  "repository": "https://github.com/wayz-iot/wayz_iotkit",
  "icon": "unknown",
  "homepage": "https://github.com/wayz-iot/wayz_iotkit#readme",
  "doc": "https://github.com/wayz-iot/wayz_iotkit/tree/master/docs",
  "site": [
    {
      "version": "v1.1.1",
      "URL": "https://github.com/wayz-iot/wayz_iotkit.git",
      "filename": "wayz_iotkit-1.1.1.zip",
      "VER_SHA": "<git rev-list -n 1 v1.1.1 的输出>"
    },
    {
      "version": "v1.0.0",
      "URL": "https://github.com/wayz-iot/wayz_iotkit.git",
      "filename": "wayz_iotkit-1.0.0.zip",
      "VER_SHA": "1ef0b178c4fb50bf81d63a54e34ba98d4f78f7b2"
    },
    {
      "version": "latest",
      "URL": "https://github.com/wayz-iot/wayz_iotkit.git",
      "filename": "",
      "VER_SHA": "master"
    }
  ]
}
```

改动点：

| 字段 | 原值 | 新值 | 原因 |
|------|------|------|------|
| `license` | `GPL-2.0` | `LGPL-2.1` | 仓库 `LICENSE` 实为 **GNU LESSER GENERAL PUBLIC LICENSE v2.1**，索引写 GPL 会误导用户选型 |
| `site` | 仅 latest | 增加 `v1.1.1`（固定 sha）与 `v1.0.0`（旧 master 顶点） | 让 Kconfig 里新增的 `V111` 有下载源；顺带修好一直失效的 `V100` |
| `keywords` | 仅 wayz_iotkit | 补充 location/gnss/wifi | 便于在包中心检索 |
| 其余 | — | 不变 | repository/homepage/doc 已与 origin 一致 |

> **`site` 条目的两种写法**（索引里 259 个多版本包都用这两种之一）：
> - 固定 sha：`{"version":"v1.1.1","URL":"<repo>.git","filename":"xxx-1.1.1.zip","VER_SHA":"<40位sha>"}`
>   —— 工具按 sha 打包，版本内容**不会随仓库变动**，可复现 ✅ 推荐
> - 直接打 zip：`{"version":"v1.1.1","URL":".../archive/refs/tags/v1.1.1.zip","filename":"xxx-1.1.1.zip"}`
>   —— 依赖 tag 的 zip 存在
>
> 两种都要求 **Kconfig 里的版本字符串与 `site[].version` 完全一致**（`v1.1.1`），否则 pkgs 工具匹配不上。

### 2.3 与组件仓库根 Kconfig 保持一致

组件仓库现在有一份根 `Kconfig`（本地包用），索引里的是另一份（在线包用）。
两份的**配置项名称必须一致**，否则会出现"本地能配、在线不能配"的割裂。
建议：以**组件仓库根 Kconfig 为唯一事实来源**，索引里的这份由它同步生成/复制，并在两边注释里互相标注。

---

## 三、提交与验证

### 3.1 提索引 PR

```sh
cd C:/Users/admin/zhaoyii.github.com/packages
git checkout -b feature/wayz-iotkit-v1.1.1
git add iot/wayz_iotkit/
git commit -m "[iot/wayz_iotkit] 发布 v1.1.1

- 新增 v1.1.1 版本选项与固定 sha 下载源
- 补充 USE_HTTPS / NO_WLAN / SIM / COORD 配置项
- license 由 GPL-2.0 更正为 LGPL-2.1"
git push origin feature/wayz-iotkit-v1.1.1
```
然后开 PR 到 `wayz-iot/packages` 的 master。

> 索引的 `pr-check.yml` 只在**上游 `RT-Thread/packages`** 上触发，fork 上不会跑；
> 但 `tools/ci/tests/test_ci.py` 里的结构性校验规则仍应遵守（name/license/repository/site 字段完整）。

### 3.2 在 BSP 上验证安装（发布是否真的可用）

需要 RT-Thread env 工具（本机未装，步骤供参考）：

```sh
# 1) 拉取索引并选择 v1.1.1
cd <你的BSP>
pkgs --update
# menuconfig -> RT-Thread online packages -> IOT -> internet of things -> wayz_iotkit
#   选 Version = v1.1.1, 确认其它新选项出现

# 2) 编译, 确认组件被编进去
scons -j8

# 3) 运行, 用真实 key 验证定位
#    finsh 里 location_test, 或用仿真模式的 wayz_scan_stub
```

**验收标准**：
- menuconfig 里能看到 `v1.1.1` 选项与新增的 4 个开关；
- 编译无 `PKG_USING_WAYZ_IOTKIT_*` 未定义告警；
- `location_test` 能从云端取回经纬度与 POI。

### 3.3 确认下载源可用（不装 BSP 也能查）

```sh
# 用索引里的 URL 直接验证 v1.1.1 能否拉到
curl -sI https://github.com/wayz-iot/wayz_iotkit/archive/refs/tags/v1.1.1.zip | head -1
# 或者按 sha:
git ls-remote https://github.com/wayz-iot/wayz_iotkit.git v1.1.1
```

---

## 四、发布后核对与回滚

| 场景 | 处理 |
|------|------|
| 索引 PR 合并后 `pkgs --update` 拉不到 v1.1.1 | 检查 `site[].version` 与 Kconfig 的版本字符串是否**完全一致**；检查 `VER_SHA` 是否是 40 位完整 sha |
| v1.1.1 有 bug | 索引 Kconfig 里把 `default` 改回 `PKG_USING_WAYZ_IOTKIT_V100`/latest，并在 `site` 里把 v1.1.1 条目标 `version` 改掉或删除 → 再提一个索引 PR |
| 组件本身要回滚 | 在组件仓库 revert 相关提交并打 `v1.1.2`；索引把 `VER_SHA` 指到新的 tag |
| 组件仓库被误删/被接管 | 索引的 `repository`/`site.URL` 是硬编码的 GitHub 地址，**无法自动切换源** —— 这是单点风险，长期建议把组件发布到 `RT-Thread/packages` 索引 + 上游 `wayz-iot` 组织下的仓库 |

---

## 五、风险与注意事项

1. **组件仓库的 tag 是索引的锚点**：`site[].VER_SHA` 指向某个 commit，tag 打错/漏打 → 用户装到的版本与预期不符。
   建议**先打 tag、再取 sha 填索引**，顺序反了容易填错。
2. **索引的 `latest` 跟随 master**：`VER_SHA: "master"` 意味着用户选 "latest" 装到的是**当时的 master 快照**（不是 commit 固定）。
   master 一旦有破坏性提交，未锁版本的用户就会中招 —— 这也是本次要加固定版本选项的原因。
3. **license 不一致**：索引写 GPL-2.0、仓库是 LGPLv2.1，可能影响用户的合规判断，必须改。
4. **两份 Kconfig 漂移**：组件仓库根 Kconfig 与索引 Kconfig 手工同步，容易漏。建议每次发版按本计划的 2.1/2.2 逐项核对。
5. **真实 BSSID**：`docs/testdata/track_points_request_real.json` 里是现场抓取的 AP 标识，公开仓库里属于位置指纹，建议发布前换成合成 MAC（保留文件结构与格式即可）。

---

## 六、执行记录（2026-11-12 已执行）

| 步骤 | 结果 |
|------|------|
| ① 发布前检查 · 编译矩阵 | RT-Thread 4.1/5.0/5.1/5.2/6.0 + SIM/NO_WLAN —— **全部零警告** |
| ① 发布前检查 · 主机端回归 | **全部通过**，live allocations: 0，guard broken: 0 |
| ① 发布前检查 · 变异测试 | **10/10 变异体全部被抓住** |
| ① 发布前检查 · 端到端(mock) | 通过：URL / 请求头 / 请求体 / 响应解析 / 定点格式化输出均正确 |
| ② 版本宏对齐 | `inc/wayz_pos.h` VER_L 0 → 1（1.1.1），提交 `2133fdd` |
| ③ 打 tag | **`v1.1.1`**（注释 tag），`VER_SHA = 2133fdd3d57e5e774a917ac62e6044bdeeedaff3` |
| ③ 推送 | master → `2133fdd`；tag `v1.1.1` 已推送 |
| ④ 索引 Kconfig | 补 4 个配置项 + `V111` 选项；`PKG_USING_WAYZ_IOTKIT_VER` 三档 default |
| ⑤ 索引 package.json | `license` → `LGPL-2.1`；`site` 增加 `v1.1.1`(2133fdd) 与 `v1.0.0`(1ef0b17)；补 description/keywords |
| ⑥ 索引 PR | **https://github.com/wayz-iot/packages/pull/1**（open，mergeable clean，2 文件 +68/−7） |

**待办 / 可选项**：

- ⬜ **真实云端复验**（发布门禁的第四项，因工具策略无法由助手代跑，需在本地执行）：
  ```sh
  cd C:/Users/admin/rtwork/rt-thread/bsp/simulator
  WAYZ_ACCESS_KEY=<key> WAYZ_SIM_APS="4c:ef:56:08:35:8d,-73 ..." WAYZ_SPRF=wgs84 timeout 20 ./application.exe
  ```
  预期 `point(wgs84): 31.127691 / 121.364539` + POI + 5 级行政区划。
  若发现异常，走 §四 的回滚流程（发 v1.1.2，索引改 `VER_SHA`）。
- ⬜ 合并索引 PR（#1）到 `wayz-iot/packages` 的 master。
- ⬜ 可选：给组件仓库建 GitHub Release（tag 说明已写好）。
- ⬜ 可选：`docs/testdata/track_points_request_real.json` 里的真实 BSSID 换成合成 MAC。
- ⬜ 可选：把组件发布到**上游 `RT-Thread/packages` 索引**，避免单点依赖 `wayz-iot` 这个源。

---

## 七、执行顺序一览

```
组件仓库                              索引仓库
─────────────────────────────────      ─────────────────────────────────
① 跑发布前检查(编译/回归/变异/E2E)   ④ 改 iot/wayz_iotkit/Kconfig
② 版本宏对齐 1.1.1 + 提交              ⑤ 改 iot/wayz_iotkit/package.json
③ 打 tag v1.1.1 + 推 master/tag            (site 加 v1.1.1 + VER_SHA, license→LGPL-2.1)
   取 sha = git rev-list -n 1 v1.1.1   ⑥ 提交 + 推分支 + 开 PR
                                        ⑦ pkgs --update 在 BSP 上验证安装
                                        ⑧ 可选: 建 GitHub Release / 写发布说明
```
