/*
 * wayz_iotkit 主机端测试台(不属于仓库, 仅用于本地验证)
 *
 * 目的:
 *   1) 打印组件实际生成的定位上报请求体, 与官方 API 契约逐字段比对
 *   2) 用官方文档中的真实响应样例验证解析器
 *
 * 编译:
 *   gcc -I <stub>/inc -I <repo>/inc -I <repo>/src test_wayz.c cJSON.c -o test_wayz
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "rtthread.h"
#include "webclient.h"    /* 必须早于下面的 webclient 桩实现, 保证签名一致 */
#include "wlan_mgnt.h"

/* ---------------- RT-Thread 桩实现 ---------------- */
int rt_kprintf(const char *fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt); r = vprintf(fmt, ap); va_end(ap);
    return r;
}
int rt_snprintf(char *buf, rt_size_t size, const char *fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt); r = vsnprintf(buf, size, fmt, ap); va_end(ap);
    return r;
}
void *rt_calloc(rt_size_t n, rt_size_t size) { return calloc(n, size); }
void *rt_malloc(rt_size_t size) { return malloc(size); }
void  rt_free(void *ptr) { free(ptr); }
void *rt_memcpy(void *d, const void *s, rt_size_t n) { return memcpy(d, s, n); }
void *rt_memset(void *s, int c, rt_size_t n) { return memset(s, c, n); }
rt_size_t rt_strlen(const char *s) { return strlen(s); }
int rt_strcmp(const char *a, const char *b) { return strcmp(a, b); }
char *rt_strstr(const char *a, const char *b) { return (char *)strstr(a, b); }
char *rt_strncpy(char *d, const char *s, rt_size_t n) { return strncpy(d, s, n); }
rt_uint64_t rt_tick_from_millisecond(rt_uint64_t ms) { return ms; }
rt_uint32_t rt_tick_get(void) { static rt_uint32_t t = 1000; return t += 10; }
rt_err_t rt_sem_init(rt_semaphore *s, const char *n, rt_uint32_t v, rt_uint8_t f) { return 0; }
rt_err_t rt_sem_take(rt_semaphore *s, rt_uint64_t t) { return 0; }
rt_err_t rt_sem_release(rt_semaphore *s) { return 0; }
rt_err_t rt_sem_detach(rt_semaphore *s) { return 0; }

/* ---------------- webclient 桩 ---------------- */
struct webclient_session *webclient_session_create(size_t sz) { (void)sz; return NULL; }
int webclient_close(struct webclient_session *s) { (void)s; return 0; }
int webclient_get(struct webclient_session *s, const char *u) { (void)s; (void)u; return -1; }
#if WEBCLIENT_SW_VERSION_NUM >= 0x20200
int webclient_post(struct webclient_session *s, const char *u, const void *d, size_t l)
{ (void)s; (void)u; (void)d; (void)l; return -1; }
int webclient_response(struct webclient_session *s, void **r, size_t *l)
{ (void)s; (void)r; (void)l; return -1; }
#else
int webclient_post(struct webclient_session *s, const char *u, const char *d)
{ (void)s; (void)u; (void)d; return -1; }
int webclient_response(struct webclient_session *s, unsigned char **r)
{ (void)s; (void)r; return -1; }
#endif
int webclient_resp_status_get(struct webclient_session *s) { (void)s; return 0; }
int webclient_header_fields_add(struct webclient_session *s, const char *f, ...)
{ (void)s; (void)f; return 0; }

/* ---------------- WLAN 桩 ---------------- */
rt_err_t rt_wlan_get_mac(rt_uint8_t *mac) { memset(mac, 0x02, 6); return RT_EOK; }
rt_bool_t rt_wlan_is_connected(void) { return RT_FALSE; }
rt_err_t rt_wlan_get_info(struct rt_wlan_info *info) { memset(info, 0, sizeof(*info)); return RT_EOK; }
rt_err_t rt_wlan_connect(const char *s, const char *p) { (void)s; (void)p; return RT_EOK; }
rt_err_t rt_wlan_scan(void) { return RT_EOK; }
rt_err_t rt_wlan_register_event_handler(rt_wlan_event_t e, rt_wlan_event_handler h, void *p)
{ (void)e; (void)h; (void)p; return RT_EOK; }
rt_err_t rt_wlan_unregister_event_handler(rt_wlan_event_t e) { (void)e; return RT_EOK; }
#if !defined(RT_VERSION_MAJOR) || (RT_VERSION_MAJOR < 5)
void rt_wlan_scan_result_clean(void) { }
#endif

/* ---------------- NTP 桩 ---------------- */
time_t wayz_get_time_by_ntp(const char *host_name)
{
    (void)host_name;
    return 1700000000;    /* 固定值，便于比对 */
}

/* 组件本体: 直接 include 以便访问 static 函数 */
#include "wayz_pos.c"

