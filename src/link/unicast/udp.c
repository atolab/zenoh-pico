/*
 * Copyright (c) 2017, 2021 ADLINK Technology Inc.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Apache License, Version 2.0
 * which is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *
 * Contributors:
 *   ADLINK zenoh team, <zenoh@adlink-labs.tech>
 */

#include <string.h>
#include "zenoh-pico/system/platform.h"
#include "zenoh-pico/link/manager.h"

z_str_t _zn_parse_port_segment_udp_unicast(z_str_t address)
{
    z_str_t p_start = strrchr(address, ':');
    if (p_start == NULL)
        return NULL;
    p_start++;

    z_str_t p_end = &address[strlen(address)];

    int len = p_end - p_start;
    z_str_t port = (z_str_t)malloc((len + 1) * sizeof(char));
    strncpy(port, p_start, len);
    port[len] = '\0';

    return port;
}

z_str_t _zn_parse_address_segment_udp_unicast(z_str_t address)
{
    z_str_t p_start = &address[0];
    z_str_t p_end = strrchr(address, ':');

    if (*p_start == '[' && *(p_end - 1) == ']')
    {
        p_start++;
        p_end--;
        int len = p_end - p_start;
        z_str_t ip6_addr = (z_str_t)malloc((len + 1) * sizeof(char));
        strncpy(ip6_addr, p_start, len);
        ip6_addr[len] = '\0';

        return ip6_addr;
    }
    else
    {
        int len = p_end - p_start;
        z_str_t ip4_addr_or_domain = (z_str_t)malloc((len + 1) * sizeof(char));
        strncpy(ip4_addr_or_domain, p_start, len);
        ip4_addr_or_domain[len] = '\0';

        return ip4_addr_or_domain;
    }

    return NULL;
}

_zn_socket_result_t _zn_f_link_open_udp_unicast(void *arg, const clock_t tout)
{
    _zn_link_t *self = (_zn_link_t *)arg;
    _zn_socket_result_t r;
    r.tag = _z_res_t_OK;

    self->sock = _zn_open_udp_unicast(self->raddr, tout);
    if (self->sock < 0)
        goto ERR;

    r.value.socket = self->sock;
    return r;

ERR:
    r.tag = _z_res_t_ERR;
    r.value.error = _zn_err_t_OPEN_TRANSPORT_FAILED;
    return r;
}

_zn_socket_result_t _zn_f_link_listen_udp_unicast(void *arg, const clock_t tout)
{
    _zn_link_t *self = (_zn_link_t *)arg;
    _zn_socket_result_t r;
    r.tag = _z_res_t_OK;

    self->sock = _zn_listen_udp_unicast(self->raddr, tout);
    if (self->sock < 0)
        goto ERR;

    r.value.socket = self->sock;
    return r;

ERR:
    r.tag = _z_res_t_ERR;
    r.value.error = _zn_err_t_OPEN_TRANSPORT_FAILED;
    return r;
}

void _zn_f_link_close_udp_unicast(void *arg)
{
    _zn_link_t *self = (_zn_link_t *)arg;

    _zn_close_udp_unicast(self->sock);
}

void _zn_f_link_free_udp_unicast(void *arg)
{
    _zn_link_t *self = (_zn_link_t *)arg;

    _zn_free_endpoint_udp(self->raddr);
}

size_t _zn_f_link_write_udp_unicast(const void *arg, const uint8_t *ptr, size_t len)
{
    const _zn_link_t *self = (const _zn_link_t *)arg;

    return _zn_send_udp_unicast(self->sock, ptr, len, self->raddr);
}

size_t _zn_f_link_write_all_udp_unicast(const void *arg, const uint8_t *ptr, size_t len)
{
    const _zn_link_t *self = (const _zn_link_t *)arg;

    return _zn_send_udp_unicast(self->sock, ptr, len, self->raddr);
}

size_t _zn_f_link_read_udp_unicast(const void *arg, uint8_t *ptr, size_t len, z_bytes_t *addr)
{
    const _zn_link_t *self = (const _zn_link_t *)arg;

    return _zn_read_udp_unicast(self->sock, ptr, len);
}

size_t _zn_f_link_read_exact_udp_unicast(const void *arg, uint8_t *ptr, size_t len, z_bytes_t *addr)
{
    const _zn_link_t *self = (const _zn_link_t *)arg;

    return _zn_read_exact_udp_unicast(self->sock, ptr, len);
}

size_t _zn_get_link_mtu_udp_unicast()
{
    // @TODO: not implemented
    return -1;
}

_zn_link_t *_zn_new_link_udp_unicast(_zn_endpoint_t endpoint)
{
    _zn_link_t *lt = (_zn_link_t *)malloc(sizeof(_zn_link_t));

    lt->is_reliable = 0;
    lt->is_streamed = 0;
    lt->is_multicast = 0;
    lt->mtu = _zn_get_link_mtu_udp_unicast();

    z_str_t s_addr = _zn_parse_address_segment_udp_unicast(endpoint.locator.address);
    z_str_t s_port = _zn_parse_port_segment_udp_unicast(endpoint.locator.address);
    lt->raddr = _zn_create_endpoint_udp(s_addr, s_port);
    lt->endpoint = endpoint;

    lt->open_f = _zn_f_link_open_udp_unicast;
    lt->listen_f = _zn_f_link_listen_udp_unicast;
    lt->close_f = _zn_f_link_close_udp_unicast;
    lt->free_f = _zn_f_link_free_udp_unicast;

    lt->write_f = _zn_f_link_write_udp_unicast;
    lt->write_all_f = _zn_f_link_write_all_udp_unicast;
    lt->read_f = _zn_f_link_read_udp_unicast;
    lt->read_exact_f = _zn_f_link_read_exact_udp_unicast;

    free(s_addr);
    free(s_port);

    return lt;
}