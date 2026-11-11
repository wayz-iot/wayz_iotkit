#!/bin/sh
#
# wayz_iotkit 主机端测试台构建脚本
#
#   cd tools/host_test && ./build.sh
#
# 依赖: gcc (或 cc)、cJSON 源码
# cJSON 可从 RT-Thread 官方软件包仓库获取(MIT 许可):
#   https://github.com/RT-Thread-packages/cJSON
# 脚本会在 cJSON.c 不存在时尝试下载, 离线环境请手动放置。
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
OUT="$HERE/stub"

if [ ! -f "$OUT/cJSON.c" ] || [ ! -f "$OUT/cJSON.h" ]; then
    echo "[INFO] 正在获取 cJSON (RT-Thread-packages/cJSON, MIT) ..."
    curl -fsSL -o "$OUT/cJSON.c" "https://cdn.jsdelivr.net/gh/RT-Thread-packages/cJSON@v1.7.17/cJSON.c"
    curl -fsSL -o "$OUT/cJSON.h" "https://cdn.jsdelivr.net/gh/RT-Thread-packages/cJSON@v1.7.17/cJSON.h"
fi

${CC:-gcc} -Wall -Wextra -Wno-unused-parameter \
    -I "$OUT" -I "$ROOT/inc" -I "$ROOT/src" \
    "$HERE/test_wayz.c" "$OUT/cJSON.c" "$ROOT/src/wayz_coord.c" -o "$HERE/test_wayz"

echo "[OK] 已生成 $HERE/test_wayz, 运行: $HERE/test_wayz"
