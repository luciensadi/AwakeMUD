#include "../src/awlua_core.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <catch.hpp>


// static void DumpRegistry(lua_State *LS)
// {
//     printf("Registry keys: \n");
//     lua_pushnil(LS);
//     while (lua_next(LS, LUA_REGISTRYINDEX) != 0)
//     {
//         lua_pushvalue(LS, -2);
//         printf("k: %s, v: %s\n", lua_tostring(LS, -1), lua_tostring(LS, -2));
//         lua_pop(LS, 2);
//     }
// }

/* Returns the count of objects in registry that are equal to value at given index */
static int GetRefCount(lua_State *LS, int index)
{
    const int tgt = lua_absindex(LS, index);
    int cnt = 0;
    lua_pushnil(LS);
    while (lua_next(LS, LUA_REGISTRYINDEX) != 0) /* pops nil */
    {
        if (1 == lua_compare(LS, -1, tgt, LUA_OPEQ))
        {
            ++cnt;
        }
        lua_pop(LS, 1); /* pop val */
    }
    return cnt;
}

TEST_CASE("ScriptCall")
{
    lua_State *LS = luaL_newstate();
    luaL_openlibs(LS);
    awlua::SetLS(LS);

    SECTION("timeout")
    {
        int rc;

        lua_settop(LS, 0);
        luaL_loadstring(LS,
            "while true do end");
        rc = awlua::ScriptCall(LS, 0, 0);
        REQUIRE(LUA_OK != rc);
        REQUIRE(1 == lua_gettop(LS));

        const std::string errMsg(lua_tostring(LS, -1));
        INFO(errMsg);
        REQUIRE(errMsg.find("Script timeout") != std::string::npos);
    }

    SECTION("No error")
    {
        int rc;

        lua_settop(LS, 0);
        luaL_loadstring(LS,
            "local a, b, c = ...\n"
            "return a, b, c, 1234\n");
        lua_pushinteger(LS, 1);
        lua_pushinteger(LS, 2);
        lua_pushinteger(LS, 3);
        rc = awlua::ScriptCall(LS, 3, LUA_MULTRET);
        REQUIRE(LUA_OK == rc);
        REQUIRE(4 == lua_gettop(LS));
        REQUIRE(1 == luaL_checkinteger(LS, -4));
        REQUIRE(2 == luaL_checkinteger(LS, -3));
        REQUIRE(3 == luaL_checkinteger(LS, -2));
        REQUIRE(1234 == luaL_checkinteger(LS, -1));
    }

    SECTION("Error")
    {
        int rc;
        lua_settop(LS, 0);
        luaL_loadstring(LS,
            "local function func()\n"
            "  -- some comment\n"
            "  error('Some error')\n"
            "end\n"
            "func()\n");

        rc = awlua::ScriptCall(LS, 0, LUA_MULTRET);
        REQUIRE(LUA_OK != rc);
        REQUIRE(1 == lua_gettop(LS));

        const std::string errMsg(lua_tostring(LS, -1));
        INFO(errMsg);
        REQUIRE(errMsg.find("3: Some error") != std::string::npos);
    }
}

TEST_CASE("CallWithTraceback")
{
    lua_State *LS = luaL_newstate();
    luaL_openlibs(LS);
    awlua::SetLS(LS);

    SECTION("No error")
    {
        int rc;

        lua_settop(LS, 0);
        luaL_loadstring(LS,
            "local a, b, c = ...\n"
            "return a, b, c, 1234\n");
        lua_pushinteger(LS, 1);
        lua_pushinteger(LS, 2);
        lua_pushinteger(LS, 3);
        rc = awlua::CallWithTraceback(LS, 3, LUA_MULTRET);
        REQUIRE(LUA_OK == rc);
        REQUIRE(4 == lua_gettop(LS));
        REQUIRE(1 == luaL_checkinteger(LS, -4));
        REQUIRE(2 == luaL_checkinteger(LS, -3));
        REQUIRE(3 == luaL_checkinteger(LS, -2));
        REQUIRE(1234 == luaL_checkinteger(LS, -1));
    }

    SECTION("Error")
    {
        int rc;
        lua_settop(LS, 0);
        luaL_loadstring(LS,
            "local function func()\n"
            "  -- some comment\n"
            "  error('Some error')\n"
            "end\n"
            "func()\n");

        rc = awlua::CallWithTraceback(LS, 0, LUA_MULTRET);
        REQUIRE(LUA_OK != rc);
        REQUIRE(1 == lua_gettop(LS));

        const std::string errMsg(lua_tostring(LS, -1));
        INFO(errMsg);
        REQUIRE(errMsg.find("3: Some error") != std::string::npos);
    }
}

