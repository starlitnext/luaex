/**
 *
 * Copyright (C) 2015 by David Lin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALING IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>

#include "ikcp.h"

#define RECV_BUFFER_LEN 256*1024

#define check_kcp(L, idx)\
    *(ikcpcb**)luaL_checkudata(L, idx, "kcp_meta")

#define check_buf(L, idx)\
    (struct RecvBuffer *)luaL_checkudata(L, idx, "recv_buffer")

struct Callback {
    uint64_t handle;
    lua_State* L;
};

struct RecvBuffer {
    uint32_t offset;
    uint32_t size;
    char buffer[0];
};

static int kcp_output_callback(const char *buf, int len, ikcpcb *kcp, void *arg) {
    struct Callback* c = (struct Callback*)arg;
    lua_State* L = c -> L;
    uint64_t handle = c -> handle;

    lua_rawgeti(L, LUA_REGISTRYINDEX, handle);
    lua_pushlstring(L, buf, len);
    lua_call(L, 1, 0);

    return 0;
}

static int kcp_gc(lua_State* L) {
    ikcpcb* kcp = check_kcp(L, 1);
    if (kcp == NULL) {
        return 0;
    }
    if (kcp->user != NULL) {
        struct Callback* c = (struct Callback*)kcp -> user;
        uint64_t handle = c -> handle;
        luaL_unref(L, LUA_REGISTRYINDEX, handle);
        free(c);
        kcp->user = NULL;
    }
    ikcp_release(kcp);
    kcp = NULL;
    return 0;
}

static int lkcp_create(lua_State* L){
    uint64_t handle = luaL_ref(L, LUA_REGISTRYINDEX);
    int32_t conv = luaL_checkinteger(L, 1);

    struct Callback* c = malloc(sizeof(struct Callback));
    memset(c, 0, sizeof(struct Callback));
    c -> handle = handle;
    c -> L = L;

    ikcpcb* kcp = ikcp_create(conv, (void*)c);
    if (kcp == NULL) {
        lua_pushnil(L);
        lua_pushstring(L, "error: fail to create kcp");
        return 2;
    }

    kcp->output = kcp_output_callback;

    *(ikcpcb**)lua_newuserdata(L, sizeof(void*)) = kcp;
    luaL_getmetatable(L, "kcp_meta");
    lua_setmetatable(L, -2);
    return 1;
}

static int lkcp_recv(lua_State* L){
    ikcpcb* kcp = check_kcp(L, 1);
    if (kcp == NULL) {
        lua_pushnil(L);
        lua_pushstring(L, "error: kcp not args");
        return 2;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, "kcp_lua_recv_buffer");
    struct RecvBuffer * buf = check_buf(L, -1);
    lua_pop(L, 1);

    // if data in buffer, return directly
    if (buf->size > 0) {
        lua_pushinteger(L, buf->size);
        lua_pushlstring(L, &buf->buffer[buf->offset], buf->size);
        buf->size = 0;
        buf->offset = 0;
        return 2;
    }

    int32_t hr = ikcp_recv(kcp, buf->buffer, RECV_BUFFER_LEN);
    lua_pushinteger(L, hr);
    if (hr < 0) {
        return 1;
    }
    lua_pushlstring(L, buf->buffer, hr);
    return 2;
}

static inline int read_size(uint8_t *buffer, uint32_t offset) {
    int r = (int)buffer[offset] << 8 | (int)buffer[offset+1];
	return r;
}

// return
//      0       if uncomplete packet
//      size    when complete packet
static inline int complete_packet(struct RecvBuffer *buf) {
    if (buf->size < 2) {
        return 0;
    }
    int size = read_size((uint8_t *)buf->buffer, buf->offset);
    if (buf->size >= size + 2) {
        return size;
    }
    return 0;
}

// read a complete size packet from buf
static inline int read_packet(lua_State *L, struct RecvBuffer *buf, int size) {
    lua_pushinteger(L, size);
    lua_pushlstring(L, &buf->buffer[buf->offset+2], size);
    int length = size + 2;
    buf->offset += length;
    buf->size -= length;
    if (buf->size == 0) {
        buf->offset = 0;
    }
    return 2;
}

static int recv_packet(lua_State* L){
    ikcpcb* kcp = check_kcp(L, 1);
    if (kcp == NULL) {
        lua_pushnil(L);
        lua_pushstring(L, "error: kcp not args");
        return 2;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, "kcp_lua_recv_buffer");
    struct RecvBuffer * buf = check_buf(L, -1);
    lua_pop(L, 1);
    int size = complete_packet(buf);
    if (size > 0) {
        return read_packet(L, buf, size);
    }

    int32_t pos = buf->offset + buf->size;
    int32_t hr = ikcp_recv(kcp, &buf->buffer[pos], RECV_BUFFER_LEN - pos);
    if (hr < 0) {
        lua_pushinteger(L, hr);
        return 1;
    }

    buf->size += hr;
    size = complete_packet(buf);

    if (size > 0) {
        return read_packet(L, buf, size);
    }

    lua_pushinteger(L, 0);
    return 1;
}

static int lkcp_send(lua_State* L){
    ikcpcb* kcp = check_kcp(L, 1);
    if (kcp == NULL) {
        lua_pushnil(L);
        lua_pushstring(L, "error: kcp not args");
        return 2;
    }

    int t = lua_type(L, 2);
    const char *data = NULL;
    size_t size = 0;

    switch (t)
    {
    case LUA_TSTRING: {
        data = lua_tolstring(L, 2, &size);
        break;
    }
    case LUA_TLIGHTUSERDATA: {
        data = (char *)lua_touserdata(L, 2);
        size = luaL_checkinteger(L, 3);
        break;
    }
    default:
        luaL_error(L, "lkcp_input invalid param %s", lua_typename(L,t));
    }

    int32_t hr = ikcp_send(kcp, data, size);
    lua_pushinteger(L, hr);
    return 1;
}

static int lkcp_update(lua_State* L){
    ikcpcb* kcp = check_kcp(L, 1);
    if (kcp == NULL) {
        lua_pushnil(L);
        lua_pushstring(L, "error: kcp not args");
        return 2;
    }
    int32_t current = luaL_checkinteger(L, 2);
    ikcp_update(kcp, current);
    return 0;
}

static int lkcp_check(lua_State* L){
    ikcpcb* kcp = check_kcp(L, 1);
    if (kcp == NULL) {
        lua_pushnil(L);
        lua_pushstring(L, "error: kcp not args");
        return 2;
    }
    int32_t current = luaL_checkinteger(L, 2);
    int32_t hr = ikcp_check(kcp, current);
    lua_pushinteger(L, hr);
    return 1;
}

static int lkcp_input(lua_State* L){
    ikcpcb* kcp = check_kcp(L, 1);
    if (kcp == NULL) {
        lua_pushnil(L);
        lua_pushstring(L, "error: kcp not args");
        return 2;
    }

    int t = lua_type(L, 2);
    const char *data = NULL;
    size_t size = 0;

    switch (t)
    {
    case LUA_TSTRING: {
        data = lua_tolstring(L, 2, &size);
        break;
    }
    case LUA_TLIGHTUSERDATA: {
        data = (char *)lua_touserdata(L, 2);
        size = luaL_checkinteger(L, 3);
        break;
    }
    default:
        luaL_error(L, "lkcp_input invalid param %s", lua_typename(L,t));
    }

    int32_t hr = ikcp_input(kcp, data, size);
    lua_pushinteger(L, hr);
    return 1;
}

static int lkcp_flush(lua_State* L){
    ikcpcb* kcp = check_kcp(L, 1);
    if (kcp == NULL) {
        lua_pushnil(L);
        lua_pushstring(L, "error: kcp not args");
        return 2;
    }
    ikcp_flush(kcp);
    return 0;
}

static int lkcp_wndsize(lua_State* L){
    ikcpcb* kcp = check_kcp(L, 1);
    if (kcp == NULL) {
        lua_pushnil(L);
        lua_pushstring(L, "error: kcp not args");
        return 2;
    }
    int32_t sndwnd = luaL_checkinteger(L, 2);
    int32_t rcvwnd = luaL_checkinteger(L, 3);
    ikcp_wndsize(kcp, sndwnd, rcvwnd);
    return 0;
}

static int lkcp_nodelay(lua_State* L){
    ikcpcb* kcp = check_kcp(L, 1);
    if (kcp == NULL) {
        lua_pushnil(L);
        lua_pushstring(L, "error: kcp not args");
        return 2;
    }
    int32_t nodelay = luaL_checkinteger(L, 2);
    int32_t interval = luaL_checkinteger(L, 3);
    int32_t resend = luaL_checkinteger(L, 4);
    int32_t nc = luaL_checkinteger(L, 5);
    int32_t hr = ikcp_nodelay(kcp, nodelay, interval, resend, nc);
    lua_pushinteger(L, hr);
    return 1;
}

static int getconv(lua_State *L) {
    void * msg = lua_touserdata(L, 1);
    lua_pushinteger(L, ikcp_getconv(msg));
    return 1;
}

// optimize for skynet
static const struct luaL_Reg lkcp_methods [] = {
    { "lkcp_recv" , lkcp_recv },
    { "lkcp_send" , lkcp_send },
    { "lkcp_update" , lkcp_update },
    { "lkcp_check" , lkcp_check },
    { "lkcp_input" , lkcp_input },
    { "lkcp_flush" , lkcp_flush },
    { "lkcp_wndsize" , lkcp_wndsize },
    { "lkcp_nodelay" , lkcp_nodelay },
    { "recv_packet" , recv_packet },
    {NULL, NULL},
};

static const struct luaL_Reg l_methods[] = {
    { "lkcp_create" , lkcp_create },
    { "getconv", getconv},
    {NULL, NULL},
};

int luaopen_lkcp(lua_State* L) {
    luaL_checkversion(L);

    luaL_newmetatable(L, "kcp_meta");

    lua_newtable(L);
    luaL_setfuncs(L, lkcp_methods, 0);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, kcp_gc);
    lua_setfield(L, -2, "__gc");

    uint32_t size = sizeof(struct RecvBuffer) + sizeof(char) * RECV_BUFFER_LEN;
    struct RecvBuffer * global_recv_buffer = lua_newuserdata(L, size);
    memset(global_recv_buffer, 0, size);
    luaL_newmetatable(L, "recv_buffer");
    lua_setmetatable(L, -2);
    lua_setfield(L, LUA_REGISTRYINDEX, "kcp_lua_recv_buffer");

    luaL_newlib(L, l_methods);

    return 1;
}
