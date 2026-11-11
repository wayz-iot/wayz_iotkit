@echo off
rem wayz_iotkit 主机端测试台构建脚本 (Windows)
rem   cd tools\host_test && build.bat
setlocal

set HERE=%~dp0
set OUT=%HERE%stub
set ROOT=%HERE%..\..

if not exist "%OUT%\cJSON.c" (
    echo [INFO] 正在获取 cJSON (RT-Thread-packages\cJSON, MIT) ...
    curl -fsSL -o "%OUT%\cJSON.c" https://cdn.jsdelivr.net/gh/RT-Thread-packages/cJSON@v1.7.17/cJSON.c
    curl -fsSL -o "%OUT%\cJSON.h" https://cdn.jsdelivr.net/gh/RT-Thread-packages/cJSON@v1.7.17/cJSON.h
)

gcc -Wall -Wextra -Wno-unused-parameter ^
    -I "%OUT%" -I "%ROOT%\inc" -I "%ROOT%\src" ^
    "%HERE%test_wayz.c" "%OUT%\cJSON.c" -o "%HERE%test_wayz.exe"

if errorlevel 1 exit /b 1
echo [OK] 已生成 %HERE%test_wayz.exe, 运行: test_wayz.exe
endlocal
