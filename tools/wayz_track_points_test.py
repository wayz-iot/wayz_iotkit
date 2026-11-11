#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
wayz_iotkit —— track_points 链路验证脚本

用法:
    export WAYZ_ACCESS_KEY=<在 lothub.newayz.com 申请的 access_key>
    python tools/wayz_track_points_test.py                 # 使用默认合成请求体
    python tools/wayz_track_points_test.py req.json        # 使用自定义请求体

行为:
    1. POST https://api.newayz.com/location/hub/v1/track_points?access_key=<KEY>
    2. 打印 HTTP 状态码与响应体
    3. 返回 200 时把响应保存到 docs/testdata/track_points_response.json 作为回归基线

注意: 每次执行都会向云端写入一条轨迹点记录。
"""

import json
import os
import ssl
import sys
import time
import urllib.error
import urllib.request

# Windows 控制台默认 GBK, 强制 UTF-8 输出避免中文乱码
try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_REQ = os.path.join(BASE_DIR, "docs", "testdata", "track_points_request_real.json")
SAVE_RESP = os.path.join(BASE_DIR, "docs", "testdata", "track_points_response.json")
# 与组件默认传输协议保持一致: http(无需 TLS 组件与证书)
URL = "http://api.newayz.com/location/hub/v1/track_points"

ERROR_HINTS = {
    400: "请求数据不被接受(no reference data provided)：合成 AP 不在云端定位库，"
         "请改用真实环境扫描到的 4 个以上 AP",
    401: "请求中没有 access_key",
    403: "access_key 无效或没有权限(Access key forbidden)",
    404: "场景不存在(scenario not found)",
    429: "触发限流(API rate limit)，请稍后重试",
}


def fail(msg):
    print("[FAIL] %s" % msg)
    sys.exit(1)


def main():
    key = os.environ.get("WAYZ_ACCESS_KEY", "").strip()
    if not key and len(sys.argv) > 2:
        key = sys.argv[2].strip()
    if not key:
        fail("缺少 access_key：请设置环境变量 WAYZ_ACCESS_KEY，或作为第二个参数传入")

    req_file = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_REQ
    if not os.path.exists(req_file):
        fail("请求体文件不存在: %s" % req_file)

    with open(req_file, "r", encoding="utf-8") as f:
        body = json.load(f)

    # 时间戳刷新为当前时间(UTC 毫秒)，其余字段保持不变
    now_ms = int(time.time() * 1000)
    body["timestamp"] = now_ms
    if "location" in body:
        body["location"]["timestamp"] = now_ms
        for wifi in body.get("location", {}).get("wifis", []):
            wifi["timestamp"] = now_ms

    payload = json.dumps(body, ensure_ascii=False).encode("utf-8")
    url = "%s?access_key=%s" % (URL, key)

    print("[INFO] POST %s" % URL)
    print("[INFO] request: %s" % req_file)

    req = urllib.request.Request(url, data=payload, method="POST")
    req.add_header("Content-Type", "application/json")
    req.add_header("Content-Length", str(len(payload)))

    ctx = ssl.create_default_context()
    try:
        with urllib.request.urlopen(req, timeout=20, context=ctx) as resp:
            status = resp.getcode()
            data = resp.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as e:
        status = e.code
        data = e.read().decode("utf-8", "replace")
    except urllib.error.URLError as e:
        fail("网络错误: %s" % e)
        return

    print("[INFO] HTTP %d" % status)
    try:
        print(json.dumps(json.loads(data), ensure_ascii=False, indent=2))
    except ValueError:
        print(data)

    if status == 200:
        with open(SAVE_RESP, "w", encoding="utf-8") as f:
            f.write(data)
        print("[OK] 链路打通，响应已保存到 %s" % SAVE_RESP)
        return

    print("[FAIL] HTTP %d" % status)
    if status in ERROR_HINTS:
        print("[HINT] %s" % ERROR_HINTS[status])
    sys.exit(1)


if __name__ == "__main__":
    main()