static void set_mac(rt_uint8_t *mac, const char *hex)
{
    int i;
    for (i = 0; i < 6; i++)
    {
        unsigned int v = 0;
        sscanf(hex + i * 2, "%2x", &v);
        mac[i] = (rt_uint8_t)v;
    }
}

static const char *OFFICIAL_RESPONSE =
"{"
"  \"asset\": \"390928a0-06d4-11e9-a952-024cxxxxxxxx\","
"  \"location\": {"
"    \"timestamp\": 1515743846504,"
"    \"address\": {"
"      \"name\": \"上海市浦东新区花木街道\","
"      \"context\": ["
"        {\"type\":\"Country\",\"name\":\"中国\",\"code\":\"CN\"},"
"        {\"type\":\"Province\",\"name\":\"上海市\",\"code\":\"310000\"},"
"        {\"type\":\"City\",\"name\":\"上海市\",\"code\":\"310000\"},"
"        {\"type\":\"District\",\"name\":\"浦东新区\",\"code\":\"310115\"},"
"        {\"type\":\"Township\",\"name\":\"花木街道\",\"code\":\"310115125\"}"
"      ]},"
"    \"place\": {\"type\":\"Entity\",\"name\":\"中建东湖明珠国际公馆\",\"distance\":{\"line\":3.7579}},"
"    \"position\": {"
"      \"timestamp\": 1730790836549,"
"      \"source\": \"wifi\","
"      \"point\": {\"longitude\":121.604634,\"latitude\":31.179703},"
"      \"spatialReference\": \"gcj02\","
"      \"accuracy\": 30}"
"  }"
"}";

/**
 * 读取仓库内的响应样例, 存在则优先使用(便于用真实响应做回归)
 */
static char *load_response_file(const char *path, char *fallback)
{
    FILE *fp = fopen(path, "rb");
    char *buf;
    long size;

    if (NULL == fp)
    {
        return fallback;
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0)
    {
        fclose(fp);
        return fallback;
    }

    buf = (char *)malloc((size_t)size + 1);
    if (NULL == buf)
    {
        fclose(fp);
        return fallback;
    }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size)
    {
        free(buf);
        fclose(fp);
        return fallback;
    }
    buf[size] = '\0';
    fclose(fp);

    return buf;
}

/**
 * 按候选列表依次尝试打开文件, 返回第一个成功读取的内容
 */
static char *load_first(const char *const *candidates, int count, char *fallback)
{
    int i;
    char *buf;

    for (i = 0; i < count; i++)
    {
        buf = load_response_file(candidates[i], NULL);
        if (NULL != buf)
        {
            printf("[INFO] 使用响应样例文件 %s\n", candidates[i]);
            return buf;
        }
    }

    return fallback;
}

