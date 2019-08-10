#include "../src/awlua_ud.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <catch.hpp>

#define REQUIRE_CSTR_EQ(arg1, arg2) \
    REQUIRE( (!strcmp((arg1), (arg2))) )


struct UdRefTest
{
    UdRefTest()
        : ref(this)
    { }

    awlua::LuaUdRef<UdRefTest *> ref;
};

template<> const std::string awlua::UdType<UdRefTest*>::mt_name("UdRefTest");
template<> const awlua::ud_field awlua::UdType<UdRefTest*>::get_fields[] = 
{
    {0, 0}
};
template<> const awlua::ud_field awlua::UdType<UdRefTest*>::set_fields[] = 
{
    {0, 0}
};
template<> const awlua::ud_field awlua::UdType<UdRefTest*>::meth_fields[] = 
{
    {0, 0}
};

TEST_CASE("LuaUdRef")
{
    lua_State *LS = luaL_newstate();
    luaL_openlibs(LS);
    awlua::SetLS(LS);

    awlua::UdType<UdRefTest*>::InitType(LS);

    REQUIRE(0 == lua_gettop(LS));
    {
        UdRefTest rt;

        rt.ref.Push();
        REQUIRE(1 == lua_gettop(LS));

        void *ud = lua_touserdata(LS, -1);
        UdRefTest *ptr = *(static_cast<UdRefTest**>(ud));
        REQUIRE(ptr == &rt);
    }

    void *ud = lua_touserdata(LS, -1);
    UdRefTest *ptr = *(static_cast<UdRefTest**>(ud));
    REQUIRE(ptr == nullptr);
}

struct UdTypeTest
{
    UdTypeTest()
        : ref(this)
    { }

    awlua::LuaUdRef<UdTypeTest *> ref;

    void meth_a() { meth_a_called = true; }

    int field_a;
    bool meth_a_called;
};

static int L_get_field_a(lua_State *LS)
{
    UdTypeTest *ptr = awlua::UdType<UdTypeTest*>::CheckUd(LS, 1);
    lua_pushinteger(LS, ptr->field_a);
    return 1;
}

static int L_set_field_a(lua_State *LS)
{
    UdTypeTest *ptr = awlua::UdType<UdTypeTest*>::CheckUd(LS, 1);
    ptr->field_a = luaL_checkinteger(LS, 2);
    return 0;
}

static int L_meth_a(lua_State *LS)
{
    UdTypeTest *ptr = awlua::UdType<UdTypeTest*>::CheckUd(LS, 1);
    ptr->meth_a();
    return 0;
}

template<> const std::string awlua::UdType<UdTypeTest*>::mt_name("UdTypeTest");
template<> const awlua::ud_field awlua::UdType<UdTypeTest*>::get_fields[] = 
{
    { "field_a", L_get_field_a },
    {0, 0}
};
template<> const awlua::ud_field awlua::UdType<UdTypeTest*>::set_fields[] = 
{
    { "field_a", L_set_field_a },
    {0, 0}
};
template<> const awlua::ud_field awlua::UdType<UdTypeTest*>::meth_fields[] = 
{
    { "meth_a", L_meth_a },
    {0, 0}
};

TEST_CASE("UdType")
{
    lua_State *LS = luaL_newstate();
    luaL_openlibs(LS);
    awlua::SetLS(LS);

    awlua::UdType<UdTypeTest*>::InitType(LS);

    REQUIRE(0 == lua_gettop(LS));

    SECTION("is_valid")
    {
        {
            UdTypeTest rt;

            rt.ref.Push();
            lua_getfield(LS, -1, "is_valid");

            REQUIRE(LUA_TBOOLEAN == lua_type(LS, -1));
            REQUIRE(true == lua_toboolean(LS, -1));
            lua_pop(LS, 1);
        }

        lua_getfield(LS, -1, "is_valid");
        REQUIRE(LUA_TBOOLEAN == lua_type(LS, -1));
        REQUIRE(false == lua_toboolean(LS, -1));
        lua_pop(LS, 1);
    }

    SECTION("type_name")
    {
        // type_name should work whether ud is valid or not
        {
            UdTypeTest rt;

            rt.ref.Push();
            lua_getfield(LS, -1, "type_name");

            const std::string name1(luaL_checkstring(LS, -1));
            REQUIRE(name1 == "UdTypeTest");
            lua_pop(LS, 1);
        }

        lua_getfield(LS, -1, "type_name");
        const std::string name2(luaL_checkstring(LS, -1));
        REQUIRE(name2 == "UdTypeTest");
        lua_pop(LS, 1);
    }

    SECTION("get")
    {
        UdTypeTest rt;

        rt.ref.Push();
        REQUIRE(1 == lua_gettop(LS));

        lua_getfield(LS, -1, "some_field");
        REQUIRE(1 == lua_isnil(LS, -1));
        lua_pop(LS, 1);

        rt.field_a = 1234;
        lua_getfield(LS, -1, "field_a");
        REQUIRE(1234 == luaL_checkinteger(LS, -1));
        lua_pop(LS, 1);

        rt.field_a = 4321;
        lua_getfield(LS, -1, "field_a");
        REQUIRE(4321 == luaL_checkinteger(LS, -1));
        lua_pop(LS, 1);
    }

    SECTION("set")
    {
        UdTypeTest rt;

        luaL_loadstring(LS,
            "local ud = ...\n"
            "ud.some_field = 1234\n");

        rt.ref.Push();
        int rc = lua_pcall(LS, 1, 0, 0);

        REQUIRE(LUA_ERRRUN == rc);

        const std::string errMsg(lua_tostring(LS, -1));
        REQUIRE(errMsg.find("Can't set field 'some_field'") != std::string::npos);
        lua_pop(LS, 1);

        rt.ref.Push();
        lua_pushinteger(LS, 1234);
        lua_setfield(LS, -2, "field_a");
        REQUIRE(rt.field_a == 1234);

        lua_pushinteger(LS, 4321);
        lua_setfield(LS, -2, "field_a");
        REQUIRE(rt.field_a == 4321);
    }

    SECTION("method")
    {
        UdTypeTest rt;
        
        rt.meth_a_called = false;
        luaL_loadstring(LS,
            "local ud = ...\n"
            "ud:meth_a()\n");
        rt.ref.Push();
        lua_call(LS, 1, 0);
        REQUIRE(true == rt.meth_a_called);

    }

    SECTION("CheckUd")
    {
        {
            UdTypeTest rt;
            luaL_loadstring(LS,
                "myobj = ...\n");
            rt.ref.Push();
            lua_call(LS, 1, 0);

            // Call before destruction, should be no error
            luaL_loadstring(LS,
                "myobj:meth_a()\n");
            lua_call(LS, 0, 0);
        }

        // Call after destruction, should be an error
        luaL_loadstring(LS,
            "myobj:meth_a()\n");
        int rc = lua_pcall(LS, 0, 0, 0);
        REQUIRE(LUA_ERRRUN == rc);
        const std::string errMsg(lua_tostring(LS, -1));
        INFO("errMsg: " << errMsg);
        REQUIRE(errMsg.find("Invalid UdTypeTest") != std::string::npos);
        lua_pop(LS, 1);
    }
}