TEST_CASE("LuaRef")
{
    lua_State *LS = luaL_newstate();
    luaL_openlibs(LS);
    awlua::SetLS(LS);

    SECTION("Save")
    {
        awlua::LuaRef ref;
        
        lua_pushinteger(LS, 4321);
        REQUIRE(1 == lua_gettop(LS));

        ref.Save(-1);
        REQUIRE(1 == lua_gettop(LS));
        REQUIRE(1 == GetRefCount(LS, -1));
        
        lua_pushinteger(LS, 4322);
        ref.Save(-1);
        REQUIRE(0 == GetRefCount(LS, -2));
        REQUIRE(1 == GetRefCount(LS, -1));

        lua_pushinteger(LS, 4323);
        ref.Save(-1);
        REQUIRE(0 == GetRefCount(LS, -3));
        REQUIRE(0 == GetRefCount(LS, -2));
        REQUIRE(1 == GetRefCount(LS, -1));
    }

    SECTION("IsSet")
    {
        awlua::LuaRef ref;

        REQUIRE(false == ref.IsSet());
        REQUIRE(0 == lua_gettop(LS));
        
        lua_pushinteger(LS, 4321);
        REQUIRE(1 == lua_gettop(LS));

        ref.Save(-1);
        REQUIRE(1 == lua_gettop(LS));
        REQUIRE(true == ref.IsSet());
        
        ref.Release();
        REQUIRE(false == ref.IsSet());
        REQUIRE(1 == lua_gettop(LS));
    }

    SECTION("Release")
    {
        awlua::LuaRef ref;

        REQUIRE(false == ref.IsSet());
        // Release when not set is just no-op
        ref.Release();
        REQUIRE(false == ref.IsSet());

        lua_pushinteger(LS, 4321);
        REQUIRE(1 == lua_gettop(LS));

        ref.Save(-1);
        REQUIRE(1 == lua_gettop(LS));
        REQUIRE(true == ref.IsSet());

        ref.Release();
        REQUIRE(false == ref.IsSet());
    }

    SECTION("Push")
    {
        awlua::LuaRef ref;

        REQUIRE(false == ref.IsSet());

        REQUIRE(0 == lua_gettop(LS));
        ref.Push();
        REQUIRE(1 == lua_gettop(LS));
        REQUIRE(lua_isnil(LS, -1));
        lua_pop(LS, 1);

        REQUIRE(0 == lua_gettop(LS));
        ref.Push();
        ref.Push();
        ref.Push();
        REQUIRE(3 == lua_gettop(LS));
        REQUIRE(lua_isnil(LS, -1));
        REQUIRE(lua_isnil(LS, -2));
        REQUIRE(lua_isnil(LS, -3));
        lua_pop(LS, 3);

        lua_pushinteger(LS, 1342);
        ref.Save(-1);
        lua_pop(LS, 1);

        REQUIRE(0 == lua_gettop(LS));
        ref.Push();
        ref.Push();
        ref.Push();
        REQUIRE(3 == lua_gettop(LS));
        lua_pushinteger(LS, 1342);
        REQUIRE(1 == lua_compare(LS, -1, -2, LUA_OPEQ));
        REQUIRE(1 == lua_compare(LS, -1, -3, LUA_OPEQ));
        REQUIRE(1 == lua_compare(LS, -1, -4, LUA_OPEQ));

        lua_pop(LS, 4);
    }

    SECTION("Destructor")
    {
        {
            awlua::LuaRef ref1;
            awlua::LuaRef ref2;
            awlua::LuaRef ref3;

            lua_newtable(LS);
            REQUIRE(0 == GetRefCount(LS, -1));

            ref1.Save(-1);
            ref2.Save(-1);
            ref3.Save(-1);

            REQUIRE(3 == GetRefCount(LS, -1));
        }

        REQUIRE(0 == GetRefCount(LS, -1));
    }
    
    lua_close(LS);
}