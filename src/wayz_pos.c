
#include "wayz_pos.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include <math.h>
#include <webclient.h>
#include <sys/time.h>

/*
 * WLAN 框架按需引入:
 *   - RT-Thread 5.2.0 起头文件由 wlan_mgnt.h 更名为 dev_wlan_mgnt.h;
 *   - 没有 WiFi 芯片的 BSP(仿真器 / qemu)根本不存在这两个头文件,
 *     此时组件只能使用 wayz_set_scan_result() 注入的扫描结果。
 */
#if defined(PKG_WAYZ_IOTKIT_NO_WLAN)
#  define WAYZ_USE_WLAN      0
#elif defined(__has_include)
#  if __has_include(<dev_wlan_mgnt.h>)
#    include <dev_wlan_mgnt.h>
#    define WAYZ_USE_WLAN    1
#  elif __has_include(<wlan_mgnt.h>)
#    include <wlan_mgnt.h>
#    define WAYZ_USE_WLAN    1
#  else
#    define WAYZ_USE_WLAN    0
#  endif
#else
#  include <wlan_mgnt.h>
#  define WAYZ_USE_WLAN      1
#endif

/*
 * RT-Thread 版本判断。RTTHREAD_VERSION 在 4.1.0 与 5.x 上都存在:
 *   4.1.0 -> 40100, 5.0.0 -> 50000, 5.1.0 -> 50100, 5.2.0 -> 50200
 */
#if defined(RTTHREAD_VERSION)
#  define WAYZ_RT_VERSION     RTTHREAD_VERSION
#else
#  define WAYZ_RT_VERSION     0
#endif

/* RT-Thread 5.0.0 起 WLAN 框架不再提供 rt_wlan_scan_sync()/rt_wlan_scan_result_clean() */
#if (WAYZ_RT_VERSION >= 50000)
#  define WAYZ_RT_5X_OR_NEWER    1
#else
#  define WAYZ_RT_5X_OR_NEWER    0
#endif

/*
 * webclient 2.2.0 起 session 接口签名发生变化:
 *   webclient_post()    增加 data_len 参数
 *   webclient_response() 响应缓冲由 unsigned char** 改为 void**，并增加 resp_len
 * 2.0.0 ~ 2.1.2 保持旧签名。兼容点集中在本文件这一处。
 */
#if defined(WEBCLIENT_SW_VERSION_NUM) && (WEBCLIENT_SW_VERSION_NUM >= 0x20200)
#  define WAYZ_WEBCLIENT_NEW_API 1
#else
#  define WAYZ_WEBCLIENT_NEW_API 0
#endif

#define  NET_READY_TIME_OUT       (rt_tick_from_millisecond(15 * 1000))
#define  SCAN_WAIT_TIME_OUT       (rt_tick_from_millisecond(10 * 1000))
#define  SCAN_DRAIN_TIME_OUT     (rt_tick_from_millisecond(1 * 1000))
#define  URL_BUFFER_SIZE          192
/* 早于 2020-01-01 的系统时间视为"未校准"(无 RTC 的板子或软 RTC 的默认值) */
#define  WAYZ_TIME_MIN_VALID_SEC  1577836800ull

static tapinfo aucApInfo = {0};
static struct rt_semaphore net_ready;
static rt_uint8_t sta_bssid[MAC_COUNT];      /* 当前连接 AP 的 BSSID */
static rt_uint8_t sta_connected = 0;
static tapinfo s_injected_ap = {0};          /* 注入的扫描结果(仿真/单测) */
static rt_uint8_t s_scan_injected = 0;

/*
 * cJSON 默认使用 libc 的 malloc/free, 而本组件使用 RT-Thread 的堆,
 * 两者不是同一个分配器。这里把 cJSON 的内存钩子统一到 rt_malloc/rt_free,
 * 否则用 rt_free 释放 cJSON_Print 的结果会造成堆损坏。
 */
static cJSON_Hooks wayz_cjson_hooks =
{
    rt_malloc,
    rt_free
};

static void wayz_cjson_hooks_init(void)
{
    static rt_uint8_t inited = 0;

    if (inited)
    {
        return;
    }
    inited = 1;

    cJSON_InitHooks(&wayz_cjson_hooks);
}

/*
 * 时间戳提供者: 组件不自带 NTP 客户端, 时间由系统时钟提供,
 * 需要精确校时请使用平台自带的 NTP/SNTP 组件, 或通过
 * wayz_set_time_provider() 接入 RTC 等其它时间源。
 */
static wayz_time_provider_t s_time_provider = RT_NULL;

static void wayz_error(char *msg);
static void wayz_op_lock(void);
static void wayz_op_unlock(void);
static void wayz_scan_lock(void);
static void wayz_scan_unlock(void);

/* 云端返回的坐标系(response_sprf), 默认 gcj02 */
static char s_response_sprf[8] = WAYZ_SPRF_GCJ02;

/*
 * 无系统时钟时的基准时间。
 * 不能留 0 —— 否则无 RTC 的设备会上传 1970 年的时间戳(云端会拒绝或错位归档),
 * 这里退到一个"看起来合法"的固定基准, 再叠加开机以来的 tick 增量, 保证单调递增。
 */
static rt_uint64_t s_base_ms = TIMESTAMP_DEL;
static rt_uint32_t s_base_tick = 0;
static rt_uint8_t s_base_warned = 0;

/**
 * 默认时间戳提供者: 优先取系统时钟(gettimeofday), 否则用"基准时间 + tick 增量"
 */
static int wayz_default_time_provider(rt_uint64_t *timestamp_ms)
{
    struct timeval tv;

    /*
     * gettimeofday 在没有 RTC 的板子上会直接失败(返回 -1),
     * 未校时的软 RTC 也会给出一个很早的默认时间, 两者都视为"没有可用时钟"。
     */
    if ((0 == gettimeofday(&tv, RT_NULL)) &&
        ((rt_uint64_t)tv.tv_sec >= WAYZ_TIME_MIN_VALID_SEC))
    {
        *timestamp_ms = (rt_uint64_t)tv.tv_sec * 1000 + (rt_uint64_t)(tv.tv_usec / 1000);
        return RT_EOK;
    }

    if (0 == s_base_warned)
    {
        s_base_warned = 1;
        wayz_error("no valid system clock, fallback to the built-in base time. "
                   "Use wayz_set_time_provider() to provide a real time source.");
    }

    /* 没有系统时钟时保证时间戳单调递增 */
    {
        rt_uint32_t now_tick = rt_tick_get();
        rt_uint32_t elapsed;

        if (0 == s_base_tick)
        {
            s_base_tick = now_tick;
        }
        elapsed = now_tick - s_base_tick;
        *timestamp_ms = s_base_ms + (rt_uint64_t)((rt_uint64_t)elapsed * 1000 / RT_TICK_PER_SECOND);
    }

    return RT_EOK;
}

