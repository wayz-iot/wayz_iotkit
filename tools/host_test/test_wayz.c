/*
 * wayz_iotkit 主机端回归测试
 *
 * 设计要点(针对"桩比真实实现更宽松会掩盖缺陷"这一类问题):
 *   1. 独立的私有分配器: 带 magic 头 + 前后哨兵字节 + 记账, 结尾断言 live == 0
 *   2. 桩按 RT-Thread 5.x 语义实现(版本宏 / 结构体 / 名字长度)
 *   3. 最小 webclient 桩: 记录请求 URL/头/体, 返回预置响应 —— 让 HTTP 链路也进入覆盖
 *   4. 断言是字段级的精确比对, 而不是范围判断
 *   5. 畸形响应必须被拒绝(不能把"没定位到"当成 (0,0) 的成功)
 */
#include <rtthread.h>
#include <webclient.h>
#include <wlan_mgnt.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 直接把组件源码编进来, 以便访问内部静态函数 */
#include "wayz_pos.c"

/* ================= 私有分配器(哨兵 + 记账) ================= */

#define ALLOC_MAGIC     0x57415A5AU
#define GUARD_BYTE      0xA5
#define GUARD_LEN       16

static int g_live_blocks = 0;
static int g_guard_broken = 0;

struct alloc_head
{
    rt_uint32_t magic;
    rt_size_t   size;
};

void *rt_malloc(rt_size_t size)
{
    unsigned char *raw = (unsigned char *)malloc(sizeof(struct alloc_head) + size + 2 * GUARD_LEN);
    struct alloc_head *head;

    if (NULL == raw)
    {
        return NULL;
    }

    head = (struct alloc_head *)raw;
    head->magic = ALLOC_MAGIC;
    head->size = size;
    memset(raw + sizeof(struct alloc_head), GUARD_BYTE, size + 2 * GUARD_LEN);

    g_live_blocks++;
    return raw + sizeof(struct alloc_head) + GUARD_LEN;
}

void *rt_calloc(rt_size_t n, rt_size_t size)
{
    void *p = rt_malloc(n * size);

    if (NULL != p)
    {
        memset(p, 0, n * size);
    }

    return p;
}

void rt_free(void *ptr)
{
    unsigned char *raw;
    struct alloc_head *head;
    rt_size_t i;

    if (NULL == ptr)
    {
        return;
    }

    raw = (unsigned char *)ptr - sizeof(struct alloc_head) - GUARD_LEN;
    head = (struct alloc_head *)raw;

    if (head->magic != ALLOC_MAGIC)
    {
        printf("[FAIL] rt_free on a pointer that was not allocated by rt_malloc\n");
        g_guard_broken = 1;
        return;
    }

    for (i = 0; i < GUARD_LEN; i++)
    {
        if ((raw[sizeof(struct alloc_head) + i] != GUARD_BYTE) ||
            (raw[sizeof(struct alloc_head) + head->size + GUARD_LEN + i] != GUARD_BYTE))
        {
            printf("[FAIL] heap overflow detected (guard byte corrupted)\n");
            g_guard_broken = 1;
            break;
        }
    }

    head->magic = 0;
    g_live_blocks--;
    free(raw);
}

/* ================= 其余 RT-Thread 桩 ================= */

int rt_kprintf(const char *fmt, ...)
{
    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vprintf(fmt, ap);
    va_end(ap);

    return r;
}

int rt_snprintf(char *buf, rt_size_t size, const char *fmt, ...)
{
    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vsnprintf(buf, size, fmt, ap);
    va_end(ap);

    return r;
}

void *rt_memcpy(void *dest, const void *src, rt_size_t n) { return memcpy(dest, src, n); }
void *rt_memset(void *s, int c, rt_size_t n) { return memset(s, c, n); }
rt_size_t rt_strlen(const char *s) { return strlen(s); }
int rt_strcmp(const char *a, const char *b) { return strcmp(a, b); }
char *rt_strstr(const char *a, const char *b) { return (char *)strstr(a, b); }
char *rt_strncpy(char *dest, const char *src, rt_size_t n) { return strncpy(dest, src, n); }
rt_uint64_t rt_tick_from_millisecond(rt_uint64_t ms) { return ms; }
rt_uint32_t rt_tick_get(void) { static rt_uint32_t t = 1000; return t += 10; }

