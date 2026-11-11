# 版本和修订 #

| Date       | Version   |  Author       | Note  |
| --------   | :-----:   | :----         | :---- |
| 2020-11-28 | v1.0.0    | jianxiong.ye  | 初始版本 |
| 2026-11-11 | v1.1.0    | -             | 适配 RT-Thread 5.x/6.0：① 修复 16 处内存与返回值缺陷（堆越界、free 字面量、分配器不匹配、char 无符号导致失败检测失效等）；② webclient 新旧 API 兼容、HTTPS 支持；③ 扫描改为事件驱动（5.0 起无 rt_wlan_scan_sync）；④ 只保留 track_points 接口，移除设备注册链路；⑤ 移除自带 NTP 客户端，时间源改为可插拔；⑥ 坐标换算抽为 wayz_coord.c；⑦ 新增扫描结果注入接口与主机端回归测试台 |
