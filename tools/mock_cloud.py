#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
wayz_iotkit 本地 mock 云

用途: 在没有开发板、也不希望向云端发数据的情况下, 验证组件的完整链路
      (组包 -> HTTP POST -> 解析 -> 打印), 用于端到端回归。

用法:
    python tools/mock_cloud.py [--port 8099] [--no-position]

然后让组件指向它(仿真模式):
    export WAYZ_TRACK_POINTS_URL=http://127.0.0.1:8099/location/hub/v1/track_points
    export WAYZ_ACCESS_KEY=LOCAL_TEST

行为:
    - 收到 POST 后把请求体打印出来(便于与官方契约逐字段核对)
    - 返回 200 + docs/testdata/track_points_response.json 里的真实响应
    - --no-position 则返回"云端未定位"的响应(只有时间戳, 没有 position)
"""

import argparse
import json
import os
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESP_FILE = os.path.join(BASE_DIR, "docs", "testdata", "track_points_response.json")

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass


def load_response(with_position=True):
    if not os.path.exists(RESP_FILE):
        return {"id": "mock", "asset": "mock", "location": {"timestamp": 0}}
    with open(RESP_FILE, "r", encoding="utf-8") as f:
        data = json.load(f)
    if not with_position:
        data = {
            "id": data.get("id", "mock"),
            "asset": data.get("asset", "mock"),
            "location": {"timestamp": data.get("location", {}).get("timestamp", 0)},
        }
    return data


class Handler(BaseHTTPRequestHandler):
    with_position = True

    def _reply(self, code, payload):
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(length) if length else b""

        print("\n===== POST %s =====" % self.path)
        print("Content-Type: %s" % self.headers.get("Content-Type"))
        try:
            print(json.dumps(json.loads(raw.decode("utf-8")), ensure_ascii=False, indent=2))
        except Exception as e:
            print("(raw body, %d bytes, parse failed: %s)" % (len(raw), e))
            print(raw[:512])
        sys.stdout.flush()

        self._reply(200, load_response(self.with_position))

    def do_GET(self):
        print("\n===== GET %s (not supported) =====" % self.path)
        sys.stdout.flush()
        self._reply(405, {"message": "only POST is supported by the mock"})

    def log_message(self, fmt, *args):
        pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8099)
    parser.add_argument("--no-position", action="store_true",
                        help="返回不带 position 的响应(模拟无线信号不足)")
    args = parser.parse_args()

    Handler.with_position = not args.no_position

    server = HTTPServer(("127.0.0.1", args.port), Handler)
    print("mock cloud listening on http://127.0.0.1:%d (position=%s)"
          % (args.port, Handler.with_position))
    print("point the component at: "
          "http://127.0.0.1:%d/location/hub/v1/track_points" % args.port)
    sys.stdout.flush()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