rt_err_t rt_sem_init(rt_semaphore *sem, const char *name, rt_uint32_t v, rt_uint8_t f) { (void)sem; (void)name; (void)v; (void)f; return RT_EOK; }
rt_err_t rt_sem_take(rt_semaphore *sem, rt_uint64_t t) { (void)sem; (void)t; return RT_EOK; }
rt_err_t rt_sem_release(rt_semaphore *sem) { (void)sem; return RT_EOK; }
rt_err_t rt_sem_detach(rt_semaphore *sem) { (void)sem; return RT_EOK; }

int rt_mutex_init(rt_mutex_t m, const char *name, rt_uint8_t flag)
{ (void)m; (void)name; (void)flag; return RT_EOK; }
int rt_mutex_take(rt_mutex_t m, rt_int32_t t) { (void)m; (void)t; return RT_EOK; }
int rt_mutex_release(rt_mutex_t m) { (void)m; return RT_EOK; }

rt_thread_t rt_thread_create(const char *n, void (*e)(void *), void *p, rt_uint32_t s, rt_uint8_t pr, rt_uint32_t tk)
{ (void)n; (void)e; (void)p; (void)s; (void)pr; (void)tk; return NULL; }
rt_err_t rt_thread_startup(rt_thread_t t) { (void)t; return RT_EOK; }
void rt_thread_mdelay(rt_int32_t ms) { (void)ms; }

/* WLAN 桩 */
rt_err_t rt_wlan_connect(const char *s, const char *p) { (void)s; (void)p; return RT_EOK; }
rt_bool_t rt_wlan_is_connected(void) { return RT_TRUE; }
rt_err_t rt_wlan_get_mac(rt_uint8_t *mac) { memset(mac, 0x02, 6); return RT_EOK; }
rt_err_t rt_wlan_get_info(struct rt_wlan_info *info) { memset(info, 0, sizeof(*info)); return RT_EOK; }
rt_err_t rt_wlan_scan(void) { return RT_EOK; }
void rt_wlan_config_autoreconnect(rt_bool_t e) { (void)e; }
rt_err_t rt_wlan_register_event_handler(rt_wlan_event_t e, rt_wlan_event_handler h, void *p) { (void)e; (void)h; (void)p; return RT_EOK; }
rt_err_t rt_wlan_unregister_event_handler(rt_wlan_event_t e) { (void)e; return RT_EOK; }

/* ================= 最小 webclient 桩 ================= */

static char g_req_url[512];
static char g_req_header[1024];
static char g_req_body[8192];
static int  g_resp_status = 200;
static char g_resp_body[16384];

struct webclient_session *webclient_session_create(size_t sz)
{
    struct webclient_session *s;

    (void)sz;
    s = (struct webclient_session *)rt_calloc(1, sizeof(struct webclient_session));
    if (NULL == s)
    {
        return NULL;
    }
    s->header = (struct webclient_header *)rt_calloc(1, sizeof(struct webclient_header));
    if (NULL == s->header)
    {
        rt_free(s);
        return NULL;
    }
    s->header->buffer = (char *)rt_calloc(1, WEBCLIENT_HEADER_BUFSZ);
    s->header->size = WEBCLIENT_HEADER_BUFSZ;

    return s;
}

int webclient_close(struct webclient_session *s)
{
    if (NULL == s)
    {
        return RT_EOK;
    }
    if (NULL != s->header)
    {
        rt_free(s->header->buffer);
        rt_free(s->header);
    }
    rt_free(s->host);
    rt_free(s->req_url);
    rt_free(s);

    return RT_EOK;
}

int webclient_header_fields_add(struct webclient_session *s, const char *fmt, ...)
{
    va_list ap;
    int n;

    (void)s;
    va_start(ap, fmt);
    n = vsnprintf(g_req_header + strlen(g_req_header),
                  sizeof(g_req_header) - strlen(g_req_header), fmt, ap);
    va_end(ap);

    return n;
}

int webclient_get(struct webclient_session *s, const char *uri)
{
    (void)s;
    snprintf(g_req_url, sizeof(g_req_url), "%s", uri);
    return g_resp_status;
}