int wayz_set_time_provider(wayz_time_provider_t provider)
{
    wayz_op_lock();
    s_time_provider = provider;
    wayz_op_unlock();

    return RT_EOK;
}

int wayz_set_response_sprf(const char *sprf)
{
    if (RT_NULL == sprf)
    {
        wayz_op_lock();
        rt_snprintf(s_response_sprf, sizeof(s_response_sprf), "%s", WAYZ_SPRF_GCJ02);
        wayz_op_unlock();
        return RT_EOK;
    }

    if ((0 != rt_strcmp(sprf, WAYZ_SPRF_GCJ02)) &&
        (0 != rt_strcmp(sprf, WAYZ_SPRF_WGS84)) &&
        (0 != rt_strcmp(sprf, WAYZ_SPRF_BD09)))
    {
        wayz_error("unsupported response_sprf, only gcj02/wgs84/bd09 are allowed.");
        return RT_ERROR;
    }

    wayz_op_lock();
    rt_snprintf(s_response_sprf, sizeof(s_response_sprf), "%s", sprf);
    wayz_op_unlock();

    return RT_EOK;
}

/**
 * 取 track_points 接口地址
 *
 * 仿真模式下允许用环境变量 WAYZ_TRACK_POINTS_URL 覆盖, 便于对接本地 mock 服务
 * 做端到端回归(不产生任何外发流量)。
 */
static const char *wayz_track_points_url(void)
{
#ifdef PKG_WAYZ_IOTKIT_SIM
    {
        extern char *getenv(const char *name);
        char *url = getenv("WAYZ_TRACK_POINTS_URL");

        if (RT_NULL != url && '\0' != url[0])
        {
            return url;
        }
    }
#endif

    return DEV_POSTION_URL;
}

/**
 * 取当前的 UTC 毫秒时间戳
 */
static rt_uint64_t wayz_get_timestamp_ms(void)
{
    rt_uint64_t timestamp = 0;

    if (RT_NULL != s_time_provider)
    {
        if (RT_EOK == s_time_provider(&timestamp) && 0 != timestamp)
        {
            return timestamp;
        }
    }

    wayz_default_time_provider(&timestamp);

    return timestamp;
}


#define  FLOAT_EPS      1e-6
#define  DOUBLE_EPS     1e-15

static void wayz_error(char* msg)
{
    rt_kprintf("\033[31;22m[E/wayz]: ERROR %s\033[0m\n", msg); // Print the error message to stderr.
}

static unsigned char check_double_equal_zero(double f)
{
    if(fabs(f) <= DOUBLE_EPS)
    {
        return RT_EOK;
    }
    else
    {
        return RT_ERROR;
    }
}

static unsigned char check_float_equal_zero(float f)
{
    if(fabs(f) <= FLOAT_EPS)
    {
        return RT_EOK;
    }
    else
    {
        return RT_ERROR;
    }
}

/**
 * 线程安全的字符串复制(使用 rt 堆分配器), 避免 libc strdup 与 rt_free 混用
 */
static char *wayz_strdup(const char *src)
{
    rt_size_t len;
    char *dst;

    if (RT_NULL == src)
    {
        return RT_NULL;
    }

    len = rt_strlen(src);
    dst = (char *)rt_calloc(1, len + 1);
    if (RT_NULL == dst)
    {
        return RT_NULL;
    }
    rt_memcpy(dst, src, len);

    return dst;
}

/**
 * @param data: get mac address data, eg:XX:XX:XX:XX:XX:XX
 * @note  调用者需保证 data 至少有 MAC_LEN(18) 字节
 */
