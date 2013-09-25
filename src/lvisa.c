/*
 * Copyright (c) 2013 Wenhao Li. All Rights Reserved.
 *
 */
#include <lua.h>
#include <lauxlib.h>
#include <visa.h>
#include <windows.h>
#include <string.h>

/*
 * a simple visa binding
 */
#define VSESSION "VSESSION"
#define tovsession(L) ((ViSession*)luaL_checkudata(L, 1, VSESSION))

#define VISA_DLL_NAME "visa32.dll"

typedef ViStatus (_VI_FUNC *viOpenDefaultRM_t)(ViPSession vi);
typedef ViStatus (_VI_FUNC *viFindRsrc_t)(ViSession sesn, ViString expr, ViPFindList vi, ViPUInt32 retCnt, ViChar _VI_FAR desc[]);
typedef ViStatus (_VI_FUNC *viFindNext_t)(ViFindList vi, ViChar _VI_FAR desc[]);
typedef ViStatus (_VI_FUNC *viOpen_t)(ViSession sesn, ViRsrc name, ViAccessMode mode, ViUInt32 timeout, ViPSession vi);
typedef ViStatus (_VI_FUNC *viClose_t)(ViObject vi);
typedef ViStatus (_VI_FUNC *viSetAttribute_t)(ViObject vi, ViAttr attrName, ViAttrState attrValue);
typedef ViStatus (_VI_FUNC *viGetAttribute_t)(ViObject vi, ViAttr attrName, void _VI_PTR attrValue);
typedef ViStatus (_VI_FUNC *viStatusDesc_t)(ViObject vi, ViStatus status, ViChar _VI_FAR desc[]);
typedef ViStatus (_VI_FUNC *viRead_t)(ViSession vi, ViPBuf buf, ViUInt32 cnt, ViPUInt32 retCnt);
typedef ViStatus (_VI_FUNC *viWrite_t)(ViSession vi, ViBuf  buf, ViUInt32 cnt, ViPUInt32 retCnt);

#define VIFUN(x) x##_t p##x
VIFUN(viOpenDefaultRM);
VIFUN(viFindRsrc);
VIFUN(viFindNext);
VIFUN(viOpen);
VIFUN(viClose);
VIFUN(viSetAttribute);
VIFUN(viGetAttribute);
VIFUN(viStatusDesc);
VIFUN(viRead);
VIFUN(viWrite);

#define VILOAD(x) do {						  \
    p##x = (x##_t)GetProcAddress(g_visa, #x); \
		if (p##x == NULL) {					  \
			FreeLibrary(g_visa);			  \
			g_visa = NULL;					  \
			lua_pushstring(L, #x"not found"); \
			lua_error(L);					  \
		}									  \
	}while (0)

HANDLE g_visa = NULL;
static int load_visa(lua_State *L)
{
	if (g_visa!=NULL)
		return 0;

    g_visa = LoadLibrary(TEXT(VISA_DLL_NAME));

    if (g_visa == NULL) {
		lua_pushfstring(L, "load visa lib failed: %s:%d", VISA_DLL_NAME, GetLastError());
		lua_error(L);
        return 0;
	}

    VILOAD(viOpenDefaultRM);
    VILOAD(viFindRsrc);
    VILOAD(viFindNext);
    VILOAD(viOpen);
    VILOAD(viClose);
    VILOAD(viSetAttribute);
	VILOAD(viGetAttribute);
    VILOAD(viStatusDesc);
    VILOAD(viRead);
    VILOAD(viWrite);

    return 0;
}

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

    err = pviOpenDefaultRM(&dfm);
    if (err) {
        pviStatusDesc(dfm, err, desc);
        lua_pushstring(L, desc);
        lua_error(L);
    }

    err = pviFindRsrc(dfm, addr, &find, &count, name);
    if (err < VI_SUCCESS) {
        pviStatusDesc(dfm, err, desc);
        pviClose(dfm);
        lua_pushstring(L, desc);
        lua_error(L);
    }
    pviClose(find);

    err = pviOpen(dfm, name, VI_NULL, timeout, instr);
    if (err) {
        pviStatusDesc(dfm, err, desc);
        pviClose(dfm);
        lua_pushstring(L, desc);
        lua_error(L);
    }

    err = pviSetAttribute(*instr, VI_ATTR_TMO_VALUE, timeout);
    if (err) {
        pviStatusDesc(dfm, err, desc);
        pviClose(dfm);
        pviClose(*instr);
        lua_pushstring(L, desc);
        lua_error(L);
    }

    return 1;
}

static int vclose(lua_State *L)
{
    ViSession instr = *tovsession(L);
    if (instr) {
	pviClose(instr);
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
        ViStatus err = pviRead(instr, (ViPBuf)p, LUAL_BUFFERSIZE, &nr);
        if (err && err != VI_SUCCESS_MAX_CNT) {
            ViChar desc[1024];
            pviStatusDesc(instr, err, desc);
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
        err = pviWrite(instr, (ViBuf)data, size, &ret);
        if (err) {
            ViChar desc[1024];
            pviStatusDesc(instr, err, desc);
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
    pviGetAttribute(instr, VI_ATTR_RSRC_NAME,rsrc);
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
	load_visa(L);
    luaL_newlib(L, lvisa);
    create_vsession_meta(L);
    return 1;
}


