# wayz_iotkit

## 1、介绍

wayz_iotkit 是上海图趣信息科技有限公司，针对RT-thread开发的能够实现定位功能的物联网组件。在使用传统上定位，如 GPS 等信号的同时，也支持使用基站、Wifi等数据，通过 WAYZ 定位云来进行定位。当前版本已实现支持wifi、gnss、基站定位，其中wifi定位为自动获取定位数据，gnss和基站需要开发者手动填写相关信息，后期会逐步实现对其他信号源的自动采集，进一步提升定位效果。

### 1.1 目录结构

| 名称 | 说明 |
| ---- | ---- |
| docs  | 文档目录 |
| examples | 例子目录，并有相应的一些说明 |
| inc  | 头文件目录 |
| src  | 源代码目录 |

### 1.2 许可证

wayz_iotkit package 遵循 LGPLv2.1 许可，详见 `LICENSE` 文件。

### 1.3 依赖

- RT-Thread 3.0+
- RT-Thread 开发 WLAN 框架
- webclient软件包
- cJSON软件包

## 2、如何打开 wayz_iotkit

使用 wayz_iotkit package 需要在 RT-Thread 的包管理器中选择它，具体路径如下：

```
RT-Thread online packages
    IOT - internet of things --->
        [*] wayz_iotkit: wayz iot location
            (wayz123) wifi ssid
            (wayz1234) wifi password
            Version (v1.0.0)   --->
```

- 然后让 RT-Thread 的包管理器自动更新，或者使用 `pkgs --update` 命令更新包到 BSP 中。

## 3、使用 wayz_iotkit

- 如何从零开始使用，请参考 [用户手册](docs/user-guide.md)
- 完整的 API 文档，请参考 [API 手册](docs/api.md)
- 详细的示例介绍，请参考 [示例文档](docs/samples.md) 

## 4、注意事项

- 该组件是基于wlan框架上运用的wifi定位，前提条件必须要有wifi芯片
- 利用该组件时，需要重新开启任务处理，防止内存过小引发堆栈溢出错误

## 5、联系方式 & 感谢

* 维护：jianxiong.ye
* 主页：https://github.com/wayz-iot/wayz_iotkit
