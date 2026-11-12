#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
回归测试有效性检查(变异测试)

往组件源码里注入一个缺陷, 看主机端回归是否会变红。
全部变异都应被抓住; 有逃逸就说明断言太弱。

用法:
    python tools/host_test/mutation_check.py
"""

import io
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))

# (名称, 文件, 原文本, 变异文本)
MUTATIONS = [
    ("id 前缀被截短", "src/wayz_pos.c",
     '"38efe26e-bcd8-"', '"38efe26e-bcd"'),
    ("timestamp 恒为 0", "src/wayz_pos.c",
     "wayz_itoa64(location->timestamp, sizeof(location->timestamp), (rt_int64_t)ts);",
     "wayz_itoa64(location->timestamp, sizeof(location->timestamp), (rt_int64_t)0);"),
    ("macAddress 去掉冒号", "src/wayz_pos.c",
     '""MACPRINT, PRINT(ap->mac, 0)', '""MACPRINTID, PRINT(ap->mac, 0)'),
    ("frequency 写死", "src/wayz_pos.c",
     "cJSON_AddNumberToObject(fld, \"frequency\", freq);",
     "cJSON_AddNumberToObject(fld, \"frequency\", 9999);"),
    ("字符串拷贝改成无界(堆溢出)", "src/wayz_pos.c",
     'rt_snprintf(dst, dst_size, "%s", item->valuestring);',
     'rt_memcpy(dst, item->valuestring, rt_strlen(item->valuestring) + 1);'),
    ("云端没定位也算成功", "src/wayz_pos.c",
     "        wayz_error(\"the cloud did not return a position, maybe the wifi signal is not enough.\\r\\n\");\n"
     "        rc = WAYZ_ERR_NO_SIGNAL;",
     "        wayz_error(\"the cloud did not return a position, maybe the wifi signal is not enough.\\r\\n\");\n"
     "        rc = RT_EOK;"),
    ("AP 数量不裁剪", "src/wayz_pos.c",
     "wifi_count = (n > AP_MAX_UPLOAD) ? AP_MAX_UPLOAD : n;",
     "wifi_count = n;"),
    ("经纬度互换", "src/wayz_pos.c",
     "        location->point.longitude = lng;\n        location->point.latitude = lat;",
     "        location->point.longitude = lat;\n        location->point.latitude = lng;"),
    ("AP 不按信号强度排序", "src/wayz_pos.c",
     "while ((j > 0) && ((int)aucApInfo.tinfoAp[order[j - 1]].rssi < key_rssi))",
     "while (0)"),
    ("状态码不分类", "src/wayz_pos.c",
     "        return WAYZ_ERR_AUTH;",
     "        return WAYZ_ERR_NET;"),
]


def run(cmd, cwd):
    # 显式用 utf-8: 构建输出含中文, 用系统默认的 GBK 解码会抛异常
    return subprocess.run(cmd, cwd=cwd, capture_output=True,
                          encoding="utf-8", errors="replace")


def ascii_safe(text):
    """控制台可能是 GBK, 统一转成可打印 ASCII"""
    return "".join(c if 32 <= ord(c) < 127 else "?" for c in text)


def main():
    # 用一个较短的临时根目录: Windows 下链接器对输出路径长度有限制
    base = os.environ.get("WAYZ_MUT_TMP", "C:/wz")
    if not os.path.isdir(base):
        os.makedirs(base, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix="m", dir=base)
    escaped = []
    print("变异测试: 每个缺陷都应被回归测试抓住\n")
    print("%-32s %s" % ("变异", "结果"))
    print("-" * 60)

    for idx, (name, rel, old, new) in enumerate(MUTATIONS):
        # 目录名必须用 ASCII: MinGW 的链接器无法处理含中文的输出路径
        work = os.path.join(tmp, "m%02d" % idx)
        shutil.copytree(ROOT, work, ignore=shutil.ignore_patterns(".git"))

        path = os.path.join(work, rel)
        with io.open(path, "r", encoding="utf-8", newline="") as f:
            raw = f.read()
        # 源码可能是 CRLF, 统一按 LF 匹配, 写回时还原
        crlf = "\r\n" in raw
        src = raw.replace("\r\n", "\n")
        old_n = old.replace("\r\n", "\n")
        new_n = new.replace("\r\n", "\n")
        if old_n not in src:
            print("%-32s SKIP (未找到锚点)" % name)
            continue
        out = src.replace(old_n, new_n, 1)
        if crlf:
            out = out.replace("\n", "\r\n")
        with io.open(path, "w", encoding="utf-8", newline="") as f:
            f.write(out)

        b = run(["sh", "build.sh"], os.path.join(work, "tools", "host_test"))
        if b.returncode != 0:
            print("%-32s BUILD-FAIL" % name)
            tail = ((b.stdout or "") + (b.stderr or "")).strip().splitlines()
            for ln in tail[-6:]:
                print("    " + ascii_safe(ln[:220]))
            continue
        t = run([os.path.join(work, "tools", "host_test", "test_wayz")], work)
        if t.returncode != 0:
            print("%-32s OK (测试变红)" % name)
        else:
            print("%-32s ESCAPED (测试仍然绿!)" % name)
            escaped.append(name)

    shutil.rmtree(tmp, ignore_errors=True)
    print("-" * 60)
    if escaped:
        print("逃逸 %d 个: %s" % (len(escaped), ", ".join(escaped)))
        return 1

    print("全部 %d 个变异体都被抓住" % len(MUTATIONS))
    return 0


if __name__ == "__main__":
    sys.exit(main())
