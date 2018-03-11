#include "camera-net.h"
#include "camera-app.h"

#include <uv.h>

#include "logerr.h"
#include "mac.h"

static struct sockaddr_in udp_broadcast_addr;
static struct sockaddr_in udp_server_addr;
static struct sockaddr_in udp_client_addr;

static uv_udp_t udp_server;
static uv_udp_t udp_client;

static uv_udp_send_t udp_send_request;

static uv_buf_t ping_buffer;

static char mac_address[18];

static void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    // FIXME: Fix the memory leak, free() must be called.
    *buf = uv_buf_init((char*) malloc(suggested_size), suggested_size);
}

static void udp_client_on_send_cb(uv_udp_send_t *req, int status)
{
    if(status==-1)
        LOG_ERROR("UDP send error");

    LOG_MESSAGE("PONG");
}

static WARN_UNUSED enum error_code ping(void)
{
    int result;

    LOG_MESSAGE("PING");

    ping_buffer = uv_buf_init(mac_address, sizeof(mac_address));

    result = uv_udp_send(udp_send_request, &udp_client, &ping_buffer, 1, (const struct sockaddr *)&udp_client_addr, &udp_client_on_send_ping_cb);

    if(result!=0) {
        LOG_ERROR("PING UDP send (%8X)", result);
        return ERROR;
    }

    return OK;
}

static void udb_server_recv_cb(uv_udp_t* handle,
        ssize_t nread,
        const struct uv_buf_t *buf,
        const struct sockaddr* addr,
        unsigned flags)
{
    enum error_code result;

    if(!addr || !nread)
        return;

    switch(buf->base[0])
    {
        case 0:
            result = ping(); if(result!=OK) { /* ignore any errors */ }
            break;

        case 1:
            if( nread != 1+sizeof(struct camera_configuration) )
                LOG_ERROR("Wrong request size %d expected %d",
                        nread, 1+sizeof(struct camera_configuration));

            result = shoot(*(struct camera_configuration *)&buf->base[1]);
            if(result!=OK) { /* ignore any errors */ }
            break;

        case 2:
            if(nread != 2)
                LOG_ERROR("Wrong request size %d expected %d", nread, 2);

            result = erase(buf->base[1]); if(result!=OK) { /* ignore any errors */ }
            break;

        default:
            LOG_ERROR("Unknown UDP request %d (%02X)", nread, buf->base[0]);
            break;
    }
}

enum error_code net_session(void) {
    enum error_code result;
    int result_uv;

    uv_loop_t *loop = uv_default_loop();

    result = get_mac_address("eth0", &mac_address); if(result!=OK) return result;

    result_uv = uv_udp_init(loop, &udp_server);                                                        if(result_uv!=0) { LOG_ERROR("Initialising UDP server (%d)",  result_uv); return ERROR; }
    result_uv = uv_udp_init(loop, &udp_client);                                                        if(result_uv!=0) { LOG_ERROR("Initialising UDP client (%d)",  result_uv); return ERROR; }

    result_uv = uv_ip4_addr("0.0.0.0",   6502, &udp_server_addr);                                      if(result_uv!=0) { LOG_ERROR("IP4 address (%d)",              result_uv); return ERROR; }
    result_uv = uv_ip4_addr("0.0.0.0",      0, &udp_broadcast_addr);                                   if(result_uv!=0) { LOG_ERROR("IP4 address (%d)",              result_uv); return ERROR; }
    result_uv = uv_ip4_addr("224.1.1.1", 6501, &udp_client_addr);                                      if(result_uv!=0) { LOG_ERROR("IP4 address (%d)",              result_uv); return ERROR; }

    result_uv = uv_udp_bind(&udp_server, (const struct sockaddr *)&udp_server_addr, UV_UDP_REUSEADDR); if(result_uv!=0) { LOG_ERROR("Server UDP bind (%d)",          result_uv); return ERROR; }
    result_uv = uv_udp_bind(&udp_client, (const struct sockaddr *)&udp_broadcast_addr,             0); if(result_uv!=0) { LOG_ERROR("Client UDP bind (%d)",          result_uv); return ERROR; }

    result_uv = uv_udp_set_membership(&udp_server, "224.1.1.1", NULL, UV_JOIN_GROUP);                  if(result_uv!=0) { LOG_ERROR("Server UDP membership (%d)",    result_uv); return ERROR; }
    result_uv = uv_udp_recv_start    (&udp_server, alloc_cb, udb_server_recv_cb    );                  if(result_uv!=0) { LOG_ERROR("Server UDP receive start (%d)", result_uv); return ERROR; }

    result_uv = uv_udp_set_broadcast(&udp_client, 1);                                                  if(result_uv!=0) { LOG_ERROR("Client UDP broadcast (%d)",     result_uv); return ERROR; }

    uv_run(loop, UV_RUN_DEFAULT);

    return OK;
}
