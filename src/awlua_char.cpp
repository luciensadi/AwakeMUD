#include "awlua_char.h"

#include "types.h"
#include "interpreter.h"
#include "utils.h"
#include "structs.h"


template<> const std::string awlua::UdChar::mt_name("char");


static int L_char_get_name(lua_State *LS)
{
    char_data *ch = awlua::UdChar::CheckUd(LS, 1);
    lua_pushstring(LS, GET_CHAR_NAME(ch));
    return 1;
}

static int L_char_say(lua_State *LS)
{
    ACMD_CONST(do_say);

    char_data *ch = awlua::UdChar::CheckUd(LS, 1);
    do_say(ch, luaL_checkstring(LS, 2), 0, 0);
    return 0;
}


template<> const awlua::ud_field awlua::UdChar::get_fields[] = 
{
    {"name", L_char_get_name},
    {0, 0}
};
template<> const awlua::ud_field awlua::UdChar::set_fields[] = 
{
    {0, 0}
};
template<> const awlua::ud_field awlua::UdChar::meth_fields[] = 
{
    {"say", L_char_say},
    {0, 0}
};
