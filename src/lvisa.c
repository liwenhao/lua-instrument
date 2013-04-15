/*
 * Copyright (c) 2013 Wenhao Li. All Rights Reserved.
 *
 */
#include <lua.h>
#include <lauxlib.h>
#include <visa.h>

#include <string.h>

/*
 * a simple visa binding
 */
#define VSESSION "VSESSION"
#define tovsession(L) ((ViSession*)luaL_checkudata(L, 1, VSESSION))

/**
 * session(address [, timeout])
 */
static int open_vsession(lua_State *L)
{
    ViSession dfm, find, *instr;
    ViUInt32 count;
    ViChar name[1024];
    ViChar desc[1024];
    char addr[1024];
    ViStatus err;
    int timeout = luaL_optint(L, 2, 5000);

    strncpy(addr, luaL_checkstring(L, 1), 1024);

    instr = (ViSession*)lua_newuserdata(L, sizeof(ViSession));
    luaL_getmetatable(L, VSESSION);
    lua_setmetatable(L, -2);

    err = viOpenDefaultRM(&dfm);
    if (err) {
        viStatusDesc(dfm, err, desc);
        lua_pushstring(L, desc);
        lua_error(L);
    }

    err = viFindRsrc(dfm, addr, &find, &count, name);
    if (err < VI_SUCCESS) {
        viStatusDesc(dfm, err, desc);
        viClose(dfm);
        lua_pushstring(L, desc);
        lua_error(L);
    }
    viClose(find);

    err = viOpen(dfm, name, VI_NULL, timeout, instr);
    if (err) {
        viStatusDesc(dfm, err, desc);
        viClose(dfm);
        lua_pushstring(L, desc);
        lua_error(L);
    }

    err = viSetAttribute(*instr, VI_ATTR_TMO_VALUE, timeout);
    if (err) {
        viStatusDesc(dfm, err, desc);
        viClose(dfm);
        viClose(*instr);
        lua_pushstring(L, desc);
        lua_error(L);
    }

    return 1;
}

static int vclose(lua_State *L)
{
    ViSession instr = *tovsession(L);
    if (instr) {
	viClose(instr);
	*tovsession(L) = 0;
    }
    return 0;
}

static int vread(lua_State *L)
{
    ViSession instr = *tovsession(L);
    ViUInt32 nr;
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    for (;;) {
        char *p = luaL_prepbuffer(&b);
        ViStatus err = viRead(instr, (ViPBuf)p, LUAL_BUFFERSIZE, &nr);
        if (err && err != VI_SUCCESS_MAX_CNT) {
            ViChar desc[1024];
            viStatusDesc(instr, err, desc);
            lua_pushstring(L, desc);
            lua_error(L);
        }
        luaL_addsize(&b, nr);
        if (nr < LUAL_BUFFERSIZE)
            break;
    }

    luaL_pushresult(&b);
    return 1;
}

static int vwrite(lua_State *L)
{
    ViSession instr = *tovsession(L);
    size_t size;
    ViUInt32 ret;
    ViStatus err;
    const char* data = luaL_checklstring(L, 2, &size);

    lua_pushvalue(L, 1);

    for(;;) {
        err = viWrite(instr, (ViBuf)data, size, &ret);
        if (err) {
            ViChar desc[1024];
            viStatusDesc(instr, err, desc);
            lua_pushstring(L, desc);
            lua_error(L);
        }
        if (size == ret)
            break;
        else {
            size -= ret;
            data += ret;
        }
    }

    return 1;
}

static int vtostring(lua_State *L)
{
    ViChar rsrc[256];
    ViSession instr = *tovsession(L);
    viGetAttribute(instr, VI_ATTR_RSRC_NAME,rsrc);
    lua_pushfstring(L, "Session: %p, Resource: %s", instr, rsrc);
    return 1;
}

static const luaL_Reg vslib[] = {
    {"close", vclose},
    {"read", vread},
    {"write", vwrite},
    {"__gc", vclose},
    {"__tostring", vtostring},
    {NULL, NULL}
};

static const struct luaL_Reg lvisa [] = {
    {"open", open_vsession},
    {NULL, NULL}
};

static void create_vsession_meta(lua_State *L)
{
    luaL_newmetatable(L, VSESSION);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, vslib, 0);
    lua_pop(L, 1);
}

LUALIB_API int luaopen_lvisa (lua_State *L)
{
    luaL_newlib(L, lvisa);
    create_vsession_meta(L);
    return 1;
}


