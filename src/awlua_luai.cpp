#include "awlua_luai.h"

#include "awlua_char.h"
#include "awlua_core.h"

#include "structs.h"
#include "interpreter.h"
#include "comm.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <sstream>


ACMD(do_luai)
{
    if (!ch->desc)
        return;

    if (ch->desc->ref_luai.IsSet())
    {
        send_to_char(ch, "Lua interpreter already active!\r\n");
        return;
    }

    lua_State *LS = awlua::GetLS();
    int top = lua_gettop(LS);

    lua_newtable(LS);
    ch->desc->ref_luai.Save(-1);

    send_to_char(ch, "Entered lua interpreter mode.\r\n"
                     "Use @ on a blank line to exit.\r\n"
                     "Use do and end to create multiline chunks.\r\n");

    lua_settop(LS, top);
}

void awlua::luai_handle(descriptor_data *d, const char *comm)
{
    if (!strcmp(comm, "@"))
    {
        SEND_TO_Q("Exited lua interpreter.\r\n", d);
        d->ref_luai.Release();
        return;
    }

    lua_State *LS = GetLS();
    int top = lua_gettop(LS);
    AWLUA_ASSERT(d->ref_luai.IsSet());

    ref::awlua::luai_handle.Push();
    d->ref_luai.Push();

    // TODO: clean up env creation
    if (d->character->ref_env.IsSet())
    {
        d->character->ref_env.Push();
    }
    else
    {
        ref::awlua::new_script_env.Push();
        d->character->ref_ud.Push();
        lua_call(LS, 1, 1);
        d->character->ref_env.Save(-1);
    }

    lua_pushstring(LS, comm);

    if (CallWithTraceback(LS, 3, 1))
    {
        SEND_TO_Q("Error in luai_handle: ", d);
        SEND_TO_Q(lua_tostring(LS, -1), d);
    }
    else if (lua_isstring(LS, -1))
    {
        SEND_TO_Q(lua_tostring(LS, -1), d);
    }
    else if (lua_isnil(LS, -1))
    {
        // do nothing
    }
    else
    {
        SEND_TO_Q("Unexpected return type in luai_handle\r\n", d);
    }

    lua_settop(LS, top);
    return;
}

const char *awlua::luai_get_prompt(descriptor_data *d)
{
    AWLUA_ASSERT(d->ref_luai.IsSet());

    lua_State *LS = GetLS();

    int top = lua_gettop(LS);

    const char *rtn;
    d->ref_luai.Push();
    lua_getfield(LS, -1, "input");

    if (lua_isnil(LS, -1))
    {
        rtn = "lua> ";
    }
    else
    {
        rtn = "lua>> ";
    }
    lua_pop(LS, 2);

    AWLUA_ASSERT(top == lua_gettop(LS));

    return rtn;
}