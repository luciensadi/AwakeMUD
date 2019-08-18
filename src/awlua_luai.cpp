#include "awlua_luai.h"

#include "awlua_char.h"
#include "awlua_core.h"

#include "structs.h"
#include "interpreter.h"
#include "comm.h"
#include "handler.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <sstream>


ACMD(do_luai)
{
    if (!ch->desc)
        return;

    descriptor_data *d = ch->desc;

    if (ch->desc->ref_luai.IsSet())
    {
        send_to_char(ch, "Lua interpreter already active!\r\n");
        return;
    }

    lua_State *LS = awlua::GetLS();
    awlua::LuaRef *p_ref_env;

    char arg1[MAX_INPUT_LENGTH];
    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0')
    {
        // TODO: clean up env creation
        if (!d->character->ref_env.IsSet())
        {
            awlua::ref::awlua::new_script_env.Push();
            d->character->ref_ud.Push();
            lua_call(LS, 1, 1);
            d->character->ref_env.Save(-1);
            lua_pop(LS, 1);
        }

        p_ref_env = &d->character->ref_env;
    }
    else if (!strcmp(arg1, "mob"))
    {
        if (!ch->in_room)
        {
            send_to_char(ch, "You are not in a room!\r\n");
            return;
        }

        char_data *mob = get_char_room(argument, ch->in_room);

        if (!mob)
        {
            send_to_char(ch, "Could not find %s in the room.\r\n", argument);
            return;
        }
        else if (!IS_NPC(mob))
        {
            send_to_char(ch, "Not on players.\r\n");
            return;
        }

        // TODO: clean up env creation
        if (!mob->ref_env.IsSet())
        {
            awlua::ref::awlua::new_script_env.Push();
            mob->ref_ud.Push();
            lua_call(LS, 1, 1);
            mob->ref_env.Save(-1);
            lua_pop(LS, 1);
        }

        p_ref_env = &mob->ref_env;
    }
    else
    {
        send_to_char(ch, "luai              -- open interpreter in your own env\r\n"
                         "luai mob <target> -- open interpreter in env of target mob (in same room)\r\n");
        return;
    }

    if (!p_ref_env)
    {
        AWLUA_ASSERT(false);
        return;
    }

    int top = lua_gettop(LS);

    lua_newtable(LS);
    ch->desc->ref_luai.Save(-1);
    p_ref_env->Push();
    lua_setfield(LS, -2, "env");

    send_to_char(ch, "Entered lua interpreter mode.\r\n"
                     "Use @ on a blank line to exit.\r\n"
                     "Use do and end to create multiline chunks.\r\n");

    lua_settop(LS, top);
}

void awlua::luai_handle(descriptor_data *d, const char *comm)
{
    lua_State *LS = GetLS();
    int top = lua_gettop(LS);
    AWLUA_ASSERT(d->ref_luai.IsSet());

    ref::awlua::luai_handle.Push();
    d->ref_luai.Push();
    lua_pushstring(LS, comm);

    if (CallWithTraceback(LS, 2, 2))
    {
        SEND_TO_Q("Error in luai_handle: ", d);
        SEND_TO_Q(lua_tostring(LS, -1), d);
    }
    else
    {
        AWLUA_ASSERT(1 == lua_isboolean(LS, -2));
        if (lua_isstring(LS, -1))
        {
            SEND_TO_Q(lua_tostring(LS, -1), d);       
        }

        if (1 == lua_toboolean(LS, -2))
        {
            SEND_TO_Q("Exited lua interpreter.\r\n", d);
            d->ref_luai.Release();
        }
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