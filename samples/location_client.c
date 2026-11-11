
#include <rtthread.h>
#include <wayz_pos.h>

#ifdef PKG_WAYZ_IOTKIT_WIFI_SSID
#define  WAYZ_WIFI_SSID      PKG_WAYZ_IOTKIT_WIFI_SSID
#else
#define  WAYZ_WIFI_SSID      "thread"
#endif

#ifdef PKG_WAYZ_IOTKIT_WIFI_PASSWORD
#define  WAYZ_WIFI_PWD      PKG_WAYZ_IOTKIT_WIFI_PASSWORD
#else
#define  WAYZ_WIFI_PWD      "12345678"
#endif

#define  POINT_FRQ      3000
#define  THREAD_STACK   (12 * 1024)

#define  ACCESS_KEY     "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" // 需要申请

/*
 * 运行时保存的 access_key, 可用 wayz_key 命令设置, 避免把密钥写死在源码里
 */
static char s_access_key[KEY_LENGTH] = {0};

static const char *location_access_key(void)
{
    if ('\0' != s_access_key[0])
    {
        return s_access_key;
    }

#ifdef PKG_WAYZ_IOTKIT_SIM
    {
        /* 仿真环境(POSIX): 允许从环境变量读取, 便于自动化验证 */
        extern char *getenv(const char *name);
        char *env = getenv("WAYZ_ACCESS_KEY");

        if (RT_NULL != env && '\0' != env[0])
        {
            return env;
        }
    }
#endif

    return ACCESS_KEY;
}

#ifdef PKG_WAYZ_IOTKIT_SIM
static void sim_auto_inject(void);
#if defined(PKG_WAYZ_IOTKIT_NET_DIAG)
static void sim_net_diag(void);
#endif
#endif

static void location_client_entry(void *parameter)
{
    char mac_addr[MAC_LEN] = {0};
    twifi_info *wlan_info = RT_NULL;
    tlocation_info location = {0};
    const char *key = location_access_key();
    int ret;

#ifdef PKG_WAYZ_IOTKIT_SIM
#if defined(PKG_WAYZ_IOTKIT_NET_DIAG)
    sim_net_diag();
#endif
    sim_auto_inject();
    {
        /* 仿真模式: 允许用环境变量选择云端返回的坐标系(gcj02/wgs84/bd09) */
        extern char *getenv(const char *name);
        char *sprf = getenv("WAYZ_SPRF");

        if (RT_NULL != sprf && '\0' != sprf[0])
        {
            wayz_set_response_sprf(sprf);
            rt_kprintf("simulation mode: response_sprf = %s\n", sprf);
        }
    }
#endif

    get_sta_mac_addr(mac_addr);
    rt_kprintf("station mac : %s \r\n", mac_addr);

    if (RT_EOK != wifi_param_init(WAYZ_WIFI_SSID, WAYZ_WIFI_PWD, &wlan_info))
    {
        rt_kprintf("\033[31;22mwifi param init failure. \033[0m\n");
        return;
    }

    /*
     * 云端在收到第一个轨迹点时会自动创建设备, 无需再调用注册接口,
     * 这里只需要把设备连上网络。
     */
    ret = wayz_connect(wlan_info);
    if (RT_EOK != ret)
    {
        rt_kprintf("\033[31;22mconnect to the internet failure. \033[0m\n");
        wifi_info_free(wlan_info);
        return;
    }

    while (1)
    {
        ret = get_position_info(wlan_info, key, RT_NULL, &location);
        if (RT_EOK == ret)
        {
            location_print(&location);
        }
        else
        {
            rt_kprintf("\033[31;22mthe device failed to obtain latitude and longitude information.\033[0m\n");
        }

        rt_thread_mdelay(POINT_FRQ);
        rt_memset(&location, 0, sizeof (location));
    }
}

int location_client_start(void)
{
    rt_thread_t tid;

    tid = rt_thread_create("loc_client", location_client_entry, RT_NULL,
                           THREAD_STACK, RT_THREAD_PRIORITY_MAX / 3 - 1, 5);
    if (tid)
    {
        rt_thread_startup(tid);
        return RT_EOK;
    }

    return -RT_ERROR;
}