int main(void)
{
    tapinfo ap = {0};
    char *body = NULL;
    tlocation_info location;
    int i;
    int failed = 0;

    printf("========== 1. 组件生成的定位上报请求体 ==========\n");

    /* 设备 MAC: 注入结果里带上, 模拟真实设备身份 */
    set_mac(ap.sta_mac, "020000000001");

    /* 注入 4 个 AP (云端要求 4~10 个) */
    {
        static const char *macs[4] = { "8825936fdfbb", "94772b651a00", "3c1ecc1a2b3c", "a42b8c112233" };
        for (i = 0; i < 4; i++)
        {
            set_mac(ap.tinfoAp[i].mac, macs[i]);
            ap.tinfoAp[i].rssi = (rt_int8_t)(-31 - i * 12);
            ap.tinfoAp[i].channel = (rt_uint8_t)(1 + i);
        }
        ap.count = 4;
    }
    wayz_set_scan_result(&ap);
    get_wifi_scan_info();     /* 注入结果在这里生效 */

    body = point_cJson_handler(NULL);
    if (NULL == body)
    {
        printf("[FAIL] point_cJson_handler 返回 NULL\n");
        return 1;
    }
    printf("%s\n\n", body);

    /* 契约检查: 字段名与类型 */
    {
        const char *must[] = {
            "\"timestamp\"", "\"id\"", "\"asset\"", "\"location\"",
            "\"wifis\"", "\"macAddress\"", "\"signalStrength\"",
            "\"frequency\"", "\"channel\"", "\"connected\"", "\"id\""
        };
        for (i = 0; i < (int)(sizeof(must) / sizeof(must[0])); i++)
        {
            if (NULL == strstr(body, must[i]))
            {
                printf("[FAIL] 请求体缺少字段 %s\n", must[i]);
                failed = 1;
            }
        }
        printf("[INFO] 请求体字段检查: %s\n", failed ? "有缺失" : "全部存在");
    }

    /* AP 数量裁剪到 10 */
    {
        tapinfo many = {0};
        for (i = 0; i < 20; i++)
        {
            set_mac(many.tinfoAp[i].mac, "aabbccddeeff");
            many.tinfoAp[i].rssi = -50;
            many.tinfoAp[i].channel = 6;
        }
        many.count = 20;
        wayz_set_scan_result(&many);
        get_wifi_scan_info();
        body = point_cJson_handler(NULL);
        if (NULL != body)
        {
            int n = 0;
            const char *p = body;
            while ((p = strstr(p, "\"macAddress\"")) != NULL) { n++; p += 11; }
            printf("[INFO] 注入 20 个 AP, 实际上报 %d 个 (期望 10)\n", n);
            if (n != 10) { printf("[FAIL] AP 数量裁剪异常\n"); failed = 1; }
            free(body);
        }
        wayz_set_scan_result(&ap);
    }

    printf("\n========== 2. 官方响应样例解析 ==========\n");
    memset(&location, 0, sizeof(location));
    {
        /*
         * 响应样例文件定位: 依次尝试
         *   1) 相对源码路径(__FILE__ 所在目录上溯两级)
         *   2) 相对当前工作目录(从 tools/host_test 或仓库根运行)
         */
        char dir[512] = {0};
        char cand[4][512];
        const char *cands[4];
        int ncand = 0;
        char *resp = NULL;
        char *slash;

        snprintf(dir, sizeof(dir), "%s", __FILE__);
        slash = strrchr(dir, '/');
        if (NULL != slash)
        {
            *slash = '\0';
        }

        snprintf(cand[0], sizeof(cand[0]), "%s/../../docs/testdata/track_points_response.json", dir);
        cands[ncand++] = cand[0];
        snprintf(cand[1], sizeof(cand[1]), "docs/testdata/track_points_response.json");
        cands[ncand++] = cand[1];
        snprintf(cand[2], sizeof(cand[2]), "../docs/testdata/track_points_response.json");
        cands[ncand++] = cand[2];
        snprintf(cand[3], sizeof(cand[3]), "../../docs/testdata/track_points_response.json");
        cands[ncand++] = cand[3];

        resp = load_first(cands, ncand, NULL);
        if (NULL == resp)
        {
            /* 没有真实响应时退回官方文档样例 */
            snprintf(cand[0], sizeof(cand[0]), "%s/../../docs/testdata/track_points_response.sample.json", dir);
            snprintf(cand[1], sizeof(cand[1]), "docs/testdata/track_points_response.sample.json");
            snprintf(cand[2], sizeof(cand[2]), "../docs/testdata/track_points_response.sample.json");
            snprintf(cand[3], sizeof(cand[3]), "../../docs/testdata/track_points_response.sample.json");
            resp = load_first(cands, ncand, (char *)OFFICIAL_RESPONSE);
        }

        i = parse_point_cJson_handler(resp, &location);
        if (resp != (char *)OFFICIAL_RESPONSE)
        {
            free(resp);
        }
    }
    if (RT_EOK != i)
    {
        printf("[FAIL] 解析官方响应失败\n");
        return 1;
    }

    printf("timestamp      : %s\n", location.timestamp);
    printf("point          : %.12f, %.12f\n", location.point.latitude, location.point.longitude);
    printf("source         : %s\n", location.source);
    printf("spatialRef     : %s\n", location.spatial_reference);
    printf("accuracy       : %d\n", (int)location.accuracy);
    printf("address        : %s\n", location.address);
    printf("place.name     : %s\n", location.place.name);
    printf("place.type     : %s\n", location.place.type);
    printf("place.distance : %.4f\n", location.place.distance);
    printf("context        : %d\n", location.context_count);
    for (i = 0; i < location.context_count; i++)
    {
        printf("   %s = %s (%s)\n", location.context[i].type,
               location.context[i].name, location.context[i].code);
    }

    /* 断言: 结构与取值范围(不绑定具体坐标, 以便真实响应与官方样例都能用) */
    if (fabs(location.point.longitude) < 73.0 || fabs(location.point.longitude) > 135.0 ||
        fabs(location.point.latitude) < 3.0 || fabs(location.point.latitude) > 54.0)
    {
        printf("[FAIL] 经纬度不在合理范围内\n"); failed = 1;
    }
    if ('\0' == location.place.name[0])
    {
        printf("[FAIL] POI 名称未解析\n"); failed = 1;
    }
    if ('\0' == location.address[0])
    {
        printf("[FAIL] 地址未解析\n"); failed = 1;
    }
    if (5 != location.context_count)
    {
        printf("[FAIL] 行政区划数量错误: %d\n", location.context_count); failed = 1;
    }
    if (0 != strcmp(location.source, "wifi") || 0 != strcmp(location.spatial_reference, "gcj02"))
    {
        printf("[FAIL] source/spatialReference 解析错误\n"); failed = 1;
    }
    if (0 == (int)location.accuracy)
    {
        printf("[FAIL] accuracy 未解析\n"); failed = 1;
    }
    if ('\0' == location.timestamp[0])
    {
        printf("[FAIL] timestamp 未解析\n"); failed = 1;
    }

    printf("\n========== 结果: %s ==========\n", failed ? "存在失败项" : "全部通过");
    return failed;
}
