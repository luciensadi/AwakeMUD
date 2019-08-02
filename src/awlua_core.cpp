#include "awlua_core.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>


static lua_State *awLS = nullptr;

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