/*
 * ---------------------------------------------------------------------------
 * 仿真辅助: 注入 WiFi 扫描结果
 *
 * 没有 WiFi 硬件的仿真环境(qemu-vexpress-a9 / simulator)可以用下面的命令
 * 注入一组固定的 AP, 从而在不依赖硬件的情况下驱动完整的定位流程。
 *
 *   wayz_scan_stub 88:25:93:6f:df:bb,-31,1 94:77:2b:65:1a:00,-43,6 ...
 *   wayz_scan_stub off        恢复真实扫描
 * ---------------------------------------------------------------------------
 */

static const char *find_char(const char *str, char ch)
{
    while (RT_NULL != str && *str != '\0')
    {
        if (*str == ch)
        {
            return str;
        }
        str++;
    }

    return RT_NULL;
}

static int parse_int(const char *str)
{
    int value = 0;
    int negative = 0;

    if (RT_NULL == str)
    {
        return 0;
    }

    if ('-' == *str)
    {
        negative = 1;
        str++;
    }

    while (*str >= '0' && *str <= '9')
    {
        value = value * 10 + (*str - '0');
        str++;
    }

    return negative ? -value : value;
}

/**
 * 把 "xx:xx:xx:xx:xx:xx" 转换为 6 字节 MAC
 */
static int mac_string_to_bytes(const char *str, rt_uint8_t *mac)
{
    int i, j;

    if (RT_NULL == str || RT_NULL == mac)
    {
        return RT_ERROR;
    }

    if (rt_strlen(str) < (rt_size_t)(MAC_COUNT * 3 - 1))
    {
        return RT_ERROR;
    }

    for (i = 0; i < MAC_COUNT; i++)
    {
        const char *p = str + i * 3;
        rt_uint8_t value = 0;

        for (j = 0; j < 2; j++)
        {
            char c = p[j];
            rt_uint8_t digit;

            if (c >= '0' && c <= '9')
            {
                digit = c - '0';
            }
            else if (c >= 'a' && c <= 'f')
            {
                digit = c - 'a' + 10;
            }
            else if (c >= 'A' && c <= 'F')
            {
                digit = c - 'A' + 10;
            }
            else
            {
                return RT_ERROR;
            }
            value = (rt_uint8_t)((value << 4) | digit);
        }
        mac[i] = value;
    }

    return RT_EOK;
}

/**
 * 解析 "mac,rssi,channel"
 */
/**
 * 解析 "mac,rssi" 或 "mac,rssi,channel"
 */
static int parse_ap_arg(const char *arg, tapStructInfo *info)
{
    const char *p1, *p2;
    char mac[24] = {0};
    rt_size_t len;

    if (RT_NULL == arg || RT_NULL == info)
    {
        return RT_ERROR;
    }

    p1 = find_char(arg, ',');
    if (RT_NULL == p1)
    {
        return RT_ERROR;
    }

    len = (rt_size_t)(p1 - arg);
    if (len >= sizeof(mac))
    {
        return RT_ERROR;
    }
    rt_memcpy(mac, arg, len);
    mac[len] = '\0';

    if (RT_EOK != mac_string_to_bytes(mac, info->mac))
    {
        return RT_ERROR;
    }

    info->rssi = (rt_int8_t)parse_int(p1 + 1);

    /* 信道可选, 缺省为 0(云端不依赖信道) */
    p2 = find_char(p1 + 1, ',');
    info->channel = (RT_NULL == p2) ? 0 : (rt_uint8_t)parse_int(p2 + 1);

    return RT_EOK;
}

static tapinfo s_stub_ap = {0};
static rt_uint8_t s_stub_mac[MAC_COUNT] = {0};
static rt_uint8_t s_stub_ready = 0;

/**
 * 把当前的桩数据交给组件
 */
static void stub_apply(void)
{
    if (0 == s_stub_ready)
    {
        return;
    }

    rt_memcpy(s_stub_ap.sta_mac, s_stub_mac, MAC_COUNT);
    wayz_set_scan_result(&s_stub_ap);
}

