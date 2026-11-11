/*
 * 主机端测试台的 RT-Thread 桩头文件(最小实现)
 *
 * 语义对齐 RT-Thread 5.2/master, 避免"桩比真实实现更宽松"掩盖缺陷:
 *   - 版本宏按 5.x 给出(RTTHREAD_VERSION = 50200)
 *   - 名字长度上限 12(RT_NAME_MAX)
 *   - 提供互斥量/信号量/时钟等被组件用到的 API
 *
 * 注意: 这里只声明接口, 实现见 tools/host_test/test_wayz.c
 */
#ifndef __RTSTUB_H__
#define __RTSTUB_H__

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#ifndef RT_VERSION_MAJOR
#define RT_VERSION_MAJOR        5
#endif
#ifndef RT_VERSION_MINOR
#define RT_VERSION_MINOR        2
#endif
#define RT_VERSION_CHECK(major, minor, revise)  ((major * 10000) + (minor * 100) + revise)
#define RTTHREAD_VERSION        RT_VERSION_CHECK(RT_VERSION_MAJOR, RT_VERSION_MINOR, 0)

#define RT_NAME_MAX             12
#define RT_TICK_PER_SECOND      1000
#define RT_WAITING_FOREVER      (-1)
#define RT_IPC_FLAG_FIFO        0
#define RT_IPC_FLAG_PRIO        1

typedef uint8_t   rt_uint8_t;
typedef int8_t    rt_int8_t;
typedef uint16_t  rt_uint16_t;
typedef int16_t   rt_int16_t;
typedef uint32_t  rt_uint32_t;
typedef int32_t   rt_int32_t;
typedef uint64_t  rt_uint64_t;
typedef int64_t   rt_int64_t;
typedef size_t    rt_size_t;
typedef int       rt_err_t;
typedef int       rt_bool_t;

#define RT_EOK          0
#define RT_ERROR        (-1)
#define RT_TRUE         1
#define RT_FALSE        0
#define RT_NULL         ((void *)0)

int rt_kprintf(const char *fmt, ...);
int rt_snprintf(char *buf, rt_size_t size, const char *fmt, ...);

void *rt_malloc(rt_size_t size);
void *rt_calloc(rt_size_t n, rt_size_t size);
void  rt_free(void *ptr);

void *rt_memcpy(void *dest, const void *src, rt_size_t n);
void *rt_memset(void *s, int c, rt_size_t n);
rt_size_t rt_strlen(const char *s);
int  rt_strcmp(const char *a, const char *b);
char *rt_strstr(const char *a, const char *b);
char *rt_strncpy(char *dest, const char *src, rt_size_t n);

rt_uint64_t rt_tick_from_millisecond(rt_uint64_t ms);
rt_uint32_t rt_tick_get(void);

typedef struct rt_semaphore { int dummy; } rt_semaphore;
rt_err_t rt_sem_init(rt_semaphore *sem, const char *name, rt_uint32_t value, rt_uint8_t flag);
rt_err_t rt_sem_take(rt_semaphore *sem, rt_uint64_t timeout);
rt_err_t rt_sem_release(rt_semaphore *sem);
rt_err_t rt_sem_detach(rt_semaphore *sem);

typedef struct rt_mutex { int dummy; } rt_mutex;
typedef struct rt_mutex *rt_mutex_t;
int rt_mutex_init(rt_mutex_t m, const char *name, rt_uint8_t flag);
int rt_mutex_take(rt_mutex_t m, rt_int32_t time);
int rt_mutex_release(rt_mutex_t m);

typedef struct rt_thread *rt_thread_t;
rt_thread_t rt_thread_create(const char *name, void (*entry)(void *), void *param,
                             rt_uint32_t stack, rt_uint8_t priority, rt_uint32_t tick);
rt_err_t rt_thread_startup(rt_thread_t thread);
void rt_thread_mdelay(rt_int32_t ms);

/* 桩: 用构造函数在 main 之前执行初始化 */
#define INIT_COMPONENT_EXPORT(fn)                                 \
    static void __wayz_init_##fn(void) __attribute__((constructor)); \
    static void __wayz_init_##fn(void) { fn(); }

#endif
