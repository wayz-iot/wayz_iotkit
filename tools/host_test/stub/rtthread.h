/* 仅用于本地语法检查的 RT-Thread 桩头文件, 不属于仓库 */
#ifndef __RTSTUB_H__
#define __RTSTUB_H__

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifndef RT_VERSION_MAJOR
/* RT-Thread 4.1.0 风格: 没有 RT_VERSION_MAJOR, 只有 RT_VERSION + RTTHREAD_VERSION */
#define RT_VERSION          4L
#define RTTHREAD_VERSION    ((RT_VERSION * 10000) + (1 * 100) + 0)
#else
#ifndef RT_VERSION_MINOR
#define RT_VERSION_MINOR    0
#endif
#ifndef RT_VERSION_PATCH
#define RT_VERSION_PATCH    0
#endif
#define RT_VERSION_CHECK(major, minor, revise)  ((major * 10000) + (minor * 100) + revise)
#define RTTHREAD_VERSION    RT_VERSION_CHECK(RT_VERSION_MAJOR, RT_VERSION_MINOR, RT_VERSION_PATCH)
#endif

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

#define RT_EOK      0
#define RT_ERROR    (-1)
#define RT_TRUE     1
#define RT_FALSE    0
#define RT_NULL     ((void *)0)
#define RT_IPC_FLAG_FIFO  0
#define RT_THREAD_PRIORITY_MAX 32

int rt_kprintf(const char *fmt, ...);
int rt_snprintf(char *buf, rt_size_t size, const char *fmt, ...);
void *rt_calloc(rt_size_t n, rt_size_t size);
void *rt_malloc(rt_size_t size);
void  rt_free(void *ptr);
void *rt_memcpy(void *dest, const void *src, rt_size_t n);
void *rt_memset(void *s, int c, rt_size_t n);
rt_size_t rt_strlen(const char *s);
int  rt_strcmp(const char *a, const char *b);
char *rt_strstr(const char *a, const char *b);
char *rt_strncpy(char *dest, const char *src, rt_size_t n);
rt_uint64_t rt_tick_from_millisecond(rt_uint64_t ms);



#define RT_TICK_PER_SECOND 1000
rt_uint32_t rt_tick_get(void);

typedef struct rt_semaphore { int dummy; } rt_semaphore;
rt_err_t rt_sem_init(rt_semaphore *sem, const char *name, rt_uint32_t value, rt_uint8_t flag);
rt_err_t rt_sem_take(rt_semaphore *sem, rt_uint64_t timeout);
rt_err_t rt_sem_release(rt_semaphore *sem);
rt_err_t rt_sem_detach(rt_semaphore *sem);

typedef struct rt_thread *rt_thread_t;
rt_thread_t rt_thread_create(const char *name, void (*entry)(void *), void *param,
                             rt_uint32_t stack, rt_uint8_t priority, rt_uint32_t tick);
rt_err_t rt_thread_startup(rt_thread_t thread);
void rt_thread_mdelay(rt_int32_t ms);

#endif
