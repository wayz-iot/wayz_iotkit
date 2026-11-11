/* 仅用于本地语法检查的 webclient 桩头文件, 不属于仓库 */
#ifndef __WEBCLIENT_STUB_H__
#define __WEBCLIENT_STUB_H__

#include <rtthread.h>

#ifndef WEBCLIENT_SW_VERSION_NUM
#define WEBCLIENT_SW_VERSION_NUM 0x20300
#endif

#define WEBCLIENT_HEADER_BUFSZ 4096

struct webclient_header
{
    char *buffer;
    size_t length;
    size_t size;
};

struct webclient_session
{
    struct webclient_header *header;
    int socket;
    int resp_status;
    char *host;
    char *req_url;
};

struct webclient_session *webclient_session_create(size_t header_sz);
int webclient_get(struct webclient_session *session, const char *URI);
int webclient_close(struct webclient_session *session);
int webclient_header_fields_add(struct webclient_session *session, const char *fmt, ...);
int webclient_resp_status_get(struct webclient_session *session);
int webclient_request_header_add(char **request_header, const char *fmt, ...);

#if WEBCLIENT_SW_VERSION_NUM >= 0x20200
int webclient_post(struct webclient_session *session, const char *URI, const void *post_data, size_t data_len);
int webclient_response(struct webclient_session *session, void **response, size_t *resp_len);
#else
int webclient_post(struct webclient_session *session, const char *URI, const char *post_data);
int webclient_response(struct webclient_session *session, unsigned char **response);
#endif

#define web_free    rt_free

#endif
