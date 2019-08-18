#include "../src/awlua_core.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <catch.hpp>


TEST_CASE("luai_handle")
{
    awlua::Init();
    lua_State *LS = awlua::GetLS();
    
    lua_newtable(LS);
    awlua::LuaRef ref_luai;
    ref_luai.Save(-1);
    lua_newtable(LS);
    lua_newtable(LS);
    lua_pushboolean(LS, true);
    lua_setfield(LS, -2, "is_valid");
    lua_setfield(LS, -2, "self");
    lua_setfield(LS, -2, "env");
    lua_pop(LS, 1);

    REQUIRE(0 == lua_gettop(LS));

    SECTION("@")
    {
        awlua::ref::awlua::luai_handle.Push();
        ref_luai.Push();
        lua_pushliteral(LS, "@");
        lua_call(LS, 2, 2);

        REQUIRE(2 == lua_gettop(LS));
        REQUIRE(1 == lua_isboolean(LS, -2));
        REQUIRE(1 == lua_isnil(LS, -1));
        REQUIRE(1 == lua_toboolean(LS, -2));
    }

    SECTION("invalid")
    {
        ref_luai.Push();
        lua_getfield(LS, -1, "env");
        lua_getfield(LS, -1, "self");
        lua_pushboolean(LS, 0);
        lua_setfield(LS, -2, "is_valid");
        lua_pop(LS, 3);
        REQUIRE(0 == lua_gettop(LS));

        awlua::ref::awlua::luai_handle.Push();
        ref_luai.Push();
        lua_pushliteral(LS, "");
        lua_call(LS, 2, 2);

        REQUIRE(2 == lua_gettop(LS));
        REQUIRE(1 == lua_isboolean(LS, -2));
        REQUIRE(1 == lua_isstring(LS, -1));
        REQUIRE(1 == lua_toboolean(LS, -2));

        const std::string output(lua_tostring(LS, -1));
        INFO("output: " << output);
        REQUIRE(output.find("Target is no longer valid.") != std::string::npos);
    }

    SECTION("undefined var, no return")
    {
        awlua::ref::awlua::luai_handle.Push();
        ref_luai.Push();
        lua_pushliteral(LS, "some_undef_var");
        lua_call(LS, 2, 2);

        REQUIRE(2 == lua_gettop(LS));
        REQUIRE(1 == lua_isboolean(LS, -2));
        REQUIRE(1 == lua_isstring(LS, -1));

        REQUIRE(0 == lua_toboolean(LS, -2));
        const std::string output(lua_tostring(LS, -1));
        REQUIRE("nil\r\n" == output);
    }

    SECTION("undefined var, return")
    {
        awlua::ref::awlua::luai_handle.Push();
        ref_luai.Push();
        lua_pushliteral(LS, "return some_undef_var");
        lua_call(LS, 2, 2);

        REQUIRE(2 == lua_gettop(LS));
        REQUIRE(1 == lua_isboolean(LS, -2));
        REQUIRE(1 == lua_isstring(LS, -1));

        REQUIRE(0 == lua_toboolean(LS, -2));
        const std::string output(lua_tostring(LS, -1));
        REQUIRE("nil\r\n" == output);
    }

    SECTION("defined var")
    {
        awlua::ref::awlua::luai_handle.Push();
        ref_luai.Push();
        lua_pushliteral(LS, "myvar = 12345");
        lua_call(LS, 2, 2);

        REQUIRE(2 == lua_gettop(LS));
        REQUIRE(1 == lua_isnil(LS, -1));
        lua_pop(LS, 2);

        SECTION("no return")
        {
            awlua::ref::awlua::luai_handle.Push();
            ref_luai.Push();
            lua_pushliteral(LS, "myvar");
            lua_call(LS, 2, 2);

            REQUIRE(2 == lua_gettop(LS));
            REQUIRE(1 == lua_isboolean(LS, -2));
            REQUIRE(1 == lua_isstring(LS, -1));

            REQUIRE(0 == lua_toboolean(LS, -2));
            {
                const std::string output(lua_tostring(LS, -1));
                REQUIRE("12345\r\n" == output);
            }
        }

        SECTION("return")
        {
            awlua::ref::awlua::luai_handle.Push();
            ref_luai.Push();
            lua_pushliteral(LS, "return myvar");
            lua_call(LS, 2, 2);

            REQUIRE(2 == lua_gettop(LS));
            REQUIRE(1 == lua_isboolean(LS, -2));
            REQUIRE(1 == lua_isstring(LS, -1));

            REQUIRE(0 == lua_toboolean(LS, -2));
            {
                const std::string output(lua_tostring(LS, -1));
                REQUIRE("12345\r\n" == output);
            }
        }
    }

    SECTION("mult result, no return")
    {
        awlua::ref::awlua::luai_handle.Push();
        ref_luai.Push();
        lua_pushliteral(LS, "5,nil,3,true,'abc'");
        lua_call(LS, 2, 2);

        REQUIRE(2 == lua_gettop(LS));
        REQUIRE(1 == lua_isboolean(LS, -2));
        REQUIRE(1 == lua_isstring(LS, -1));
        const std::string output(lua_tostring(LS, -1));
        REQUIRE("5, nil, 3, true, abc\r\n" == output);
    }

    SECTION("mult result, return")
    {
        awlua::ref::awlua::luai_handle.Push();
        ref_luai.Push();
        lua_pushliteral(LS, "return 5,nil,3,true,'abc'");
        lua_call(LS, 2, 2);

        REQUIRE(2 == lua_gettop(LS));
        REQUIRE(1 == lua_isboolean(LS, -2));
        REQUIRE(1 == lua_isstring(LS, -1));
        const std::string output(lua_tostring(LS, -1));
        REQUIRE("5, nil, 3, true, abc\r\n" == output);
    }

    SECTION("incomplete")
    {
        ref_luai.Push();
        lua_getfield(LS, -1, "env");
        lua_getglobal(LS, "table");
        lua_setfield(LS, -2, "table");
        lua_pop(LS, 2);

        awlua::ref::awlua::luai_handle.Push();
        ref_luai.Push();
        lua_getfield(LS, -1, "input");
        REQUIRE(1 == lua_isnil(LS, -1));
        lua_pop(LS, 1);
        lua_pushliteral(LS, "do");
        lua_call(LS, 2, 2);

        REQUIRE(1 == lua_isboolean(LS, -2));
        REQUIRE(0 == lua_toboolean(LS, -2));
        REQUIRE(1 == lua_isnil(LS, -1));
        lua_pop(LS, 2);

        awlua::ref::awlua::luai_handle.Push();
        ref_luai.Push();
        lua_getfield(LS, -1, "input");
        REQUIRE(1 == lua_isstring(LS, -1));
        lua_pop(LS, 1);
        lua_pushliteral(LS, "local res = {}");
        lua_call(LS, 2, 2);

        REQUIRE(1 == lua_isboolean(LS, -2));
        REQUIRE(0 == lua_toboolean(LS, -2));
        REQUIRE(1 == lua_isnil(LS, -1));
        lua_pop(LS, 2);

        awlua::ref::awlua::luai_handle.Push();
        ref_luai.Push();
        lua_getfield(LS, -1, "input");
        REQUIRE(1 == lua_isstring(LS, -1));
        lua_pop(LS, 1);
        lua_pushliteral(LS, "for i = 1,4 do res[i] = i end return table.unpack(res) end");
        lua_call(LS, 2, 2);

        REQUIRE(1 == lua_isboolean(LS, -2));
        REQUIRE(1 == lua_isstring(LS, -1));
        REQUIRE(0 == lua_toboolean(LS, -2));
        const std::string output(lua_tostring(LS, -1));
        REQUIRE("1, 2, 3, 4\r\n" == output);

        ref_luai.Push();
        lua_getfield(LS, -1, "input");
        REQUIRE(1 == lua_isnil(LS, -1));
        lua_pop(LS, 2);
    }

    SECTION("error")
    {
        ref_luai.Push();
        lua_getfield(LS, -1, "env");
        lua_getglobal(LS, "error");
        lua_setfield(LS, -2, "error");
        lua_pop(LS, 2);

        awlua::ref::awlua::luai_handle.Push();
        ref_luai.Push();
        lua_pushliteral(LS, "do");
        lua_call(LS, 2, 2);

        REQUIRE(1 == lua_isboolean(LS, -2));
        REQUIRE(0 == lua_toboolean(LS, -2));
        REQUIRE(1 == lua_isnil(LS, -1));
        lua_pop(LS, 2);

        ref_luai.Push();
        lua_getfield(LS, -1, "input");
        REQUIRE(0 == lua_isnil(LS, -1));
        lua_pop(LS, 2);

        awlua::ref::awlua::luai_handle.Push();
        ref_luai.Push();
        lua_pushliteral(LS, "error('some error') end");
        lua_call(LS, 2, 2);

        REQUIRE(2 == lua_gettop(LS));
        REQUIRE(1 == lua_isboolean(LS, -2));
        REQUIRE(0 == lua_toboolean(LS, -2));
        REQUIRE(1 == lua_isstring(LS, -1));
        const std::string output(lua_tostring(LS, -1));
        INFO("output: " << output);
        REQUIRE(output.find("some error") != std::string::npos);

        ref_luai.Push();
        lua_getfield(LS, -1, "input");
        REQUIRE(1 == lua_isnil(LS, -1));
        lua_pop(LS, 2);
    }

    SECTION("syntax error")
    {
        awlua::ref::awlua::luai_handle.Push();
        ref_luai.Push();
        lua_pushliteral(LS, "do");
        lua_call(LS, 2, 2);

        REQUIRE(1 == lua_isboolean(LS, -2));
        REQUIRE(0 == lua_toboolean(LS, -2));
        REQUIRE(1 == lua_isnil(LS, -1));
        lua_pop(LS, 2);

        ref_luai.Push();
        lua_getfield(LS, -1, "input");
        REQUIRE(0 == lua_isnil(LS, -1));
        lua_pop(LS, 2);

        awlua::ref::awlua::luai_handle.Push();
        ref_luai.Push();
        lua_pushliteral(LS, "32 32323 2 r 2adf2");
        lua_call(LS, 2, 2);

        REQUIRE(2 == lua_gettop(LS));
        REQUIRE(1 == lua_isboolean(LS, -2));
        REQUIRE(0 == lua_toboolean(LS, -2));
        REQUIRE(1 == lua_isstring(LS, -1));
        const std::string output(lua_tostring(LS, -1));
        REQUIRE(output.find("unexpected symbol near '32") != std::string::npos);

        ref_luai.Push();
        lua_getfield(LS, -1, "input");
        REQUIRE(1 == lua_isnil(LS, -1));
        lua_pop(LS, 2);
    }

    SECTION("timeout")
    {
        awlua::ref::awlua::luai_handle.Push();
        ref_luai.Push();
        lua_pushliteral(LS, "do");
        lua_call(LS, 2, 2);

        REQUIRE(1 == lua_isboolean(LS, -2));
        REQUIRE(0 == lua_toboolean(LS, -2));
        REQUIRE(1 == lua_isnil(LS, -1));
        lua_pop(LS, 2);

        ref_luai.Push();
        lua_getfield(LS, -1, "input");
        REQUIRE(0 == lua_isnil(LS, -1));
        lua_pop(LS, 2);

        awlua::ref::awlua::luai_handle.Push();
        ref_luai.Push();
        lua_pushliteral(LS, "while true do end end");
        lua_call(LS, 2, 2);

        REQUIRE(2 == lua_gettop(LS));
        REQUIRE(1 == lua_isboolean(LS, -2));
        REQUIRE(0 == lua_toboolean(LS, -2));
        REQUIRE(1 == lua_isstring(LS, -1));
        const std::string output(lua_tostring(LS, -1));
        INFO("output: " << output);
        REQUIRE(output.find("Script timeout") != std::string::npos);

        ref_luai.Push();
        lua_getfield(LS, -1, "input");
        REQUIRE(1 == lua_isnil(LS, -1));
        lua_pop(LS, 2);
    }
}
