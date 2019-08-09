#include "awlua_ud.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

void awlua::UdTypeBase::NewUd(lua_State *LS, const char *mt_name)
{
    void *ud_mem = lua_newuserdata(LS, sizeof(void*));
    AWLUA_ASSERT(nullptr != ud_mem);
    AWLUA_ASSERT(1 == lua_isuserdata(LS, -1));
    AWLUA_ASSERT(LUA_TNIL != luaL_getmetatable(LS, mt_name));
    lua_setmetatable(LS, -2);
}

void awlua::UdTypeBase::InitType(lua_State *LS, const char *mt_name, const ud_field *get_fields, const ud_field *set_fields, const ud_field *meth_fields)
{
    int top = lua_gettop(LS);
    luaL_newmetatable(LS, mt_name);

    lua_newtable(LS);
    for (int i = 0; get_fields[i].field; ++i)
    {
        lua_pushcfunction(LS, get_fields[i].func);
        lua_setfield(LS, -2, get_fields[i].field);
    }
    lua_newtable(LS);
    for (int i = 0; meth_fields[i].field; ++i)
    {
        lua_pushcfunction(LS, meth_fields[i].func);
        lua_setfield(LS, -2, meth_fields[i].field);
    }
    lua_pushcclosure(LS, L_index, 2);
    lua_setfield(LS, -2, "__index");

    lua_newtable(LS);
    for (int i = 0; set_fields[i].field; ++i)
    {
        lua_pushcfunction(LS, set_fields[i].func);
        lua_setfield(LS, -2, set_fields[i].field);
    }
    lua_pushcclosure(LS, L_newindex, 1);
    lua_setfield(LS, -2, "__newindex");

    lua_settop(LS, top);
}

int awlua::UdTypeBase::L_index(lua_State *LS)
{
    // upvalue 1 is get table
    // upvalue 2 is meth table
    // 1 is t (CH)
    // 2 is k

    lua_pushvalue(LS, 2); // copy k
    lua_gettable(LS, lua_upvalueindex(1));
    if (!lua_isnil(LS, -1))
    {
        // get property
        lua_pushvalue(LS, 1);
        lua_call(LS, 1, 1);
        return 1;    
    }
    else
    {
        lua_pop(LS, 1); // pop nil
    }

    lua_gettable(LS, lua_upvalueindex(2));
    if (!lua_isnil(LS, -1))
    {
        // method
        return 1;
    }

    return 0;
}

int awlua::UdTypeBase::L_newindex(lua_State *LS)
{
    // upvalue 1 is set table
    // 1 is t (CH)
    // 2 is k
    // 3 is v
    lua_pushvalue(LS, 2);
    lua_gettable(LS, lua_upvalueindex(1));
    if (lua_isnil(LS, -1))
    {
        // TODO: include type name in error message
        return luaL_error(LS, "Can't set field '%s'", lua_tostring(LS, 2));
    }
    else
    {
        // set property
        lua_remove(LS, 2);
        lua_insert(LS, 1);
        lua_call(LS, 2, 0);
        return 0;    
    }
}