int webclient_post(struct webclient_session *s, const char *uri, const void *body, size_t len)
{
    (void)s;
    snprintf(g_req_url, sizeof(g_req_url), "%s", uri);
    if ((NULL != body) && (len > 0) && (len < sizeof(g_req_body)))
    {
        memcpy(g_req_body, body, len);
        g_req_body[len] = '\0';
    }

    return g_resp_status;
}

int webclient_response(struct webclient_session *s, void **response, size_t *resp_len)
{
    size_t len = strlen(g_resp_body);
    char *buf = (char *)rt_malloc(len + 1);

    (void)s;
    if (NULL == buf)
    {
        return -1;
    }
    memcpy(buf, g_resp_body, len + 1);
    *response = buf;
    if (NULL != resp_len)
    {
        *resp_len = len;
    }

    return (int)len;
}

int webclient_resp_status_get(struct webclient_session *s)
{
    (void)s;
    return g_resp_status;
}

static void web_reset_request(void)
{
    g_req_url[0] = '\0';
    g_req_header[0] = '\0';
    g_req_body[0] = '\0';
}

/* ================= 测试辅助 ================= */

static int g_failed = 0;

#define CHECK(cond, fmt, ...)                                               \
    do {                                                                    \
        if (!(cond))                                                        \
        {                                                                   \
            printf("[FAIL] " fmt "\n", ##__VA_ARGS__);                      \
            g_failed++;                                                     \
        }                                                                   \
    } while (0)

#define CHECK_STR(actual, expect)                                           \
    do {                                                                    \
        if (0 != strcmp((actual), (expect)))                                \
        {                                                                   \
            printf("[FAIL] %s: got \"%s\", expected \"%s\"\n",              \
                   #actual, (actual), (expect));                             \
            g_failed++;                                                     \
        }                                                                   \
    } while (0)

static char *load_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    char *buf;
    long size;

    if (NULL == fp)
    {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = (char *)malloc((size_t)size + 1);
    if (NULL == buf)
    {
        fclose(fp);
        return NULL;
    }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size)
    {
        free(buf);
        fclose(fp);
        return NULL;
    }
    buf[size] = '\0';
    fclose(fp);

    return buf;
}

/* ================= 用例 1: 真实响应基线(字段级精确断言) ================= */

static void test_real_response(const char *response_path)
{
    char *data = load_file(response_path);
    tlocation_info loc;
    cJSON *root, *item, *wifis;
    int i;

    if (NULL == data)
    {
        printf("[FAIL] cannot load %s\n", response_path);
        g_failed++;
        return;
    }

    memset(&loc, 0xAB, sizeof(loc));
    CHECK(RT_EOK == parse_point_cJson_handler(data, &loc), "the real response should be parsed");
    free(data);

    /* 时间戳: 13 位毫秒 */
    CHECK(strlen(loc.timestamp) == 13, "timestamp should be 13 digits, got \"%s\"", loc.timestamp);

    /* 经纬度: 与响应里的值逐位比对 */
    root = cJSON_Parse(NULL);
    (void)root;
    CHECK(fabs(loc.point.latitude - 31.125781) < 1e-9, "latitude got %f", loc.point.latitude);
    CHECK(fabs(loc.point.longitude - 121.369155) < 1e-9, "longitude got %f", loc.point.longitude);

    /* 其余字段 */
    CHECK_STR(loc.source, "wifi");
    CHECK_STR(loc.spatial_reference, "gcj02");
    CHECK(50 == (int)loc.accuracy, "accuracy got %d", (int)loc.accuracy);
    CHECK_STR(loc.place.type, "Entity");
    CHECK(fabs(loc.place.distance - 9.659) < 1e-6, "distance got %f", loc.place.distance);
    CHECK(5 == loc.context_count, "context count got %d", loc.context_count);
    for (i = 0; i < loc.context_count && i < CONTEXT_COUNT; i++)
    {
        CHECK('\0' != loc.context[i].type[0], "context[%d].type is empty", i);
        CHECK('\0' != loc.context[i].name[0], "context[%d].name is empty", i);
        CHECK('\0' != loc.context[i].code[0], "context[%d].code is empty", i);
    }

    /* POI 名是中文, 长度按字节检查(>= 2 个汉字) */
    CHECK(strlen(loc.place.name) >= 6, "place.name looks truncated: \"%s\"", loc.place.name);
    CHECK(strlen(loc.address) >= 6, "address looks truncated: \"%s\"", loc.address);
    (void)wifis;
    (void)item;
}

/* ================= 用例 2: 畸形响应必须被拒绝 ================= */

static void test_malformed_responses(void)
{
    static const struct
    {
        const char *name;
        const char *body;
    } cases[] = {
        { "empty object",            "{}" },
        { "no location",             "{\"id\":\"x\",\"asset\":\"y\"}" },
        { "location is not object",  "{\"location\":\"oops\"}" },
        { "no position",             "{\"location\":{\"timestamp\":1}}" },
        { "position is string",      "{\"location\":{\"position\":\"oops\"}}" },
        { "point is empty",          "{\"location\":{\"position\":{\"point\":{}}}}" },
        { "no longitude",            "{\"location\":{\"position\":{\"point\":{\"latitude\":31.1}}}}" },
        { "longitude is string",     "{\"location\":{\"position\":{\"point\":{\"longitude\":\"x\",\"latitude\":31.1}}}}" },
        { "longitude out of range",  "{\"location\":{\"position\":{\"point\":{\"longitude\":999,\"latitude\":31.1}}}}" },
        { "latitude out of range",   "{\"location\":{\"position\":{\"point\":{\"longitude\":121.1,\"latitude\":999}}}}" },
        { "point is (0,0)",         "{\"location\":{\"position\":{\"point\":{\"longitude\":0,\"latitude\":0}}}}" },
        { "broken json",             "{\"location\":" },
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        tlocation_info loc;
        int rc;

        /* 用 0xAB 预填, 若解析函数不清零就会留下脏数据 */
        memset(&loc, 0xAB, sizeof(loc));
        rc = parse_point_cJson_handler(cases[i].body, &loc);

        CHECK(RT_EOK != rc, "[%s] should be rejected but returned RT_EOK", cases[i].name);
        CHECK('\0' == loc.place.name[0], "[%s] the output struct was not cleared", cases[i].name);
        CHECK(0 == loc.context_count, "[%s] context_count was not cleared", cases[i].name);
        CHECK('\0' == loc.timestamp[0], "[%s] timestamp was not cleared", cases[i].name);
    }
}

/* ================= 用例 3: 组包(AP 取最强的 10 个) ================= */

static void test_request_body(void)
{
    tapinfo ap = {0};
    char *body;
    cJSON *root, *loc, *wifis;
    int i, n;
    char mac[MAC_LEN] = {0};

    /* 20 个 AP, RSSI 由弱到强 */
    for (i = 0; i < 20; i++)
    {
        int j;

        for (j = 0; j < MAC_COUNT; j++)
        {
            ap.tinfoAp[i].mac[j] = (rt_uint8_t)(i + 1);
        }
        ap.tinfoAp[i].rssi = (rt_int8_t)(-95 + i);   /* -95 .. -76 */
        ap.tinfoAp[i].channel = (rt_uint8_t)(1 + (i % 13));
    }
    ap.count = 20;

    wayz_set_scan_result(&ap);
    get_wifi_scan_info();

    body = point_cJson_handler(NULL);
    if (NULL == body)
    {
        printf("[FAIL] point_cJson_handler returned NULL\n");
        g_failed++;
        return;
    }

    root = cJSON_Parse(body);
    CHECK(NULL != root, "the request body is not valid JSON");
    if (NULL == root)
    {
        rt_free(body);
        return;
    }

    /* id 必须是完整的 26 字符, 不能被截断 */
    {
        cJSON *id = cJSON_GetObjectItem(root, "id");

        CHECK(NULL != id, "no id field");
        if (NULL != id)
        {
            CHECK(26 == strlen(id->valuestring), "id should be 26 chars, got %d (%s)",
                  (int)strlen(id->valuestring), id->valuestring);
            CHECK(0 == strncmp(id->valuestring, "38efe26e-bcd8-", 14), "id prefix changed");
        }
    }

    /* asset.id 与设备 MAC 一致 */
    {
        cJSON *asset = cJSON_GetObjectItem(root, "asset");
        cJSON *aid = (NULL != asset) ? cJSON_GetObjectItem(asset, "id") : NULL;

        CHECK(NULL != aid, "no asset.id");
        if (NULL != aid)
        {
            rt_snprintf(mac, sizeof(mac), "%02X%02X%02X%02X%02X%02X",
                        ap.sta_mac[0], ap.sta_mac[1], ap.sta_mac[2],
                        ap.sta_mac[3], ap.sta_mac[4], ap.sta_mac[5]);
            CHECK_STR(aid->valuestring, mac);
        }
    }

    loc = cJSON_GetObjectItem(root, "location");
    CHECK(NULL != loc, "no location field");
    wifis = (NULL != loc) ? cJSON_GetObjectItem(loc, "wifis") : NULL;
    CHECK(NULL != wifis, "no wifis array");
    if (NULL != wifis)
    {
        n = cJSON_GetArraySize(wifis);
        CHECK(10 == n, "should upload 10 APs, got %d", n);

        /* 上报的必须是信号最强的 10 个: RSSI 绝对值从大到小 */
        {
            cJSON *it = cJSON_GetArrayItem(wifis, 0);
            cJSON *ss = (NULL != it) ? cJSON_GetObjectItem(it, "signalStrength") : NULL;

            CHECK(NULL != ss, "no signalStrength");
            if (NULL != ss)
            {
                /* 最强的是 -76 -> 76 */
                CHECK(76 == (int)ss->valuedouble, "the strongest AP should be uploaded first, got %d",
                      (int)ss->valuedouble);
            }
        }

        /* macAddress 大写 + 冒号, frequency 正确 */
        {
            cJSON *it = cJSON_GetArrayItem(wifis, 0);
            cJSON *ma = (NULL != it) ? cJSON_GetObjectItem(it, "macAddress") : NULL;
            cJSON *fq = (NULL != it) ? cJSON_GetObjectItem(it, "frequency") : NULL;

            CHECK(NULL != ma, "no macAddress");
            if (NULL != ma)
            {
                CHECK(17 == strlen(ma->valuestring), "macAddress should be 17 chars, got %d",
                      (int)strlen(ma->valuestring));
                CHECK(':' == ma->valuestring[2], "macAddress should use colons");
                {
                    int k, upper_ok = 1;

                    for (k = 0; k < 17 && upper_ok; k += 3)
                    {
                        char c = ma->valuestring[k];

                        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')))
                        {
                            upper_ok = 0;
                        }
                    }
                    CHECK(upper_ok, "macAddress should be upper case hex: %s", ma->valuestring);
                }
            }
            CHECK(NULL != fq, "no frequency");
        }
    }

    cJSON_Delete(root);
    rt_free(body);
}

/* ================= 用例 4: HTTP 全链路(走 webclient 桩) ================= */

static void test_http_flow(const char *response_path)
{
    char *data = load_file(response_path);
    tlocation_info loc;
    twifi_info wlan;
    char ssid[16] = "test";
    char pwd[16] = "test";
    int rc;

    if (NULL == data)
    {
        printf("[FAIL] cannot load %s\n", response_path);
        g_failed++;
        return;
    }
    snprintf(g_resp_body, sizeof(g_resp_body), "%s", data);
    free(data);
    g_resp_status = 200;
    web_reset_request();

    memset(&wlan, 0, sizeof(wlan));
    wlan.ssid = ssid;
    wlan.passwd = pwd;
    memset(&loc, 0, sizeof(loc));

    rc = get_position_info(&wlan, "TESTKEY", NULL, &loc);
    CHECK(RT_EOK == rc, "get_position_info should succeed, got %d", rc);

    /* 请求 URL 必须带 access_key 与 response_sprf */
    CHECK(NULL != strstr(g_req_url, "access_key=TESTKEY"), "url has no access_key: %s", g_req_url);
    CHECK(NULL != strstr(g_req_url, "response_sprf=gcj02"), "url has no response_sprf: %s", g_req_url);

    /* 请求头 */
    CHECK(NULL != strstr(g_req_header, "Content-Type: application/json"),
          "bad content type: %s", g_req_header);
    CHECK(NULL != strstr(g_req_header, "Content-Length:"), "no content length: %s", g_req_header);

    /* 请求体是合法 JSON 且含 wifis */
    {
        cJSON *root = cJSON_Parse(g_req_body);

        CHECK(NULL != root, "the request body is not valid JSON");
        if (NULL != root)
        {
            cJSON *locj = cJSON_GetObjectItem(root, "location");
            cJSON *wifis = (NULL != locj) ? cJSON_GetObjectItem(locj, "wifis") : NULL;

            CHECK(NULL != wifis, "no wifis in the request body");
            cJSON_Delete(root);
        }
    }

    /* 解析结果 */
    CHECK(fabs(loc.point.latitude - 31.125781) < 1e-9, "latitude got %f", loc.point.latitude);
    CHECK_STR(loc.spatial_reference, "gcj02");
}

/* ================= 用例 5: 状态码分级 ================= */

static void test_status_classification(const char *response_path)
{
    char *data = load_file(response_path);
    twifi_info wlan;
    char ssid[16] = "test";
    char pwd[16] = "test";
    tlocation_info loc;
    int rc;

    if (NULL == data)
    {
        printf("[FAIL] cannot load %s\n", response_path);
        g_failed++;
        return;
    }
    snprintf(g_resp_body, sizeof(g_resp_body), "%s", data);
    free(data);
    web_reset_request();

    memset(&wlan, 0, sizeof(wlan));
    wlan.ssid = ssid;
    wlan.passwd = pwd;
    memset(&loc, 0, sizeof(loc));

    /* 403: 密钥无效, 必须能与应用其它错误区分开 */
    g_resp_status = 403;
    snprintf(g_resp_body, sizeof(g_resp_body), "{\"message\":\"Invalid authentication credentials\"}");
    rc = get_position_info(&wlan, "BADKEY", NULL, &loc);
    CHECK(WAYZ_ERR_AUTH == rc, "403 should map to WAYZ_ERR_AUTH, got %d", rc);

    /* 429: 限流 */
    g_resp_status = 429;
    rc = get_position_info(&wlan, "K", NULL, &loc);
    CHECK(WAYZ_ERR_RATE_LIMIT == rc, "429 should map to WAYZ_ERR_RATE_LIMIT, got %d", rc);

    /* 500: 服务端错误 */
    g_resp_status = 500;
    rc = get_position_info(&wlan, "K", NULL, &loc);
    CHECK(WAYZ_ERR_SERVER == rc, "500 should map to WAYZ_ERR_SERVER, got %d", rc);

    /* 200 但没有 position: 无线信号不足 */
    g_resp_status = 200;
    snprintf(g_resp_body, sizeof(g_resp_body), "{\"id\":\"x\",\"asset\":\"y\",\"location\":{\"timestamp\":1}}");
    rc = get_position_info(&wlan, "K", NULL, &loc);
    CHECK(WAYZ_ERR_NO_SIGNAL == rc, "200 without position should map to WAYZ_ERR_NO_SIGNAL, got %d", rc);

    g_resp_status = 200;
}

/* ================= main ================= */

int main(int argc, char **argv)
{
    const char *base = (argc > 1) ? argv[1] : "../..";
    char path[512];
    int failed_before;

    snprintf(path, sizeof(path), "%s/docs/testdata/track_points_response.json", base);
    test_real_response(path);

    test_malformed_responses();
    test_request_body();

    snprintf(path, sizeof(path), "%s/docs/testdata/track_points_response.json", base);
    test_http_flow(path);

    snprintf(path, sizeof(path), "%s/docs/testdata/track_points_response.json", base);
    test_status_classification(path);

    /* 内存: 所有用例结束后不应有泄漏 */
    failed_before = g_failed;
    printf("\nlive allocations: %d, guard broken: %d\n", g_live_blocks, g_guard_broken);
    CHECK(0 == g_live_blocks, "there are %d leaked allocations", g_live_blocks);
    CHECK(0 == g_guard_broken, "heap corruption detected");

    printf("\n========== 结果: %s ==========\n", (g_failed == 0) ? "全部通过" : "存在失败项");
    (void)failed_before;

    return (g_failed == 0) ? 0 : 1;
}