static void cmd_wayz_scan_stub(int argc, char **argv)
{
    int i;

    if (argc < 2)
    {
        rt_kprintf("usage: wayz_scan_stub <mac,rssi,channel> [mac,rssi,channel ...]\r\n");
        rt_kprintf("       wayz_scan_stub off    restore the real wifi scan\r\n");
        return;
    }

    if (0 == rt_strcmp(argv[1], "off"))
    {
        s_stub_ready = 0;
        wayz_set_scan_result(RT_NULL);
        rt_kprintf("scan result injection disabled\r\n");
        return;
    }

    s_stub_ap.count = 0;
    for (i = 1; i < argc && s_stub_ap.count < AP_MAX_NUMBER; i++)
    {
        if (RT_EOK != parse_ap_arg(argv[i], &s_stub_ap.tinfoAp[s_stub_ap.count]))
        {
            rt_kprintf("invalid ap: %s\r\n", argv[i]);
            continue;
        }
        s_stub_ap.count++;
    }

    if (0 == s_stub_ap.count)
    {
        rt_kprintf("no valid ap found\r\n");
        return;
    }

    s_stub_ready = 1;
    stub_apply();
    rt_kprintf("%d ap(s) injected\r\n", s_stub_ap.count);
}

/**
 * 设置设备 MAC(仿真时没有 WiFi 芯片, 取不到真实 MAC)
 */
static void cmd_wayz_dev_mac(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("usage: wayz_dev_mac <xx:xx:xx:xx:xx:xx>\r\n");
        return;
    }

    if (RT_EOK != mac_string_to_bytes(argv[1], s_stub_mac))
    {
        rt_kprintf("invalid mac: %s\r\n", argv[1]);
        return;
    }

    stub_apply();
    rt_kprintf("device mac set to %s\r\n", argv[1]);
}

/**
 * 运行时设置 access_key, 避免把密钥写死在源码中
 */
static void cmd_wayz_key(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("usage: wayz_key <access_key>\r\n");
        return;
    }

    if (rt_strlen(argv[1]) >= KEY_LENGTH)
    {
        rt_kprintf("the access key is too long\r\n");
        return;
    }

    rt_snprintf(s_access_key, sizeof(s_access_key), "%s", argv[1]);
    rt_kprintf("access key updated\r\n");
}


#if defined(PKG_WAYZ_IOTKIT_SIM) && defined(PKG_WAYZ_IOTKIT_NET_DIAG)
/* 网络连通性诊断(排查 socket/DNS 问题用, 默认不编译) */
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
static void sim_net_diag(void)
{
    struct hostent *he;
    struct sockaddr_in addr;
    int sock, rc;

    rt_kprintf("diag: resolving api.newayz.com ...\n");
    he = gethostbyname("api.newayz.com");
    rt_kprintf("diag: gethostbyname -> %p\n", (void *)he);
    if (RT_NULL == he) return;
    rt_kprintf("diag: ip=%d.%d.%d.%d\n",
        (unsigned char)he->h_addr_list[0][0], (unsigned char)he->h_addr_list[0][1],
        (unsigned char)he->h_addr_list[0][2], (unsigned char)he->h_addr_list[0][3]);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    rt_kprintf("diag: socket -> %d\n", sock);
    if (sock < 0) return;

    rt_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    addr.sin_addr.s_addr = *(rt_uint32_t *)he->h_addr_list[0];
    rc = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    rt_kprintf("diag: connect(80) -> %d\n", rc);
    if (0 == rc)
    {
        static const char req[] =
            "GET / HTTP/1.1\r\nHost: api.newayz.com\r\nConnection: close\r\n\r\n";
        char buf[128];
        int n;

        n = send(sock, req, (int)(sizeof(req) - 1), 0);
        rt_kprintf("diag: send -> %d\n", n);
        n = recv(sock, buf, sizeof(buf) - 1, 0);
        rt_kprintf("diag: recv -> %d\n", n);
        if (n > 0)
        {
            buf[n] = '\0';
            rt_kprintf("diag: resp head: %.60s\n", buf);
        }
    }
    closesocket(sock);

    /* webclient 走的是 getaddrinfo */
    {
        struct addrinfo hints;
        struct addrinfo *res = RT_NULL, *it;
        int gai;

        rt_memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        gai = getaddrinfo("api.newayz.com", "80", &hints, &res);
        rt_kprintf("diag: getaddrinfo -> %d\n", gai);
        if (0 == gai && RT_NULL != res)
        {
            for (it = res; RT_NULL != it; it = it->ai_next)
            {
                rt_kprintf("diag:   family=%d socktype=%d\n", it->ai_family, it->ai_socktype);
            }
        }

        /* webclient 用的是全零 hints, 这里模拟它的调用方式 */
        rt_memset(&hints, 0, sizeof(hints));
        res = RT_NULL;
        gai = getaddrinfo("api.newayz.com", "80", &hints, &res);
        rt_kprintf("diag: getaddrinfo(zero hints) -> %d\n", gai);
        if (0 == gai && RT_NULL != res)
        {
            for (it = res; RT_NULL != it; it = it->ai_next)
            {
                rt_kprintf("diag:   family=%d socktype=%d protocol=%d\n",
                           it->ai_family, it->ai_socktype, it->ai_protocol);
            }
        }
    }
}
#endif

