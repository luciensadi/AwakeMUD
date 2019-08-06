#include "awlua_core.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>


static lua_State *awLS = nullptr;

void awlua::Init()
{
    lua_State *LS = luaL_newstate();
    AWLUA_ASSERT(LS);
    luaL_openlibs(LS);    
    awLS = LS;

    // TODO: check return val
    luaL_dostring(LS, 
        "local traceback = debug.traceback \n"
        "debug.traceback = function(message, level) \n"
          "level = level or 1 \n"
          "local rtn = traceback(message, level + 1) \n"
          "if not rtn then return rtn end \n"
          "rtn = rtn:gsub(\"\\t\", \"    \") \n"
          "rtn = rtn:gsub(\"\\n\", \"\\n\\r\") \n"
          "return rtn \n"
        "end \n"
        "return debug.traceback \n");

    luaL_loadstring(LS, "package.path = '../src/lua/?.lua'");
    lua_call(LS, 0, 0);

    lua_getglobal(LS, "require");
    lua_pushliteral(LS, "awlua");
    lua_call(LS, 1, 1);

    // TODO: save awlua ref somewhere
    lua_pop(LS, 1);


    AWLUA_ASSERT(0 == lua_gettop(LS));
}

lua_State *awlua::GetLS()
{
    return awLS;
}

#ifdef UNITTEST
void awlua::SetLS(lua_State *LS)
{
    awLS = LS;
}
#endif

awlua::LuaRef::LuaRef()
    : val_(LUA_NOREF)
{ }

awlua::LuaRef::~LuaRef()
{
    Release();
}

void awlua::LuaRef::Save(int index)
{
    Release();
    lua_pushvalue(GetLS(), index);
    val_ = luaL_ref(GetLS(), LUA_REGISTRYINDEX);
}

void awlua::LuaRef::Release()
{
    luaL_unref(GetLS(), LUA_REGISTRYINDEX, val_); 
    val_ = LUA_NOREF;
}

void awlua::LuaRef::Push()
{
    lua_rawgeti(GetLS(), LUA_REGISTRYINDEX, val_);
}

bool awlua::LuaRef::IsSet()
{
    return (val_ != LUA_NOREF);
}