void get_sta_mac_addr(char *data)
{
    rt_uint8_t mac[MAC_COUNT] = {0};

    if (RT_NULL == data)
    {
        return;
    }

#if WAYZ_USE_WLAN
    rt_wlan_get_mac(mac);
#endif
    rt_snprintf(data, MAC_LEN, "%02x:%02x:%02x:%02x:%02x:%02x",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * 拷贝 cJSON 字符串字段到定长缓冲区, 超长截断而不是溢出
 */
static void copy_json_string(char *dst, rt_size_t dst_size, const cJSON *item)
{
    if (RT_NULL == dst || 0 == dst_size)
    {
        return;
    }

    if (RT_NULL == item || RT_NULL == item->valuestring)
    {
        dst[0] = '\0';
        return;
    }

    if (rt_strlen(item->valuestring) >= dst_size)
    {
        rt_kprintf("\033[33;22m[W/wayz]: the field is too long and has been truncated (%d >= %d)\033[0m\n",
                   (int)rt_strlen(item->valuestring), (int)dst_size);
    }

    rt_snprintf(dst, dst_size, "%s", item->valuestring);
}

static double json_number(const cJSON *item)
{
    if (RT_NULL == item || item->type != cJSON_Number)
    {
        return 0.0;
    }

    return item->valuedouble;
}

/**
 * 取数字字段, 类型不符时返回失败(用于必填字段的校验)
 */
static int json_number_ok(const cJSON *item, double *out)
{
    if (RT_NULL == item || item->type != cJSON_Number)
    {
        return RT_ERROR;
    }

    *out = item->valuedouble;

    return RT_EOK;
}

/**
 * 64 位整数转十进制字符串
 *
 * 部分 RT-Thread 的 klibc vsnprintf 不支持 "%lld"(会原样输出格式串),
 * 因此这里自行转换, 保证在任何 libc 上行为一致。
 */
static void wayz_itoa64(char *buf, rt_size_t size, rt_int64_t value)
{
    char tmp[24];
    rt_uint64_t v;
    int i = 0;
    rt_size_t len = 0;

    if (RT_NULL == buf || 0 == size)
    {
        return;
    }
    buf[0] = '\0';

    if (value < 0)
    {
        if (size > 1)
        {
            buf[len++] = '-';
        }
        v = (rt_uint64_t)(-(value + 1)) + 1;
    }
    else
    {
        v = (rt_uint64_t)value;
    }

    do
    {
        tmp[i++] = (char)('0' + (v % 10));
        v /= 10;
    } while ((0 != v) && (i < (int)sizeof(tmp)));

    while ((i > 0) && (len < size - 1))
    {
        buf[len++] = tmp[--i];
    }
    buf[len] = '\0';
}

/*
 * 两把锁:
 *   s_scan_mutex  保护扫描结果收集区(由 WLAN 线程的回调写, 由调用线程读)
 *   s_op_mutex    串行化整个定位流程, 并保护可配置的全局状态
 *                 (注入的扫描结果 / 坐标系 / 时间源)
 * 注意: 绝不能在等待扫描完成的期间持有 s_op_mutex, 回调会因此阻塞。
 */
static struct rt_mutex s_scan_mutex_obj;
static struct rt_mutex s_op_mutex_obj;
static rt_mutex_t s_scan_mutex;
static rt_mutex_t s_op_mutex;

static int wayz_lock_init(void)
{
    /*
     * 静态互斥量: RT-Thread 的 rt_mutex_init 接收 rt_mutex_t(指针),
     * 且只能在线程上下文调用 —— 组件初始化阶段(init 线程)满足该条件。
     */
    rt_mutex_init((rt_mutex_t)&s_scan_mutex_obj, "wayz_scan", RT_IPC_FLAG_PRIO);
    rt_mutex_init((rt_mutex_t)&s_op_mutex_obj, "wayz_op", RT_IPC_FLAG_PRIO);
    s_scan_mutex = (rt_mutex_t)&s_scan_mutex_obj;
    s_op_mutex = (rt_mutex_t)&s_op_mutex_obj;

    return RT_EOK;
}
INIT_COMPONENT_EXPORT(wayz_lock_init);

static void wayz_op_lock(void)
{
    rt_mutex_take(s_op_mutex, RT_WAITING_FOREVER);
}

static void wayz_op_unlock(void)
{
    rt_mutex_release(s_op_mutex);
}

static void wayz_scan_lock(void)
{
    rt_mutex_take(s_scan_mutex, RT_WAITING_FOREVER);
}

static void wayz_scan_unlock(void)
{
    rt_mutex_release(s_scan_mutex);
}

#if WAYZ_USE_WLAN
static tapinfo scan_ctx = {0};              /* 扫描结果收集区 */
static struct rt_semaphore scan_sem;
static rt_uint8_t scan_sem_ready = 0;



/**
 * 扫描结果上报回调。
 * RT-Thread 4.1.0 与 5.x 的事件负载一致: buff->data 指向 struct rt_wlan_info。
 */
static void scan_report_handler(int event, struct rt_wlan_buff *buff, void *parameter)
{
    struct rt_wlan_info *info;

    (void)parameter;

    if (RT_WLAN_EVT_SCAN_REPORT != event || RT_NULL == buff || RT_NULL == buff->data)
    {
        return;
    }

    info = (struct rt_wlan_info *)buff->data;

    wayz_scan_lock();
    if (scan_ctx.count < AP_MAX_NUMBER)
    {
        rt_memcpy(scan_ctx.tinfoAp[scan_ctx.count].mac, info->bssid, MAC_COUNT);
        scan_ctx.tinfoAp[scan_ctx.count].rssi = (rt_int8_t)info->rssi;
        /* 驱动的无效信道是 -1, 转成无符号会变成 255, 这里直接丢弃 */
        scan_ctx.tinfoAp[scan_ctx.count].channel = (info->channel > 0) ? (rt_uint8_t)info->channel : 0;
        scan_ctx.count++;
    }
    wayz_scan_unlock();
}

static void scan_done_handler(int event, struct rt_wlan_buff *buff, void *parameter)
{
    (void)buff;
    (void)parameter;

    if (RT_WLAN_EVT_SCAN_DONE == event)
    {
        rt_sem_release(&scan_sem);
    }
}

/**
 * 事件驱动的 WiFi 扫描。
 *
 * RT-Thread 5.0.0 起 WLAN 框架不再实现 rt_wlan_scan_sync(),
 * 而 4.1.0 与 5.x 都提供 RT_WLAN_EVT_SCAN_REPORT / RT_WLAN_EVT_SCAN_DONE 事件,
 * 因此统一走事件方式, 不随版本分叉。
 */
static int wayz_wlan_scan(tapinfo *out)
{
    rt_err_t err;

    wayz_scan_lock();
    rt_memset(&scan_ctx, 0, sizeof(scan_ctx));
    wayz_scan_unlock();

    if (0 == scan_sem_ready)
    {
        rt_sem_init(&scan_sem, "wayz_scan", 0, RT_IPC_FLAG_FIFO);
        scan_sem_ready = 1;
    }
    /* 清掉上一次扫描可能残留的信号量 */
    while (RT_EOK == rt_sem_take(&scan_sem, 0))
    {
        ;
    }

    rt_wlan_register_event_handler(RT_WLAN_EVT_SCAN_REPORT, scan_report_handler, RT_NULL);
    rt_wlan_register_event_handler(RT_WLAN_EVT_SCAN_DONE, scan_done_handler, RT_NULL);

    err = rt_wlan_scan();
    if (RT_EOK == err)
    {
        if (RT_EOK != rt_sem_take(&scan_sem, SCAN_WAIT_TIME_OUT))
        {
            wayz_error("wifi scan timeout.");
            err = RT_ERROR;

            /*
             * 上一次扫描超时后, 迟到的"扫描完成"事件可能落到下一次扫描的等待窗口里,
             * 让下一次扫描被提前唤醒并返回不完整的结果(AP 不足却当成成功)。
             * 这里在注销回调前先消费掉可能迟到的完成事件。
             */
            rt_sem_take(&scan_sem, SCAN_DRAIN_TIME_OUT);
        }
    }
    else
    {
        wayz_error("wifi scan failed.");
    }

    rt_wlan_unregister_event_handler(RT_WLAN_EVT_SCAN_REPORT);
    rt_wlan_unregister_event_handler(RT_WLAN_EVT_SCAN_DONE);

#if !WAYZ_RT_5X_OR_NEWER
    /* RT-Thread 4.x 在框架内部缓存了扫描结果, 需要显式释放 */
    rt_wlan_scan_result_clean();
#endif

    wayz_scan_lock();
    *out = scan_ctx;
    wayz_scan_unlock();

    return (RT_EOK == err) ? RT_EOK : RT_ERROR;
}
#endif /* WAYZ_USE_WLAN */

/**
 * 判断扫描到的 AP 是否为当前连接的 AP
 */
static int ap_is_connected(const rt_uint8_t *bssid)
{
    int i;

    if (RT_NULL == bssid || 0 == sta_connected)
    {
        return 0;
    }

    for (i = 0; i < MAC_COUNT; i++)
    {
        if (bssid[i] != sta_bssid[i])
        {
            return 0;
        }
    }

    return 1;
}

static void get_wifi_scan_info(void)
{
    int i;

    /* 注入模式: 仿真与单元测试使用, 不访问 WiFi 硬件 */
    if (s_scan_injected)
    {
        wayz_op_lock();
        aucApInfo = s_injected_ap;
        /* 注入数据里带了设备 MAC 时一并采用(无 WiFi 芯片时取不到真实 MAC) */
        for (i = 0; i < MAC_COUNT; i++)
        {
            if (0 != s_injected_ap.sta_mac[i])
            {
                aucApInfo.sta_mac[i] = s_injected_ap.sta_mac[i];
            }
        }
        rt_kprintf("using the injected scan result, %d ap(s)\n", aucApInfo.count);
        return;
    }

#if WAYZ_USE_WLAN
    rt_kprintf("\nstart to scan ap ...\n");
    if (RT_EOK != wayz_wlan_scan(&aucApInfo))
    {
        wayz_error("not found ap information ");
        return;
    }
    rt_kprintf("the scan is complete, %d ap(s) found\n", aucApInfo.count);
#else
    wayz_error("no wifi chip, please inject the scan result by wayz_set_scan_result().");
#endif
}

#if WAYZ_USE_WLAN
static void wifi_ready_handler(int event, struct rt_wlan_buff *buff, void *parameter)
{
    (void)event;
    (void)buff;
    (void)parameter;

    rt_sem_release(&net_ready);
}

/**
 * The callback of wlan disconected event
 */
static void wifi_station_disconnect_handler(int event, struct rt_wlan_buff *buff, void *parameter)
{
    (void)event;
    (void)buff;
    (void)parameter;

    rt_kprintf("disconnect from the network!\n");
}
#endif /* WAYZ_USE_WLAN */

int wayz_set_scan_result(const tapinfo *ap_info)
{
    if (RT_NULL == ap_info)
    {
        wayz_op_lock();
        rt_memset(&s_injected_ap, 0, sizeof(s_injected_ap));
        s_scan_injected = 0;
        wayz_op_unlock();
        rt_kprintf("scan result injection disabled\n");
        return RT_EOK;
    }

    if (ap_info->count > AP_MAX_NUMBER)
    {
        wayz_error("the injected scan result is too large.");
        return RT_ERROR;
    }

    wayz_op_lock();
    s_injected_ap = *ap_info;
    s_scan_injected = 1;
    wayz_op_unlock();
    rt_kprintf("scan result injection enabled, %d ap(s)\n", s_injected_ap.count);

    return RT_EOK;
}

int wifi_param_init(const char *ssid, const char *passwd, twifi_info **out)
{
    twifi_info *wlan_info = RT_NULL;

    if (RT_NULL == out || RT_NULL == ssid || RT_NULL == passwd)
    {
        wayz_error("wifi param is invalid.");
        return RT_ERROR;
    }
    *out = RT_NULL;

    wlan_info = (twifi_info *) rt_calloc(1, sizeof (twifi_info));
    if (RT_NULL == wlan_info)
    {
        wayz_error("wifi param malloc fail.");
        return RT_ERROR;
    }

    wlan_info->ssid = wayz_strdup(ssid);
    wlan_info->passwd = wayz_strdup(passwd);
    if (RT_NULL == wlan_info->ssid || RT_NULL == wlan_info->passwd)
    {
        wayz_error("wifi param strdup fail.");
        wifi_info_free(wlan_info);
        return RT_ERROR;
    }

    *out = wlan_info;

    return RT_EOK;
}

void wifi_info_free(twifi_info *wlan_info)
{
    if (RT_NULL == wlan_info)
    {
        return;
    }

    if (RT_NULL != wlan_info->ssid)
    {
        rt_free(wlan_info->ssid);
        wlan_info->ssid = RT_NULL;
    }
    if (RT_NULL != wlan_info->passwd)
    {
        rt_free(wlan_info->passwd);
        wlan_info->passwd = RT_NULL;
    }

    rt_free(wlan_info);
}



/**
 * 连接路由器, 等待网络就绪
 *
 * @return RT_EOK: success
 *         RT_ERROR: failed
 */
static int wifi_init(const char *ssid, const char *password)
{
#if !WAYZ_USE_WLAN
    /*
     * 没有 WiFi 芯片的仿真环境(如 RT-Thread 模拟器 / qemu):
     * 宿主本身已经联网, 不需要也不能再走 WiFi 连接流程。
     */
    (void)ssid;
    (void)password;
    rt_kprintf("no wifi chip, use the host network directly.\n");
    return RT_EOK;
#else
    struct rt_wlan_info info;
    int result = RT_EOK;

    rt_wlan_get_mac(aucApInfo.sta_mac);

    if (RT_TRUE == rt_wlan_is_connected())
    {
        rt_memset(&info, 0, sizeof(info));
        rt_wlan_get_info(&info);
        rt_memcpy(sta_bssid, info.bssid, MAC_COUNT);
        sta_connected = 1;
        return RT_EOK;
    }

    sta_connected = 0;
    rt_sem_init(&net_ready, "net_ready", 0, RT_IPC_FLAG_FIFO);
    /* register network ready event callback */
    rt_wlan_register_event_handler(RT_WLAN_EVT_READY, wifi_ready_handler, RT_NULL);
    /* register wlan disconnect event callback */
    rt_wlan_register_event_handler(RT_WLAN_EVT_STA_DISCONNECTED, wifi_station_disconnect_handler, RT_NULL);

    result = rt_wlan_connect(ssid, password);
    if (RT_EOK == result)
    {
        rt_memset(&info, 0, sizeof(struct rt_wlan_info));
        /* Get the information of the current connection AP */
        rt_wlan_get_info(&info);
        rt_memcpy(sta_bssid, info.bssid, MAC_COUNT);
        sta_connected = 1;

        /* waiting for IP to be got successfully  */
        if (RT_EOK != rt_sem_take(&net_ready, NET_READY_TIME_OUT))
        {
            wayz_error("wait ip got timeout!");
            result = RT_ERROR;
        }
        else
        {
            rt_kprintf("networking ready!\n");
        }
    }
    else
    {
        rt_kprintf("\033[31;22mThe AP(%s) is connect failed!\033[0m\n", ssid);
        result = RT_ERROR;
    }

    /* 先注销事件回调再销毁信号量, 避免回调访问已销毁的信号量 */
    rt_wlan_unregister_event_handler(RT_WLAN_EVT_READY);
    rt_wlan_unregister_event_handler(RT_WLAN_EVT_STA_DISCONNECTED);
    rt_sem_detach(&net_ready);

    return result;
#endif /* WAYZ_USE_WLAN */
}

/**
 * 基站类型归一
 *
 * 云端只接受 gsm / wcdma / lte / cdma / nr(小写)。
 * 传入大写或非法值时返回空串, 由调用方决定是否省略该字段。
 */
static void wayz_normalize_radio_type(const char *type, char *out, rt_size_t size)
{
    static const char *valid[] = { "gsm", "wcdma", "lte", "cdma", "nr" };
    rt_size_t i, len;

    if (RT_NULL == out || 0 == size)
    {
        return;
    }
    out[0] = '\0';

    if (RT_NULL == type)
    {
        return;
    }

    len = rt_strlen(type);
    if (len >= size)
    {
        len = size - 1;
    }
    for (i = 0; i < len; i++)
    {
        char c = type[i];

        out[i] = ((c >= 'A') && (c <= 'Z')) ? (char)(c - 'A' + 'a') : c;
    }
    out[len] = '\0';

    for (i = 0; i < sizeof(valid) / sizeof(valid[0]); i++)
    {
        if (0 == rt_strcmp(out, valid[i]))
        {
            return;
        }
    }

    /* 不在白名单内 */
    out[0] = '\0';
}

/**
 * 信道 -> 中心频率(MHz)
 *
 * 2.4GHz: 2412 + (ch - 1) * 5
 * 5GHz:   5000 + ch * 5
 * 其它(6GHz/未知)不做换算, 返回 0 表示无效。
 */
static rt_uint16_t chnTofreq(rt_uint8_t channel)
{
    if ((channel >= 1) && (channel <= 14))
    {
        return (FREQ_START + (channel - 1) * SEGMEMTATION);
    }

    if ((channel >= 32) && (channel <= 165))
    {
        return (5000 + (rt_uint16_t)channel * 5);
    }

    return 0;
}

/**
 * 组装定位上报的 JSON 请求体
 *
 * @return 堆内存字符串(调用者负责 rt_free), 失败返回 RT_NULL
 */
static char *point_cJson_handler(const tpost_data *post_data)
{
    wayz_cjson_hooks_init();
    /* declare a few. */
    cJSON *root = NULL;
    cJSON *fmt = NULL;
    cJSON *img = NULL;
    cJSON *thm = NULL;
    cJSON *fld = NULL;
    int i = 0;
    rt_uint8_t wifi_count = 0;
    char macBuf[MAC_LEN] = {0};
    char idBuf[40] = {0};      /* "38efe26e-bcd8-" + 12 位十六进制 + '\0' */
    char *cJsonBuffer = NULL;
    char *buffer = NULL;
    unsigned char state = 0;
    uint64_t time = 0;

    root = cJSON_CreateObject();
    if (RT_NULL == root)
    {
        wayz_error("create cjson object failed.");
        return RT_NULL;
    }

    time = wayz_get_timestamp_ms();
    if (0 == time)
    {
        time = TIMESTAMP_DEL;
    }

    rt_snprintf(idBuf, sizeof(idBuf), "38efe26e-bcd8-"MACPRINTID, PRINT(aucApInfo.sta_mac, 0));

    cJSON_AddNumberToObject(root, "timestamp", (double)time);
    cJSON_AddItemToObject(root, "id", cJSON_CreateString(idBuf));
    cJSON_AddItemToObject(root, "asset", fmt = cJSON_CreateObject());
    /*
     * asset.id 必须与注册设备时使用的 id 保持一致(注册接口用的是无冒号的 MAC),
     * 否则平台无法把这条轨迹点关联到已注册的设备。
     */
    rt_snprintf(macBuf, sizeof(macBuf), ""MACPRINTID, PRINT(aucApInfo.sta_mac, 0));
    cJSON_AddItemToObject(fmt, "id", cJSON_CreateString(macBuf));
    cJSON_AddItemToObject(root, "location", img = cJSON_CreateObject());
    cJSON_AddNumberToObject(img, "timestamp", (double)time);

    /* add wifis: 云端要求 4 ~ 10 个 */
    if (aucApInfo.count >= AP_MIN_UPLOAD)
    {
        rt_uint8_t order[AP_MAX_NUMBER];
        rt_uint8_t n = aucApInfo.count;

        /*
         * 云端文档建议"提供至少 10 个 WiFi 信息以确保定位成功率",
         * 因此按信号强度从强到弱排序, 取前 AP_MAX_UPLOAD 个,
         * 而不是简单取扫描顺序的前 N 个(驱动返回的顺序通常不是按强度排的)。
         * 注意: RSSI 是负值, 越接近 0 越强, 所以按 rssi 降序排,
         *       不能按 abs(rssi) 排(那会把最弱的排到最前)。
         */
        for (i = 0; i < n; i++)
        {
            order[i] = i;
        }
        for (i = 1; i < n; i++)
        {
            rt_uint8_t key = order[i];
            int key_rssi = (int)aucApInfo.tinfoAp[key].rssi;
            int j = i;

            while ((j > 0) && ((int)aucApInfo.tinfoAp[order[j - 1]].rssi < key_rssi))
            {
                order[j] = order[j - 1];
                j--;
            }
            order[j] = key;
        }

        wifi_count = (n > AP_MAX_UPLOAD) ? AP_MAX_UPLOAD : n;

        cJSON_AddItemToObject(img, "wifis", thm = cJSON_CreateArray());
        for (i = 0; i < wifi_count; i++)
        {
            const tapStructInfo *ap = &aucApInfo.tinfoAp[order[i]];
            rt_uint16_t freq = chnTofreq(ap->channel);

            cJSON_AddItemToArray(thm, fld = cJSON_CreateObject());
            rt_snprintf(macBuf, sizeof(macBuf), ""MACPRINT, PRINT(ap->mac, 0));
            cJSON_AddStringToObject(fld, "macAddress", macBuf);
            cJSON_AddNumberToObject(fld, "timestamp", (double)time);
            cJSON_AddNumberToObject(fld, "signalStrength", abs((int)ap->rssi));
            if ((ap->channel > 0) && (0 != freq))
            {
                cJSON_AddNumberToObject(fld, "channel", ap->channel);
                cJSON_AddNumberToObject(fld, "frequency", freq);
            }
            cJSON_AddBoolToObject(fld, "connected", ap_is_connected(ap->mac));
        }
        state = 1;
    }

    if (RT_NULL != post_data)
    {
        /* add gnss */
        if (RT_EOK != check_double_equal_zero(post_data->gnss.lng) &&
            RT_EOK != check_double_equal_zero(post_data->gnss.lat) &&
            RT_EOK != check_float_equal_zero(post_data->gnss.accuracy))
        {
            cJSON_AddItemToObject(img, "gnss", thm = cJSON_CreateObject());
            cJSON_AddNumberToObject(thm, "timestamp", (double)post_data->gnss.timestamp);
            cJSON_AddItemToObject(thm, "point", fld = cJSON_CreateObject());
            cJSON_AddNumberToObject(fld, "longitude", post_data->gnss.lng);
            cJSON_AddNumberToObject(fld, "latitude", post_data->gnss.lat);
            if (RT_EOK != check_float_equal_zero(post_data->gnss.altitude))
            {
                cJSON_AddNumberToObject(fld, "altitude", post_data->gnss.altitude);
            }
            cJSON_AddNumberToObject(thm, "accuracy", post_data->gnss.accuracy);
            state = 1;
        }
        /* add cellulars */
        if (0 != post_data->cellulars.count)
        {
            cJSON_AddItemToObject(img, "cellulars", thm = cJSON_CreateArray());

            for (i = 0; i < post_data->cellulars.count && i < CELL_COUNT; i ++)
            {
                char radio[8] = {0};

                cJSON_AddItemToArray(thm, fld = cJSON_CreateObject());
                cJSON_AddNumberToObject(fld, "timestamp", (double)post_data->cellulars.cell[i].timestamp);
                cJSON_AddNumberToObject(fld, "cellId", post_data->cellulars.cell[i].cell_id);
                wayz_normalize_radio_type(post_data->cellulars.cell[i].radio_type, radio, sizeof(radio));
                if ('\0' != radio[0])
                {
                    cJSON_AddStringToObject(fld, "radioType", radio);
                }
                cJSON_AddNumberToObject(fld, "mobileCountryCode", post_data->cellulars.cell[i].mcc);
                cJSON_AddNumberToObject(fld, "mobileNetworkCode", post_data->cellulars.cell[i].mnc);
                cJSON_AddNumberToObject(fld, "locationAreaCode", post_data->cellulars.cell[i].lac);
                /* 云端要求取信号强度的绝对值 */
                cJSON_AddNumberToObject(fld, "signalStrength", abs((int)post_data->cellulars.cell[i].signal_strength));
                cJSON_AddBoolToObject(fld, "connected", post_data->cellulars.cell[i].connected);
            }
            state = 1;
        }
    }

    cJsonBuffer = cJSON_Print(root);
    cJSON_Delete(root);
    if (RT_NULL == cJsonBuffer)
    {
        wayz_error("print cjson failed.");
        return RT_NULL;
    }

    if (1 != state)
    {
        rt_free(cJsonBuffer);
        wayz_error("location No wifi, GNSS, cellulars signal data");
        return RT_NULL;
    }

    buffer = (char *)rt_malloc(rt_strlen(cJsonBuffer) + 1);
    if (RT_NULL == buffer)
    {
        wayz_error("point_cJson_handler create malloc failure.");
        rt_free(cJsonBuffer);
        return RT_NULL;
    }

    rt_snprintf(buffer, rt_strlen(cJsonBuffer) + 1, "%s", cJsonBuffer);
    rt_free(cJsonBuffer);

    return buffer; // 需要free
}


/**
 * 统一的 HTTP 请求封装, 屏蔽 webclient 各版本 session 接口的差异
 *
 * @param url  请求地址
 * @param body 请求体, RT_NULL 表示 GET
 * @param resp 响应数据, 由调用者 web_free
 * @param status HTTP 状态码
 *
 * @return >=0: 响应长度
 *         <0 : 失败
 */
static int wayz_http_request(const char *url, const char *body, unsigned char **resp, int *status)
{
    struct webclient_session *session = RT_NULL;
    int rc = -RT_ERROR;
    int resp_len = 0;

    if (RT_NULL == url || RT_NULL == resp)
    {
        return -RT_ERROR;
    }

    *resp = RT_NULL;
    if (RT_NULL != status)
    {
        *status = 0;
    }

    session = webclient_session_create(WEBCLIENT_HEADER_BUFSZ);
    if (RT_NULL == session)
    {
        wayz_error("webclient session create failed.");
        return -RT_ERROR;
    }

    if (RT_NULL != body)
    {
        /*
         * 云端要求 Content-Type: application/json,
         * webclient 在缺失时会补 application/octet-stream 导致服务端拒收。
         * Content-Length 需要自行添加。
         */
        if (webclient_header_fields_add(session, "Content-Type: application/json\r\n") < 0 ||
            webclient_header_fields_add(session, "Content-Length: %d\r\n", rt_strlen(body)) < 0)
        {
            wayz_error("webclient header add failed.");
            goto __exit;
        }
    }

    if (RT_NULL == body)
    {
        rc = webclient_get(session, url);
    }
    else
    {
#if WAYZ_WEBCLIENT_NEW_API
        rc = webclient_post(session, url, body, rt_strlen(body));
#else
        rc = webclient_post(session, url, body);
#endif
    }

    if (RT_NULL != status)
    {
        *status = webclient_resp_status_get(session);
    }

    /* 无论状态码如何都把响应体读出来, 服务端的错误信息在 body 里 */
#if WAYZ_WEBCLIENT_NEW_API
    {
        size_t len = 0;
        resp_len = (int)webclient_response(session, (void **)resp, &len);
    }
#else
    resp_len = webclient_response(session, resp);
#endif

    if (resp_len <= 0)
    {
        if (RT_NULL != status && 200 != *status)
        {
            rt_kprintf("\033[31;22m[E/wayz]: http request failed, status %d\033[0m\n", *status);
        }
        else
        {
            wayz_error("http response is empty.");
        }
        rc = -RT_ERROR;
        goto __exit;
    }

    rc = resp_len;

__exit:
    if (RT_NULL != session)
    {
        webclient_close(session);
    }

    if (rc < 0 && RT_NULL != *resp)
    {
        web_free(*resp);
        *resp = RT_NULL;
    }

    return rc;
}



/**
 * parse point cjson handler
 *
 * @return RT_EOK: 解析成功
 *         RT_ERROR: 解析失败
 */
static int parse_point_cJson_handler(const char *data, tlocation_info *location)
{
    wayz_cjson_hooks_init();
    cJSON *root = NULL;
    cJSON *object = NULL;
    cJSON *position = NULL;
    cJSON *point = NULL;
    cJSON *place = NULL;
    cJSON *category = NULL;
    cJSON *address = NULL;
    cJSON *context = NULL;
    cJSON *item = NULL;
    int rc = RT_ERROR;
    int index = 0;

    if (RT_NULL == data || RT_NULL == location)
    {
        return WAYZ_ERR_PARAM;
    }

    /*
     * 输出结构体完全由本函数填充: 先清零, 避免上一次调用的残留
     * (POI/地址/行政区划) 被当成本次结果返回。
     */
    rt_memset(location, 0, sizeof(*location));

    root = cJSON_Parse(data);
    if (RT_NULL == root)
    {
        rt_kprintf("\033[31;22m[E/wayz]Error before: [%s]\033[0m\n", cJSON_GetErrorPtr());
        return WAYZ_ERR_PARSE;
    }

    object = cJSON_GetObjectItem(root, "location");
    if (RT_NULL == object)
    {
        wayz_error("location failure\r\n");
        rc = WAYZ_ERR_NO_SIGNAL;
        goto __exit;
    }

    position = cJSON_GetObjectItem(object, "position");
    if (RT_NULL == position)
    {
        /*
         * 云端在无线信号不足时会接受上报但只回一个时间戳, 不返回 position。
         * 例如使用未经定位库收录的 AP 时就是这个结果。
         */
        wayz_error("the cloud did not return a position, maybe the wifi signal is not enough.\r\n");
        rc = WAYZ_ERR_NO_SIGNAL;
        goto __exit;
    }

    item = cJSON_GetObjectItem(position, "timestamp");
    if (RT_NULL != item)
    {
        double ts = json_number(item);

        /* double -> int64 的越界转换是未定义行为, 先做范围检查 */
        if ((ts > -9.0e18) && (ts < 9.0e18))
        {
            wayz_itoa64(location->timestamp, sizeof(location->timestamp), (rt_int64_t)ts);
        }
    }

    item = cJSON_GetObjectItem(position, "source");
    copy_json_string(location->source, sizeof(location->source), item);

    item = cJSON_GetObjectItem(position, "spatialReference");
    copy_json_string(location->spatial_reference, sizeof(location->spatial_reference), item);

    item = cJSON_GetObjectItem(position, "accuracy");
    if (RT_NULL != item)
    {
        location->accuracy = (float)json_number(item);
    }

    point = cJSON_GetObjectItem(position, "point");
    if (RT_NULL == point)
    {
        wayz_error("point failure\r\n");
        rc = WAYZ_ERR_PARSE;
        goto __exit;
    }

    /*
     * 坐标由云端按请求参数 response_sprf 给出(坐标系见 spatial_reference),
     * 组件不做本地坐标换算。
     * 经纬度是必填项: 缺失或类型不符时必须报错, 不能默默当成 (0,0) 的有效定位。
     */
    {
        double lng = 0.0, lat = 0.0;

        if ((RT_EOK != json_number_ok(cJSON_GetObjectItem(point, "longitude"), &lng)) ||
            (RT_EOK != json_number_ok(cJSON_GetObjectItem(point, "latitude"), &lat)))
        {
            wayz_error("the position has no valid longitude/latitude.\r\n");
            rc = WAYZ_ERR_PARSE;
            goto __exit;
        }

        if ((lng > 180.0) || (lng < -180.0) || (lat > 90.0) || (lat < -90.0))
        {
            wayz_error("the position longitude/latitude is out of range.\r\n");
            rc = WAYZ_ERR_PARSE;
            goto __exit;
        }

        if ((0.0 == lng) && (0.0 == lat))
        {
            wayz_error("the position is (0, 0), treat it as invalid.\r\n");
            rc = WAYZ_ERR_PARSE;
            goto __exit;
        }

        location->point.longitude = lng;
        location->point.latitude = lat;
    }

    place = cJSON_GetObjectItem(object, "place");
    if (RT_NULL != place)
    {
        item = cJSON_GetObjectItem(place, "id");
        copy_json_string(location->place.id, sizeof(location->place.id), item);
        item = cJSON_GetObjectItem(place, "type");
        copy_json_string(location->place.type, sizeof(location->place.type), item);
        item = cJSON_GetObjectItem(place, "name");
        copy_json_string(location->place.name, sizeof(location->place.name), item);

        item = cJSON_GetObjectItem(place, "distance");
        if (RT_NULL != item)
        {
            cJSON *line = cJSON_GetObjectItem(item, "line");
            if (RT_NULL != line)
            {
                location->place.distance = json_number(line);
            }
        }

        /* 兼容旧版本的 POI 分类信息 */
        category = cJSON_GetObjectItem(place, "categories");
        if (RT_NULL != category)
        {
            item = cJSON_GetArrayItem(category, 0);
            if (RT_NULL != item)
            {
                cJSON *sub = cJSON_GetObjectItem(item, "id");
                copy_json_string(location->place.category.id, sizeof(location->place.category.id), sub);
                sub = cJSON_GetObjectItem(item, "name");
                copy_json_string(location->place.category.name, sizeof(location->place.category.name), sub);
            }
        }
    }

    address = cJSON_GetObjectItem(object, "address");
    if (RT_NULL != address)
    {
        item = cJSON_GetObjectItem(address, "name");
        copy_json_string(location->address, sizeof(location->address), item);

        context = cJSON_GetObjectItem(address, "context");
        if (RT_NULL != context)
        {
            cJSON *ctx_item = RT_NULL;

            cJSON_ArrayForEach(ctx_item, context)
            {
                if (index >= CONTEXT_COUNT)
                {
                    break;
                }

                item = cJSON_GetObjectItem(ctx_item, "type");
                copy_json_string(location->context[index].type, sizeof(location->context[index].type), item);
                item = cJSON_GetObjectItem(ctx_item, "name");
                copy_json_string(location->context[index].name, sizeof(location->context[index].name), item);
                item = cJSON_GetObjectItem(ctx_item, "code");
                copy_json_string(location->context[index].code, sizeof(location->context[index].code), item);
                index++;
            }
            location->context_count = (rt_uint8_t)index;
        }
    }

    rc = RT_EOK;

__exit:
    cJSON_Delete(root);

    return rc;
}

/**
 * @param location: print location info
 *
 * @warning rt_kprintf no print float/double
 *          printf is ok
 *
*/
/**
 * 把 double 格式化成定点字符串(保留 6 位小数)
 *
 * 不使用 libc printf 的 %f:
 *   - RT-Thread 自带的 libc(klibc)不支持浮点格式化, 会原样打印 "%f";
 *   - libc printf 只在链了 newlib 的 BSP 上存在, 且默认不接 RT-Thread 控制台,
 *     经纬度会打印不到串口上; 不带 newlib 的 BSP 还会链接失败。
 *
 * @param value  待格式化的数值
 * @param buf    输出缓冲
 * @param size   缓冲大小
 */
static void wayz_fmt_fixed6(double value, char *buf, rt_size_t size)
{
    rt_uint64_t ip, fp;
    char ip_digits[24];
    int ip_len = 0;
    int i;
    rt_size_t out = 0;
    int neg = 0;

    if (RT_NULL == buf || 0 == size)
    {
        return;
    }
    buf[0] = '\0';

    if (value < 0.0)
    {
        neg = 1;
        value = -value;
    }
    if (!(value < 1.0e12))
    {
        value = 0.0;
    }

    ip = (rt_uint64_t)value;
    fp = (rt_uint64_t)((value - (double)ip) * 1000000.0 + 0.5);
    if (fp > 999999)
    {
        fp = 999999;
    }

    do
    {
        ip_digits[ip_len++] = (char)('0' + (ip % 10));
        ip /= 10;
    } while ((ip > 0) && (ip_len < (int)sizeof(ip_digits)));

    if (neg && (out + 1 < size))
    {
        buf[out++] = '-';
    }
    for (i = ip_len - 1; i >= 0; i--)
    {
        if (out + 1 < size)
        {
            buf[out++] = ip_digits[i];
        }
    }

    if (out + 7 < size)
    {
        buf[out++] = '.';
        buf[out++] = (char)('0' + (fp / 100000));
        buf[out++] = (char)('0' + ((fp / 10000) % 10));
        buf[out++] = (char)('0' + ((fp / 1000) % 10));
        buf[out++] = (char)('0' + ((fp / 100) % 10));
        buf[out++] = (char)('0' + ((fp / 10) % 10));
        buf[out++] = (char)('0' + (fp % 10));
    }

    buf[out] = '\0';
}

void location_print(const tlocation_info *location)
{
    char lat_buf[32] = {0};
    char lng_buf[32] = {0};
    int i;

    if (RT_NULL == location)
    {
        return;
    }

    wayz_fmt_fixed6(location->point.latitude, lat_buf, sizeof(lat_buf));
    wayz_fmt_fixed6(location->point.longitude, lng_buf, sizeof(lng_buf));

    rt_kprintf("-------------------location result-------------------------------\r\n");
    rt_kprintf("timestamp: %s ms \r\n", location->timestamp);
    rt_kprintf("point(%s):\r\n\tlatitude:%s\r\n\tlongitude:%s\r\n",
               location->spatial_reference, lat_buf, lng_buf);
    rt_kprintf("accuracy: %d m\r\n", (int)location->accuracy);
    rt_kprintf("source: %s, spatialReference: %s\r\n",
               location->source, location->spatial_reference);
    rt_kprintf("address: %s\r\n", location->address);

    for (i = 0; i < location->context_count && i < CONTEXT_COUNT; i++)
    {
        rt_kprintf("context: %s = %s (%s)\r\n", location->context[i].type,
                   location->context[i].name, location->context[i].code);
    }

    rt_kprintf("POI: {\"id\": %s,\"type\": %s,\"name\": %s}\r\n",
               location->place.id, location->place.type, location->place.name);
    if (location->place.category.name[0] != '\0')
    {
        rt_kprintf("POI category: {\"id\": %s,\"name\": %s}\r\n",
                   location->place.category.id, location->place.category.name);
    }
    rt_kprintf("-------------------location result end---------------------------\r\n");
}

/**
 * 检查云端访问密钥
 *
 * @return RT_EOK: valid
 *         RT_ERROR: invalid
 */
static int wayz_key_check(const char *key)
{
    if (RT_NULL == key || key[0] == '\0')
    {
        wayz_error("Visiting the website key failure");
        return RT_ERROR;
    }

    if (rt_strlen(key) >= KEY_LENGTH)
    {
        wayz_error("Visiting the website key is too long");
        return RT_ERROR;
    }

    return RT_EOK;
}


/**
 * 按 HTTP 状态码给出错误码, 便于上层区分"可重试"与"必须换密钥"
 */
static int wayz_status_to_error(int status)
{
    switch (status)
    {
    case 400:
    case 404:
        return WAYZ_ERR_PARAM;
    case 401:
    case 403:
        return WAYZ_ERR_AUTH;
    case 429:
        return WAYZ_ERR_RATE_LIMIT;
    default:
        break;
    }

    if (status >= 500)
    {
        return WAYZ_ERR_SERVER;
    }

    return WAYZ_ERR_NET;
}

/**
 * 打印云端返回的错误信息, 便于定位是密钥问题还是参数问题
 */
static void wayz_print_cloud_message(const char *body)
{
    cJSON *root = RT_NULL;
    cJSON *item = RT_NULL;

    if (RT_NULL == body)
    {
        return;
    }

    wayz_cjson_hooks_init();
    root = cJSON_Parse(body);
    if (RT_NULL == root)
    {
        return;
    }

    item = cJSON_GetObjectItem(root, "message");
    if (RT_NULL != item && RT_NULL != item->valuestring)
    {
        rt_kprintf("\033[33;22m[W/wayz]: cloud says: %s\033[0m\n", item->valuestring);
    }

    cJSON_Delete(root);
}

/**
 * Get the positioning result function
 *
 * @param wlan_info Wifi related information
 * @param key Visiting the website key
 * @param post_data post gnss and cellulars data, obtain positioning results
 * @param location get location result
 *
 * @return RT_EOK: success
 *         WAYZ_ERR_*: 失败, 见 inc/wayz_pos.h 的错误码说明
 */
int wayz_connect(twifi_info *wlan_info)
{
    if (RT_NULL == wlan_info || RT_NULL == wlan_info->ssid || RT_NULL == wlan_info->passwd)
    {
        wayz_error("the wifi info is invalid.");
        return WAYZ_ERR_PARAM;
    }

    return wifi_init(wlan_info->ssid, wlan_info->passwd);
}

int get_position_info(twifi_info *wlan_info, const char *key, const tpost_data *post_data, \
                      tlocation_info *location)
{
    char url[URL_BUFFER_SIZE] = {0};
    unsigned char *buffer = RT_NULL;
    char *cJsonBuffer = NULL;
    int status = 0;
    int result = WAYZ_ERR_INTERNAL;
    int len = 0;

    if (RT_NULL == wlan_info || RT_NULL == location)
    {
        wayz_error("the wifi info or location is invalid.");
        return WAYZ_ERR_PARAM;
    }

    if (RT_EOK != wayz_key_check(key))
    {
        return WAYZ_ERR_PARAM;
    }

    /*
     * 串行化整个定位流程: 并发调用会互相覆盖扫描结果与坐标系,
     * 且其中一个调用会抢走另一个的"扫描完成"事件。
     * (扫描等待期间不持有 s_scan_mutex, 回调不会被阻塞)
     */
    wayz_op_lock();

    /*
     * 1 connect to the internet
     * 2 scan wifi info
     * 顺序很重要: 多数 WLAN 驱动在未关联/未连接时扫描结果为空或受限,
     * 先联网再扫描才能拿到可用的 AP 列表。
     */
    if (RT_ERROR == wifi_init(wlan_info->ssid, wlan_info->passwd))
    {
        wayz_error("connect to the internet failure\r\n");
        result = WAYZ_ERR_NET;
        goto __exit;
    }

    get_wifi_scan_info();

    /* 3 Upload data to get positioning results */
    cJsonBuffer = point_cJson_handler(post_data);
    if (RT_NULL == cJsonBuffer)
    {
        wayz_error("location No wifi, GNSS, cellulars signal data");
        result = WAYZ_ERR_NO_SIGNAL;
        goto __exit;
    }

    len = rt_snprintf(url, sizeof(url), "%s?response_sprf=%s&access_key=%s",
                      wayz_track_points_url(), s_response_sprf, key);
    if ((len < 0) || ((rt_size_t)len >= sizeof(url)))
    {
        wayz_error("the request url is too long.");
        result = WAYZ_ERR_PARAM;
        goto __exit;
    }

    len = wayz_http_request(url, cJsonBuffer, &buffer, &status);
    if (len < 0)
    {
        result = (status > 0) ? wayz_status_to_error(status) : WAYZ_ERR_NET;
        rt_kprintf("\033[31;22m[E/wayz]: http request failed, status %d\033[0m\n", status);
        goto __exit;
    }

    if (200 != status)
    {
        result = wayz_status_to_error(status);
        rt_kprintf("\033[31;22m[E/wayz]: http request failed, status %d\033[0m\n", status);
        wayz_print_cloud_message((char *)buffer);
        goto __exit;
    }

    result = parse_point_cJson_handler((char *)buffer, location);
    if (RT_EOK != result)
    {
        wayz_error("parse the location response failed.");
        goto __exit;
    }

    result = RT_EOK;

__exit:
    if (RT_NULL != buffer)
    {
        web_free(buffer);
    }
    if (RT_NULL != cJsonBuffer)
    {
        rt_free(cJsonBuffer);
    }

    wayz_op_unlock();

    return result;
}
