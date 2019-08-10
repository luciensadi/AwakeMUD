#ifndef awlua_ud_h_
#define awlua_ud_h_

#include "awlua_core.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <string>


namespace awlua
{

struct ud_field
{
    const char *field;
    int (*func)(lua_State *);
};


class UdTypeBase
{
public:
    static void NewUd(lua_State *LS, const char *mt_name);
    static void InitType(lua_State *LS, const char *mt_name, const ud_field *get_fields, const ud_field *set_fields, const ud_field *meth_fields);

    static int L_index(lua_State *LS);
    static int L_newindex(lua_State *LS);
    // static int L_
};


template <typename Tptr>
class UdType
{
public:
    static void NewUd(lua_State *LS)
    {
        UdTypeBase::NewUd(LS, mt_name.c_str());
    }

    static Tptr CheckUd(lua_State *LS, int index)
    {
        Tptr rtn = *(static_cast<Tptr*>(luaL_checkudata(LS, index, mt_name.c_str())));
        if (rtn == nullptr)
        {
            luaL_error(LS, "Invalid %s", mt_name.c_str());
        }
        return rtn;
    }

    static void InitType(lua_State *LS)
    {
        UdTypeBase::InitType(LS, mt_name.c_str(), get_fields, set_fields, meth_fields);
    }

    static const std::string mt_name;

    static const ud_field get_fields[];
    static const ud_field set_fields[];
    static const ud_field meth_fields[];
};

template <typename Tptr>
class LuaUdRef
{
public:
    LuaUdRef(Tptr ptr)
        : ref_()
        , ptr_(ptr)
    { }
    ~LuaUdRef()
    {
        if (ref_.IsSet())
        {
            ref_.Push();
            void *ud = lua_touserdata(GetLS(), -1);
            *(static_cast<Tptr*>(ud)) = nullptr;
            // TODO: probably also remove metatable
            lua_pop(GetLS(), 1);
        }
    }
    void Push()
    {
        if (ref_.IsSet())
        {
            ref_.Push();
        }
        else
        {
            UdType<Tptr>::NewUd(GetLS());
            AWLUA_ASSERT(1 == lua_isuserdata(GetLS(), -1));
            void *ud = lua_touserdata(GetLS(), -1);
            AWLUA_ASSERT(nullptr != ud);
            *(static_cast<Tptr*>(ud)) = ptr_;
            ref_.Save(-1);
        }
    }

private:
    LuaRef ref_;
    Tptr ptr_;
};


} // namespace awlua

#endif // awlua_ud_h_