#ifdef PKG_WAYZ_IOTKIT_SIM
/**
 * 仿真模式: 自动注入一组 AP 与设备 MAC, 使定位流程可以无人值守地跑起来
 *
 * AP 列表来源:
 *   1) 环境变量 WAYZ_SIM_APS, 空格分隔的 "mac,rssi,channel" 列表;
 *   2) 内置的合成 AP(仅用于验证链路, 云端定位库中没有, 不会返回经纬度)。
 */
static void sim_auto_inject(void)
{
    static const char *default_aps[6] = {
        "00:11:22:33:44:01,-73,0",
        "00:11:22:33:44:02,-74,0",
        "00:11:22:33:44:03,-79,0",
        "00:11:22:33:44:04,-80,0",
        "00:11:22:33:44:05,-83,0",
        "00:11:22:33:44:06,-91,0"
    };
    const char *env = RT_NULL;
    char buf[512] = {0};
    char *token;
    int i;

    extern char *getenv(const char *name);

    env = getenv("WAYZ_SIM_APS");
    if (RT_NULL != env && '\0' != env[0])
    {
        rt_snprintf(buf, sizeof(buf), "%s", env);

        /* 手工按空格切分, 避免依赖 strtok_r(部分 libc 没有) */
        token = buf;
        while (RT_NULL != token && s_stub_ap.count < AP_MAX_NUMBER)
        {
            char *next = token;

            while ('\0' != *next && ' ' != *next)
            {
                next++;
            }
            if ('\0' != *next)
            {
                *next = '\0';
                next++;
            }
            else
            {
                next = RT_NULL;
            }

            if (RT_EOK == parse_ap_arg(token, &s_stub_ap.tinfoAp[s_stub_ap.count]))
            {
                s_stub_ap.count++;
            }
            token = next;
        }
    }
    else
    {
        for (i = 0; i < 6; i++)
        {
            if (RT_EOK != parse_ap_arg(default_aps[i], &s_stub_ap.tinfoAp[s_stub_ap.count]))
            {
                continue;
            }
            s_stub_ap.count++;
        }
    }

    s_stub_ready = 1;

    mac_string_to_bytes("02:00:00:00:00:01", s_stub_mac);
    stub_apply();
    rt_kprintf("simulation mode: %d ap(s) injected, device mac %02x:%02x:%02x:%02x:%02x:%02x\r\n",
               s_stub_ap.count, s_stub_mac[0], s_stub_mac[1], s_stub_mac[2],
               s_stub_mac[3], s_stub_mac[4], s_stub_mac[5]);
}
#endif /* PKG_WAYZ_IOTKIT_SIM */

#ifdef PKG_WAYZ_IOTKIT_SIM
/**
 * 仿真模式下自动启动定位线程, 使验证流程可以无人值守地运行
 */
static int location_client_auto_start(void)
{
    rt_kprintf("simulation mode: starting the location thread automatically.\n");
    return location_client_start();
}
INIT_APP_EXPORT(location_client_auto_start);
#endif

#ifdef RT_USING_FINSH
#include <finsh.h>
/*
 * 注意: FINSH_FUNCTION_EXPORT_ALIAS 在 RT-Thread 5.x 中已不存在,
 * 这里统一使用 MSH_CMD_EXPORT_ALIAS(msh 命令) 与 FINSH_FUNCTION_EXPORT(finsh 接口)。
 */
#ifdef FINSH_USING_MSH
MSH_CMD_EXPORT_ALIAS(location_client_start, location_test, location client test start);
MSH_CMD_EXPORT_ALIAS(cmd_wayz_scan_stub, wayz_scan_stub, inject the wifi scan result for simulation);
MSH_CMD_EXPORT_ALIAS(cmd_wayz_dev_mac, wayz_dev_mac, set the device mac for simulation);
MSH_CMD_EXPORT_ALIAS(cmd_wayz_key, wayz_key, set the wayz access key at runtime);
#endif
FINSH_FUNCTION_EXPORT(location_client_start, location client test start);
#endif
